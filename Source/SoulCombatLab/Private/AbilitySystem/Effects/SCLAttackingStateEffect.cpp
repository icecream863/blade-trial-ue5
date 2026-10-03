#include "AbilitySystem/Effects/SCLAttackingStateEffect.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayTags/SCLGameplayTags.h"

namespace
{
constexpr float LightAttackStateDurationSeconds{1.0F};
}

USCLAttackingStateEffect::USCLAttackingStateEffect(const FObjectInitializer& ObjectInitializer)
	: Super{ObjectInitializer}
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude{
		FScalableFloat{LightAttackStateDurationSeconds}};

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(SCLGameplayTags::State_Attacking);
	UTargetTagsGameplayEffectComponent* const TargetTags =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this,
			TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}

bool USCLAttackingStateEffect::GrantsAttackingState() const
{
	return GetGrantedTags().HasTagExact(SCLGameplayTags::State_Attacking);
}
