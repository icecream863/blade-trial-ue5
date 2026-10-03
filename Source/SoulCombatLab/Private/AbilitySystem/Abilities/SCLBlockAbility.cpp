#include "AbilitySystem/Abilities/SCLBlockAbility.h"
#include "Characters/Components/SCLActionMovementComponent.h"

#include "AbilitySystem/Effects/SCLBlockingStateEffect.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Characters/SCLCharacterBase.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Characters/Player/SCLWeaponPresentationComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Targeting/SCLTargetingComponent.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "SoulCombatLab.h"
#include "UObject/ConstructorHelpers.h"

/*
 * 格挡 Ability 的生命周期：
 *
 *   CanActivateAbility()
 *       只做资格检查，不修改角色状态。
 *
 *   ActivateAbility()
 *       创建并施加 BlockingStateEffect，打开格挡状态；
 *       同时切换表现、限制移动速度，并播放一次格挡起手 Montage。
 *       Ability 不会因为起手 Montage 播放完就结束，而是继续保持激活，
 *       直到玩家松开输入或其他系统主动取消它。
 *
 *   ReleaseBlock() / EndAbility()
 *       松开输入走正常结束；受击、失衡、死亡等情况走取消结束。
 *       两条路径都会撤销格挡状态和移动限制，但只有正常结束才播放收势 Montage。
 *
 * 持续格挡姿势由 Gameplay Tag 与 AnimBP 共同维持：GameplayEffect 提供
 * State.Blocking，AnimBP 根据该状态选择 Blend Space；起手和收势 Montage
 * 只负责进入/离开动作，不负责整个按住期间的姿势。
 */

