#include "AbilitySystem/Effects/SCLDodgingStateEffect.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayTags/SCLGameplayTags.h"

USCLDodgingStateEffect::USCLDodgingStateEffect(const FObjectInitializer& ObjectInitializer)
	: Super{ObjectInitializer}
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(SCLGameplayTags::State_Dodging);
	UTargetTagsGameplayEffectComponent* const TargetTags =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this,
			TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}

bool USCLDodgingStateEffect::GrantsDodgingState() const
{
	return GetGrantedTags().HasTagExact(SCLGameplayTags::State_Dodging);
}
