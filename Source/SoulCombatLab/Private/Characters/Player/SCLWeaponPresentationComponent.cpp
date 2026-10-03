#include "Characters/Player/SCLWeaponPresentationComponent.h"
#include "Characters/Components/SCLActionMovementComponent.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "Combat/SCLCombatComponent.h"
#include "Combat/SCLWeapon.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimNotifyQueue.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "SoulCombatLab.h"

USCLWeaponPresentationComponent::USCLWeaponPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SheathMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/SoulCombatLab/Characters/Player/Combat/Animations/AM_PlayerSheath.AM_PlayerSheath")));
	DrawMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/SoulCombatLab/Characters/Player/Combat/Animations/AM_PlayerDraw.AM_PlayerDraw")));
}

// 加载项目动画并订阅角色状态；角色重生会创建新组件与新计时器。
void USCLWeaponPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	// 当前玩法先保持持刀；关闭时不加载 Montage，也不注册自动收刀计时器。
	if (!bEnableSheathDraw) return;
	LoadedSheathMontage = SheathMontage.LoadSynchronous();
	LoadedDrawMontage = DrawMontage.LoadSynchronous();
	auto* Character = Cast<ASCLPlayerCharacter>(GetOwner());
	if (!Character || !LoadedSheathMontage) return;
	ObservedASC = Character->GetSCLAbilitySystemComponent();
	if (auto* ASC = ObservedASC.Get())
		TagChangedHandle = ASC->RegisterGenericGameplayTagEvent().AddUObject(this, &USCLWeaponPresentationComponent::HandleTagChanged);
	ArmIdleTimer(IdleSheathDelay);
}

// 离开世界时移除订阅和计时，避免旧角色回调影响新角色。
void USCLWeaponPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(IdleTimer);
	if (auto* ASC = ObservedASC.Get()) ASC->RegisterGenericGameplayTagEvent().Remove(TagChangedHandle);
	CancelSheath();
	CancelDraw();
	Super::EndPlay(Reason);
}

// 仅由事件和单次定时器推进，无常驻 Tick；忙碌时稍后再检查。
void USCLWeaponPresentationComponent::ArmIdleTimer(const float Delay)
{
	if (bEnableSheathDraw && LoadedSheathMontage && GetWorld() && !bSheathed && !IsSheathing())
		GetWorld()->GetTimerManager().SetTimer(IdleTimer, this, &USCLWeaponPresentationComponent::TryAutoSheath, Delay, false);
}

bool USCLWeaponPresentationComponent::IsCombatBusy() const
{
	const auto* Character = Cast<ASCLPlayerCharacter>(GetOwner());
	const auto* ASC = ObservedASC.Get();
	if (!Character || !ASC || !Character->GetController()) return true;
	if (Character->GetCombatComponent_Implementation()->IsAttackActive()) return true;
	FGameplayTagContainer Busy;
	Busy.AddTag(SCLGameplayTags::State_Attacking); Busy.AddTag(SCLGameplayTags::State_Dodging);
	Busy.AddTag(SCLGameplayTags::State_Blocking); Busy.AddTag(SCLGameplayTags::State_Parrying);
	Busy.AddTag(SCLGameplayTags::State_ParryAction);
	Busy.AddTag(SCLGameplayTags::State_Staggered); Busy.AddTag(SCLGameplayTags::State_Dead);
	return ASC->HasAnyMatchingGameplayTags(Busy);
}

void USCLWeaponPresentationComponent::TryAutoSheath()
{
	if (!bEnableSheathDraw) return;
	if (bSheathed || IsSheathing() || IsDrawing()) return;
	auto* Character = Cast<ASCLPlayerCharacter>(GetOwner());
	if (!Character || !LoadedSheathMontage) return;
	if (auto* ASC = ObservedASC.Get(); ASC && ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Dead)) return;
	auto* Anim = Character->GetMesh()->GetAnimInstance();
	// 不抢攻击或其他动作的 Montage；普通移动本身不阻止自动收刀。
	if (IsCombatBusy() || !Anim || Anim->IsAnyMontagePlaying())
	{ ArmIdleTimer(0.5F); return; }
	if (!Character->GetMesh()->DoesSocketExist(SheathedSocket))
	{ UE_LOG(LogSoulCombatLab, Warning, TEXT("Sheath socket missing: %s"), *SheathedSocket.ToString()); return; }
	Cast<ASCLCharacterBase>(GetOwner())->GetActionMovementComponent()->RequestRootMotionMode(TEXT("WeaponPresentation"), ERootMotionMode::IgnoreRootMotion, 10);
	const float Duration = Anim->Montage_Play(LoadedSheathMontage);
	auto* Instance = Anim->GetActiveInstanceForMontage(LoadedSheathMontage);
	if (Duration <= 0.0F || !Instance) { RestoreRootMotion(); ArmIdleTimer(IdleSheathDelay); return; }
	ActiveSheathInstanceId = Instance->GetInstanceID();
	FOnMontageEnded Ended;
	Ended.BindUObject(this, &USCLWeaponPresentationComponent::HandleMontageEnded, ActiveSheathInstanceId);
	Anim->Montage_SetEndDelegate(Ended, LoadedSheathMontage);
	FOnMontageBlendingOutStarted Blend;
	Blend.BindUObject(this, &USCLWeaponPresentationComponent::HandleMontageBlendingOut, ActiveSheathInstanceId);
	Anim->Montage_SetBlendingOutDelegate(Blend, LoadedSheathMontage);
	UE_LOG(LogSoulCombatLab, Log, TEXT("Weapon sheath started: Owner=%s Instance=%d"), *GetNameSafe(Character), ActiveSheathInstanceId);
}

