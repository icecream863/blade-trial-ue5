#include "AbilitySystem/Abilities/SCLParryAbility.h"
#include "Characters/Components/SCLActionMovementComponent.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimNotifyQueue.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Effects/SCLParryingStateEffect.h"
#include "Characters/SCLCharacterBase.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Characters/Player/SCLWeaponPresentationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Targeting/SCLTargetingComponent.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "SoulCombatLab.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

USCLParryAbility::USCLParryAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ParryingStateEffectClass = USCLParryingStateEffect::StaticClass();
	static ConstructorHelpers::FObjectFinder<UAnimMontage> ParryAsset(TEXT("/Game/SoulCombatLab/Characters/Player/Combat/Animations/AM_PlayerParry"));
	ParryMontage = ParryAsset.Object;
	ActivationOwnedTags.AddTag(SCLGameplayTags::State_ParryAction);

	FGameplayTagContainer DefaultAssetTags;
	DefaultAssetTags.AddTag(SCLGameplayTags::Ability_Parry);
	SetAssetTags(DefaultAssetTags);
}

bool USCLParryAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	const FGameplayTagContainer* const SourceTags,
	const FGameplayTagContainer* const TargetTags,
	FGameplayTagContainer* const OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* const AbilitySystem =
		ActorInfo != nullptr ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (AbilitySystem == nullptr)
	{
		return false;
	}
	const ASCLPlayerCharacter* Player = ActorInfo ? Cast<ASCLPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Player || !Player->GetMesh() || !Player->GetMesh()->GetAnimInstance() || !ParryMontage.LoadSynchronous()) return false;

	FGameplayTagContainer BlockingStates;
	BlockingStates.AddTag(SCLGameplayTags::State_Attacking);
	BlockingStates.AddTag(SCLGameplayTags::State_Dodging);
	// 格挡中允许主动转弹反；真正激活并确认动画可用后，再取消格挡。
	// 攻击、躲闪、失衡等状态仍不能靠弹反强行取消。
	BlockingStates.AddTag(SCLGameplayTags::State_Parrying);
	BlockingStates.AddTag(SCLGameplayTags::State_ParryAction);
	BlockingStates.AddTag(SCLGameplayTags::State_Staggered);
	BlockingStates.AddTag(SCLGameplayTags::State_Dead);
	return !AbilitySystem->HasAnyMatchingGameplayTags(BlockingStates);
}

void USCLParryAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* const TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	ASCLPlayerCharacter* const Character = ActorInfo != nullptr
		? Cast<ASCLPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAnimMontage* const Montage = ParryMontage.LoadSynchronous();
	UAnimInstance* const Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Character || !Montage || !Anim)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bCounterDamageResolved = false;
	bParrySucceeded = false;
	GetWorld()->GetTimerManager().ClearTimer(RecoveryTimer);
	// 请求可能被 GAS 拒绝；到这里动画和角色都有效，才取消收刀或待发拔刀攻击。
	Character->GetWeaponPresentationComponent()->PrepareForAction();
	FGameplayTagContainer GuardAbilities;
	GuardAbilities.AddTag(SCLGameplayTags::Ability_Block);
	ActorInfo->AbilitySystemComponent->CancelAbilities(&GuardAbilities);
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Character->ConsumeMovementInputVector();
		Character->SetMovementFacingLocked(TEXT("Parry"), true);
	}
	{
		const USCLTargetingComponent* const Targeting = Character->FindComponentByClass<USCLTargetingComponent>();
		const ASCLCharacterBase* const Target = Targeting != nullptr ? Targeting->GetCurrentTarget() : nullptr;
		if (const APlayerController* const Controller = Cast<APlayerController>(Character->GetController()))
		{
			const float FacingYaw = Target != nullptr
				? (Target->GetActorLocation() - Character->GetActorLocation()).Rotation().Yaw
				: Controller->GetControlRotation().Yaw;
			Character->SetActorRotation(FRotator{0.0F, FacingYaw, 0.0F});
		}
	}
	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, TEXT("ParryMontage"), Montage, 1.0F, NAME_None, true, 0.0F, 0.0F, true);
	Task->OnCompleted.AddDynamic(this, &USCLParryAbility::FinishParry);
	Task->OnInterrupted.AddDynamic(this, &USCLParryAbility::HandleMontageInterrupted);
	Task->OnCancelled.AddDynamic(this, &USCLParryAbility::HandleMontageInterrupted);
	Task->ReadyForActivation();
	if (FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage))
	{
		ParryAnimInstance = Anim;
		ActiveMontageInstanceId = Instance->GetInstanceID();
		Cast<ASCLCharacterBase>(ActorInfo->AvatarActor.Get())->GetActionMovementComponent()->RequestRootMotionMode(TEXT("Parry"), ERootMotionMode::IgnoreRootMotion);
	}
	else EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void USCLParryAbility::ScheduleRecovery(const float Seconds)
{
	if (!IsActive() || !GetWorld()) return;
	// 留到后续帧结束技能，避免在 TakeDamage 的同步调用栈中销毁 Montage 任务。
	GetWorld()->GetTimerManager().SetTimer(RecoveryTimer, this, &USCLParryAbility::FinishParry,
		FMath::IsFinite(Seconds) ? FMath::Max(Seconds, 0.01F) : 0.18F, false);
}

