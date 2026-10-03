#include "Combat/SCLCombatComponent.h"
#include "Characters/Components/SCLActionMovementComponent.h"
#include "Characters/SCLEnemyCharacter.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimNotifyQueue.h"
#include "Targeting/SCLTargetingComponent.h"
#include "MotionWarpingComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "AbilitySystem/Effects/SCLAttackingStateEffect.h"
#include "AbilitySystem/Effects/SCLLightAttackCostEffect.h"

#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Characters/Player/SCLWeaponPresentationComponent.h"
#include "Characters/Player/SCLPlayerComboComponent.h"
#include "Combat/SCLHitTraceComponent.h"
#include "Combat/SCLWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/SCLAttackData.h"
#include "Engine/World.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "SoulCombatLab.h"
#include "UObject/ConstructorHelpers.h"

// 阅读路线：请求入口 → 玩家/敌人执行 → 播放与结束 → 通知/命中。
// 连招选择在 PlayerCombo，跨动作的移动限制在 ActionMovement；攻击内部流程集中在这份实现。

// ===== 01 装配与退出：创建默认配置、装备武器；退出统一取消攻击 =====

USCLCombatComponent::USCLCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	DefaultWeaponClass = ASCLWeapon::StaticClass();
	PlayerAttackCostEffectClass = USCLLightAttackCostEffect::StaticClass();
	PlayerAttackingStateEffectClass = USCLAttackingStateEffect::StaticClass();

	static ConstructorHelpers::FObjectFinder<USCLAttackData> LightAttackDataAsset(
		TEXT("/Game/SoulCombatLab/Data/Attacks/DA_LightAttack.DA_LightAttack"));
	LightAttackData = LightAttackDataAsset.Object;
}

/**
 * 角色进入世界后生成并挂接默认武器；武器命中事件在装备流程中绑定。
 */
void USCLCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	SpawnAndEquipDefaultWeapon();
}

/**
 * 先使旧 Montage 回调失效，再恢复移动、清理范围定时器、武器和攻击状态。
 */
void USCLCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelActiveAttack();
	if (IsValid(EquippedWeapon) && !EquippedWeapon->IsActorBeingDestroyed())
	{
		EquippedWeapon->Destroy();
	}
	EquippedWeapon = nullptr;
	ActiveAttackData = nullptr;
	ResetPendingAttackProfile();
	ResetActiveAttackProfile();
	ResetComboState();

	Super::EndPlay(EndPlayReason);
}

/**
 * 只在有 Mesh、世界和武器类且尚未装备时生成武器。
 * 设置 Owner/Instigator、附着 Socket，再绑定 TraceHit，供共享伤害处理使用。
 */
