#include "Characters/SCLTrainingDummy.h"

#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ASCLTrainingDummy::ASCLTrainingDummy()
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> DummyMesh(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	if (DummyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(DummyMesh.Object);
		GetMesh()->SetRelativeLocation(FVector{0.0F, 0.0F, -90.0F});
		GetMesh()->SetRelativeRotation(FRotator{0.0F, -90.0F, 0.0F});
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> DummyAnimation(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (DummyAnimation.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(DummyAnimation.Class);
	}
}

