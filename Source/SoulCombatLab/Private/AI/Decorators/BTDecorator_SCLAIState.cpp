#include "AI/Decorators/BTDecorator_SCLAIState.h"

#include "BehaviorTree/BlackboardComponent.h"

UBTDecorator_SCLAIState::UBTDecorator_SCLAIState()
{
	NodeName = TEXT("AI State");
	BlackboardKey.AddEnumFilter(
		this,
		TEXT("BlackboardKey"),
		StaticEnum<ESCLEnemyAIState>());
	BlackboardKey.SelectedKeyName = SCLBlackboardKeys::CombatState;
	FlowAbortMode = EBTFlowAbortMode::Both;
}

void UBTDecorator_SCLAIState::ConfigureState(const ESCLEnemyAIState InExpectedState)
{
	ExpectedState = InExpectedState;
	NodeName = FString::Printf(
		TEXT("State Is %s"),
		*StaticEnum<ESCLEnemyAIState>()->GetNameStringByValue(
			static_cast<int64>(ExpectedState)));
}

FString UBTDecorator_SCLAIState::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("%s == %s"),
		*SCLBlackboardKeys::CombatState.ToString(),
		*StaticEnum<ESCLEnemyAIState>()->GetNameStringByValue(
			static_cast<int64>(ExpectedState)));
}

bool UBTDecorator_SCLAIState::CalculateRawConditionValue(
	UBehaviorTreeComponent& OwnerComp,
	uint8* const NodeMemory) const
{
	const UBlackboardComponent* const BlackboardComponent = OwnerComp.GetBlackboardComponent();
	return BlackboardComponent != nullptr &&
		BlackboardComponent->GetValueAsEnum(SCLBlackboardKeys::CombatState) ==
			static_cast<uint8>(ExpectedState);
}
