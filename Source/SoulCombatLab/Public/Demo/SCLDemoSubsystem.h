#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "SCLDemoSubsystem.generated.h"

class ASCLCharacterBase;
class ASCLPlayerCharacter;
class USCLDemoWidget;
class ASCLDemoMap;

UENUM()
enum class ESCLDemoState : uint8 { Inactive, Title, Playing, StageClear, Defeat, Victory, Error };

UENUM()
enum class ESCLDemoStage : uint8 { Spawn, Training, Sword, Heavy, MiniArena, BossGate, Boss };

// 单人 Demo 流程管理器：生成关卡对象、推进区域、处理失败与重试。
// 战斗规则由 GAS 和 AI 负责；本类只管理流程与对象生命周期。
UCLASS()
class SOULCOMBATLAB_API USCLDemoSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void OnWorldEndPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	void PrimaryAction();
	void RestartRun();
	void TogglePause();
	void Quit();
	// 重新判断玩家是否进入下一区域；移动后或自动化检查时调用。
	void CheckRegion();
	ESCLDemoState GetState() const { return State; }
	ESCLDemoStage GetStage() const { return Stage; }
	bool IsActive() const { return State != ESCLDemoState::Inactive; }
	bool IsPaused() const { return bPaused; }
	ASCLPlayerCharacter* GetPlayer() const { return Player.Get(); }
	ASCLCharacterBase* GetOpponent() const;
	int32 GetLivingOpponentCount() const;
	bool IsExploring() const { return State == ESCLDemoState::Playing || State == ESCLDemoState::StageClear; }
	FText GetObjective() const;
	FText GetPrimaryLabel() const;
	static ESCLDemoStage NextStage(ESCLDemoStage Current);
private:
	void InitializeDemo();
	void StartStage(ESCLDemoStage NewStage, bool bRespawnPlayer = true);
	void ClearOpponents();
	void ClearEncounter();
	void Shutdown();
	void HandleDeathTag(FGameplayTag Tag, int32 Count);
	UFUNCTION() void HandlePlayerDestroyed(AActor* DestroyedActor);
	UFUNCTION() void HandleOpponentDestroyed(AActor* DestroyedActor);
	void ResolveOutcome();
	void ApplyPresentation();
	void Fail(const TCHAR* Reason);
	UPROPERTY(Transient) TObjectPtr<USCLDemoWidget> Widget;
	TWeakObjectPtr<ASCLPlayerCharacter> Player;
	struct FOpponentObserver
	{
		TWeakObjectPtr<ASCLCharacterBase> Character;
		TWeakObjectPtr<AController> Controller;
		FDelegateHandle DeadHandle;
	};
	TArray<FOpponentObserver> Opponents;
	TWeakObjectPtr<ASCLDemoMap> Map;
	FDelegateHandle PlayerDeadHandle;
	FTimerHandle StartupTimer;
	FTimerHandle OutcomeTimer;
	FTimerHandle RegionTimer;
	ESCLDemoState State{ESCLDemoState::Inactive};
	ESCLDemoStage Stage{ESCLDemoStage::Spawn};
	bool bPaused{false};
	bool bShuttingDown{false};
};
