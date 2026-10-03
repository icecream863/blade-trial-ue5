#pragma once

#include "BehaviorTree/BTService.h"
#include "CoreMinimal.h"

#include "BTService_SCLUpdateCombatContext.generated.h"

// 行为树服务：周期更新目标距离与战斗上下文，供后续任务决策。
UCLASS()
class SOULCOMBATLAB_API UBTService_SCLUpdateCombatContext : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_SCLUpdateCombatContext();

protected:
	virtual void TickNode(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		float DeltaSeconds) override;
	virtual void OnSearchStart(FBehaviorTreeSearchData& SearchData) override;
};
