#if WITH_DEV_AUTOMATION_TESTS

#include "Debug/SCLCombatDebugSubsystem.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSCLCombatDebugLifecycleTest,
	"SoulCombatLab.Debug.CombatWorldLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSCLCombatDebugLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* const World = UWorld::CreateWorld(EWorldType::Game, false);
	UWorld* const OtherWorld = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT
	{
		if (OtherWorld != nullptr)
		{
			OtherWorld->DestroyWorld(false);
		}
		if (World != nullptr)
		{
			World->DestroyWorld(false);
		}
	};
	if (!TestNotNull(TEXT("Test world"), World) || !TestNotNull(TEXT("Second world"), OtherWorld))
	{
		return false;
	}
	USCLCombatDebugSubsystem* const Debug = World->GetSubsystem<USCLCombatDebugSubsystem>();
	USCLCombatDebugSubsystem* const OtherDebug = OtherWorld->GetSubsystem<USCLCombatDebugSubsystem>();
	if (!TestNotNull(TEXT("Debug subsystem"), Debug) || !TestNotNull(TEXT("Second subsystem"), OtherDebug))
	{
		return false;
	}
	TestFalse(TEXT("Debug disabled at startup"), Debug->IsEnabled());
	TestFalse(TEXT("No timer while disabled"), Debug->IsRefreshTimerActive());
	TestFalse(TEXT("Null world is safe"), USCLCombatDebugSubsystem::IsEnabledForWorld(nullptr));
	Debug->SetEnabled(true);
	Debug->SetEnabled(true);
	TestTrue(TEXT("Enabled in its own world"), USCLCombatDebugSubsystem::IsEnabledForWorld(World));
	TestTrue(TEXT("Enabled has a refresh timer"), Debug->IsRefreshTimerActive());
	TestFalse(TEXT("Another world is unaffected"), OtherDebug->IsEnabled());
	TestFalse(TEXT("Another world has no debug timer"), OtherDebug->IsRefreshTimerActive());
	Debug->SetEnabled(false);
	TestFalse(TEXT("Disabling clears timer"), Debug->IsRefreshTimerActive());
	TestFalse(TEXT("Trace gate closes immediately"), USCLCombatDebugSubsystem::IsEnabledForWorld(World));
	Debug->SetEnabled(true);
	Debug->OnWorldEndPlay(*World);
	TestFalse(TEXT("EndPlay disables drawing"), Debug->IsEnabled());
	TestFalse(TEXT("EndPlay clears timer"), Debug->IsRefreshTimerActive());
	Debug->SetEnabled(true);
	TestFalse(TEXT("Cannot restart drawing during teardown"), Debug->IsEnabled());
	return true;
}

#endif
