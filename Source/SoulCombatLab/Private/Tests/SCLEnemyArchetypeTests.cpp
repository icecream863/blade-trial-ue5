#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/SCLEnemyCharacter.h"
#include "Combat/SCLCombatComponent.h"
#include "Data/SCLEnemyArchetypeData.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSCLEnemyArchetypeContractTest,
	"SoulCombatLab.AI.EnemyArchetypeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSCLEnemyArchetypeContractTest::RunTest(const FString& Parameters)
{
	const ASCLSwordEnemyCharacter* const SwordEnemy = GetDefault<ASCLSwordEnemyCharacter>();
	const ASCLHeavyEnemyCharacter* const HeavyEnemy = GetDefault<ASCLHeavyEnemyCharacter>();
	TestNotNull(TEXT("Sword Enemy CDO exists"), SwordEnemy);
	TestNotNull(TEXT("Heavy Enemy CDO exists"), HeavyEnemy);
	if (SwordEnemy == nullptr || HeavyEnemy == nullptr)
	{
		return false;
	}

	const USCLEnemyArchetypeData* const Sword = SwordEnemy->GetArchetypeData();
	const USCLEnemyArchetypeData* const Heavy = HeavyEnemy->GetArchetypeData();
	TestTrue(TEXT("Sword Enemy uses Sword archetype data"), Sword != nullptr && Sword->IsA<USCLSwordEnemyArchetypeData>());
	TestTrue(TEXT("Heavy Enemy uses Heavy archetype data"), Heavy != nullptr && Heavy->IsA<USCLHeavyEnemyArchetypeData>());
	if (Sword == nullptr || Heavy == nullptr)
	{
		return false;
	}

	TestEqual(TEXT("Sword movement speed"), Sword->GetMoveSpeed(), 500.0F);
	TestEqual(TEXT("Sword attack burst"), Sword->GetAttacksPerBurst(), 3);
	TestEqual(TEXT("Sword recovery"), Sword->GetAttackRecoveryDuration(), 0.55F);
	TestTrue(TEXT("Sword attacks are parryable"), Sword->AreAttacksParryable());

	TestEqual(TEXT("Heavy movement speed"), Heavy->GetMoveSpeed(), 260.0F);
	TestEqual(TEXT("Heavy max health"), Heavy->GetMaxHealth(), 160.0F);
	TestEqual(TEXT("Heavy max poise"), Heavy->GetMaxPoise(), 50.0F);
	TestEqual(TEXT("Heavy damage scale"), Heavy->GetDamageScale(), 1.75F);
	TestEqual(TEXT("Heavy attack burst"), Heavy->GetAttacksPerBurst(), 1);
	TestEqual(TEXT("Heavy recovery"), Heavy->GetAttackRecoveryDuration(), 1.35F);
	TestFalse(TEXT("Heavy attack is not parryable"), Heavy->AreAttacksParryable());
	TestTrue(
		TEXT("Heavy weapon presentation is larger"),
		Heavy->GetWeaponVisualScale() > Sword->GetWeaponVisualScale());
	TestEqual(TEXT("Sword material slot count"), Sword->GetBodyMaterials().Num(), 2);
	TestEqual(TEXT("Heavy material slot count"), Heavy->GetBodyMaterials().Num(), 2);
	if (Sword->GetBodyMaterials().Num() == 2 && Heavy->GetBodyMaterials().Num() == 2)
	{
		TestNotNull(TEXT("Sword primary material is configured"), Sword->GetBodyMaterials()[0].Get());
		TestNotNull(TEXT("Heavy primary material is configured"), Heavy->GetBodyMaterials()[0].Get());
		TestNotEqual(
			TEXT("Heavy and Sword primary materials differ"),
			Heavy->GetBodyMaterials()[0].Get(),
			Sword->GetBodyMaterials()[0].Get());
	}
	TestTrue(
		TEXT("Heavy is slower than Sword"),
		Heavy->GetMoveSpeed() < Sword->GetMoveSpeed());
	TestTrue(
		TEXT("Heavy has more poise than Sword"),
		Heavy->GetMaxPoise() > Sword->GetMaxPoise());

	USCLCombatComponent* const Combat = NewObject<USCLCombatComponent>(GetTransientPackage());
	Combat->ConfigureEnemyAttackProfile(
		Heavy->GetDamageScale(),
		Heavy->AreAttacksParryable(),
		Heavy->GetWeaponVisualScale());
	TestEqual(TEXT("Combat profile receives Heavy damage scale"), Combat->GetEnemyDamageScale(), 1.75F);
	TestEqual(TEXT("Combat profile receives Heavy weapon scale"), Combat->GetEnemyWeaponVisualScale(), 1.40F);
	TestFalse(TEXT("Combat profile receives Heavy parry rule"), Combat->IsActiveAttackParryable());
	return true;
}

#endif