void USCLParryAbility::NotifySuccessfulParry()
{
	if (!IsActive() || bParrySucceeded || !GetWorld()) return;
	bParrySucceeded = true;
	LastSuccessTime = GetWorld()->GetTimeSeconds();
	ScheduleRecovery(SuccessfulRecoverySeconds);
}

bool USCLParryAbility::WasRecentlySuccessful(const float Seconds) const
{
	return GetWorld() && LastSuccessTime >= 0.0 && GetWorld()->GetTimeSeconds() - LastSuccessTime < Seconds;
}

float USCLParryAbility::ApplyCounterDamage(AActor* const Attacker)
{
	ASCLCharacterBase* const Defender = CurrentActorInfo
		? Cast<ASCLCharacterBase>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	if (!IsActive() || !Defender || !IsValid(Attacker) || Attacker == Defender ||
		bCounterDamageResolved || !FMath::IsFinite(CounterDamage) || CounterDamage <= 0.0F)
	{
		return 0.0F;
	}
	// 先占用本次结算，再进入伤害回调，防止同步重入重复扣血。
	bCounterDamageResolved = true;
	// 这是成功弹反的结算伤害，不再生成新的刀刃点命中，避免两名弹反者互相递归反弹。
	// 仍走目标 TakeDamage → ASC / GameplayEffect，保留无敌、生命归零和死亡清理。
	return UGameplayStatics::ApplyDamage(Attacker, CounterDamage, Defender->GetController(),
		Defender, UDamageType::StaticClass());
}

void USCLParryAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RecoveryTimer);
	RemoveParryingState();
	if (ASCLCharacterBase* Character = ActorInfo ? Cast<ASCLCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr)
		Character->SetMovementFacingLocked(TEXT("Parry"), false);
	if (ASCLCharacterBase* Character = ActorInfo ? Cast<ASCLCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr)
		Character->GetActionMovementComponent()->ReleaseRootMotionMode(TEXT("Parry"));
	ParryAnimInstance.Reset();
	ActiveMontageInstanceId = INDEX_NONE;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USCLParryAbility::HandleParryWindowNotify(const bool bBegin, const FAnimNotifyEventReference& Reference)
{
	const auto* Context = Reference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
	if (!IsActive() || !Context || Context->MontageInstanceID != ActiveMontageInstanceId) return;
	if (bBegin) BeginParryWindow();
	else EndParryWindow();
}

void USCLParryAbility::BeginParryWindow()
{
	if (ParryingStateEffectClass == nullptr || CurrentActorInfo == nullptr)
	{
		FinishParry();
		return;
	}

	const FGameplayEffectSpecHandle ParryingStateSpec = MakeOutgoingGameplayEffectSpec(
		CurrentSpecHandle,
		CurrentActorInfo,
		CurrentActivationInfo,
		ParryingStateEffectClass,
		GetAbilityLevel());
	if (!ParryingStateSpec.IsValid())
	{
		FinishParry();
		return;
	}

	ParryingStateHandle = ApplyGameplayEffectSpecToOwner(
		CurrentSpecHandle,
		CurrentActorInfo,
		CurrentActivationInfo,
		ParryingStateSpec);
	if (!ParryingStateHandle.IsValid())
	{
		FinishParry();
		return;
	}

	if (ASCLCharacterBase* const Character =
		Cast<ASCLCharacterBase>(CurrentActorInfo->AvatarActor.Get()))
	{
		Character->SetGuardVisualActive(true);
	}

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Parry window started: Owner=%s State=State.Parrying"),
		*GetNameSafe(CurrentActorInfo->AvatarActor.Get()));

}

void USCLParryAbility::EndParryWindow()
{
	RemoveParryingState();
	// 成功定时器已经从接触时刻计时，窗口结束不能覆盖它、延长硬直。
	if (!bParrySucceeded) ScheduleRecovery(MissRecoverySeconds);
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Parry window ended: Owner=%s"),
		*GetNameSafe(CurrentActorInfo != nullptr ? CurrentActorInfo->AvatarActor.Get() : nullptr));

}

void USCLParryAbility::FinishParry()
{
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USCLParryAbility::HandleMontageInterrupted()
{
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void USCLParryAbility::RemoveParryingState()
{
	if (CurrentActorInfo != nullptr)
	{
		if (ASCLCharacterBase* const Character =
			Cast<ASCLCharacterBase>(CurrentActorInfo->AvatarActor.Get()))
		{
			Character->SetGuardVisualActive(false);
		}

		if (ParryingStateHandle.IsValid() && CurrentActorInfo->AbilitySystemComponent.IsValid())
		{
			CurrentActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(ParryingStateHandle);
		}
	}
	ParryingStateHandle.Invalidate();
}