void USCLCombatComponent::SpawnAndEquipDefaultWeapon()
{
	AActor* const ComponentOwner = GetOwner();
	UWorld* const World = GetWorld();
	if (ComponentOwner == nullptr || World == nullptr || DefaultWeaponClass == nullptr || IsValid(EquippedWeapon))
	{
		return;
	}

	USkeletalMeshComponent* const OwnerMesh = ComponentOwner->FindComponentByClass<USkeletalMeshComponent>();
	if (OwnerMesh == nullptr)
	{
		UE_LOG(LogSoulCombatLab, Warning, TEXT("%s cannot equip a weapon without a skeletal mesh component."), *GetNameSafe(ComponentOwner));
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = ComponentOwner;
	SpawnParameters.Instigator = Cast<APawn>(ComponentOwner);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	EquippedWeapon = World->SpawnActor<ASCLWeapon>(DefaultWeaponClass, FTransform::Identity, SpawnParameters);
	if (!IsValid(EquippedWeapon))
	{
		UE_LOG(LogSoulCombatLab, Warning, TEXT("%s failed to spawn its default weapon."), *GetNameSafe(ComponentOwner));
		return;
	}

	EquippedWeapon->AttachToComponent(
		OwnerMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		WeaponAttachSocket);
	EquippedWeapon->SetActorRelativeScale3D(FVector{EnemyWeaponVisualScale});

	if (USCLHitTraceComponent* const HitTrace = EquippedWeapon->GetHitTraceComponent())
	{
		HitTrace->OnTraceHit.AddDynamic(this, &USCLCombatComponent::HandleTraceHit);
	}
}

// ===== 02 请求入口：外部只发请求，玩家和敌人在这里分路 =====

/**
 * 旧蓝图兼容入口；返回是否接收请求，不等同于已经播放下一刀。
 * 正常玩家输入通过 Character 请求 GAS，以完成状态和体力检查。
 */
bool USCLCombatComponent::StartLightAttack()
{
	return RequestAttack();
}

bool USCLCombatComponent::RequestAttack()
{
	return RequestAttack(ESCLPlayerAttackInput::Light) != ESCLAttackRequestResult::Rejected;
}

bool USCLCombatComponent::UsesPlayerMoveset() const
{
	return Cast<ASCLPlayerCharacter>(GetOwner()) != nullptr;
}

ESCLAttackRequestResult USCLCombatComponent::RequestAttack(const ESCLPlayerAttackInput Input)
{
	// 从这里分两条路读：玩家由 Moveset 选招，敌人由通用 AttackData 选招。
	if (ASCLPlayerCharacter* const Player = Cast<ASCLPlayerCharacter>(GetOwner()))
	{
		// 玩家缺少完整 Moveset 时直接拒绝，不能误走敌人的攻击路径。
		return HasValidPlayerAttackData()
			? RequestPlayerAttack(*Player, Input) : ESCLAttackRequestResult::Rejected;
	}
	return RequestEnemyAttack();
}

// ===== 03 玩家攻击：Combo 选招 → 起播检查 → 播放 → 扣费与确认 =====

/**
 * 编排玩家请求：先向 Combo 选招，缓存直接返回，待执行节点交给本类起播。
 * Combo 没有 Combat 引用，因此不会再出现 Combat → Combo → Combat 的嵌套调用。
 */
ESCLAttackRequestResult USCLCombatComponent::RequestPlayerAttack(ASCLPlayerCharacter& Player, const ESCLPlayerAttackInput Input)
{
	const FSCLPlayerComboSelection Selection = Player.GetPlayerComboComponent()->SelectAttack(Input);
	if (Selection.bBuffered)
	{
		return ESCLAttackRequestResult::Buffered;
	}
	// INDEX_NONE 表示 Combo 没有选出有效节点，例如没有后继招式或 Moveset 索引无效。
	// 只有拿到有效 StepIndex，Combat 才允许尝试真正起播；否则本次请求必须拒绝。
	return Selection.StepIndex != INDEX_NONE && StartPlayerAttackStep(Selection.StepIndex)
		? ESCLAttackRequestResult::Executed : ESCLAttackRequestResult::Rejected;
}

/**
 * 玩家执行仍需要共享 AttackData（伤害与韧性基础参数）和完整 Moveset。
 */
bool USCLCombatComponent::HasValidPlayerAttackData() const
{
	const ASCLPlayerCharacter* const Player = Cast<ASCLPlayerCharacter>(GetOwner());
	return LightAttackData != nullptr && Player != nullptr && Player->GetPlayerComboComponent() != nullptr &&
		Player->GetPlayerComboComponent()->HasValidMoveset();
}

/**
 * 执行 ComboComponent 已选定的 Steps 下标，不在这里重新选择轻重分支。
 * 起手、窗口内输入、Notify 消费缓存都共用这里：检查 → 起播 → 扣费 → 确认节点/状态。
 * 费用直接取 Step，不能在确认节点后重新查“下一招”，否则会把起手按后继价格扣费。
 * 动画失败不扣费；费用提交失败则取消已起播动作。返回 true 才表示完整启动成功。
 */
bool USCLCombatComponent::StartPlayerAttackStep(const int32 StepIndex)
{
	// 1. 先检查选定节点和执行资源。失败请求不能打断收刀/拔刀等当前动作。
	ASCLPlayerCharacter* const Player = Cast<ASCLPlayerCharacter>(GetOwner());
	USCLPlayerComboComponent* const PlayerCombo = Player != nullptr ? Player->GetPlayerComboComponent() : nullptr;
	const FSCLPlayerAttackStep* const Step = PlayerCombo != nullptr ? PlayerCombo->GetStep(StepIndex) : nullptr;
	if (Step == nullptr || Step->Montage == nullptr || LightAttackData == nullptr)
	{
		return false;
	}
	UAnimInstance* const Anim = Player != nullptr ? Player->GetMesh()->GetAnimInstance() : nullptr;
	if (Anim == nullptr || Step->Montage->GetPlayLength() <= 0.0F ||
		Step->Montage->GetSkeleton() == nullptr || Step->Montage->SlotAnimTracks.IsEmpty())
	{
		return false;
	}
	USCLAbilitySystemComponent* const ASC = Player->GetSCLAbilitySystemComponent();
	if (ASC == nullptr || !FMath::IsFinite(Step->StaminaCost) || Step->StaminaCost < 0.0F ||
		ASC->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()) < Step->StaminaCost ||
		PlayerAttackCostEffectClass == nullptr || PlayerAttackingStateEffectClass == nullptr)
	{
		// 每次真正起播都重新检查，包括缓存后被格挡等动作消耗了体力的情况。
		return false;
	}
	// 在切换动作前准备两份 Effect。它们不是两个动画：
	//   CostSpec 负责扣 Stamina；StateSpec 负责添加“正在攻击”状态。
	// 配置无效时拒绝，不等动画起播后才发现缺少费用/状态。
	const FGameplayEffectSpecHandle CostSpec = Step->StaminaCost > 0.0F
		? ASC->MakeOutgoingSpec(PlayerAttackCostEffectClass, 1.0F, ASC->MakeEffectContext())
		: FGameplayEffectSpecHandle{};
	const FGameplayEffectSpecHandle StateSpec = ASC->MakeOutgoingSpec(
		PlayerAttackingStateEffectClass, 1.0F, ASC->MakeEffectContext());
	if ((Step->StaminaCost > 0.0F && !CostSpec.IsValid()) || !StateSpec.IsValid())
	{
		return false;
	}
	// 2. 只有具备起播资格后才切换动作；先废弃旧实例身份，防止同步回调误清新招。
	// 收刀属于表现组件；实际攻击前中断收刀并恢复握刀，不改变本招扣费流程。
	if (auto* Presentation = Player->GetWeaponPresentationComponent()) Presentation->PrepareForAction();
	UAnimMontage* const NewMontage = Step->Montage;
	// CancelAttackPlayback 先废弃旧实例身份；旧回调不能清理即将启动的新招。
	CancelAttackPlayback();
	EndWeaponTrace();
	ResetComboState();
	ResetActiveAttackProfile();
	ActiveAttackData = LightAttackData;
	// 这里是“请求播放”的入口；真正调用 AnimInstance->Montage_Play 的位置在
	// PlayAttackMontage() 内部。只有它返回 true，才说明动画确实成功起播。
	if (!PlayAttackMontage(*NewMontage, 1.0F, nullptr, false))
	{
		CancelActiveAttack();
		return false;
	}
	ActiveAttackProfile.AttackName = Step->AttackName;
	ActiveAttackProfile.DamageMultiplier = Step->DamageMultiplier;
	ActiveAttackProfile.PoiseDamageMultiplier = Step->PoiseDamageMultiplier;
	ActiveAttackProfile.AttackStateDuration = GetActiveAttackStateDuration();
	bHasActiveAttackProfile = true;
	// 3. Montage 成功才扣费。即时与延迟接招不再区分提交者，更不会两边各扣一次。
	if (ASC->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()) < Step->StaminaCost)
	{
		CancelActiveAttack();
		return false;
	}
	if (CostSpec.IsValid())
	{
		// Effect 使用加法修改 Stamina，因此负值才是消耗；零成本不创建扣费 Effect。
		CostSpec.Data->SetSetByCallerMagnitude(SCLGameplayTags::Data_Cost_Stamina, -Step->StaminaCost);
		ASC->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());
		ASC->RestartStaminaRegenerationDelay();
	}
	// 4. 确认“当前招”并施加状态。只缓存的请求永远走不到这里。
	PlayerCombo->ConfirmAttackStarted(StepIndex);
	StateSpec.Data->SetDuration(GetActiveAttackStateDuration(), true);
	ASC->ApplyGameplayEffectSpecToSelf(*StateSpec.Data.Get());
	UE_LOG(LogSoulCombatLab, Log, TEXT("Ghost attack started: Owner=%s Step=%d Attack=%s Cost=%.0f"),
		*GetNameSafe(GetOwner()), StepIndex, *Step->AttackName.ToString(), Step->StaminaCost);
	return true;
}

