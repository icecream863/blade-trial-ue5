#include "AbilitySystem/Effects/SCLBlockingStateEffect.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayTags/SCLGameplayTags.h"

USCLBlockingStateEffect::USCLBlockingStateEffect(const FObjectInitializer& ObjectInitializer)
	: Super{ObjectInitializer}
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(SCLGameplayTags::State_Blocking);
	UTargetTagsGameplayEffectComponent* const TargetTags =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this,
			TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}
