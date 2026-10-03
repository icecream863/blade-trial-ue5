#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "SCLHUD.generated.h"

class USCLBossWidget;
class USCLDeveloperDebugWidget;

// 游戏 HUD 入口：创建并管理 Boss 与开发调试界面。
UCLASS()
class SOULCOMBATLAB_API ASCLHUD final : public AHUD
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "UI")
	USCLBossWidget* GetBossWidget() const { return BossWidget; }

	UFUNCTION(BlueprintPure, Category = "UI|Developer Debug")
	USCLDeveloperDebugWidget* GetDeveloperDebugWidget() const { return DeveloperDebugWidget; }

	void ToggleDeveloperDebugHUD();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleActorSpawned(AActor* SpawnedActor);

	UPROPERTY(Transient)
	TObjectPtr<USCLBossWidget> BossWidget;

	UPROPERTY(Transient)
	TObjectPtr<USCLDeveloperDebugWidget> DeveloperDebugWidget;

	FDelegateHandle ActorSpawnedDelegateHandle;
};