// ===== 04 敌人攻击：消费配置 → 播放 → 安排 Section 与范围命中 =====

/**
 * 敌人使用共享 AttackData 的 Montage Section，玩家 Moveset 不走此路径。
 * 正在播放时只能缓存或安排后继段；新起播返回 Executed，等待后继返回 Buffered。
 */
ESCLAttackRequestResult USCLCombatComponent::RequestEnemyAttack()
{
	// 以下是敌人使用的通用 Montage 连段；与玩家的轻重分支互不混用。
	AActor* const ComponentOwner = GetOwner();
	UAnimMontage* const AttackMontage =
		LightAttackData != nullptr ? LightAttackData->GetAttackMontage() : nullptr;
	USkeletalMeshComponent* const OwnerMesh =
		ComponentOwner != nullptr ? ComponentOwner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	UAnimInstance* const AnimInstance = OwnerMesh != nullptr ? OwnerMesh->GetAnimInstance() : nullptr;
	if (AnimInstance == nullptr || AttackMontage == nullptr)
	{
		return ESCLAttackRequestResult::Rejected;
	}
	if (AnimInstance->Montage_IsPlaying(AttackMontage))
	{
		return ActiveAttackData ? EnemyCombo.Buffer(*ActiveAttackData, *AnimInstance) : ESCLAttackRequestResult::Rejected;
	}

	// 同一资产重新播放时，旧实例可能仍在混出。
	// 先废弃旧实例身份，再 Montage_Play，防止排队的旧回调影响新攻击。
	CancelAttackPlayback();
	EndWeaponTrace();
	ResetComboState();
	CancelAreaImpact();
	ActivatePendingAttackProfile();
	ActiveAttackData = LightAttackData;
	if (!PlayAttackMontage(*AttackMontage, ActiveAttackProfile.MontagePlayRate,
		AttackTarget.Get(), ActiveAttackProfile.bWeaponTraceEnabled))
	{
		CancelActiveAttack();
		return ESCLAttackRequestResult::Rejected;
	}
	EnemyCombo.Start(*ActiveAttackData, *AnimInstance, ActiveAttackProfile.MontageStepIndex, ActiveAttackProfile.MontageStepCount);
	const auto* FirstStep = ActiveAttackData->FindComboStep(EnemyCombo.GetActiveStep());
	const FName ActiveSectionName = FirstStep ? FirstStep->MontageSection : NAME_None;
	ScheduleAreaImpact();

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Attack started: Owner=%s Attack=%s Tag=%s Section=%s PlayRate=%.2f DamageMultiplier=%.2f PoiseMultiplier=%.2f Parryable=%s StateDuration=%.2f"),
		*GetNameSafe(ComponentOwner),
		ActiveAttackProfile.AttackName.IsNone()
			? TEXT("LightAttack")
			: *ActiveAttackProfile.AttackName.ToString(),
		*LightAttackData->GetAttackTag().ToString(),
		*ActiveSectionName.ToString(),
		ActiveAttackProfile.MontagePlayRate,
		ActiveAttackProfile.DamageMultiplier,
		ActiveAttackProfile.PoiseDamageMultiplier,
		IsActiveAttackParryable() ? TEXT("true") : TEXT("false"),
		ActiveAttackProfile.AttackStateDuration);

	return ESCLAttackRequestResult::Executed;
}

// 玩家节点使用 Moveset；敌人 Section 反查交给自己的连段状态。
int32 USCLCombatComponent::ResolveCurrentComboStepIndex() const
{
	if (const auto* Player = Cast<ASCLPlayerCharacter>(GetOwner()))
		if (Player->GetPlayerComboComponent()->IsComboActive()) return INDEX_NONE;
	const auto* Character = Cast<ASCLCharacterBase>(GetOwner());
	const UAnimInstance* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
	return EnemyCombo.Resolve(ActiveAttackData, Anim);
}

// ===== 05 播放与结束：实例身份、委托、取消、两种结束时刻的汇合 =====

bool USCLCombatComponent::PlayAttackMontage(UAnimMontage& Montage, float Rate, AActor* FacingTarget, bool bTrackWindup)
{
	auto* Character = Cast<ASCLCharacterBase>(GetOwner());
	UAnimInstance* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
	// 1. 先验资源与倍率。无效请求不取消目前正在播放的攻击。
	if (!Anim || !FMath::IsFinite(Rate) || Rate <= 0 || Montage.GetPlayLength() <= 0 || !Montage.GetSkeleton() || Montage.SlotAnimTracks.IsEmpty()) return false;
	// 2. 撤销旧实例，再准备本次位移；动画结束时必须释放同名申请。
	CancelAttackPlayback();
	Playback.FacingTarget = FacingTarget;
	PrepareAttackMovement(*Anim, Montage, bTrackWindup);
	const float Duration = Anim->Montage_Play(&Montage, Rate);
	auto* Instance = Anim->GetActiveInstanceForMontage(&Montage);
	if (Duration <= 0 || !Instance) { ReleaseAttackMovement(); return false; }
	// 3. 只有真正起播才登记身份。委托附带此 ID，同一 Montage 重新播放也不会混淆。
	Playback.Montage = &Montage;
	Playback.InstanceId = Instance->GetInstanceID();
	FOnMontageEnded Ended;
	Ended.BindUObject(this, &USCLCombatComponent::HandleAttackMontageEnded, Playback.InstanceId);
	Anim->Montage_SetEndDelegate(Ended, &Montage);
	FOnMontageBlendingOutStarted Blending;
	Blending.BindUObject(this, &USCLCombatComponent::HandleAttackMontageBlendingOut, Playback.InstanceId);
	Anim->Montage_SetBlendingOutDelegate(Blending, &Montage);
	return true;
}

void USCLCombatComponent::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 InstanceId)
{
	if (InstanceId == Playback.InstanceId && Montage == Playback.Montage) FinishAttackPlayback(bInterrupted);
}

void USCLCombatComponent::HandleAttackMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int32 InstanceId)
{
	if (bInterrupted && InstanceId == Playback.InstanceId && Montage == Playback.Montage) FinishAttackPlayback(true);
}

