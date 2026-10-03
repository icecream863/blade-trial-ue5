#include "Data/SCLEnemyArchetypeData.h"

#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

USCLSwordEnemyArchetypeData::USCLSwordEnemyArchetypeData()
{
	ArchetypeName = TEXT("SwordEnemy");
	MoveSpeed = 500.0F;
	MaxHealth = 100.0F;
	MaxPoise = 30.0F;
	DamageScale = 1.0F;
	AttackRecoveryDuration = 0.55F;
	AttacksPerBurst = 3;
	bAttacksParryable = true;
	WeaponVisualScale = 1.0F;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PrimaryMaterial(
		TEXT("/Game/Characters/Mannequins/Materials/Quinn/MI_Quinn_01.MI_Quinn_01"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SecondaryMaterial(
		TEXT("/Game/Characters/Mannequins/Materials/Quinn/MI_Quinn_02.MI_Quinn_02"));
	if (PrimaryMaterial.Succeeded() && SecondaryMaterial.Succeeded())
	{
		BodyMaterials = {PrimaryMaterial.Object, SecondaryMaterial.Object};
	}
}

USCLHeavyEnemyArchetypeData::USCLHeavyEnemyArchetypeData()
{
	ArchetypeName = TEXT("HeavyEnemy");
	MoveSpeed = 260.0F;
	MaxHealth = 160.0F;
	MaxPoise = 50.0F;
	DamageScale = 1.75F;
	AttackRecoveryDuration = 1.35F;
	AttacksPerBurst = 1;
	bAttacksParryable = false;
	WeaponVisualScale = 1.40F;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PrimaryMaterial(
		TEXT("/Game/Characters/Mannequins/Materials/Manny/MI_Manny_01_New.MI_Manny_01_New"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SecondaryMaterial(
		TEXT("/Game/Characters/Mannequins/Materials/Manny/MI_Manny_02_New.MI_Manny_02_New"));
	if (PrimaryMaterial.Succeeded() && SecondaryMaterial.Succeeded())
	{
		BodyMaterials = {PrimaryMaterial.Object, SecondaryMaterial.Object};
	}
}

USCLBossArchetypeData::USCLBossArchetypeData()
{
	ArchetypeName = TEXT("BossPrototype");
	MoveSpeed = 330.0F;
	MaxHealth = 400.0F;
	MaxPoise = 60.0F;
	DamageScale = 0.90F;
	AttackRecoveryDuration = 1.20F;
	AttacksPerBurst = 2;
	bAttacksParryable = true;
	WeaponVisualScale = 1.65F;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PrimaryMaterial(
		TEXT("/Game/Characters/Mannequins/Materials/Manny/MI_Manny_01_New.MI_Manny_01_New"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SecondaryMaterial(
		TEXT("/Game/Characters/Mannequins/Materials/Quinn/MI_Quinn_02.MI_Quinn_02"));
	if (PrimaryMaterial.Succeeded() && SecondaryMaterial.Succeeded())
	{
		BodyMaterials = {PrimaryMaterial.Object, SecondaryMaterial.Object};
	}
}
