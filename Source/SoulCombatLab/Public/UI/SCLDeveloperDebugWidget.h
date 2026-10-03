#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "SCLDeveloperDebugWidget.generated.h"

class ASCLEnemyCharacter;
class ASCLPlayerCharacter;
class UTextBlock;

// 开发调试面板，显示玩家与敌人的实时诊断数据。
UCLASS()
class SOULCOMBATLAB_API USCLDeveloperDebugWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindPlayer(ASCLPlayerCharacter& Player);
	void RegisterEnemy(ASCLEnemyCharacter& Enemy);
	void ToggleDebugDisplay();

	UFUNCTION(BlueprintPure, Category = "Developer Debug")
	bool IsDebugDisplayVisible() const;

	UFUNCTION(BlueprintPure, Category = "Developer Debug")
	float GetRefreshInterval() const { return RefreshIntervalSeconds; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	void RefreshDebugText();
	void StartRefreshing();
	void StopRefreshing();
	ASCLEnemyCharacter* ResolveObservedEnemy();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DebugBodyText;

	TWeakObjectPtr<ASCLPlayerCharacter> ObservedPlayer;
	TArray<TWeakObjectPtr<ASCLEnemyCharacter>> KnownEnemies;
	FTimerHandle RefreshTimerHandle;
	float RefreshIntervalSeconds{0.25F};
};
