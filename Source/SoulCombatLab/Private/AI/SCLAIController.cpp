
#include "AI/SCLAIController.h"

#include "AI/SCLBehaviorTreeBuilder.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Characters/SCLEnemyCharacter.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Sight.h"
#include "SoulCombatLab.h"
#include "TimerManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
constexpr float DefaultSightRadius{1600.0F};
constexpr float DefaultLoseSightRadius{1900.0F};
constexpr float DefaultPeripheralVisionHalfAngle{65.0F};
constexpr float DefaultSightMaxAge{2.0F};
constexpr float DefaultSightLossGracePeriod{2.0F};
}

ASCLAIController::ASCLAIController()
{
	PrimaryActorTick.bCanEverTick = false;
	SetGenericTeamId(FGenericTeamId{1});

	AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));
	SetPerceptionComponent(*AIPerceptionComponent);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = DefaultSightRadius;
	SightConfig->LoseSightRadius = DefaultLoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = DefaultPeripheralVisionHalfAngle;
	SightConfig->SetMaxAge(DefaultSightMaxAge);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	AIPerceptionComponent->ConfigureSense(*SightConfig);
	AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
		this,
		&ASCLAIController::HandleTargetPerceptionUpdated);
}

float ASCLAIController::GetConfiguredSightRadius() const
{
	return SightConfig != nullptr ? SightConfig->SightRadius : 0.0F;
}

float ASCLAIController::GetConfiguredLoseSightRadius() const
{
	return SightConfig != nullptr ? SightConfig->LoseSightRadius : 0.0F;
}

float ASCLAIController::GetConfiguredPeripheralVisionHalfAngle() const
{
	return SightConfig != nullptr ? SightConfig->PeripheralVisionAngleDegrees : 0.0F;
}

float ASCLAIController::GetConfiguredSightMaxAge() const
{
	return SightConfig != nullptr ? SightConfig->GetMaxAge() : 0.0F;
}

float ASCLAIController::GetConfiguredSightLossGracePeriod() const
{
	return DefaultSightLossGracePeriod;
}

void ASCLAIController::OnPossess(APawn* const InPawn)
{
	Super::OnPossess(InPawn);
	ClearTarget(TEXT("PossessChanged"));

	if (!IsValid(InPawn) || !InPawn->IsA<ASCLEnemyCharacter>())
	{
		UE_LOG(
			LogSoulCombatLab,
			Warning,
			TEXT("AI possession rejected: Controller=%s Pawn=%s Expected=SCLEnemyCharacter"),
			*GetNameSafe(this),
			*GetNameSafe(InPawn));
		UnPossess();
		return;
	}

	InitializeBehaviorTree();
	RefreshBlackboardContext();
}

void ASCLAIController::OnUnPossess()
{
	ClearTarget(TEXT("UnPossessed"));
	Super::OnUnPossess();
}

void ASCLAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTarget(TEXT("EndPlay"));
	if (AIPerceptionComponent != nullptr)
	{
		AIPerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(
			this,
			&ASCLAIController::HandleTargetPerceptionUpdated);
	}

	Super::EndPlay(EndPlayReason);
}

void ASCLAIController::HandleTargetPerceptionUpdated(AActor* const Actor, const FAIStimulus Stimulus)
{
	ASCLPlayerCharacter* const PlayerCandidate = Cast<ASCLPlayerCharacter>(Actor);
	if (PlayerCandidate == nullptr)
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed() && IsValidPerceptionTarget(PlayerCandidate))
	{
		AcquireTarget(*PlayerCandidate);
		return;
	}

	if (PerceivedTarget.Get() == PlayerCandidate)
	{
		BeginSightLossGrace();
	}
}

void ASCLAIController::AcquireTarget(ASCLPlayerCharacter& NewTarget)
{
	RegisterTarget(NewTarget, true, TEXT("Sight"));
}

void ASCLAIController::NotifyDamageReceived(ASCLPlayerCharacter& DamageInstigator)
{
	if (!IsValidPerceptionTarget(&DamageInstigator))
	{
		return;
	}

	RegisterTarget(DamageInstigator, false, TEXT("Damage"));
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("AI damage awareness: Controller=%s Pawn=%s Instigator=%s Distance=%.1f LOS=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetPawn()),
		*GetNameSafe(&DamageInstigator),
		GetPawn() != nullptr
			? FVector::Dist(GetPawn()->GetActorLocation(), DamageInstigator.GetActorLocation())
			: 0.0F,
		bTargetCurrentlyVisible ? TEXT("true") : TEXT("false"));
}

