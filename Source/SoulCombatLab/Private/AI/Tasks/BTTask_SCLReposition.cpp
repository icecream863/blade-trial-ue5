#include "AI/Tasks/BTTask_SCLReposition.h"

#include "AI/SCLAIState.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "SoulCombatLab.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
constexpr float RepositionAcceptanceRadius{35.0F};
}

UBTTask_SCLReposition::UBTTask_SCLReposition()
{
	NodeName = TEXT("Move To EQS Combat Position");
	BlackboardKey.SelectedKeyName = SCLBlackboardKeys::IdealCombatLocation;
	AcceptableRadius = FValueOrBBKey_Float{RepositionAcceptanceRadius};
	ObservedBlackboardValueTolerance = FValueOrBBKey_Float{20.0F};
	bAllowStrafe = FValueOrBBKey_Bool{true};
	bAllowPartialPath = FValueOrBBKey_Bool{false};
	bTrackMovingGoal = FValueOrBBKey_Bool{false};
	bRequireNavigableEndLocation = FValueOrBBKey_Bool{true};
	bProjectGoalLocation = FValueOrBBKey_Bool{true};
	bReachTestIncludesAgentRadius = FValueOrBBKey_Bool{true};
	bReachTestIncludesGoalRadius = FValueOrBBKey_Bool{false};
	bStartFromPreviousPath = FValueOrBBKey_Bool{false};
	bObserveBlackboardValue = false;
}

float UBTTask_SCLReposition::GetConfiguredAcceptanceRadius() const
{
	return AcceptableRadius.GetValue(static_cast<const UBlackboardComponent*>(nullptr));
}

EBTNodeResult::Type UBTTask_SCLReposition::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* const NodeMemory)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SCL_EQSRepositionDispatch);
	UBlackboardComponent* const Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* const Controller = OwnerComp.GetAIOwner();
	APawn* const Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
	AActor* const Target = Blackboard != nullptr
		? Cast<AActor>(Blackboard->GetValueAsObject(SCLBlackboardKeys::TargetActor))
		: nullptr;
	if (Blackboard == nullptr || Pawn == nullptr || !IsValid(Target))
	{
		return EBTNodeResult::Failed;
	}

	const FVector SelectedLocation = Blackboard->GetValueAsVector(
		SCLBlackboardKeys::IdealCombatLocation);
	const float TravelDistance = FVector::Dist2D(Pawn->GetActorLocation(), SelectedLocation);
	const float TargetDistance = FVector::Dist2D(Target->GetActorLocation(), SelectedLocation);
	const FVector TargetToPosition =
		(SelectedLocation - Target->GetActorLocation()).GetSafeNormal2D();
	const float FacingDot = FMath::Clamp(
		FVector::DotProduct(Target->GetActorForwardVector().GetSafeNormal2D(), TargetToPosition),
		-1.0F,
		1.0F);
	const float AngleFromTargetFacing = FMath::RadiansToDegrees(FMath::Acos(FacingDot));

	const EBTNodeResult::Type Result = Super::ExecuteTask(OwnerComp, NodeMemory);
	if (Result == EBTNodeResult::Failed)
	{
		UE_LOG(
			LogSoulCombatLab,
			Warning,
			TEXT("AI EQS reposition move failed: Controller=%s Location=%s"),
			*GetNameSafe(Controller),
			*SelectedLocation.ToCompactString());
	}
	else
	{
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("AI EQS reposition selected: Controller=%s Location=%s TargetDistance=%.1f TravelDistance=%.1f AngleFromTargetFacing=%.1f Result=%s"),
			*GetNameSafe(Controller),
			*SelectedLocation.ToCompactString(),
			TargetDistance,
			TravelDistance,
			AngleFromTargetFacing,
			*UBehaviorTreeTypes::DescribeNodeResult(Result));
	}
	return Result;
}
