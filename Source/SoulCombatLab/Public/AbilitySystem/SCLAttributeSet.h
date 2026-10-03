#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"

#include "SCLAttributeSet.generated.h"

#define SCL_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

struct FGameplayEffectModCallbackData;
class FLifetimeProperty;

// 角色数值表：保存生命、体力、攻击、防御和韧性，并处理效果结算与复制回调。
UCLASS()
class SOULCOMBATLAB_API USCLAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	USCLAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Vital")
	FGameplayAttributeData Health;
	SCL_ATTRIBUTE_ACCESSORS(USCLAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Vital")
	FGameplayAttributeData MaxHealth;
	SCL_ATTRIBUTE_ACCESSORS(USCLAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Attributes|Vital")
	FGameplayAttributeData Stamina;
	SCL_ATTRIBUTE_ACCESSORS(USCLAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Attributes|Vital")
	FGameplayAttributeData MaxStamina;
	SCL_ATTRIBUTE_ACCESSORS(USCLAttributeSet, MaxStamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "Attributes|Combat")
	FGameplayAttributeData AttackPower;
	SCL_ATTRIBUTE_ACCESSORS(USCLAttributeSet, AttackPower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Defense, Category = "Attributes|Combat")
	FGameplayAttributeData Defense;
	SCL_ATTRIBUTE_ACCESSORS(USCLAttributeSet, Defense)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Poise, Category = "Attributes|Combat")
	FGameplayAttributeData Poise;
	SCL_ATTRIBUTE_ACCESSORS(USCLAttributeSet, Poise)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxPoise, Category = "Attributes|Combat")
	FGameplayAttributeData MaxPoise;
	SCL_ATTRIBUTE_ACCESSORS(USCLAttributeSet, MaxPoise)

private:
	void AdjustAttributeForMaxChange(
		FGameplayAttributeData& AffectedAttribute,
		const FGameplayAttributeData& MaxAttribute,
		float NewMaxValue,
		const FGameplayAttribute& AffectedAttributeProperty) const;

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& PreviousHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& PreviousMaxHealth);

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& PreviousStamina);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& PreviousMaxStamina);

	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& PreviousAttackPower);

	UFUNCTION()
	void OnRep_Defense(const FGameplayAttributeData& PreviousDefense);

	UFUNCTION()
	void OnRep_Poise(const FGameplayAttributeData& PreviousPoise);

	UFUNCTION()
	void OnRep_MaxPoise(const FGameplayAttributeData& PreviousMaxPoise);
};

#undef SCL_ATTRIBUTE_ACCESSORS
