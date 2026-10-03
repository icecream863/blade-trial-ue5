#include "AbilitySystem/Effects/SCLStaggeredStateEffect.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayTags/SCLGameplayTags.h"

namespace
{
constexpr float StaggeredStateDurationSeconds{1.0F};
}

USCLStaggeredStateEffect::USCLStaggeredStateEffect(const FObjectInitializer& ObjectInitializer)
	: Super{ObjectInitializer}
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude{
		FScalableFloat{StaggeredStateDurationSeconds}};

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(SCLGameplayTags::State_Staggered);
	UTargetTagsGameplayEffectComponent* const TargetTags =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this,
			TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}

bool USCLStaggeredStateEffect::GrantsStaggeredState() const
{
	return GetGrantedTags().HasTagExact(SCLGameplayTags::State_Staggered);
}
