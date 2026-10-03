#include "AI/EQS/SCLEnvQueryNodes.h"

#include "AI/SCLAIController.h"
#include "AIController.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Item.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "GameFramework/Pawn.h"

void USCLEnvQueryContext_TargetActor::ProvideContext(
	FEnvQueryInstance& QueryInstance,
	FEnvQueryContextData& ContextData) const
{
	const UObject* const QueryOwner = QueryInstance.Owner.Get();
	const ASCLAIController* Controller = Cast<ASCLAIController>(QueryOwner);
	if (Controller == nullptr)
	{
		const APawn* const Pawn = Cast<APawn>(QueryOwner);
		Controller = Pawn != nullptr
			? Cast<ASCLAIController>(Pawn->GetController())
			: nullptr;
	}

	if (Controller != nullptr && IsValid(Controller->GetPerceivedTarget()))
	{
		UEnvQueryItemType_Actor::SetContextHelper(
			ContextData,
			Controller->GetPerceivedTarget());
	}
}

USCLEnvQueryGenerator_CombatRing::USCLEnvQueryGenerator_CombatRing(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CircleRadius.DefaultValue = 185.0F;
	PointOnCircleSpacingMethod = EEnvQueryPointSpacingMethod::ByNumberOfPoints;
	NumberOfPoints.DefaultValue = 12;
	CircleCenter = USCLEnvQueryContext_TargetActor::StaticClass();
	CircleCenterZOffset.DefaultValue = 0.0F;
	bDefineArc = false;
	bIgnoreAnyContextActorsWhenGeneratingCircle = true;

	ProjectionData.SetNavmeshOnly();
	ProjectionData.ProjectDown = 250.0F;
	ProjectionData.ProjectUp = 100.0F;
	ProjectionData.ExtentX = 40.0F;
}

USCLEnvQueryTest_LateralPosition::USCLEnvQueryTest_LateralPosition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);
	TestPurpose = EEnvTestPurpose::Score;
	ScoringEquation = EEnvTestScoreEquation::InverseLinear;
	ScoringFactor.DefaultValue = 1.25F;
	FloatValueMin.DefaultValue = 0.0F;
	FloatValueMax.DefaultValue = 1.0F;
}

bool USCLEnvQueryTest_LateralPosition::UsesAbsoluteTwoDimensionalDot() const
{
	return true;
}

bool USCLEnvQueryTest_LateralPosition::UsesTargetFacingAndTargetToItemDirection() const
{
	return true;
}

void USCLEnvQueryTest_LateralPosition::RunTest(FEnvQueryInstance& QueryInstance) const
{
	UObject* const QueryOwner = QueryInstance.Owner.Get();
	if (QueryOwner == nullptr)
	{
		return;
	}

	FloatValueMin.BindData(QueryOwner, QueryInstance.QueryID);
	FloatValueMax.BindData(QueryOwner, QueryInstance.QueryID);
	const float MinimumScore = FloatValueMin.GetValue();
	const float MaximumScore = FloatValueMax.GetValue();

	TArray<AActor*> TargetActors;
	if (!QueryInstance.PrepareContext(
		USCLEnvQueryContext_TargetActor::StaticClass(),
		TargetActors) || TargetActors.IsEmpty() || !IsValid(TargetActors[0]))
	{
		return;
	}

	const AActor& Target = *TargetActors[0];
	const FVector TargetForward = Target.GetActorForwardVector().GetSafeNormal2D();
	for (FEnvQueryInstance::ItemIterator Iterator(this, QueryInstance); Iterator; ++Iterator)
	{
		const FVector TargetToItem =
			(GetItemLocation(QueryInstance, Iterator.GetIndex()) - Target.GetActorLocation())
			.GetSafeNormal2D();
		const float AbsoluteFacingDot = FMath::Abs(
			FVector::DotProduct(TargetForward, TargetToItem));
		Iterator.SetScore(
			TestPurpose,
			FilterType,
			AbsoluteFacingDot,
			MinimumScore,
			MaximumScore);
	}
}

FText USCLEnvQueryTest_LateralPosition::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Angle: Prefer Lateral Combat Position"));
}

FText USCLEnvQueryTest_LateralPosition::GetDescriptionDetails() const
{
	return FText::FromString(
		TEXT("Scores the absolute 2D dot product between target facing and target-to-item direction."));
}
