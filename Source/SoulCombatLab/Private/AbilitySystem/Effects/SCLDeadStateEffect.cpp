#include "AbilitySystem/Effects/SCLDeadStateEffect.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayTags/SCLGameplayTags.h"

USCLDeadStateEffect::USCLDeadStateEffect(const FObjectInitializer& ObjectInitializer)
	: Super{ObjectInitializer}
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(SCLGameplayTags::State_Dead);
	UTargetTagsGameplayEffectComponent* const TargetTags =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this,
			TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}
