#include "AI/Tasks/BTTask_SCLChaseTarget.h"

#include "AIController.h"
#include "AI/SCLAIState.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Actor.h"
#include "SoulCombatLab.h"

namespace
{
constexpr float ChaseAcceptanceRadius{120.0F};
}

UBTTask_SCLChaseTarget::UBTTask_SCLChaseTarget()
{
	NodeName = TEXT("Chase Target");
	BlackboardKey.SelectedKeyName = SCLBlackboardKeys::TargetActor;
	AcceptableRadius = FValueOrBBKey_Float{ChaseAcceptanceRadius};
	ObservedBlackboardValueTolerance = FValueOrBBKey_Float{60.0F};
	bAllowStrafe = FValueOrBBKey_Bool{false};
	bAllowPartialPath = FValueOrBBKey_Bool{false};
	bTrackMovingGoal = FValueOrBBKey_Bool{true};
	bRequireNavigableEndLocation = FValueOrBBKey_Bool{true};
	bProjectGoalLocation = FValueOrBBKey_Bool{true};
	bReachTestIncludesAgentRadius = FValueOrBBKey_Bool{true};
	bReachTestIncludesGoalRadius = FValueOrBBKey_Bool{true};
	bStartFromPreviousPath = FValueOrBBKey_Bool{false};
	bObserveBlackboardValue = true;
}

float UBTTask_SCLChaseTarget::GetConfiguredAcceptanceRadius() const
{
	return AcceptableRadius.GetValue(static_cast<const UBlackboardComponent*>(nullptr));
}

EBTNodeResult::Type UBTTask_SCLChaseTarget::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* const NodeMemory)
{
	const EBTNodeResult::Type Result = Super::ExecuteTask(OwnerComp, NodeMemory);
	if (Result == EBTNodeResult::Failed)
	{
		UE_LOG(
			LogSoulCombatLab,
			Warning,
			TEXT("AI chase request failed: Controller=%s Target=%s"),
			*GetNameSafe(OwnerComp.GetAIOwner()),
			*GetNameSafe(
				OwnerComp.GetBlackboardComponent() != nullptr
					? OwnerComp.GetBlackboardComponent()->GetValueAsObject(SCLBlackboardKeys::TargetActor)
					: nullptr));
	}
	else
	{
		UE_LOG(
			LogSoulCombatLab,
			Verbose,
			TEXT("AI chase requested: Controller=%s Target=%s Result=%s"),
			*GetNameSafe(OwnerComp.GetAIOwner()),
			*GetNameSafe(
				OwnerComp.GetBlackboardComponent() != nullptr
					? OwnerComp.GetBlackboardComponent()->GetValueAsObject(SCLBlackboardKeys::TargetActor)
					: nullptr),
			*UBehaviorTreeTypes::DescribeNodeResult(Result));
	}
	return Result;
}

EBTNodeResult::Type UBTTask_SCLChaseTarget::AbortTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* const NodeMemory)
{
	UE_LOG(
		LogSoulCombatLab,
		Verbose,
		TEXT("AI chase aborted: Controller=%s"),
		*GetNameSafe(OwnerComp.GetAIOwner()));
	return Super::AbortTask(OwnerComp, NodeMemory);
}

void UBTTask_SCLChaseTarget::OnTaskFinished(
	UBehaviorTreeComponent& OwnerComp,
	uint8* const NodeMemory,
	const EBTNodeResult::Type TaskResult)
{
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
	UE_LOG(
		LogSoulCombatLab,
		Verbose,
		TEXT("AI chase finished: Controller=%s Result=%s"),
		*GetNameSafe(OwnerComp.GetAIOwner()),
		*UBehaviorTreeTypes::DescribeNodeResult(TaskResult));
}
