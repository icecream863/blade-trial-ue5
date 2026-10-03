#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "SCLDemoWidget.generated.h"
class UBorder;
class UButton;
class UTextBlock;
class UProgressBar;
class ASCLPlayerCharacter;
class ASCLCharacterBase;
// Demo 界面使用的不可键盘聚焦按钮。
UCLASS()
class USCLDemoButton final : public UButton
{
	GENERATED_BODY()
public:
	USCLDemoButton() { InitIsFocusable(false); }
};
// Demo 主界面：显示开始/暂停、区域进度、资源和战斗提示；只读取状态，不决定技能成败。
UCLASS()
class SOULCOMBATLAB_API USCLDemoWidget final : public UUserWidget
{
	GENERATED_BODY()
public:
	void Refresh();
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
private:
	// 提示优先显示当前动作、失衡机会，再显示一般操作；不会把按下 Q 当成弹反成功。
	void RefreshCombatHint(ASCLPlayerCharacter* Player, const ASCLCharacterBase* Opponent);
	UFUNCTION() void HandlePrimary();
	UFUNCTION() void HandleRestart();
	UFUNCTION() void HandleQuit();
	UPROPERTY(Transient) TObjectPtr<UBorder> Menu;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Heading;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Description;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Objective;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Vitals;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PrimaryText;
	UPROPERTY(Transient) TObjectPtr<UButton> PrimaryButton;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> StaminaBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OpponentVitals;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> PoiseBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CombatHint;
	UPROPERTY(Transient) TObjectPtr<UBorder> QuickHelp;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StaminaHint;
	FTimerHandle RefreshTimer;
};
