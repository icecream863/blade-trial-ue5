#include "AbilitySystem/Abilities/SCLDodgeAbility.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Effects/SCLDodgeCostEffect.h"
#include "AbilitySystem/Effects/SCLDodgingStateEffect.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Characters/Components/SCLActionMovementComponent.h"
#include "Characters/Player/SCLWeaponPresentationComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "SoulCombatLab.h"

namespace
{
constexpr float MinimumDodgeDurationSeconds{0.1F};
}

ESCLDodgeDirection USCLDodgeAbility::SelectDodgeDirection(const FVector WorldDirection, const FRotator Facing)
{
	const FVector Local = FRotator(0.0F, Facing.Yaw, 0.0F).UnrotateVector(WorldDirection.GetSafeNormal2D());
	if (Local.IsNearlyZero()) return ESCLDodgeDirection::Forward;
	// 每个方向占 45°，左半圈的负角绕回到数组末尾。只选动作，不修改角色朝向。
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	const int32 Sector = (FMath::FloorToInt((Angle + 22.5F) / 45.0F) + 8) % 8;
	return static_cast<ESCLDodgeDirection>(Sector);
}

UAnimMontage* USCLDodgeAbility::ResolveMontage(const ESCLDodgeDirection Direction) const
{
	if (DirectionalMontages.IsEmpty()) return DodgeMontage.LoadSynchronous();
	const TSoftObjectPtr<UAnimMontage>* Asset = DirectionalMontages.Find(Direction);
	return Asset ? Asset->LoadSynchronous() : nullptr;
}

USCLDodgeAbility::USCLDodgeAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	StaminaCostEffectClass = USCLDodgeCostEffect::StaticClass();
	DodgingStateEffectClass = USCLDodgingStateEffect::StaticClass();

	FGameplayTagContainer DefaultAssetTags;
	DefaultAssetTags.AddTag(SCLGameplayTags::Ability_Dodge);
	SetAssetTags(DefaultAssetTags);
}

bool USCLDodgeAbility::CanActivateAbility(
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

	FGameplayTagContainer BlockingStates;
	BlockingStates.AddTag(SCLGameplayTags::State_Attacking);
	BlockingStates.AddTag(SCLGameplayTags::State_Dodging);
	BlockingStates.AddTag(SCLGameplayTags::State_Blocking);
	BlockingStates.AddTag(SCLGameplayTags::State_Parrying);
	BlockingStates.AddTag(SCLGameplayTags::State_ParryAction);
	BlockingStates.AddTag(SCLGameplayTags::State_Staggered);
	BlockingStates.AddTag(SCLGameplayTags::State_Dead);
	return !AbilitySystem->HasAnyMatchingGameplayTags(BlockingStates);
}

bool USCLDodgeAbility::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	FGameplayTagContainer* const OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* const AbilitySystem =
		ActorInfo != nullptr ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return AbilitySystem != nullptr &&
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()) >= StaminaCost;
}

void USCLDodgeAbility::ApplyCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	USCLAbilitySystemComponent* const AbilitySystem = ActorInfo != nullptr
		? Cast<USCLAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get())
		: nullptr;
	if (AbilitySystem == nullptr || StaminaCostEffectClass == nullptr || StaminaCost <= 0.0F)
	{
		return;
	}

	const FGameplayEffectSpecHandle CostSpec = MakeOutgoingGameplayEffectSpec(
		Handle,
		ActorInfo,
		ActivationInfo,
		StaminaCostEffectClass,
		GetAbilityLevel(Handle, ActorInfo));
	if (!CostSpec.IsValid())
	{
		return;
	}

	CostSpec.Data->SetSetByCallerMagnitude(SCLGameplayTags::Data_Cost_Stamina, -StaminaCost);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CostSpec);
	AbilitySystem->RestartStaminaRegenerationDelay();
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("GAS dodge stamina cost applied: Owner=%s Cost=%.2f RemainingStamina=%.2f"),
		*GetNameSafe(ActorInfo->AvatarActor.Get()),
		StaminaCost,
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()));
}

void USCLDodgeAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* const TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ASCLPlayerCharacter* const PlayerCharacter = ActorInfo != nullptr
		? Cast<ASCLPlayerCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	const FVector DodgeDirection = PlayerCharacter ? PlayerCharacter->GetDesiredDodgeDirection() : FVector::ZeroVector;
	const ESCLDodgeDirection Direction = SelectDodgeDirection(DodgeDirection,
		PlayerCharacter ? PlayerCharacter->GetActorRotation() : FRotator::ZeroRotator);
	UAnimMontage* const Montage = ResolveMontage(Direction);
	if (PlayerCharacter == nullptr || Montage == nullptr || !CommitAbilityCost(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (DodgingStateEffectClass != nullptr)
	{
		const FGameplayEffectSpecHandle DodgingStateSpec = MakeOutgoingGameplayEffectSpec(
			Handle,
			ActorInfo,
			ActivationInfo,
			DodgingStateEffectClass,
			GetAbilityLevel(Handle, ActorInfo));
		if (DodgingStateSpec.IsValid())
		{
			DodgingStateHandle = ApplyGameplayEffectSpecToOwner(
				Handle,
				ActorInfo,
				ActivationInfo,
				DodgingStateSpec);
		}
	}

	const float DodgeDuration = FMath::Max(Montage->GetPlayLength(), MinimumDodgeDurationSeconds);
	// 原版动作已经包含翻滚姿势，不再人工旋转整个 Mesh。
	// 胶囊只由 MoveToForce 沿起招方向移动；动画根位移提取后忽略，避免双重位移。
	PlayerCharacter->GetWeaponPresentationComponent()->PrepareForAction();
	PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
	PlayerCharacter->GetActionMovementComponent()->SetFacingLocked(TEXT("Dodge"), true);
	PlayerCharacter->GetActionMovementComponent()->RequestRootMotionMode(TEXT("Dodge"), ERootMotionMode::IgnoreRootMotion);

	const FVector DodgeTargetLocation =
		PlayerCharacter->GetActorLocation() + DodgeDirection * DodgeDistance;
	UAbilityTask_ApplyRootMotionMoveToForce* const RootMotionTask =
		UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
			this,
			TEXT("DodgeRootMotion"),
			DodgeTargetLocation,
			DodgeDuration,
			false,
			MOVE_Walking,
			true,
			nullptr,
			ERootMotionFinishVelocityMode::SetVelocity,
			FVector::ZeroVector,
			0.0F);

	UAbilityTask_PlayMontageAndWait* const MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("DodgeMontage"),
			Montage,
			1.0F,
			NAME_None,
			true,
			0.0F,
			0.0F,
			true); // 混出时中断也释放动作所有权。
	MontageTask->OnCompleted.AddDynamic(this, &USCLDodgeAbility::HandleDodgeCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &USCLDodgeAbility::HandleDodgeInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &USCLDodgeAbility::HandleDodgeInterrupted);
	MontageTask->ReadyForActivation();

	if (!IsActive()) return;
	RootMotionTask->ReadyForActivation();

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Dodge started: Owner=%s Direction=%s Distance=%.2f Duration=%.3f Montage=%s Sector=%d"),
		*GetNameSafe(PlayerCharacter),
		*DodgeDirection.ToCompactString(),
		DodgeDistance,
		DodgeDuration,
		*GetNameSafe(Montage),
		static_cast<int32>(Direction));
}

void USCLDodgeAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	// 只撤销本次 Dodge 的申请，保留锁定组件仍在使用的朝向限制。
	if (ASCLPlayerCharacter* Player = ActorInfo ? Cast<ASCLPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
	{
		Player->GetActionMovementComponent()->SetFacingLocked(TEXT("Dodge"), false);
		Player->GetActionMovementComponent()->ReleaseRootMotionMode(TEXT("Dodge"));
	}

	if (DodgingStateHandle.IsValid() && ActorInfo != nullptr && ActorInfo->AbilitySystemComponent.IsValid())
	{
		ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(DodgingStateHandle);
		DodgingStateHandle.Invalidate();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USCLDodgeAbility::HandleDodgeCompleted()
{
	FinishDodge(false);
}

void USCLDodgeAbility::HandleDodgeInterrupted()
{
	FinishDodge(true);
}

void USCLDodgeAbility::FinishDodge(const bool bWasCancelled)
{
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}
