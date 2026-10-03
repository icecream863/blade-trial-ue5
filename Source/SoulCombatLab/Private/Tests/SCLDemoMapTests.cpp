#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Demo/SCLDemoMap.h"
#include "Demo/SCLDemoSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

class FSCLLegacyLabCommand final : public IAutomationLatentCommand
{
public:
	explicit FSCLLegacyLabCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds() - Started < 2.0) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType != EWorldType::Game) continue;
			UWorld* const World = Context.World();
			Test->TestFalse(TEXT("SCLLab bypasses demo"), World->GetSubsystem<USCLDemoSubsystem>()->IsActive());
			int32 Maps = 0, Floors = 0, Boundaries = 0;
			for (TActorIterator<ASCLDemoMap> It(World); It; ++It) ++Maps;
			for (TActorIterator<AStaticMeshActor> It(World); It; ++It) ++Floors;
			for (TActorIterator<AActor> It(World); It; ++It) if (It->ActorHasTag(TEXT("SCLArenaBoundary"))) ++Boundaries;
			Test->TestEqual(TEXT("No expanded map in lab"), Maps, 0);
			Test->TestTrue(TEXT("Saved lab geometry retained"), Floors > 0);
			Test->TestEqual(TEXT("Legacy boundary actor retained"), Boundaries, 1);
			UNavigationPath* const Path = UNavigationSystemV1::FindPathToLocationSynchronously(World,
				FVector{0.0F, 0.0F, 100.0F}, FVector{500.0F, 0.0F, 100.0F});
			Test->TestTrue(TEXT("Original laboratory navigation works"), Path != nullptr && Path->IsValid() && !Path->IsPartial());
			return true;
		}
		Test->AddError(TEXT("Legacy lab Game world missing")); return true;
	}
private:
	FAutomationTestBase* Test;
	double Started{FPlatformTime::Seconds()};
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSCLLegacyLabTest, "SoulCombatLabMap.LegacyLab",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FSCLLegacyLabTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FSCLLegacyLabCommand(this));
	return true;
}
#endif
