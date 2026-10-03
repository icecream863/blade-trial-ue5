#pragma once

#include "BehaviorTree/Tasks/BTTask_RunEQSQuery.h"
#include "CoreMinimal.h"

#include "BTTask_SCLRunCombatEQS.generated.h"

class UEnvQuery;

// 行为树 EQS 任务：查询敌人攻击后的候选换位点。
UCLASS()
class SOULCOMBATLAB_API UBTTask_SCLRunCombatEQS final : public UBTTask_RunEQSQuery
{
	GENERATED_BODY()

public:
	UBTTask_SCLRunCombatEQS();

	void ConfigureQuery(UEnvQuery& QueryTemplate);
};
