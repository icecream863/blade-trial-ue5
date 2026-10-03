#include "Animation/AnimNotifies/AN_SCLSheathWeapon.h"
#include "Characters/Player/SCLWeaponPresentationComponent.h"
#include "Components/SkeletalMeshComponent.h"
void UAN_SCLSheathWeapon::Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference)
{
	Super::Notify(Mesh, Animation, Reference);
	if (Mesh && Mesh->GetOwner())
		if (auto* Presentation = Mesh->GetOwner()->FindComponentByClass<USCLWeaponPresentationComponent>()) Presentation->CommitSheath(Reference);
}
