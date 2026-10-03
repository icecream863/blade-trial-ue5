#include "AbilitySystem/SCLAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

USCLAttributeSet::USCLAttributeSet()
{
	InitHealth(100.0F);
	InitMaxHealth(100.0F);
	InitStamina(100.0F);
	InitMaxStamina(100.0F);
	InitAttackPower(20.0F);
	InitDefense(0.0F);
	InitPoise(50.0F);
	InitMaxPoise(50.0F);
}

void USCLAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(USCLAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USCLAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USCLAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USCLAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USCLAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USCLAttributeSet, Defense, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USCLAttributeSet, Poise, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USCLAttributeSet, MaxPoise, COND_None, REPNOTIFY_Always);
}

void USCLAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0F);
		AdjustAttributeForMaxChange(Health, MaxHealth, NewValue, GetHealthAttribute());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0F);
		AdjustAttributeForMaxChange(Stamina, MaxStamina, NewValue, GetStaminaAttribute());
	}
	else if (Attribute == GetMaxPoiseAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0F);
		AdjustAttributeForMaxChange(Poise, MaxPoise, NewValue, GetPoiseAttribute());
	}
}

void USCLAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& ModifiedAttribute = Data.EvaluatedData.Attribute;
	if (ModifiedAttribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0F, GetMaxHealth()));
	}
	else if (ModifiedAttribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.0F, GetMaxStamina()));
	}
	else if (ModifiedAttribute == GetPoiseAttribute())
	{
		SetPoise(FMath::Clamp(GetPoise(), 0.0F, GetMaxPoise()));
	}
	else if (ModifiedAttribute == GetAttackPowerAttribute())
	{
		SetAttackPower(FMath::Max(GetAttackPower(), 0.0F));
	}
	else if (ModifiedAttribute == GetDefenseAttribute())
	{
		SetDefense(FMath::Max(GetDefense(), 0.0F));
	}
}

void USCLAttributeSet::AdjustAttributeForMaxChange(
	FGameplayAttributeData& AffectedAttribute,
	const FGameplayAttributeData& MaxAttribute,
	const float NewMaxValue,
	const FGameplayAttribute& AffectedAttributeProperty) const
{
	UAbilitySystemComponent* const AbilitySystem = GetOwningAbilitySystemComponent();
	if (AbilitySystem == nullptr || FMath::IsNearlyEqual(MaxAttribute.GetCurrentValue(), NewMaxValue))
	{
		return;
	}

	const float CurrentMaxValue = MaxAttribute.GetCurrentValue();
	const float NewCurrentValue = CurrentMaxValue > 0.0F
		? AffectedAttribute.GetCurrentValue() * NewMaxValue / CurrentMaxValue
		: NewMaxValue;
	AbilitySystem->ApplyModToAttributeUnsafe(
		AffectedAttributeProperty,
		EGameplayModOp::Additive,
		NewCurrentValue - AffectedAttribute.GetCurrentValue());
}

void USCLAttributeSet::OnRep_Health(const FGameplayAttributeData& PreviousHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USCLAttributeSet, Health, PreviousHealth);
}

void USCLAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& PreviousMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USCLAttributeSet, MaxHealth, PreviousMaxHealth);
}

void USCLAttributeSet::OnRep_Stamina(const FGameplayAttributeData& PreviousStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USCLAttributeSet, Stamina, PreviousStamina);
}

void USCLAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& PreviousMaxStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USCLAttributeSet, MaxStamina, PreviousMaxStamina);
}

void USCLAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& PreviousAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USCLAttributeSet, AttackPower, PreviousAttackPower);
}

void USCLAttributeSet::OnRep_Defense(const FGameplayAttributeData& PreviousDefense)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USCLAttributeSet, Defense, PreviousDefense);
}

void USCLAttributeSet::OnRep_Poise(const FGameplayAttributeData& PreviousPoise)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USCLAttributeSet, Poise, PreviousPoise);
}

void USCLAttributeSet::OnRep_MaxPoise(const FGameplayAttributeData& PreviousMaxPoise)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USCLAttributeSet, MaxPoise, PreviousMaxPoise);
}