void ASCLAIController::RegisterTarget(
	ASCLPlayerCharacter& NewTarget,
	const bool bConfirmedVisible,
	const FName AwarenessSource)
{
	if (PerceivedTarget.Get() == &NewTarget)
	{
		const bool bSightWasLost = !bTargetCurrentlyVisible;
		CancelSightLossGrace();
		bTargetCurrentlyVisible = bTargetCurrentlyVisible || bConfirmedVisible;
		RefreshBlackboardContext();
		if (bConfirmedVisible && bSightWasLost)
		{
			UE_LOG(
				LogSoulCombatLab,
				Verbose,
				TEXT("AI sight restored during grace period: Controller=%s Target=%s"),
				*GetNameSafe(this),
				*GetNameSafe(&NewTarget));
		}
		return;
	}

	ClearTarget(TEXT("Replaced"));
	PerceivedTarget = &NewTarget;
	bTargetCurrentlyVisible = bConfirmedVisible;
	PerceivedTargetAbilitySystem = NewTarget.GetSCLAbilitySystemComponent();
	if (USCLAbilitySystemComponent* const TargetAbilitySystem = PerceivedTargetAbilitySystem.Get())
	{
		TargetDeadTagDelegateHandle = TargetAbilitySystem->RegisterGameplayTagEvent(
			SCLGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved).AddUObject(
				this,
				&ASCLAIController::HandleTargetDeadTagChanged);
	}
	RefreshBlackboardContext();

	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("AI target acquired: Controller=%s Pawn=%s Target=%s Distance=%.1f Source=%s LOS=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetPawn()),
		*GetNameSafe(&NewTarget),
		GetPawn() != nullptr
			? FVector::Dist(GetPawn()->GetActorLocation(), NewTarget.GetActorLocation())
			: 0.0F,
		*AwarenessSource.ToString(),
		bTargetCurrentlyVisible ? TEXT("true") : TEXT("false"));
}

void ASCLAIController::BeginSightLossGrace()
{
	if (!PerceivedTarget.IsValid() || !bTargetCurrentlyVisible)
	{
		return;
	}

	bTargetCurrentlyVisible = false;
	RefreshBlackboardContext();
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		ClearTarget(TEXT("SightLost"));
		TryAcquireReplacementTarget();
		return;
	}

	World->GetTimerManager().SetTimer(
		SightLossGraceTimerHandle,
		this,
		&ASCLAIController::HandleSightLossGraceExpired,
		DefaultSightLossGracePeriod,
		false);
	UE_LOG(
		LogSoulCombatLab,
		Verbose,
		TEXT("AI sight loss grace started: Controller=%s Target=%s Duration=%.2f"),
		*GetNameSafe(this),
		*GetNameSafe(PerceivedTarget.Get()),
		DefaultSightLossGracePeriod);
}

void ASCLAIController::CancelSightLossGrace()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SightLossGraceTimerHandle);
	}
	SightLossGraceTimerHandle.Invalidate();
}

void ASCLAIController::HandleSightLossGraceExpired()
{
	SightLossGraceTimerHandle.Invalidate();
	if (!PerceivedTarget.IsValid() || bTargetCurrentlyVisible)
	{
		return;
	}

	if (const ASCLPlayerCharacter* const Target = PerceivedTarget.Get();
		Target != nullptr && ShouldRetainUnseenTarget(*Target))
	{
		RefreshBlackboardContext();
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("AI unseen target retained: Controller=%s Pawn=%s Target=%s Distance=%.1f AwarenessRadius=%.1f"),
			*GetNameSafe(this),
			*GetNameSafe(GetPawn()),
			*GetNameSafe(Target),
			GetPawn() != nullptr
				? FVector::Dist(GetPawn()->GetActorLocation(), Target->GetActorLocation())
				: 0.0F,
			GetConfiguredLoseSightRadius());
		return;
	}

	ClearTarget(TEXT("SightLostAfterGrace"));
	TryAcquireReplacementTarget();
}

