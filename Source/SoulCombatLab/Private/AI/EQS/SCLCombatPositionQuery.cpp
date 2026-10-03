#include "AI/EQS/SCLCombatPositionQuery.h"

#include "AI/EQS/SCLEnvQueryNodes.h"
#include "Engine/EngineTypes.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Distance.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Pathfinding.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Trace.h"
#include "UObject/UObjectGlobals.h"

UEnvQuery* SCLCombatPositionQuery::Build(UObject& Outer)
{
	UEnvQuery* const Query = NewObject<UEnvQuery>(
		&Outer,
		MakeUniqueObjectName(&Outer, UEnvQuery::StaticClass(), TEXT("SCLCombatPositionQuery")));
	UEnvQueryOption* const Option = NewObject<UEnvQueryOption>(Query);
	Query->GetOptionsMutable().Add(Option);

	USCLEnvQueryGenerator_CombatRing* const RingGenerator =
		NewObject<USCLEnvQueryGenerator_CombatRing>(Option);
	Option->Generator = RingGenerator;

	UEnvQueryTest_Distance* const DistanceTest = NewObject<UEnvQueryTest_Distance>(Option);
	DistanceTest->TestMode = EEnvTestDistance::Distance2D;
	DistanceTest->DistanceTo = USCLEnvQueryContext_TargetActor::StaticClass();
	DistanceTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
	DistanceTest->FilterType = EEnvTestFilterType::Range;
	DistanceTest->FloatValueMin.DefaultValue = MinimumDistance;
	DistanceTest->FloatValueMax.DefaultValue = MaximumDistance;
	DistanceTest->ScoringEquation = EEnvTestScoreEquation::Linear;
	DistanceTest->ScoringFactor.DefaultValue = 1.0F;
	DistanceTest->bDefineReferenceValue = true;
	DistanceTest->ReferenceValue.DefaultValue = PreferredDistance;
	Option->Tests.Add(DistanceTest);

	UEnvQueryTest_Trace* const LineOfSightTest = NewObject<UEnvQueryTest_Trace>(Option);
	LineOfSightTest->Context = USCLEnvQueryContext_TargetActor::StaticClass();
	LineOfSightTest->TestPurpose = EEnvTestPurpose::Filter;
	LineOfSightTest->FilterType = EEnvTestFilterType::Match;
	LineOfSightTest->BoolValue.DefaultValue = false;
	LineOfSightTest->TraceFromContext.DefaultValue = false;
	LineOfSightTest->ItemHeightOffset.DefaultValue = 70.0F;
	LineOfSightTest->ContextHeightOffset.DefaultValue = 70.0F;
	LineOfSightTest->TraceData.SetGeometryOnly();
	LineOfSightTest->TraceData.TraceShape = EEnvTraceShape::Line;
	LineOfSightTest->TraceData.SerializedChannel = ECC_Visibility;
	LineOfSightTest->TraceData.TraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility);
	Option->Tests.Add(LineOfSightTest);

	UEnvQueryTest_Pathfinding* const PathTest = NewObject<UEnvQueryTest_Pathfinding>(Option);
	PathTest->TestMode = EEnvTestPathfinding::PathExist;
	PathTest->Context = UEnvQueryContext_Querier::StaticClass();
	PathTest->TestPurpose = EEnvTestPurpose::Filter;
	PathTest->FilterType = EEnvTestFilterType::Match;
	PathTest->BoolValue.DefaultValue = true;
	PathTest->PathFromContext.DefaultValue = true;
	PathTest->SkipUnreachable.DefaultValue = true;
	Option->Tests.Add(PathTest);

	Option->Tests.Add(NewObject<USCLEnvQueryTest_LateralPosition>(Option));
	return Query;
}
