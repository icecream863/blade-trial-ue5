#if WITH_DEV_AUTOMATION_TESTS

#include "Demo/SCLDemoPlayerController.h"
#include "Debug/SCLDebugHUDModel.h"
#include "Misc/AutomationTest.h"
#include "UI/SCLDeveloperDebugWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSCLDeveloperDebugHUDContractTest,
	"SoulCombatLab.Debug.HUDContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSCLDeveloperDebugHUDContractTest::RunTest(const FString& Parameters)
{
	const ASCLDemoPlayerController* const ControllerDefaults = GetDefault<ASCLDemoPlayerController>();
	TestNotNull(TEXT("Controller CDO exists"), ControllerDefaults);
	if (ControllerDefaults != nullptr)
	{
		TestNotNull(
			TEXT("Controller owns a native Developer Debug HUD input action"),
			ControllerDefaults->GetDeveloperDebugHUDAction());
	}

	const USCLDeveloperDebugWidget* const WidgetDefaults =
		GetDefault<USCLDeveloperDebugWidget>();
	TestNotNull(TEXT("Developer Debug Widget CDO exists"), WidgetDefaults);
	if (WidgetDefaults != nullptr)
	{
		TestEqual(
			TEXT("Developer Debug HUD uses a bounded four-Hz refresh"),
			WidgetDefaults->GetRefreshInterval(),
			0.25F);
	}

	FSCLDebugHUDSnapshot Snapshot;
	Snapshot.FramesPerSecond = 60.0F;
	Snapshot.PlayerStateTags = TEXT("State.Attacking, State.Blocking");
	Snapshot.PlayerHealth = 75.0F;
	Snapshot.PlayerMaxHealth = 100.0F;
	Snapshot.PlayerStamina = 40.0F;
	Snapshot.PlayerMaxStamina = 100.0F;
	Snapshot.CurrentAbility = TEXT("SCLLightAttackAbility");
	Snapshot.LockedTarget = TEXT("SCLBossCharacter_0");
	Snapshot.EnemyName = TEXT("SCLBossCharacter_0");
	Snapshot.EnemyState = TEXT("Combat");
	Snapshot.EnemyDistance = 185.0F;
	Snapshot.bHasPerceivedTarget = true;
	Snapshot.bHasLineOfSight = true;
	Snapshot.bBehaviorTreeRunning = true;
	Snapshot.bCanAttack = true;
	Snapshot.CurrentAttack = TEXT("Combo");
	Snapshot.PathFollowingStatus = TEXT("Moving");
	Snapshot.bHasEQSPoint = true;
	Snapshot.EQSPoint = FVector{100.0F, 200.0F, 0.0F};
	Snapshot.DistanceToEQSPoint = 85.0F;
	Snapshot.BossPhase = TEXT("PhaseTwo");
	const FString Text = SCLDebugHUDFormatter::BuildText(Snapshot);

	const TCHAR* const RequiredFragments[] = {
		TEXT("FPS: 60.0"),
		TEXT("State.Attacking"),
		TEXT("Health: 75 / 100"),
		TEXT("Stamina: 40 / 100"),
		TEXT("SCLLightAttackAbility"),
		TEXT("Locked Target: SCLBossCharacter_0"),
		TEXT("Perception: Target=true  LOS=true"),
		TEXT("Behavior Tree: Running"),
		TEXT("Blackboard State: Combat"),
		TEXT("Distance: 185.0 cm  CanAttack=true"),
		TEXT("Current Attack: Combo"),
		TEXT("Reposition: Moving"),
		TEXT("EQS Point: (100, 200, 0)  Delta=85 cm"),
		TEXT("Boss Phase: PhaseTwo")};
	for (const TCHAR* const Fragment : RequiredFragments)
	{
		TestTrue(
			*FString::Printf(TEXT("Debug text includes: %s"), Fragment),
			Text.Contains(Fragment));
	}
	return true;
}

#endif
