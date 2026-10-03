#include "AbilitySystem/Effects/SCLStaminaRegenEffect.h"

#include "AbilitySystem/SCLAttributeSet.h"

namespace
{
constexpr float StaminaRegenPeriodSeconds{0.1F};
constexpr float StaminaPerPeriod{2.5F};
}

USCLStaminaRegenEffect::USCLStaminaRegenEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat{StaminaRegenPeriodSeconds};
	bExecutePeriodicEffectOnApplication = false;

	FGameplayModifierInfo& StaminaModifier = Modifiers.AddDefaulted_GetRef();
	StaminaModifier.Attribute = USCLAttributeSet::GetStaminaAttribute();
	StaminaModifier.ModifierOp = EGameplayModOp::Additive;
	StaminaModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude{
		FScalableFloat{StaminaPerPeriod}};
}