// 普通结束先释放播放状态，再清理招式；范围查询仍在等待时，保留本次伤害配置。
void USCLCombatComponent::FinishAttackPlayback(bool bInterrupted)
{
	Playback.InstanceId = INDEX_NONE;
	Playback.Montage.Reset();
	ReleaseAttackMovement();
	if (bInterrupted) { CancelActiveAttack(); return; }
	EndWeaponTrace();
	FinishAttackIfReady();
}

void USCLCombatComponent::CancelAttackPlayback()
{
	// 先撤销身份再 Stop：Stop 可能立即回调混出委托，旧回调凭 ID 判断后直接退出。
	UAnimInstance* Anim = Playback.AnimInstance.Get();
	UAnimMontage* Montage = Playback.Montage.Get();
	Playback.InstanceId = INDEX_NONE;
	Playback.Montage.Reset();
	ReleaseAttackMovement();
	if (Anim && Montage) Anim->Montage_Stop(0.08F, Montage);
}

/**
 * 取消当前攻击并恢复角色：先废弃播放实例 ID，再停止动画；随后撤销范围查询、轨迹与连招。
 * 顺序很重要，Montage_Stop 可能同步触发旧动画的委托。
 */
void USCLCombatComponent::CancelActiveAttack()
{
	// 取消不是只停动画：还要停止命中检测，并清掉缓存、攻击参数和移动限制。
	CancelAttackPlayback();
	CancelAreaImpact();

	EndWeaponTrace();
	ActiveAttackData = nullptr;
	ResetPendingAttackProfile();
	ResetActiveAttackProfile();
	ResetComboState();
}

void USCLCombatComponent::FinishAttackIfReady()
{
	// 动画和范围查询都结束才释放攻击数据；本函数只做结束汇合，不播放或停止动画。
	if (Playback.IsPlaying() || AreaImpact.bPending) return;
	ActiveAttackData = nullptr;
	ResetActiveAttackProfile();
	ResetComboState();
}

/**
 * 清理敌人 Section 状态，并同步重置玩家 ComboComponent；播放状态由 Cancel / Finish 路径处理。
 */
void USCLCombatComponent::ResetComboState()
{
	EnemyCombo.Reset();
	if (const ASCLPlayerCharacter* const Player = Cast<ASCLPlayerCharacter>(GetOwner()))
	{
		if (USCLPlayerComboComponent* const PlayerCombo = Player->GetPlayerComboComponent())
		{
			PlayerCombo->ResetCombo();
		}
	}
}

// ===== 06 攻击位移：跟转与 Warp；共享限制仍由 ActionMovement 管理 =====

// 起手时锁朝向，前摇可跟转；有效帧开始后由武器通知停止跟转。
void USCLCombatComponent::PrepareAttackMovement(UAnimInstance& AnimInstance, UAnimMontage& Montage, bool bTrackWindup)
{
	ReleaseAttackMovement();
	for (FSlotAnimationTrack& Slot : Montage.SlotAnimTracks)
	{
		for (FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
		{
			if (UAnimSequence* const Sequence = Cast<UAnimSequence>(Segment.GetAnimReference()))
			{
				// 修改本进程内的动画播放设置；此路径不把这些设置保存回源动画资产。
				Sequence->bEnableRootMotion = true;
				Sequence->bForceRootLock = true;
				Sequence->RootMotionRootLock = ERootMotionRootLock::AnimFirstFrame;
			}
		}
	}

	Playback.AnimInstance = &AnimInstance;
	// 从攻击 Montage 提取根运动，再由 CharacterMovement 应用位移。
	// IgnoreRootMotion 会丢弃已提取的位移，动画即使开启根运动也可能看起来在原地播放。
	Cast<ASCLCharacterBase>(GetOwner())->GetActionMovementComponent()->RequestRootMotionMode(TEXT("Attack"), ERootMotionMode::RootMotionFromMontagesOnly);
	if (ASCLCharacterBase* const Character = Cast<ASCLCharacterBase>(GetOwner()))
	{
		UCharacterMovementComponent* const Movement = Character->GetCharacterMovement();
		Movement->StopMovementImmediately();
		Character->ConsumeMovementInputVector();
		Character->SetMovementFacingLocked(TEXT("Attack"), true);
		if (ASCLPlayerCharacter* const Player = Cast<ASCLPlayerCharacter>(Character))
		{
			if (const USCLTargetingComponent* const Targeting = Player->FindComponentByClass<USCLTargetingComponent>())
			{
				if (AActor* const Target = Targeting->GetCurrentTarget())
				{
					const FVector Direction = Target->GetActorLocation() - Character->GetActorLocation();
					Character->SetActorRotation(FRotator{0.0F, Direction.Rotation().Yaw, 0.0F});
				}
			}
			ConfigurePlayerAttackWarp(*Player, Montage);
		}
		else if (Playback.FacingTarget.IsValid() && bTrackWindup)
		{
			Playback.WindupElapsed = 0.0F;
			GetWorld()->GetTimerManager().SetTimer(Playback.WindupTimer, this,
				&USCLCombatComponent::UpdateWindupFacing, 0.02F, true);
		}
	}
}

/** 目标有效且路径无遮挡时生成 Warp Target；过远、校正距离过大或无根运动时保留原动画。 */
void USCLCombatComponent::ConfigurePlayerAttackWarp(ASCLPlayerCharacter& Player, UAnimMontage& Montage)
{
	UMotionWarpingComponent* Warp = Player.GetMotionWarpingComponent();
	AActor* Target = Player.GetTargetingComponent() ? Player.GetTargetingComponent()->GetCurrentTarget() : nullptr;
	if (!Warp || !IsValid(Target) || !Montage.HasRootMotion()) return;
	const FVector ToTarget = Target->GetActorLocation() - Player.GetActorLocation();
	const float Distance = ToTarget.Size2D();
	if (Distance <= AttackWarpStandOff || Distance > AttackWarpMaxTargetDistance) return;
	const FVector Desired = Target->GetActorLocation() - ToTarget.GetSafeNormal2D() * AttackWarpStandOff;
	if (FVector::Dist2D(Desired, Player.GetActorLocation()) > AttackWarpMaxCorrection) return;
	const UCapsuleComponent* Capsule = Player.GetCapsuleComponent();
	if (!Capsule || !GetWorld()) return;
	FCollisionQueryParams Query{SCENE_QUERY_STAT(SCLAttackWarpPath), false, &Player};
	Query.AddIgnoredActor(Target);
	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, Player.GetActorLocation(), Desired, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query)) return;
	Warp->AddOrUpdateWarpTargetFromLocationAndRotation(TEXT("SCL_Attack"), Desired, Player.GetActorRotation());
}

