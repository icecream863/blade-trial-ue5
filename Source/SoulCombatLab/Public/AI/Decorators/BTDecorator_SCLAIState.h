#pragma once

#include "AI/SCLAIState.h"
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "CoreMinimal.h"

#include "BTDecorator_SCLAIState.generated.h"

// 行为树条件节点：检查敌人当前 AI 状态是否满足分支要求。
UCLASS()
class SOULCOMBATLAB_API UBTDecorator_SCLAIState : public UBTDecorator_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTDecorator_SCLAIState();

	void ConfigureState(ESCLEnemyAIState InExpectedState);
	ESCLEnemyAIState GetExpectedState() const { return ExpectedState; }

	virtual FString GetStaticDescription() const override;

protected:
	virtual bool CalculateRawConditionValue(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) const override;

private:
	UPROPERTY()
	ESCLEnemyAIState ExpectedState{ESCLEnemyAIState::Idle};
};
