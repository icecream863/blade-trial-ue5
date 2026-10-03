#include "AbilitySystem/Effects/SCLExecutableStateEffect.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayTags/SCLGameplayTags.h"

namespace
{
constexpr float ExecutableStateDurationSeconds{4.0F};
}

USCLExecutableStateEffect::USCLExecutableStateEffect(const FObjectInitializer& ObjectInitializer)
	: Super{ObjectInitializer}
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude{
		FScalableFloat{ExecutableStateDurationSeconds}};

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(SCLGameplayTags::State_Executable);
	// Keep the victim vulnerable for the whole execution opportunity.
	GrantedTags.AddTag(SCLGameplayTags::State_Staggered);
	UTargetTagsGameplayEffectComponent* const TargetTags =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this,
			TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}
