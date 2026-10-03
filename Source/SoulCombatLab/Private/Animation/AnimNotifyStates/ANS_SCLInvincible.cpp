#include "Animation/AnimNotifyStates/ANS_SCLInvincible.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "SoulCombatLab.h"

namespace
{
UAbilitySystemComponent* ResolveAbilitySystem(const USkeletalMeshComponent* const MeshComp)
{
	AActor* const MeshOwner = MeshComp != nullptr ? MeshComp->GetOwner() : nullptr;
	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MeshOwner);
}
}

void UANS_SCLInvincible::NotifyBegin(
	USkeletalMeshComponent* const MeshComp,
	UAnimSequenceBase* const Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (UAbilitySystemComponent* const AbilitySystem = ResolveAbilitySystem(MeshComp))
	{
		AbilitySystem->AddLooseGameplayTag(SCLGameplayTags::State_Invincible);
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("Dodge IFrame started: Owner=%s State=State.Invincible"),
			*GetNameSafe(MeshComp != nullptr ? MeshComp->GetOwner() : nullptr));
	}
}

void UANS_SCLInvincible::NotifyEnd(
	USkeletalMeshComponent* const MeshComp,
	UAnimSequenceBase* const Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (UAbilitySystemComponent* const AbilitySystem = ResolveAbilitySystem(MeshComp))
	{
		AbilitySystem->RemoveLooseGameplayTag(SCLGameplayTags::State_Invincible);
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("Dodge IFrame ended: Owner=%s State.Invincible removed"),
			*GetNameSafe(MeshComp != nullptr ? MeshComp->GetOwner() : nullptr));
	}
}

FString UANS_SCLInvincible::GetNotifyName_Implementation() const
{
	return TEXT("SCL Invincible");
}
