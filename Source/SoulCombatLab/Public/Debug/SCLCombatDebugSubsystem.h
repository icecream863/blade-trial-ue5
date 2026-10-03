#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "SCLCombatDebugSubsystem.generated.h"

class ASCLEnemyCharacter;

// 世界级战斗调试服务：按需绘制命中和 AI 信息；关闭时不轮询，也不依赖 Actor Tick。
UCLASS()
class SOULCOMBATLAB_API USCLCombatDebugSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void OnWorldEndPlay(UWorld& InWorld) override;
	void SetEnabled(bool bInEnabled);
	bool IsEnabled() const { return bEnabled; }
	bool IsRefreshTimerActive() const;
	static bool IsEnabledForWorld(const UWorld* World);

	static constexpr float RefreshInterval{0.1F};
	static constexpr float GeometryLifetime{0.15F};
	static constexpr float EventLifetime{0.75F};

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	void RefreshDraw();
	void HandleActorSpawned(AActor* Actor);
	void DrawEnemy(const ASCLEnemyCharacter& Enemy) const;

	TArray<TWeakObjectPtr<ASCLEnemyCharacter>> Enemies;
	FDelegateHandle ActorSpawnedHandle;
	FTimerHandle RefreshTimer;
	bool bEnabled{false};
	bool bEndingPlay{false};
};
