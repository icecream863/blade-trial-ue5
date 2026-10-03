#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SCLEnemyHealthWidget.generated.h"

class ASCLEnemyCharacter;
class UProgressBar;
class UTextBlock;
struct FOnAttributeChangeData;

// 普通敌人头顶血条，绑定该敌人的生命属性事件。
UCLASS()
class SOULCOMBATLAB_API USCLEnemyHealthWidget final : public UUserWidget
{
	GENERATED_BODY()
public:
	void BindEnemy(ASCLEnemyCharacter& Enemy);
	void UnbindEnemy();
	ASCLEnemyCharacter* GetObservedEnemy() const { return ObservedEnemy.Get(); }
	float GetDisplayedHealthFraction() const;
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
private:
	void RefreshHealth();
	void RefreshVisibility();
	void HandleHealthChanged(const FOnAttributeChangeData& Change);
	TWeakObjectPtr<ASCLEnemyCharacter> ObservedEnemy;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FTimerHandle VisibilityTimer;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HealthText;
};