/**
 * 敌人前摇定时回调，最多跟转 0.30 秒；目标失效或攻击结束就停止。
 */
void USCLCombatComponent::UpdateWindupFacing()
{
	Playback.WindupElapsed += 0.02F;
	if (!Playback.FacingTarget.IsValid() || !Playback.IsPlaying() || Playback.WindupElapsed > 0.30F)
	{
		GetWorld()->GetTimerManager().ClearTimer(Playback.WindupTimer);
		return;
	}
	const FVector Direction = Playback.FacingTarget->GetActorLocation() - GetOwner()->GetActorLocation();
	const FRotator Desired{0.0F, Direction.Rotation().Yaw, 0.0F};
	GetOwner()->SetActorRotation(FMath::RInterpConstantTo(GetOwner()->GetActorRotation(), Desired, 0.02F, 360.0F));
}

void USCLCombatComponent::StopWindupFacing()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(Playback.WindupTimer);
}

/**
 * 成对撤销攻击移动限制：停止跟转、恢复原 RootMotionMode 和 Attack 朝向锁。
 */
void USCLCombatComponent::ReleaseAttackMovement()
{
	if (GetWorld() != nullptr) GetWorld()->GetTimerManager().ClearTimer(Playback.WindupTimer);
	if (ASCLPlayerCharacter* Player = Cast<ASCLPlayerCharacter>(GetOwner()))
		Player->GetMotionWarpingComponent()->RemoveWarpTarget(TEXT("SCL_Attack"));
	if (auto* Character = Cast<ASCLCharacterBase>(GetOwner()))
	{
		Character->GetActionMovementComponent()->ReleaseRootMotionMode(TEXT("Attack"));
		Character->SetMovementFacingLocked(TEXT("Attack"), false);
	}
	Playback.AnimInstance.Reset();
}

// ===== 07 动画通知：先核对实例，再处理接招窗口和武器采样 =====

/**
 * 核对 Notify 的 Montage 实例 ID；同一资产的新旧播放实例也必须分别识别。
 */
bool USCLCombatComponent::IsActiveAttackNotify(const FAnimNotifyEventReference& EventReference) const
{
	const auto* Context = EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
	return ActiveAttackData && Playback.IsPlaying() && Context && Context->MontageInstanceID == Playback.InstanceId;
}

/**
 * ComboWindow Notify Begin 转来的窗口事件。
 * 玩家让 ComboComponent 消费 Moveset 缓存；敌人安排 Montage Section 后继。
 */
void USCLCombatComponent::OpenComboWindow()
{
	// 只有玩家连招处于活动状态时才交给 PlayerComboComponent；否则使用通用 Section 连段路径。
	const ASCLPlayerCharacter* const Player = Cast<ASCLPlayerCharacter>(GetOwner());
	USCLPlayerComboComponent* const PlayerCombo = Player != nullptr ? Player->GetPlayerComboComponent() : nullptr;
	if (PlayerCombo != nullptr && PlayerCombo->IsComboActive())
	{
		// Combo 只交出缓存节点；真正接招仍走与起手完全相同的执行/扣费函数。
		const int32 StepIndex = PlayerCombo->OpenComboWindow();
		if (StepIndex != INDEX_NONE)
		{
			StartPlayerAttackStep(StepIndex);
		}
		return;
	}
	const auto* Character = Cast<ASCLCharacterBase>(GetOwner());
	UAnimInstance* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (ActiveAttackData && Anim) EnemyCombo.OpenWindow(*ActiveAttackData, *Anim);
}

/**
 * ComboWindow Notify End 关闭窗口，并清除玩家连招组件尚未消费的输入。
 */
void USCLCombatComponent::CloseComboWindow()
{
	EnemyCombo.CloseWindow();
	if (const ASCLPlayerCharacter* const Player = Cast<ASCLPlayerCharacter>(GetOwner()))
	{
		if (USCLPlayerComboComponent* const PlayerCombo = Player->GetPlayerComboComponent())
		{
			PlayerCombo->CloseComboWindow();
		}
	}
}

/**
 * WeaponTrace Notify Begin 进入命中窗口时调用。
 * 要求当前攻击存在，且Playback 保存有效播放实例；范围攻击可禁用武器检测，避免双重伤害。
 * 返回 true 表示检测成功开启，同时开始挥刀效果。
 */
bool USCLCombatComponent::BeginWeaponTrace()
{
	if (ActiveAttackData == nullptr || !Playback.IsPlaying() || !Playback.AnimInstance.IsValid() || !Playback.Montage.IsValid()) return false;
	// 命中窗口开始就固定面向，挥刀有效帧不再追踪目标转向。
	StopWindupFacing();
	if (bHasActiveAttackProfile && !ActiveAttackProfile.bWeaponTraceEnabled)
	{
		return false;
	}

	USCLHitTraceComponent* const HitTrace =
		IsValid(EquippedWeapon) ? EquippedWeapon->GetHitTraceComponent() : nullptr;
	if (HitTrace == nullptr || !HitTrace->BeginTrace()) return false;
	TraceSampling.SampleTime = Playback.AnimInstance->Montage_GetPosition(Playback.Montage.Get());
	TraceSampling.InstanceId = Playback.InstanceId;
	TraceSampling.bPending = false;
	TraceSampling.bEndAfterPose = false;
	if (!TraceSampling.PoseReadyHandle.IsValid())
	{
		auto* Mesh = Playback.AnimInstance->GetSkelMeshComponent();
		TraceSampling.Mesh = Mesh;
		TraceSampling.OriginalTickOption = Mesh->VisibilityBasedAnimTickOption;
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		TraceSampling.PoseReadyHandle = Mesh->RegisterOnBoneTransformsFinalizedDelegate(
			FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &USCLCombatComponent::SampleWeaponTraceAfterPose, Playback.InstanceId));
	}
	EquippedWeapon->BeginSwingEffects();
	return true;
}

/**
 * WeaponTrace Notify Tick 逐帧更新扫掠，仅当前攻击有效时转发到武器。
 */
