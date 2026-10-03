#pragma once

#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "CoreMinimal.h"

#include "BTTask_SCLReposition.generated.h"

// 行为树换位任务：移动到查询出的战斗位置。
UCLASS()
class SOULCOMBATLAB_API UBTTask_SCLReposition final : public UBTTask_MoveTo
{
	GENERATED_BODY()

public:
	UBTTask_SCLReposition();

	float GetConfiguredAcceptanceRadius() const;

protected:
	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
};
