#include "AbilitySystem/Abilities/SCLExecutionAbility.h"
#include "Characters/Components/SCLActionMovementComponent.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Effects/SCLAttackingStateEffect.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimNotifyQueue.h"
#include "Characters/SCLBossCharacter.h"
#include "Characters/SCLCharacterBase.h"
#include "Combat/SCLCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Characters/Player/SCLWeaponPresentationComponent.h"
#include "MotionWarpingComponent.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "SoulCombatLab.h"
#include "UObject/ConstructorHelpers.h"

USCLExecutionAbility::USCLExecutionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	AttackingStateEffectClass = USCLAttackingStateEffect::StaticClass();
	static ConstructorHelpers::FObjectFinder<UAnimMontage> ExecutionAsset(TEXT("/Game/SoulCombatLab/Characters/Player/Combat/Animations/AM_PlayerExecution"));
	ExecutionMontage = ExecutionAsset.Object;

	FGameplayTagContainer DefaultAssetTags;
	DefaultAssetTags.AddTag(SCLGameplayTags::Ability_Execution);
	SetAssetTags(DefaultAssetTags);
}

float USCLExecutionAbility::CalculateExecutionDamage(
	const float CurrentHealth,
	const bool bBossTarget)
{
	const float SafeCurrentHealth = FMath::Max(CurrentHealth, 0.0F);
	return bBossTarget ? SafeCurrentHealth / 5.0F : SafeCurrentHealth;
}

bool USCLExecutionAbility::CanActivateAbility(
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
	BlockingStates.AddTag(SCLGameplayTags::State_Staggered);
	BlockingStates.AddTag(SCLGameplayTags::State_Dead);
	return !AbilitySystem->HasAnyMatchingGameplayTags(BlockingStates) &&
		FindExecutionTarget(ActorInfo) != nullptr;
}

void USCLExecutionAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* const TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ASCLCharacterBase* const Executor = ActorInfo != nullptr
		? Cast<ASCLCharacterBase>(ActorInfo->AvatarActor.Get())
		: nullptr;
	AActor* const Target = FindExecutionTarget(ActorInfo);
	ActiveExecutionMontage = ExecutionMontage.LoadSynchronous();
	if (Executor == nullptr || Target == nullptr || ActiveExecutionMontage == nullptr)
	{
		UE_LOG(LogSoulCombatLab, Warning, TEXT("Execution failed to start: Executor=%s Target=%s Montage=%s"),
			*GetNameSafe(Executor), *GetNameSafe(Target), *GetNameSafe(ActiveExecutionMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FVector ApproachDirection = (Target->GetActorLocation() - Executor->GetActorLocation()).GetSafeNormal2D();
	const FVector DesiredLocation = Target->GetActorLocation() - ApproachDirection * ExecutionStandOffDistance;
	FHitResult Obstacle;
	FCollisionQueryParams Query{SCENE_QUERY_STAT(SCLExecutionApproach), false, Executor};
	Query.AddIgnoredActor(Target);
	const UCapsuleComponent* Capsule = Executor->GetCapsuleComponent();
	const bool bBlocked = !Capsule || Executor->GetWorld()->SweepSingleByChannel(Obstacle,
		Executor->GetActorLocation(), DesiredLocation, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query);
	if (FVector::Dist2D(Executor->GetActorLocation(), DesiredLocation) > 180.0F || bBlocked)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// 无处决条件时不取消拔刀；预检通过后再交接动作所有权。
	if (ASCLPlayerCharacter* Player = Cast<ASCLPlayerCharacter>(Executor))
		Player->GetWeaponPresentationComponent()->PrepareForAction();

	ExecutionTarget = Target;
	// A valid punish opportunity can cancel the defender's own guard/parry recovery.
	FGameplayTagContainer DefensiveAbilities;
	DefensiveAbilities.AddTag(SCLGameplayTags::Ability_Block);
	DefensiveAbilities.AddTag(SCLGameplayTags::Ability_Parry);
	ActorInfo->AbilitySystemComponent->CancelAbilities(&DefensiveAbilities);
	// A previous light attack may still be blending out after its gameplay state expires.
	// Release its animation/movement ownership before acquiring execution ownership.
	if (USCLCombatComponent* const Combat = Executor->GetCombatComponent_Implementation())
		Combat->CancelActiveAttack();
	bImpactResolved = false;
	if (UCharacterMovementComponent* const MovementComponent = Executor->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		Executor->ConsumeMovementInputVector();
		Executor->SetMovementFacingLocked(TEXT("Execution"), true);
	}
	const FVector DirectionToTarget =
		(Target->GetActorLocation() - Executor->GetActorLocation()).GetSafeNormal2D();
	Executor->SetActorRotation(FRotator{0.0F, DirectionToTarget.Rotation().Yaw, 0.0F});
	Target->SetActorRotation(FRotator{0.0F, (-DirectionToTarget).Rotation().Yaw, 0.0F});
	if (ASCLPlayerCharacter* Player = Cast<ASCLPlayerCharacter>(Executor))
		Player->GetMotionWarpingComponent()->AddOrUpdateWarpTargetFromLocationAndRotation(
			TEXT("SCL_Execution"), DesiredLocation, Executor->GetActorRotation());

	if (AttackingStateEffectClass != nullptr)
	{
		const FGameplayEffectSpecHandle AttackingStateSpec = MakeOutgoingGameplayEffectSpec(
			Handle,
			ActorInfo,
			ActivationInfo,
			AttackingStateEffectClass,
			GetAbilityLevel(Handle, ActorInfo));
		if (AttackingStateSpec.IsValid())
		{
			AttackingStateSpec.Data->SetDuration(ActiveExecutionMontage->GetPlayLength() + 0.1F, true);
			ExecutionStateHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, AttackingStateSpec);
		}
	}

	UAnimInstance* const AnimInstance = Executor->GetMesh()->GetAnimInstance();
	if (AnimInstance != nullptr)
	{
		Executor->GetActionMovementComponent()->RequestRootMotionMode(TEXT("Execution"), ERootMotionMode::RootMotionFromMontagesOnly);
	}

	UAbilityTask_PlayMontageAndWait* const MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("ExecutionMontage"),
			ActiveExecutionMontage,
			1.0F,
			NAME_None,
			true,
			0.0F,
			0.0F,
			true); // Interruption during blend-out must also release execution ownership.
	MontageTask->OnCompleted.AddDynamic(this, &USCLExecutionAbility::HandleExecutionCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &USCLExecutionAbility::HandleExecutionInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &USCLExecutionAbility::HandleExecutionInterrupted);
	MontageTask->ReadyForActivation();
	if (!IsActive()) return;
	if (UAnimInstance* Anim = Executor->GetMesh()->GetAnimInstance())
		if (FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(ActiveExecutionMontage))
			ActiveMontageInstanceId = Instance->GetInstanceID();
	if (ActiveMontageInstanceId == INDEX_NONE)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// Montage 真正起播后才消耗敌人的可处决状态；失败请求不能白白关闭机会窗口。
	if (USCLAbilitySystemComponent* const TargetAbilitySystem = Cast<USCLAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target)))
	{
		TargetAbilitySystem->ConsumeExecutableState();
		TargetAbilitySystem->ApplyStaggeredState(TEXT("ExecutionImpactHold"));
	}

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Execution started: Executor=%s Target=%s Distance=%.2f Duration=%.2f"),
		*GetNameSafe(Executor),
		*GetNameSafe(Target),
		FVector::Distance(Executor->GetActorLocation(), Target->GetActorLocation()),
		ActiveExecutionMontage->GetPlayLength());
}

void USCLExecutionAbility::HandleImpactNotify(const FAnimNotifyEventReference& Reference)
{
	const auto* Context = Reference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
	if (IsActive() && Context && Context->MontageInstanceID == ActiveMontageInstanceId)
		ResolveExecutionImpact();
}

void USCLExecutionAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	RestoreExecutionMovementOrientation();
	RestoreExecutionRootMotionMode();
	if (ASCLPlayerCharacter* Player = ActorInfo ? Cast<ASCLPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
		Player->GetMotionWarpingComponent()->RemoveWarpTarget(TEXT("SCL_Execution"));
	ActiveMontageInstanceId = INDEX_NONE;
	UE_LOG(LogSoulCombatLab, Log, TEXT("Execution ended: Executor=%s Cancelled=%s"),
		*GetNameSafe(ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr), bWasCancelled ? TEXT("true") : TEXT("false"));
	if (ExecutionStateHandle.IsValid() && ActorInfo != nullptr && ActorInfo->AbilitySystemComponent.IsValid())
	{
		ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(ExecutionStateHandle);
	}
	ExecutionStateHandle.Invalidate();
	ExecutionTarget.Reset();
	ActiveExecutionMontage = nullptr;
	bImpactResolved = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USCLExecutionAbility::ResolveExecutionImpact()
{
	AActor* const Target = ExecutionTarget.Get();
	AActor* const Executor = CurrentActorInfo != nullptr ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (bImpactResolved) return;
	if (!IsValid(Target) || !IsValid(Executor))
	{
		FinishExecution(true);
		return;
	}
	USCLAbilitySystemComponent* const TargetAbilitySystem = Cast<USCLAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target));
	if (TargetAbilitySystem == nullptr)
	{
		return;
	}
	// 通知触发时再次检查目标：移动目标跑远或隔墙后不能凭旧的起手快照命中。
	FHitResult Obstruction;
	FCollisionQueryParams Query{SCENE_QUERY_STAT(SCLExecutionImpactVisibility), false, Executor};
	Query.AddIgnoredActor(Target);
	const bool bLostTarget =
		FVector::Dist2D(Executor->GetActorLocation(), Target->GetActorLocation()) > MaximumExecutionDistance ||
		Executor->GetWorld()->LineTraceSingleByChannel(Obstruction, Executor->GetActorLocation(),
			Target->GetActorLocation(), ECC_Visibility, Query);
	if (bLostTarget)
	{
		FinishExecution(true);
		return;
	}

	bImpactResolved = true;
	const float CurrentHealth =
		TargetAbilitySystem->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
	const float ExecutionDamage = CalculateExecutionDamage(
		CurrentHealth,
		Target->IsA<ASCLBossCharacter>());
	const FVector ShotDirection =
		(Target->GetActorLocation() - Executor->GetActorLocation()).GetSafeNormal();
	const APawn* const ExecutorPawn = Cast<APawn>(Executor);
	const float AppliedDamage = UGameplayStatics::ApplyPointDamage(
		Target,
		ExecutionDamage,
		ShotDirection,
		FHitResult{},
		ExecutorPawn != nullptr ? ExecutorPawn->GetController() : nullptr,
		Executor,
		UDamageType::StaticClass());
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Execution impact: Executor=%s Target=%s AppliedDamage=%.2f RemainingHealth=%.2f Dead=%s"),
		*GetNameSafe(Executor),
		*GetNameSafe(Target),
		AppliedDamage,
		TargetAbilitySystem->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()),
		TargetAbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Dead)
			? TEXT("true")
			: TEXT("false"));
}

void USCLExecutionAbility::HandleExecutionCompleted()
{
	FinishExecution(false);
}

void USCLExecutionAbility::HandleExecutionInterrupted()
{
	FinishExecution(true);
}

AActor* USCLExecutionAbility::FindExecutionTarget(
	const FGameplayAbilityActorInfo* const ActorInfo) const
{
	const AActor* const Executor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* const World = Executor != nullptr ? Executor->GetWorld() : nullptr;
	if (Executor == nullptr || World == nullptr)
	{
		return nullptr;
	}

	AActor* BestTarget = nullptr;
	float BestDistanceSquared = FMath::Square(MaximumExecutionDistance);
	for (TActorIterator<ASCLCharacterBase> CharacterIterator{World}; CharacterIterator; ++CharacterIterator)
	{
		ASCLCharacterBase* const Candidate = *CharacterIterator;
		if (Candidate == Executor)
		{
			continue;
		}

		const UAbilitySystemComponent* const CandidateAbilitySystem =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate);
		if (CandidateAbilitySystem == nullptr ||
			!CandidateAbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Executable) ||
			CandidateAbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Dead))
		{
			continue;
		}

		const FVector Offset = Candidate->GetActorLocation() - Executor->GetActorLocation();
		const float DistanceSquared = Offset.SizeSquared2D();
		if (DistanceSquared > BestDistanceSquared ||
			FMath::Abs(Offset.Z) > 120.0F ||
			FVector::DotProduct(Executor->GetActorForwardVector(), Offset.GetSafeNormal2D()) <
				MinimumExecutionFacingDot)
		{
			continue;
		}
		FHitResult Obstruction;
		FCollisionQueryParams QueryParams{SCENE_QUERY_STAT(SCLExecutionVisibility), false, Executor};
		QueryParams.AddIgnoredActor(Candidate);
		if (World->LineTraceSingleByChannel(Obstruction, Executor->GetActorLocation(),
			Candidate->GetActorLocation(), ECC_Visibility, QueryParams)) continue;

		BestTarget = Candidate;
		BestDistanceSquared = DistanceSquared;
	}
	return BestTarget;
}

void USCLExecutionAbility::FinishExecution(const bool bWasCancelled)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}

// 每个结束路径均可重复释放；不依赖动画实例是否仍存活，不保存另一份移动快照。
void USCLExecutionAbility::RestoreExecutionMovementOrientation()
{
	if (auto* Character = Cast<ASCLCharacterBase>(GetAvatarActorFromActorInfo()))
		Character->GetActionMovementComponent()->SetFacingLocked(TEXT("Execution"), false);
}

void USCLExecutionAbility::RestoreExecutionRootMotionMode()
{
	if (auto* Character = Cast<ASCLCharacterBase>(GetAvatarActorFromActorInfo()))
		Character->GetActionMovementComponent()->ReleaseRootMotionMode(TEXT("Execution"));
}
