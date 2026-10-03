#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_OnCircle.h"

#include "SCLEnvQueryNodes.generated.h"

// EQS 上下文：把 AI 当前目标提供给位置查询。
UCLASS()
class SOULCOMBATLAB_API USCLEnvQueryContext_TargetActor final : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(
		FEnvQueryInstance& QueryInstance,
		FEnvQueryContextData& ContextData) const override;
};

// EQS 生成器：在目标周围生成环形候选战斗位置。
UCLASS()
class SOULCOMBATLAB_API USCLEnvQueryGenerator_CombatRing final
	: public UEnvQueryGenerator_OnCircle
{
	GENERATED_BODY()

public:
	USCLEnvQueryGenerator_CombatRing(const FObjectInitializer& ObjectInitializer);

	float GetConfiguredRadius() const { return CircleRadius.DefaultValue; }
	int32 GetConfiguredPointCount() const { return NumberOfPoints.DefaultValue; }
	const FEnvTraceData& GetProjectionData() const { return ProjectionData; }
};

// EQS 测试：评估候选点相对目标的侧向位置。
UCLASS()
class SOULCOMBATLAB_API USCLEnvQueryTest_LateralPosition final : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	USCLEnvQueryTest_LateralPosition(const FObjectInitializer& ObjectInitializer);

	bool UsesAbsoluteTwoDimensionalDot() const;
	bool UsesTargetFacingAndTargetToItemDirection() const;

protected:
	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;
};