void ASCLAIController::ClearTarget(const FName Reason)
{
	ASCLPlayerCharacter* const PreviousTarget = PerceivedTarget.Get();
	CancelSightLossGrace();
	if (USCLAbilitySystemComponent* const TargetAbilitySystem = PerceivedTargetAbilitySystem.Get();
		TargetAbilitySystem != nullptr && TargetDeadTagDelegateHandle.IsValid())
	{
		TargetAbilitySystem->RegisterGameplayTagEvent(
			SCLGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved).Remove(TargetDeadTagDelegateHandle);
	}

	TargetDeadTagDelegateHandle.Reset();
	PerceivedTargetAbilitySystem.Reset();
	PerceivedTarget.Reset();
	bTargetCurrentlyVisible = false;
	RefreshBlackboardContext();

	if (PreviousTarget != nullptr)
	{
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("AI target lost: Controller=%s Pawn=%s Target=%s Reason=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetPawn()),
			*GetNameSafe(PreviousTarget),
			*Reason.ToString());
	}
}

void ASCLAIController::TryAcquireReplacementTarget()
{
	if (AIPerceptionComponent == nullptr || SightConfig == nullptr)
	{
		return;
	}

	TArray<AActor*> PerceivedActors;
	AIPerceptionComponent->GetCurrentlyPerceivedActors(
		SightConfig->GetSenseImplementation(),
		PerceivedActors);
	for (AActor* const PerceivedActor : PerceivedActors)
	{
		ASCLPlayerCharacter* const PlayerCandidate = Cast<ASCLPlayerCharacter>(PerceivedActor);
		if (IsValidPerceptionTarget(PlayerCandidate))
		{
			AcquireTarget(*PlayerCandidate);
			return;
		}
	}
}

bool ASCLAIController::IsValidPerceptionTarget(const ASCLPlayerCharacter* const Candidate) const
{
	return IsValid(Candidate) &&
		Candidate->GetSCLAbilitySystemComponent() != nullptr &&
		!Candidate->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(
			SCLGameplayTags::State_Dead);
}

bool ASCLAIController::ShouldRetainUnseenTarget(const ASCLPlayerCharacter& Target) const
{
	const APawn* const ControlledPawn = GetPawn();
	return ControlledPawn != nullptr &&
		SCLAIStatePolicy::ShouldRetainUnseenTarget(
			FVector::Dist(ControlledPawn->GetActorLocation(), Target.GetActorLocation()),
			GetConfiguredLoseSightRadius());
}

void ASCLAIController::HandleTargetDeadTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
	if (NewCount <= 0 || Tag != SCLGameplayTags::State_Dead)
	{
		return;
	}

	ClearTarget(TEXT("Dead"));
	TryAcquireReplacementTarget();
}

UBlackboardData* ASCLAIController::GetRuntimeBlackboardData() const
{
	return RuntimeBehaviorTree != nullptr ? RuntimeBehaviorTree->BlackboardAsset : nullptr;
}

ESCLEnemyAIState ASCLAIController::GetCurrentCombatState() const
{
	const UBlackboardComponent* const BlackboardComponent = GetBlackboardComponent();
	return BlackboardComponent != nullptr
		? static_cast<ESCLEnemyAIState>(
			BlackboardComponent->GetValueAsEnum(SCLBlackboardKeys::CombatState))
		: ESCLEnemyAIState::Idle;
}