void USCLCombatComponent::TickWeaponTrace(const float AnimationWindowEnd)
{
	if (ActiveAttackData == nullptr || !Playback.IsPlaying() || !Playback.AnimInstance.IsValid() || !Playback.Montage.IsValid()) return;
	// CharacterMovement 的根运动路径可能在骨骼求值前发 Notify。
	// Notify 只登记“这一帧要检测”；不能在这里读尚未完成的 Socket 姿势。
	TraceSampling.WindowEnd = AnimationWindowEnd;
	TraceSampling.bPending = true;
}

void USCLCombatComponent::FinishWeaponTraceWindow(const float AnimationWindowEnd, const bool bReachedEnd)
{
	if (!bReachedEnd) { EndWeaponTrace(); return; }
	TickWeaponTrace(AnimationWindowEnd);
	TraceSampling.bEndAfterPose = true;
}

void USCLCombatComponent::SampleWeaponTraceAfterPose(const int32 InstanceId)
{
	if (InstanceId != Playback.InstanceId || InstanceId != TraceSampling.InstanceId || !TraceSampling.bPending) return;
	if (!Playback.AnimInstance.IsValid() || !Playback.Montage.IsValid()) { EndWeaponTrace(); return; }
	const bool bFinish = TraceSampling.bEndAfterPose;
	TraceSampling.bPending = false;
	if (bHasActiveAttackProfile && !ActiveAttackProfile.bWeaponTraceEnabled)
	{
		return;
	}

	if (IsValid(EquippedWeapon))
	{
		if (USCLHitTraceComponent* const HitTrace = EquippedWeapon->GetHitTraceComponent())
		{
			const float CurrentTime = Playback.AnimInstance->Montage_GetPosition(Playback.Montage.Get());
			const float SampleEnd = TraceSampling.WindowEnd >= 0.0F ? FMath::Min(CurrentTime, TraceSampling.WindowEnd) : CurrentTime;
			const float Delta = CurrentTime - TraceSampling.SampleTime;
			const float Fraction = Delta > UE_SMALL_NUMBER ? FMath::Clamp((SampleEnd - TraceSampling.SampleTime) / Delta, 0.0F, 1.0F) : 0.0F;
			TraceSampling.SampleTime = SampleEnd;
			HitTrace->TickTrace(Fraction);
		}
	}
	// 命中可以同步取消攻击；旧回调不能清理新实例。
	if (bFinish && InstanceId == Playback.InstanceId && InstanceId == TraceSampling.InstanceId) EndWeaponTrace();
}

/**
 * 结束挥刀效果和命中检测；正常窗口结束、接招、取消都可能调用。
 */
void USCLCombatComponent::EndWeaponTrace()
{
	if (TraceSampling.Mesh.IsValid() && TraceSampling.PoseReadyHandle.IsValid())
	{
		TraceSampling.Mesh->UnregisterOnBoneTransformsFinalizedDelegate(TraceSampling.PoseReadyHandle);
		TraceSampling.Mesh->VisibilityBasedAnimTickOption = TraceSampling.OriginalTickOption;
	}
	TraceSampling = FWeaponTraceSamplingState{};
	if (IsValid(EquippedWeapon))
	{
		EquippedWeapon->EndSwingEffects();
		if (USCLHitTraceComponent* const HitTrace = EquippedWeapon->GetHitTraceComponent())
		{
			HitTrace->EndTrace();
		}
	}
}

// ===== 08 范围命中：单次计时 → 查询/过滤/去重 → 共享伤害；取消使代次失效 =====

/**
 * 只有有效范围和延迟时才安排一次定时命中；范围攻击不依赖刀刃 Notify Tick。
 */
void USCLCombatComponent::ScheduleAreaImpact()
{
	const float Radius = ActiveAttackProfile.AreaRadius;
	const float Delay = ActiveAttackProfile.AreaImpactDelay;
	CancelAreaImpact();
	if (!GetWorld() || !FMath::IsFinite(Radius) || !FMath::IsFinite(Delay) || Radius <= UE_SMALL_NUMBER || Delay <= UE_SMALL_NUMBER) return;
	AreaImpact.Radius = Radius;
	AreaImpact.bPending = true;
	GetWorld()->GetTimerManager().SetTimer(AreaImpact.Timer, this, &USCLCombatComponent::ApplyAreaImpact, Delay, false);
}

void USCLCombatComponent::CancelAreaImpact()
{
	++AreaImpact.Generation;
	AreaImpact.bPending = false;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AreaImpact.Timer);
}

void USCLCombatComponent::ApplyAreaImpact()
{
	const uint64 ThisGeneration = AreaImpact.Generation;
	if (!AreaImpact.bPending || !GetWorld() || !IsValid(GetOwner())) { CancelAreaImpact(); return; }
	// 查询期间仍保持 Pending；动画先结束不能让编排层提前丢掉本次伤害配置。
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Query{SCENE_QUERY_STAT(SCLAreaAttack), false, GetOwner()};
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetOwner()->GetActorLocation(), FQuat::Identity,
			Objects, FCollisionShape::MakeSphere(AreaImpact.Radius), Query);
	TSet<AActor*> HitActors;
	for (const auto& Overlap : Overlaps)
	{
		if (ThisGeneration != AreaImpact.Generation) return;
		AActor* Target = Overlap.GetActor();
		if (!IsValid(Target) || HitActors.Contains(Target) || !IsValidAreaTarget(*Target)) continue;
		HitActors.Add(Target);
		FHitResult Hit;
		Hit.Location = Hit.ImpactPoint = Target->GetActorLocation();
		Hit.Component = Overlap.Component;
		ApplyAreaDamage(*Target, Hit);
	}
	if (ThisGeneration != AreaImpact.Generation) return;
	AreaImpact.bPending = false;
	FinishAttackIfReady();
}

bool USCLCombatComponent::IsValidAreaTarget(AActor& Target) const
{
	const AActor* const ComponentOwner = GetOwner();
	if (&Target == ComponentOwner)
	{
		return false;
	}

	const USCLAbilitySystemComponent* const TargetAbilitySystem =
		Cast<USCLAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(&Target));
	if (TargetAbilitySystem == nullptr ||
		TargetAbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Dead))
	{
		return false;
	}

	return ComponentOwner == nullptr || !ComponentOwner->IsA<ASCLEnemyCharacter>() ||
		!Target.IsA<ASCLEnemyCharacter>();
}

