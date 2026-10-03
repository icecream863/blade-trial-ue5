#pragma once

#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "CoreMinimal.h"

#include "BTTask_SCLChaseTarget.generated.h"

// 行为树追击任务：朝当前目标移动。
UCLASS()
class SOULCOMBATLAB_API UBTTask_SCLChaseTarget : public UBTTask_MoveTo
{
	GENERATED_BODY()

public:
	UBTTask_SCLChaseTarget();

	float GetConfiguredAcceptanceRadius() const;

protected:
	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	virtual void OnTaskFinished(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		EBTNodeResult::Type TaskResult) override;
};
