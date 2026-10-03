#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "SCLAttackData.generated.h"

class UAnimMontage;

// 通用攻击的一段连击数据：记录 Montage 段与输入窗口。
USTRUCT(BlueprintType)
struct FSCLAttackInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FName MontageSection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0"))
	float DamageMultiplier{1.0F};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0"))
	float StaminaCost{0.0F};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0"))
	float PoiseDamage{0.0F};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ComboWindowStartNormalized{0.55F};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ComboWindowEndNormalized{0.85F};
};

// 敌人通用攻击数据资产：配置 Montage 和连击段。
UCLASS(BlueprintType)
class SOULCOMBATLAB_API USCLAttackData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USCLAttackData();

	UFUNCTION(BlueprintPure, Category = "Attack Data")
	UAnimMontage* GetAttackMontage() const { return AttackMontage; }

	UFUNCTION(BlueprintPure, Category = "Attack Data")
	float GetStaminaCost() const { return StaminaCost; }

	UFUNCTION(BlueprintPure, Category = "Attack Data")
	float GetDamageMultiplier() const { return DamageMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Attack Data")
	float GetPoiseDamage() const { return PoiseDamage; }

	UFUNCTION(BlueprintPure, Category = "Attack Data")
	FGameplayTag GetAttackTag() const { return AttackTag; }

	UFUNCTION(BlueprintPure, Category = "Attack Data|Combo")
	int32 GetComboStepCount() const { return LightCombo.Num(); }

	const FSCLAttackInfo* FindComboStep(int32 StepIndex) const;
	int32 FindComboStepIndex(FName MontageSection) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float StaminaCost{0.0F};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DamageMultiplier{1.0F};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float PoiseDamage{0.0F};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (AllowPrivateAccess = "true", Categories = "Ability.Attack"))
	FGameplayTag AttackTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Combo", meta = (AllowPrivateAccess = "true"))
	TArray<FSCLAttackInfo> LightCombo;
};