// 查询循环确认目标后直接读取本次招式参数，并复用武器的伤害结算。
void USCLCombatComponent::ApplyAreaDamage(AActor& Target, const FHitResult& Hit)
{
	if (!ActiveAttackData || !bHasActiveAttackProfile || !IsValid(EquippedWeapon)) return;
	const auto* Step = ActiveAttackData->FindComboStep(ResolveCurrentComboStepIndex());
	ApplyAttackDamage(Target, Hit, Step ? Step->DamageMultiplier : ActiveAttackData->GetDamageMultiplier(),
		Step ? Step->PoiseDamage : ActiveAttackData->GetPoiseDamage(), TEXT("Area"));
}

// ===== 09 伤害结算：武器与范围招共同进入目标 TakeDamage =====

/**
 * 处理武器轨迹组件去重后的命中事件：有效目标执行伤害和命中特效，
 * 然后向外部监听者广播命中事件。
 */
void USCLCombatComponent::HandleTraceHit(AActor* const HitActor, const FHitResult& HitResult)
{
	if (ActiveAttackData == nullptr || !Playback.IsPlaying()) return;
	if (IsValid(HitActor))
	{
		const int32 CurrentComboStepIndex = ResolveCurrentComboStepIndex();
		const FSCLAttackInfo* const ActiveStep = ActiveAttackData != nullptr
			? ActiveAttackData->FindComboStep(CurrentComboStepIndex)
			: nullptr;
		const float SourceDamageMultiplier = ActiveStep != nullptr
			? ActiveStep->DamageMultiplier
			: (ActiveAttackData != nullptr ? ActiveAttackData->GetDamageMultiplier() : 1.0F);
		const float SourcePoiseDamage = ActiveStep != nullptr
			? ActiveStep->PoiseDamage
			: (ActiveAttackData != nullptr ? ActiveAttackData->GetPoiseDamage() : 0.0F);
		ApplyAttackDamage(
			*HitActor,
			HitResult,
			SourceDamageMultiplier,
			SourcePoiseDamage,
			TEXT("Weapon"));
		EquippedWeapon->PlayHitEffects(HitResult);
	}

	OnWeaponHit.Broadcast(HitActor, HitResult);
}

/**
 * 武器与范围攻击共用的伤害结算入口。
 * 先经目标 TakeDamage 处理防御，再仅在实际生命伤害大于零时施加韧性伤害。
 * SourceDamageMultiplier/SourcePoiseDamage 是调用方提供的基础招式参数，
 * 玩家额外倍率通过 ActiveAttackProfile 叠加；HitType 仅用于日志识别来源。
 */
void USCLCombatComponent::ApplyAttackDamage(
	AActor& HitActor,
	const FHitResult& HitResult,
	const float SourceDamageMultiplier,
	const float SourcePoiseDamage,
	const TCHAR* const HitType)
{
	AActor* const DamageCauser = GetOwner();
	if (!IsValid(DamageCauser) || !IsValid(EquippedWeapon))
	{
		return;
	}

	const FVector ShotDirection =
		(HitActor.GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal();
	const int32 CurrentStepIndex = ResolveCurrentComboStepIndex();
	const FSCLAttackInfo* const CurrentStep = ActiveAttackData != nullptr
		? ActiveAttackData->FindComboStep(CurrentStepIndex)
		: nullptr;
	const APawn* const OwnerPawn = Cast<APawn>(DamageCauser);
	const float AppliedDamage = UGameplayStatics::ApplyPointDamage(
		&HitActor,
		EquippedWeapon->GetBaseDamage() * SourceDamageMultiplier * EnemyDamageScale *
			ActiveAttackProfile.DamageMultiplier,
		ShotDirection,
		HitResult,
		OwnerPawn != nullptr ? OwnerPawn->GetController() : nullptr,
		DamageCauser,
		UDamageType::StaticClass());
	float AppliedPoiseDamage = 0.0F;
	// TakeDamage 可能因无敌、格挡或弹反而把实际生命伤害变为零；此时不追加这条韧性伤害。
	if (AppliedDamage > 0.0F)
	{
		USCLAbilitySystemComponent* const TargetAbilitySystem = Cast<USCLAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(&HitActor));
		if (TargetAbilitySystem != nullptr)
		{
			AppliedPoiseDamage = TargetAbilitySystem->ApplyPoiseDamageToSelf(
				SourcePoiseDamage * ActiveAttackProfile.PoiseDamageMultiplier,
				DamageCauser);
		}
	}

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("%s hit: Attacker=%s Target=%s Attack=%s Section=%s AppliedDamage=%.2f AppliedPoiseDamage=%.2f"),
		HitType,
		*GetNameSafe(DamageCauser),
		*GetNameSafe(&HitActor),
		ActiveAttackProfile.AttackName.IsNone()
			? TEXT("LightAttack")
			: *ActiveAttackProfile.AttackName.ToString(),
		CurrentStep != nullptr ? *CurrentStep->MontageSection.ToString() : TEXT("None"),
		AppliedDamage,
		AppliedPoiseDamage);
}

// ===== 10 配置与查询：Pending 仅用于下一招，Active 仅属于当前招 =====

/**
 * 兼容蓝图查询轻击待选节点的费用，不执行攻击，也不扣费。
 */
float USCLCombatComponent::GetLightAttackStaminaCost() const
{
	return GetNextAttackStaminaCost(ESCLPlayerAttackInput::Light);
}

/**
 * 玩家按 Input 查询 Moveset 待选节点；敌人按待执行配置或当前 Section 后继查询费用。
 * 本函数只读不扣费；玩家实际执行直接使用选定节点的成本，敌人 Ability 保留成本快照。
 */
