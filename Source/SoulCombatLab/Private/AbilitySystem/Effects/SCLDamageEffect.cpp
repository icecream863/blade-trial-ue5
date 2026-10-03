#include "AbilitySystem/Effects/SCLDamageEffect.h"

#include "AbilitySystem/SCLAttributeSet.h"
#include "GameplayTags/SCLGameplayTags.h"

USCLDamageEffect::USCLDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat DamageMagnitude;
	DamageMagnitude.DataTag = SCLGameplayTags::Data_Damage;

	FGameplayModifierInfo& HealthModifier = Modifiers.AddDefaulted_GetRef();
	HealthModifier.Attribute = USCLAttributeSet::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Additive;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude{DamageMagnitude};
}
