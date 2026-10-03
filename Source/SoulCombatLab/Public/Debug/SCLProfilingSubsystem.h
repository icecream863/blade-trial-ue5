#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "SCLProfilingSubsystem.generated.h"

// 显式命令行性能采样入口，普通游戏和 PIE 不自动运行。
UCLASS()
class SOULCOMBATLAB_API USCLProfilingSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void OnWorldEndPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	void PrepareScenario();
	void BeginMeasurement();
	void FinishMeasurement();
	void Cleanup();
	FTimerHandle StageTimer;
	FString Scenario;
	bool bMeasuring{false};
};