float USCLCombatComponent::GetNextAttackStaminaCost(const ESCLPlayerAttackInput Input) const
{
	// 玩家读取 Moveset 将要到达的节点；敌人继续从通用 AttackData 读取成本。
	if (HasValidPlayerAttackData() && Cast<ASCLPlayerCharacter>(GetOwner()) != nullptr)
	{
		const ASCLPlayerCharacter* const Player = Cast<ASCLPlayerCharacter>(GetOwner());
		return Player->GetPlayerComboComponent()->GetNextStaminaCost(Input);
	}
	if (LightAttackData == nullptr)
	{
		return 0.0F;
	}

	const int32 CurrentStepIndex = ResolveCurrentComboStepIndex();
	const int32 NextStepIndex = ActiveAttackData == nullptr
		? (bHasPendingAttackProfile ? PendingAttackProfile.MontageStepIndex : 0)
		: CurrentStepIndex + 1;
	const FSCLAttackInfo* const NextStep = LightAttackData->FindComboStep(NextStepIndex);
	return NextStep != nullptr ? NextStep->StaminaCost : LightAttackData->GetStaminaCost();
}

/**
 * 设置敌人的基础伤害倍率、可弹反性和武器显示缩放；不启动动画。
 */
void USCLCombatComponent::ConfigureEnemyAttackProfile(
	const float DamageScale,
	const bool bParryable,
	const float WeaponVisualScale)
{
	EnemyDamageScale = FMath::Max(DamageScale, 0.0F);
	bAttacksParryable = bParryable;
	EnemyWeaponVisualScale = FMath::Max(WeaponVisualScale, 0.1F);
	if (IsValid(EquippedWeapon))
	{
		EquippedWeapon->SetActorRelativeScale3D(FVector{EnemyWeaponVisualScale});
	}
}

/**
 * 复制并约束下一次攻击配置，标记为 Pending；起播时才成为 Active。
 */
void USCLCombatComponent::ConfigureNextAttackProfile(
	const FSCLRuntimeAttackProfile& AttackProfile)
{
	PendingAttackProfile = AttackProfile;
	PendingAttackProfile.DamageMultiplier = FMath::Max(AttackProfile.DamageMultiplier, 0.0F);
	PendingAttackProfile.PoiseDamageMultiplier = FMath::Max(
		AttackProfile.PoiseDamageMultiplier,
		0.0F);
	PendingAttackProfile.MontagePlayRate = FMath::Max(
		AttackProfile.MontagePlayRate,
		UE_SMALL_NUMBER);
	PendingAttackProfile.MontageStepIndex = FMath::Max(AttackProfile.MontageStepIndex, 0);
	PendingAttackProfile.MontageStepCount = FMath::Max(AttackProfile.MontageStepCount, 1);
	PendingAttackProfile.AttackStateDuration = FMath::Max(
		AttackProfile.AttackStateDuration,
		UE_SMALL_NUMBER);
	PendingAttackProfile.AreaRadius = FMath::Max(AttackProfile.AreaRadius, 0.0F);
	PendingAttackProfile.AreaImpactDelay = FMath::Max(AttackProfile.AreaImpactDelay, 0.0F);
	bHasPendingAttackProfile = true;
}

/**
 * 活动配置优先决定可弹反性；没有活动配置时使用敌人的基础设置。
 */
bool USCLCombatComponent::IsActiveAttackParryable() const
{
	return bHasActiveAttackProfile ? ActiveAttackProfile.bParryable : bAttacksParryable;
}

/**
 * 估算当前攻击状态时长：玩家取整段 Montage，敌人累计配置 Section 并考虑播放倍率。
 * 返回时长还覆盖范围攻击的延迟命中时刻。
 */
float USCLCombatComponent::GetActiveAttackStateDuration() const
{
	if (Cast<ASCLPlayerCharacter>(GetOwner()) != nullptr && Playback.Montage.IsValid())
	{
		return Playback.Montage->GetPlayLength() + 0.1F;
	}
	const UAnimMontage* const Montage = ActiveAttackData != nullptr ? ActiveAttackData->GetAttackMontage() : nullptr;
	if (Montage == nullptr) return 1.0F;
	float Duration = 0.0F;
	const int32 Count = FMath::Max(ActiveAttackProfile.MontageStepCount, 1);
	for (int32 Step = EnemyCombo.GetActiveStep(); Step < EnemyCombo.GetActiveStep() + Count; ++Step)
	{
		if (const FSCLAttackInfo* const Info = ActiveAttackData->FindComboStep(Step))
		{
			const int32 Section = Montage->GetSectionIndex(Info->MontageSection);
			if (Section != INDEX_NONE) Duration += Montage->GetSectionLength(Section);
		}
	}
	return FMath::Max(Duration / FMath::Max(ActiveAttackProfile.MontagePlayRate, 0.1F) + 0.10F,
		ActiveAttackProfile.AreaImpactDelay + 0.10F);
}

/**
 * 把“下一次攻击准备使用的配置”转移成“当前攻击正在使用的配置”。
 *
 * 这里可以把两个结构体理解成两个盒子：
 *   PendingAttackProfile：候场盒子，存下一次攻击即将使用的临时参数；
 *   ActiveAttackProfile：在场盒子，存当前已经起播、后续结算要读取的参数。
 *
 * 典型流程是：
 *   1. Ability 或敌人攻击逻辑先写入 Pending；
 *   2. 真正准备播放 Montage 前调用本函数；
 *   3. 把 Pending 拷贝到 Active；
 *   4. 清空 Pending，防止下一次普通攻击误用上一次的特殊参数。
 *
 * bHasPendingAttackProfile / bHasActiveAttackProfile 不是多余的重复状态。
 * 因为结构体即使被重置成默认值，也无法区分“确实配置为默认值”和“根本没有配置”。
 */
void USCLCombatComponent::ActivatePendingAttackProfile()
{
	// 没有待应用配置：当前攻击也不应该继续沿用上一招的 Active 配置。
	if (!bHasPendingAttackProfile)
	{
		ResetActiveAttackProfile();
		return;
	}

	// 配置在这里完成“候场 -> 当前”的交接；后续播放、伤害和范围命中都读 Active。
	ActiveAttackProfile = PendingAttackProfile;
	bHasActiveAttackProfile = true;
	// 一次配置只消费一次，避免它泄漏到下一次攻击。
	ResetPendingAttackProfile();
}

/**
 * 清除尚未应用的下一次攻击配置及其有效标记。
 */
void USCLCombatComponent::ResetPendingAttackProfile()
{
	PendingAttackProfile = {};
	bHasPendingAttackProfile = false;
}

/**
 * 清除当前攻击配置并回到结构体默认值。
 */
void USCLCombatComponent::ResetActiveAttackProfile()
{
	ActiveAttackProfile = {};
	bHasActiveAttackProfile = false;
}