void ASCLAIController::RefreshBlackboardContext()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SCL_AIContext);
	UBlackboardComponent* const BlackboardComponent = GetBlackboardComponent();
	APawn* const ControlledPawn = GetPawn();
	ASCLPlayerCharacter* const Target = PerceivedTarget.Get();
	if (BlackboardComponent == nullptr || ControlledPawn == nullptr)
	{
		return;
	}

	if (Target != nullptr && !bTargetCurrentlyVisible && !ShouldRetainUnseenTarget(*Target))
	{
		ClearTarget(TEXT("UnseenOutOfAwarenessRange"));
		TryAcquireReplacementTarget();
		return;
	}

	const bool bHasTarget = IsValidPerceptionTarget(Target);
	const bool bHasLineOfSight = bHasTarget && bTargetCurrentlyVisible;
	const float DistanceToTarget = bHasTarget
		? FVector::Dist(ControlledPawn->GetActorLocation(), Target->GetActorLocation())
		: 0.0F;
	const ESCLEnemyAIState PreviousState = GetCurrentCombatState();
	const ESCLEnemyAIState NewState = ResolveCombatState(DistanceToTarget, PreviousState);

	BlackboardComponent->SetValueAsObject(
		SCLBlackboardKeys::TargetActor,
		bHasTarget ? Target : nullptr);
	BlackboardComponent->SetValueAsFloat(
		SCLBlackboardKeys::DistanceToTarget,
		DistanceToTarget);
	BlackboardComponent->SetValueAsBool(
		SCLBlackboardKeys::HasLineOfSight,
		bHasLineOfSight);
	BlackboardComponent->SetValueAsEnum(
		SCLBlackboardKeys::CombatState,
		static_cast<uint8>(NewState));
	BlackboardComponent->SetValueAsBool(
		SCLBlackboardKeys::CanAttack,
		bHasTarget && NewState == ESCLEnemyAIState::Combat);

	if (PreviousState != NewState)
	{
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("AI state changed: Controller=%s Pawn=%s Previous=%s Current=%s Distance=%.1f LOS=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ControlledPawn),
			*StaticEnum<ESCLEnemyAIState>()->GetNameStringByValue(static_cast<int64>(PreviousState)),
			*StaticEnum<ESCLEnemyAIState>()->GetNameStringByValue(static_cast<int64>(NewState)),
			DistanceToTarget,
			bHasLineOfSight ? TEXT("true") : TEXT("false"));
	}
}

void ASCLAIController::InitializeBehaviorTree()
{
	if (RuntimeBehaviorTree == nullptr)
	{
		RuntimeBehaviorTree = SCLBehaviorTreeBuilder::BuildEnemyBehaviorTree(*this);
	}

	UBlackboardData* const BlackboardData = GetRuntimeBlackboardData();
	UBlackboardComponent* BlackboardComponent = nullptr;
	const bool bBlackboardInitialized = BlackboardData != nullptr &&
		UseBlackboard(BlackboardData, BlackboardComponent);
	const bool bBehaviorTreeStarted = bBlackboardInitialized &&
		RunBehaviorTree(RuntimeBehaviorTree);
	if (bBehaviorTreeStarted)
	{
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("AI behavior initialized: Controller=%s Pawn=%s Blackboard=true BehaviorTree=true Keys=%d"),
			*GetNameSafe(this),
			*GetNameSafe(GetPawn()),
			BlackboardData->GetNumKeys());
	}
	else
	{
		UE_LOG(
			LogSoulCombatLab,
			Error,
			TEXT("AI behavior initialization failed: Controller=%s Pawn=%s Blackboard=%s BehaviorTree=false Keys=%d"),
			*GetNameSafe(this),
			*GetNameSafe(GetPawn()),
			bBlackboardInitialized ? TEXT("true") : TEXT("false"),
			BlackboardData != nullptr ? BlackboardData->GetNumKeys() : 0);
	}
}

ESCLEnemyAIState ASCLAIController::ResolveCombatState(
	const float DistanceToTarget,
	const ESCLEnemyAIState PreviousState) const
{
	const ASCLEnemyCharacter* const Enemy = Cast<ASCLEnemyCharacter>(GetPawn());
	const USCLAbilitySystemComponent* const EnemyAbilitySystem = Enemy != nullptr
		? Enemy->GetSCLAbilitySystemComponent()
		: nullptr;
	if (EnemyAbilitySystem != nullptr &&
		EnemyAbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Dead))
	{
		return ESCLEnemyAIState::Dead;
	}
	if (EnemyAbilitySystem != nullptr &&
		EnemyAbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Staggered))
	{
		return ESCLEnemyAIState::Staggered;
	}
	if (!PerceivedTarget.IsValid())
	{
		return ESCLEnemyAIState::Idle;
	}

	return SCLAIStatePolicy::ResolveTargetRangeState(
		DistanceToTarget,
		PreviousState,
		Enemy != nullptr ? Enemy->GetCombatEnterDistance() : SCLAIStatePolicy::CombatEnterDistance,
		Enemy != nullptr ? Enemy->GetCombatExitDistance() : SCLAIStatePolicy::CombatExitDistance);
}
