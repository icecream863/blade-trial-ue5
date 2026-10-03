#include "Animation/AnimNotifyStates/ANS_SCLParryWindow.h"
#include "AbilitySystem/Abilities/SCLParryAbility.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
USCLParryAbility* ActiveParry(const USkeletalMeshComponent* Mesh)
{
	const ASCLPlayerCharacter* Player = Mesh ? Cast<ASCLPlayerCharacter>(Mesh->GetOwner()) : nullptr;
	USCLAbilitySystemComponent* ASC = Player ? Player->GetSCLAbilitySystemComponent() : nullptr;
	// 查正在使用的派生技能实例；Montage 实例身份仍由技能内部核对，旧通知不能重新开窗。
	FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecByBaseClass(USCLParryAbility::StaticClass()) : nullptr;
	return Spec && Spec->IsActive() ? Cast<USCLParryAbility>(Spec->GetPrimaryInstance()) : nullptr;
}
}

void UANS_SCLParryWindow::NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	const float TotalDuration, const FAnimNotifyEventReference& Reference)
{
	Super::NotifyBegin(Mesh, Animation, TotalDuration, Reference);
	if (USCLParryAbility* Ability = ActiveParry(Mesh)) Ability->HandleParryWindowNotify(true, Reference);
}

void UANS_SCLParryWindow::NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& Reference)
{
	Super::NotifyEnd(Mesh, Animation, Reference);
	if (USCLParryAbility* Ability = ActiveParry(Mesh)) Ability->HandleParryWindowNotify(false, Reference);
}
