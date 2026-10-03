#include "AI/Services/BTService_SCLUpdateCombatContext.h"

#include "AI/SCLAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UBTService_SCLUpdateCombatContext::UBTService_SCLUpdateCombatContext()
{
	NodeName = TEXT("Update Combat Context");
	Interval = 0.2F;
	RandomDeviation = 0.0F;
	bCallTickOnSearchStart = true;
	INIT_SERVICE_NODE_NOTIFY_FLAGS();
}

void UBTService_SCLUpdateCombatContext::TickNode(
	UBehaviorTreeComponent& OwnerComp,
	uint8* const NodeMemory,
	const float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	if (ASCLAIController* const Controller = Cast<ASCLAIController>(OwnerComp.GetAIOwner()))
	{
		Controller->RefreshBlackboardContext();
	}
}

void UBTService_SCLUpdateCombatContext::OnSearchStart(FBehaviorTreeSearchData& SearchData)
{
	Super::OnSearchStart(SearchData);
	if (ASCLAIController* const Controller = Cast<ASCLAIController>(SearchData.OwnerComp.GetAIOwner()))
	{
		Controller->RefreshBlackboardContext();
	}
}
