#include "Animation/AnimNotifies/AN_SCLDrawWeapon.h"
#include "Characters/Player/SCLWeaponPresentationComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAN_SCLDrawWeapon::Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference)
{
	Super::Notify(Mesh, Animation, Reference);
	if (Mesh && Mesh->GetOwner())
		if (auto* Presentation = Mesh->GetOwner()->FindComponentByClass<USCLWeaponPresentationComponent>())
			Presentation->CommitDraw(Reference);
}
