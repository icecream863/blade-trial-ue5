#include "AbilitySystem/Effects/SCLLightAttackCostEffect.h"

#include "AbilitySystem/SCLAttributeSet.h"
#include "GameplayTags/SCLGameplayTags.h"

USCLLightAttackCostEffect::USCLLightAttackCostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat CostMagnitude;
	CostMagnitude.DataTag = SCLGameplayTags::Data_Cost_Stamina;

	FGameplayModifierInfo& StaminaModifier = Modifiers.AddDefaulted_GetRef();
	StaminaModifier.Attribute = USCLAttributeSet::GetStaminaAttribute();
	StaminaModifier.ModifierOp = EGameplayModOp::Additive;
	StaminaModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude{CostMagnitude};
}