// 使用原骨架分别为刀和刀鞘设计的挂点，不能把刀直接挂到刀鞘网格原点。
bool USCLWeaponPresentationComponent::AttachWeapon(const bool bToSheath)
{
	auto* Character = Cast<ASCLPlayerCharacter>(GetOwner());
	auto* Weapon = Character ? Character->GetCombatComponent_Implementation()->GetEquippedWeapon() : nullptr;
	const FName Socket = bToSheath ? SheathedSocket : (Character ? Character->GetWeaponHandSocket() : NAME_None);
	if (!IsValid(Weapon) || !Character->GetMesh()->DoesSocketExist(Socket)) return false;
	Character->GetCombatComponent_Implementation()->EndWeaponTrace();
	const bool Attached = Weapon->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	if (Attached) bSheathed = bToSheath;
	return Attached;
}

void USCLWeaponPresentationComponent::CommitSheath(const FAnimNotifyEventReference& Reference)
{
	const auto* Context = Reference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
	if (!IsSheathing() || !Context || Context->MontageInstanceID != ActiveSheathInstanceId) return;
	if (AttachWeapon(true)) UE_LOG(LogSoulCombatLab, Log, TEXT("Weapon sheath attached: Owner=%s Socket=%s"), *GetNameSafe(GetOwner()), *SheathedSocket.ToString());
}

// 收刀资源已经把剑送到鞘中；拔刀资源在手握住刀后才将武器 Actor 换到手骨骼。
void USCLWeaponPresentationComponent::CommitDraw(const FAnimNotifyEventReference& Reference)
{
	const auto* Context = Reference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
	if (!IsDrawing() || !Context || Context->MontageInstanceID != ActiveDrawInstanceId) return;
	if (AttachWeapon(false))
	{
		bDrawNotifyCommitted = true;
		UE_LOG(LogSoulCombatLab, Log, TEXT("Weapon draw attached: Owner=%s Socket=%s"), *GetNameSafe(GetOwner()), *CastChecked<ASCLPlayerCharacter>(GetOwner())->GetWeaponHandSocket().ToString());
	}
}

// 先废弃身份再 Stop，防止同步结束回调修改新动作；已经入鞘也恢复握刀。
void USCLWeaponPresentationComponent::CancelSheath(const bool bReturnToHand)
{
	const bool WasPlaying = IsSheathing();
	ActiveSheathInstanceId = INDEX_NONE;
	RestoreRootMotion();
	auto* Character = Cast<ASCLPlayerCharacter>(GetOwner());
	auto* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (WasPlaying && Anim) Anim->Montage_Stop(0.12F, LoadedSheathMontage);
	if (bReturnToHand && (WasPlaying || bSheathed)) AttachWeapon(false);
}

bool USCLWeaponPresentationComponent::QueueAttackWhileDrawing(const bool bHeavy)
{
	if (!bEnableSheathDraw) return false; // 轻重攻击直接走原有 GAS/连招入口。
	if (IsDrawing()) return true; // 只保留第一笔输入，避免一段拔刀动画排出多次攻击。
	if (!bSheathed || !LoadedDrawMontage || IsCombatBusy()) return false;
	auto* Character = Cast<ASCLPlayerCharacter>(GetOwner());
	auto* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim) return false;

	// 收刀刚到 Notify 时也能立即反向拔刀，旧通知先失效，刀仍留在入鞘挂点。
	CancelSheath(false);
	Cast<ASCLCharacterBase>(GetOwner())->GetActionMovementComponent()->RequestRootMotionMode(TEXT("WeaponPresentation"), ERootMotionMode::IgnoreRootMotion, 10);
	const float Duration = Anim->Montage_Play(LoadedDrawMontage);
	auto* Instance = Anim->GetActiveInstanceForMontage(LoadedDrawMontage);
	if (Duration <= 0.0F || !Instance)
	{
		RestoreRootMotion();
		AttachWeapon(false);
		return false; // 动画播放失败时沿用原有立即攻击路径，不吞输入。
	}
	ActiveDrawInstanceId = Instance->GetInstanceID();
	QueuedDrawAttack = bHeavy ? EQueuedDrawAttack::Heavy : EQueuedDrawAttack::Light;
	bDrawNotifyCommitted = false;
	FOnMontageEnded Ended;
	Ended.BindUObject(this, &USCLWeaponPresentationComponent::HandleDrawEnded, ActiveDrawInstanceId);
	Anim->Montage_SetEndDelegate(Ended, LoadedDrawMontage);
	FOnMontageBlendingOutStarted Blend;
	Blend.BindUObject(this, &USCLWeaponPresentationComponent::HandleDrawBlendingOut, ActiveDrawInstanceId);
	Anim->Montage_SetBlendingOutDelegate(Blend, LoadedDrawMontage);
	UE_LOG(LogSoulCombatLab, Log, TEXT("Weapon draw started: Owner=%s Heavy=%d Instance=%d"), *GetNameSafe(Character), bHeavy, ActiveDrawInstanceId);
	return true;
}

