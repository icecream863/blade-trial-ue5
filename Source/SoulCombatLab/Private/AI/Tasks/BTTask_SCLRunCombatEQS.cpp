#include "AI/Tasks/BTTask_SCLRunCombatEQS.h"

#include "AI/SCLAIState.h"
#include "EnvironmentQuery/EnvQuery.h"

UBTTask_SCLRunCombatEQS::UBTTask_SCLRunCombatEQS()
{
	NodeName = TEXT("Find EQS Combat Position");
	BlackboardKey.SelectedKeyName = SCLBlackboardKeys::IdealCombatLocation;
	bUseBBKey = false;
	EQSRequest.RunMode = EEnvQueryRunMode::SingleResult;
	EQSRequest.bUseBBKeyForQueryTemplate = false;
	bUpdateBBOnFail = false;
}

void UBTTask_SCLRunCombatEQS::ConfigureQuery(UEnvQuery& QueryTemplate)
{
	EQSRequest.QueryTemplate = &QueryTemplate;
}
