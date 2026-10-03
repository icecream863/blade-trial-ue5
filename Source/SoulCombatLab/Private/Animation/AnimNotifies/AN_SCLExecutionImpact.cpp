#include "Animation/AnimNotifies/AN_SCLExecutionImpact.h"
#include "AbilitySystem/Abilities/SCLExecutionAbility.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UAN_SCLExecutionImpact::Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& Reference)
{
	Super::Notify(Mesh, Animation, Reference);
	const ASCLPlayerCharacter* Player = Mesh ? Cast<ASCLPlayerCharacter>(Mesh->GetOwner()) : nullptr;
	USCLAbilitySystemComponent* ASC = Player ? Player->GetSCLAbilitySystemComponent() : nullptr;
	// 配置蓝图替换动画后，通知仍转给实际激活的处决子类；技能内部再核对 Montage 实例。
	FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecByBaseClass(USCLExecutionAbility::StaticClass()) : nullptr;
	if (Spec && Spec->IsActive())
		if (USCLExecutionAbility* Ability = Cast<USCLExecutionAbility>(Spec->GetPrimaryInstance()))
			Ability->HandleImpactNotify(Reference);
}
