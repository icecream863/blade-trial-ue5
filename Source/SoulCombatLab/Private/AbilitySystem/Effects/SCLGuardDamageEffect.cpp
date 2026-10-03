#include "AbilitySystem/Effects/SCLGuardDamageEffect.h"

#include "AbilitySystem/SCLAttributeSet.h"
#include "GameplayTags/SCLGameplayTags.h"

USCLGuardDamageEffect::USCLGuardDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat GuardDamageMagnitude;
	GuardDamageMagnitude.DataTag = SCLGameplayTags::Data_Damage_Guard;
	FGameplayModifierInfo& StaminaModifier = Modifiers.AddDefaulted_GetRef();
	StaminaModifier.Attribute = USCLAttributeSet::GetStaminaAttribute();
	StaminaModifier.ModifierOp = EGameplayModOp::Additive;
	StaminaModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude{GuardDamageMagnitude};
}
