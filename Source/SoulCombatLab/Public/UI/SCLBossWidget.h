#pragma once

#include "AI/Boss/SCLBossTypes.h"
#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "SCLBossWidget.generated.h"

struct FOnAttributeChangeData;
class ASCLBossCharacter;
class UProgressBar;
class UTextBlock;

// Boss 血条界面，监听 Boss 属性变化并刷新显示。
UCLASS()
class SOULCOMBATLAB_API USCLBossWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindBoss(ASCLBossCharacter& NewBoss);
	void UnbindBoss();

	UFUNCTION(BlueprintPure, Category = "Boss UI")
	ASCLBossCharacter* GetObservedBoss() const { return ObservedBoss.Get(); }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	void RefreshHealth();
	void RefreshPhase();
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleDeadTagChanged(FGameplayTag Tag, int32 NewCount);
	void HidePhaseTransition();
	void HideAfterDeath();

	UFUNCTION()
	void HandleBossPhaseChanged(ESCLBossPhase PreviousPhase, ESCLBossPhase NewPhase);

	UFUNCTION()
	void HandleBossDestroyed(AActor* DestroyedActor);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PhaseText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PhaseTransitionText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;

	TWeakObjectPtr<ASCLBossCharacter> ObservedBoss;
	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
	FDelegateHandle DeadTagDelegateHandle;
	FTimerHandle PhaseTransitionTimerHandle;
	FTimerHandle DeathHideTimerHandle;
};
