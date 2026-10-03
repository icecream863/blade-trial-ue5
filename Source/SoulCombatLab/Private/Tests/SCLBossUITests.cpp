#if WITH_DEV_AUTOMATION_TESTS

#include "Core/SCLGameMode.h"
#include "Misc/AutomationTest.h"
#include "UI/SCLBossUIModel.h"
#include "UI/SCLHUD.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSCLBossUIContractTest,
	"SoulCombatLab.Boss.UIContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSCLBossUIContractTest::RunTest(const FString& Parameters)
{
	const ASCLGameMode* const GameModeDefaults = GetDefault<ASCLGameMode>();
	TestNotNull(TEXT("GameMode CDO exists"), GameModeDefaults);
	if (GameModeDefaults != nullptr)
	{
		TestEqual(
			TEXT("GameMode uses the native SCL HUD"),
			GameModeDefaults->HUDClass.Get(),
			ASCLHUD::StaticClass());
	}

	const FSCLBossUIState FullHealth = SCLBossUIPolicy::ResolveState(
		1000.0F,
		1000.0F,
		ESCLBossPhase::PhaseOne);
	TestEqual(TEXT("Full boss Health maps to a full bar"), FullHealth.HealthFraction, 1.0F);
	TestEqual(TEXT("Phase One label"), FullHealth.PhaseLabel, FName{TEXT("PHASE I")});

	const FSCLBossUIState PhaseTwo = SCLBossUIPolicy::ResolveState(
		500.0F,
		1000.0F,
		ESCLBossPhase::PhaseTwo);
	TestEqual(TEXT("Half boss Health maps to half a bar"), PhaseTwo.HealthFraction, 0.5F);
	TestEqual(TEXT("Phase Two label"), PhaseTwo.PhaseLabel, FName{TEXT("PHASE II")});
	TestTrue(
		TEXT("Phase Two uses a warmer red accent than Phase One"),
		PhaseTwo.PhaseColor.R > FullHealth.PhaseColor.R ||
			PhaseTwo.PhaseColor.G < FullHealth.PhaseColor.G);

	TestEqual(
		TEXT("Health below zero clamps to an empty bar"),
		SCLBossUIPolicy::ResolveState(-50.0F, 1000.0F, ESCLBossPhase::PhaseOne)
			.HealthFraction,
		0.0F);
	TestEqual(
		TEXT("Invalid maximum Health maps safely to an empty bar"),
		SCLBossUIPolicy::ResolveState(100.0F, 0.0F, ESCLBossPhase::PhaseOne)
			.HealthFraction,
		0.0F);
	TestTrue(
		TEXT("Entering Phase Two shows transition feedback"),
		SCLBossUIPolicy::ShouldShowPhaseTransition(
			ESCLBossPhase::PhaseOne,
			ESCLBossPhase::PhaseTwo));
	TestFalse(
		TEXT("An unchanged phase does not replay transition feedback"),
		SCLBossUIPolicy::ShouldShowPhaseTransition(
			ESCLBossPhase::PhaseTwo,
			ESCLBossPhase::PhaseTwo));
	return true;
}

#endif