USCLBlockAbility::USCLBlockAbility()
{
	// 每个角色拥有自己的 Ability 实例，因为格挡状态句柄必须属于当前角色。
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 本地输入可以立即响应，服务器仍会验证最终的 Ability 状态。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	// 这是格挡期间要施加的持续状态；它不是一次性体力扣除 Effect。
	BlockingStateEffectClass = USCLBlockingStateEffect::StaticClass();
	// ConstructorHelpers 只提供默认资源，蓝图 Class Defaults 仍可以覆盖这些配置。
	static ConstructorHelpers::FObjectFinder<UAnimMontage> StartAsset(TEXT("/Game/SoulCombatLab/Characters/Player/Combat/Animations/AM_PlayerBlockStart"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> EndAsset(TEXT("/Game/SoulCombatLab/Characters/Player/Combat/Animations/AM_PlayerBlockEnd"));
	BlockStartMontage = StartAsset.Object;
	BlockEndMontage = EndAsset.Object;

	// 用 Ability 标签标识这是格挡技能，便于 GAS 查询、调试和其他系统区分技能类型。
	FGameplayTagContainer DefaultAssetTags;
	DefaultAssetTags.AddTag(SCLGameplayTags::Ability_Block);
	SetAssetTags(DefaultAssetTags);
}

void USCLBlockAbility::ReleaseBlock()
{
	// 输入松开只请求“正常结束”；真正的清理统一放在 EndAbility，避免遗漏状态句柄。
	// 如果 Ability 已经因受击等原因结束，IsActive() 可以避免重复结束。
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

bool USCLBlockAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	const FGameplayTagContainer* const SourceTags,
	const FGameplayTagContainer* const TargetTags,
	FGameplayTagContainer* const OptionalRelevantTags) const
{
	// 先让 GAS 执行通用规则：标签阻塞、授予条件、冷却等都在这里统一检查。
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* const AbilitySystem =
		ActorInfo != nullptr ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	// 格挡本身不在这里扣体力，但没有体力时不允许进入格挡状态。
	if (AbilitySystem == nullptr ||
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()) <= 0.0F)
	{
		return false;
	}

	FGameplayTagContainer BlockingStates;
	BlockingStates.AddTag(SCLGameplayTags::State_Attacking);
	BlockingStates.AddTag(SCLGameplayTags::State_Dodging);
	BlockingStates.AddTag(SCLGameplayTags::State_Blocking);
	BlockingStates.AddTag(SCLGameplayTags::State_Parrying);
	BlockingStates.AddTag(SCLGameplayTags::State_ParryAction);
	BlockingStates.AddTag(SCLGameplayTags::State_Staggered);
	BlockingStates.AddTag(SCLGameplayTags::State_Dead);
	// 任何互斥状态存在时都拒绝格挡；这里只读标签，不施加 Effect。
	return !AbilitySystem->HasAnyMatchingGameplayTags(BlockingStates);
}

void USCLBlockAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* const TriggerEventData)
{
	// Super 负责 GAS 的基础激活 bookkeeping；下面才是项目自己的格挡流程。
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// SoftObjectPtr 在真正激活时同步加载，避免 Ability 构造阶段强制加载动画资源。
	UAnimMontage* StartMontage = BlockStartMontage.LoadSynchronous();
	if (!StartMontage)
	{
		// 起手动画是本 Ability 的必要资源；缺失时不能留下一个没有表现的格挡状态。
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	USCLAbilitySystemComponent* const AbilitySystem = ActorInfo != nullptr
		? Cast<USCLAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get())
		: nullptr;
	if (AbilitySystem == nullptr || BlockingStateEffectClass == nullptr)
	{
		// 没有 ASC 或状态 Effect 就无法让其他系统识别“正在格挡”，因此立即失败。
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FGameplayEffectSpecHandle BlockingStateSpec = MakeOutgoingGameplayEffectSpec(
		Handle,
		ActorInfo,
		ActivationInfo,
		BlockingStateEffectClass,
		GetAbilityLevel(Handle, ActorInfo));
	if (!BlockingStateSpec.IsValid())
	{
		// Spec 是 Effect 的运行时实例；创建失败时不能继续执行后面的视觉和移动逻辑。
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BlockingStateHandle = ApplyGameplayEffectSpecToOwner(
		Handle,
		ActorInfo,
		ActivationInfo,
		BlockingStateSpec);
	if (!BlockingStateHandle.IsValid())
	{
		// 没有有效句柄就无法在 EndAbility 中精确移除本次格挡状态。
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AbilitySystem->RestartStaminaRegenerationDelay();
	if (ASCLCharacterBase* const Character = Cast<ASCLCharacterBase>(ActorInfo->AvatarActor.Get()))
	{
		// 只有状态 Effect 成功施加后才打开角色的格挡表现和移动限制。
		Character->SetGuardVisualActive(true);
		if (ASCLPlayerCharacter* Player = Cast<ASCLPlayerCharacter>(Character))
		{
			// 成功进入格挡后才取消收/拔刀，并把武器交给防御动作。
			Player->GetWeaponPresentationComponent()->PrepareForAction();
			AActor* Target = Player->GetTargetingComponent()->GetCurrentTarget();
			if (const APlayerController* Controller = Cast<APlayerController>(Player->GetController()))
			{
				const float Facing = Target
					? (Target->GetActorLocation() - Player->GetActorLocation()).Rotation().Yaw
					: Controller->GetControlRotation().Yaw;
				Player->SetActorRotation(FRotator{0.0F, Facing, 0.0F});
			}
		}
		Character->GetActionMovementComponent()->SetWalkSpeedMultiplier(TEXT("Block"), BlockingSpeedMultiplier);
		// 起手 Montage 只播放一次；Ability 持续期间的防御姿势由 AnimBP 维持。
		if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance())
			Anim->Montage_Play(StartMontage);
	}
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Block started: Owner=%s State=State.Blocking"),
		*GetNameSafe(ActorInfo->AvatarActor.Get()));
}

void USCLBlockAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	// bWasCancelled 区分两类结束：
	//   false：玩家松势开输入，允许播放正常收；
	//   true：受击/死亡/其他系统取消，直接清理，避免被打断时还播放收势。
	if (ActorInfo != nullptr)
	{
		if (ASCLCharacterBase* const Character = Cast<ASCLCharacterBase>(ActorInfo->AvatarActor.Get()))
		{
			// 无论正常结束还是取消，都必须撤销视觉标记和移动速度修改。
			Character->SetGuardVisualActive(false);
			Character->GetActionMovementComponent()->ClearWalkSpeedMultiplier(TEXT("Block"));
			// 只有正常松开、且角色没有进入失衡/死亡时，才允许播放收势动画。
			if (!bWasCancelled && BlockingStateHandle.IsValid() &&
				!Character->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Staggered) &&
				!Character->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Dead))
				if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance())
					if (UAnimMontage* EndMontage = BlockEndMontage.LoadSynchronous()) Anim->Montage_Play(EndMontage);
		}
	}

	USCLAbilitySystemComponent* const AbilitySystem = ActorInfo != nullptr
		? Cast<USCLAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get())
		: nullptr;
	if (BlockingStateHandle.IsValid() && AbilitySystem != nullptr)
	{
		// 通过保存的句柄只移除本次 Ability 施加的 Effect，不影响其他来源的同类状态。
		AbilitySystem->RemoveActiveGameplayEffect(BlockingStateHandle);
		BlockingStateHandle.Invalidate();
		// 格挡结束后重新启动体力恢复延迟；具体恢复由 ASC 的统一逻辑处理。
		AbilitySystem->RestartStaminaRegenerationDelay();
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("Block ended: Owner=%s Cancelled=%s"),
			*GetNameSafe(ActorInfo->AvatarActor.Get()),
			bWasCancelled ? TEXT("true") : TEXT("false"));
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
