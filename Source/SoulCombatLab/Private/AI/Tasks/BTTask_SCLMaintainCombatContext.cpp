#include "AI/Tasks/BTTask_SCLMaintainCombatContext.h"

#include "BehaviorTree/BehaviorTreeComponent.h"

UBTTask_SCLMaintainCombatContext::UBTTask_SCLMaintainCombatContext()
{
	NodeName = TEXT("Maintain Combat Context");
}

EBTNodeResult::Type UBTTask_SCLMaintainCombatContext::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* const NodeMemory)
{
	return EBTNodeResult::InProgress;
}