void USCLWeaponPresentationComponent::CancelDraw()
{
	const bool WasDrawing = IsDrawing();
	ActiveDrawInstanceId = INDEX_NONE;
	QueuedDrawAttack = EQueuedDrawAttack::None;
	bDrawNotifyCommitted = false;
	if (!WasDrawing) return;
	RestoreRootMotion();
	auto* Character = Cast<ASCLPlayerCharacter>(GetOwner());
	if (auto* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr)
		Anim->Montage_Stop(0.12F, LoadedDrawMontage);
	AttachWeapon(false);
}

void USCLWeaponPresentationComponent::PrepareForAction()
{
	CancelSheath();
	CancelDraw();
	ArmIdleTimer(IdleSheathDelay);
}

// 动作开始取消收刀，动作结束重新计时；死亡停止自动收刀。
void USCLWeaponPresentationComponent::HandleTagChanged(const FGameplayTag Tag, const int32 Count)
{
	if (Tag != SCLGameplayTags::State_Attacking && Tag != SCLGameplayTags::State_Dodging &&
		Tag != SCLGameplayTags::State_Blocking && Tag != SCLGameplayTags::State_Parrying &&
		Tag != SCLGameplayTags::State_ParryAction &&
		Tag != SCLGameplayTags::State_Staggered && Tag != SCLGameplayTags::State_Dead) return;
	if (Count > 0) { CancelSheath(); CancelDraw(); }
	if (Tag == SCLGameplayTags::State_Dead && Count > 0)
	{ if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(IdleTimer); return; }
	ArmIdleTimer(IdleSheathDelay);
}

void USCLWeaponPresentationComponent::HandleMontageEnded(UAnimMontage* Montage, const bool bInterrupted, const int32 InstanceId)
{
	if (Montage != LoadedSheathMontage || InstanceId != ActiveSheathInstanceId) return;
	ActiveSheathInstanceId = INDEX_NONE;
	RestoreRootMotion();
	// 未到入鞘通知或被外部 Montage 打断时回到握刀；不在动画结束时偷偷瞬移入鞘。
	if (bInterrupted || !bSheathed) { AttachWeapon(false); ArmIdleTimer(IdleSheathDelay); }
}

void USCLWeaponPresentationComponent::RestoreRootMotion()
{
	if (auto* Character = Cast<ASCLCharacterBase>(GetOwner()))
		Character->GetActionMovementComponent()->ReleaseRootMotionMode(TEXT("WeaponPresentation"));
}

void USCLWeaponPresentationComponent::HandleMontageBlendingOut(UAnimMontage* Montage, const bool bInterrupted, const int32 InstanceId)
{
	if (bInterrupted && Montage == LoadedSheathMontage && InstanceId == ActiveSheathInstanceId)
	{ CancelSheath(); ArmIdleTimer(IdleSheathDelay); }
}

void USCLWeaponPresentationComponent::HandleDrawBlendingOut(UAnimMontage* Montage, const bool bInterrupted, const int32 InstanceId)
{
	if (bInterrupted && Montage == LoadedDrawMontage && InstanceId == ActiveDrawInstanceId)
	{ CancelDraw(); ArmIdleTimer(IdleSheathDelay); }
}

void USCLWeaponPresentationComponent::HandleDrawEnded(UAnimMontage* Montage, const bool bInterrupted, const int32 InstanceId)
{
	if (Montage != LoadedDrawMontage || InstanceId != ActiveDrawInstanceId) return;
	const EQueuedDrawAttack Attack = QueuedDrawAttack;
	ActiveDrawInstanceId = INDEX_NONE;
	QueuedDrawAttack = EQueuedDrawAttack::None;
	RestoreRootMotion();
	if (!bDrawNotifyCommitted)
	{
		UE_LOG(LogSoulCombatLab, Warning, TEXT("Draw notify was not reached; restoring hand socket before attack"));
		AttachWeapon(false);
	}
	bDrawNotifyCommitted = false;
	auto* Character = Cast<ASCLPlayerCharacter>(GetOwner());
	if (!bInterrupted && Character)
	{
		// 现在才走原有轻/重攻击入口，GAS 仍在实际起播时检查费用和状态。
		if (Attack == EQueuedDrawAttack::Light) Character->RequestLightAttack();
		else if (Attack == EQueuedDrawAttack::Heavy) Character->RequestHeavyAttack();
	}
	ArmIdleTimer(IdleSheathDelay);
}
