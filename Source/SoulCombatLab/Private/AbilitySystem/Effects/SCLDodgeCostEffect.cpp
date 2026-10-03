#include "AbilitySystem/Effects/SCLDodgeCostEffect.h"

#include "AbilitySystem/SCLAttributeSet.h"
#include "GameplayTags/SCLGameplayTags.h"

USCLDodgeCostEffect::USCLDodgeCostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat StaminaCostMagnitude;
	StaminaCostMagnitude.DataTag = SCLGameplayTags::Data_Cost_Stamina;
	FGameplayModifierInfo& StaminaModifier = Modifiers.AddDefaulted_GetRef();
	StaminaModifier.Attribute = USCLAttributeSet::GetStaminaAttribute();
	StaminaModifier.ModifierOp = EGameplayModOp::Additive;
	StaminaModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude{StaminaCostMagnitude};
}
