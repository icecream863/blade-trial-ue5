#include "AbilitySystem/Effects/SCLPoiseDamageEffect.h"

#include "AbilitySystem/SCLAttributeSet.h"
#include "GameplayTags/SCLGameplayTags.h"

USCLPoiseDamageEffect::USCLPoiseDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat PoiseDamageMagnitude;
	PoiseDamageMagnitude.DataTag = SCLGameplayTags::Data_Damage_Poise;

	FGameplayModifierInfo& PoiseModifier = Modifiers.AddDefaulted_GetRef();
	PoiseModifier.Attribute = USCLAttributeSet::GetPoiseAttribute();
	PoiseModifier.ModifierOp = EGameplayModOp::Additive;
	PoiseModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude{PoiseDamageMagnitude};
}
