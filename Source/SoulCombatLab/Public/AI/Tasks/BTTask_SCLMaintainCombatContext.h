#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "CoreMinimal.h"

#include "BTTask_SCLMaintainCombatContext.generated.h"

// 行为树占位任务：ExecuteTask 保持 InProgress，本身不刷新黑板。
// 战斗上下文的周期更新由 BTService_SCLUpdateCombatContext 负责。
UCLASS()
class SOULCOMBATLAB_API UBTTask_SCLMaintainCombatContext : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SCLMaintainCombatContext();

protected:
	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
};
