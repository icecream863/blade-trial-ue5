#if WITH_DEV_AUTOMATION_TESTS
#include "Demo/SCLDemoSubsystem.h"
#include "Demo/SCLDemoPlayerController.h"
#include "Demo/SCLDemoMap.h"
#include "AIController.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Characters/SCLBossCharacter.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "UnrealClient.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSCLDemoPolicyTest, "SoulCombatLab.Demo.StageOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSCLDemoPolicyTest::RunTest(const FString& Parameters)
{
	const ESCLDemoStage Route[]{ESCLDemoStage::Spawn, ESCLDemoStage::Training, ESCLDemoStage::Sword,
		ESCLDemoStage::Heavy, ESCLDemoStage::MiniArena, ESCLDemoStage::BossGate, ESCLDemoStage::Boss};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Route) - 1; ++Index)
	{
		TestEqual(TEXT("Spatial order"), USCLDemoSubsystem::NextStage(Route[Index]), Route[Index + 1]);
		TestTrue(TEXT("Distinct entry points"), ASCLDemoMap::Checkpoint(Route[Index + 1]).X > ASCLDemoMap::Checkpoint(Route[Index]).X + 500.0F);
	}
	return true;
}

// Moves the actual CharacterMovement pawn along every connector, never teleporting across doors.
// Health changes are deliberate test stimuli, not evidence of a human combat playthrough.
class FSCLDemoRuntimeCommand final : public IAutomationLatentCommand
{
public:
	explicit FSCLDemoRuntimeCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 160.0) { Test->AddError(FString::Printf(TEXT("Spatial flow timed out step=%d"), Step)); return true; }
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Game) World = Context.World();
		if (World == nullptr) return false;
		USCLDemoSubsystem* const Demo = World->GetSubsystem<USCLDemoSubsystem>();
		if (Demo == nullptr || !Demo->IsActive()) return false;
		if (!Map.IsValid()) for (TActorIterator<ASCLDemoMap> It(World); It; ++It) { Map = *It; break; }
		if (!Map.IsValid()) { Test->AddError(TEXT("Native map missing")); return true; }
		if (Now < NextAction) return false;
		const auto Kill = [](ASCLCharacterBase* Character)
		{
			if (Character != nullptr) Character->GetSCLAbilitySystemComponent()->SetNumericAttributeBase(USCLAttributeSet::GetHealthAttribute(), 0.0F);
		};
		if (Step == 0)
		{
			if (!bScreenshotRequested && FParse::Param(FCommandLine::Get(), TEXT("SCLDemoScreenshots")))
			{
				bScreenshotRequested = true;
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/SixRegionTitle.png"), true, false);
				NextAction = Now + 1.0; return false;
			}
			Test->TestEqual(TEXT("Starts at title"), Demo->GetState(), ESCLDemoState::Title);
			Demo->PrimaryAction(); CheckActors(*Demo, *World, 0, false);
			if (FParse::Param(FCommandLine::Get(), TEXT("SCLDemoScreenshots")))
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/SixRegionSpawn.png"), true, false);
			Demo->TogglePause(); Test->TestTrue(TEXT("Pause freezes world"), UGameplayStatics::IsGamePaused(World));
			Demo->PrimaryAction(); Test->TestFalse(TEXT("Resume unpauses"), UGameplayStatics::IsGamePaused(World));
			Demo->PrimaryAction(); Test->TestEqual(TEXT("Enter does not teleport out of spawn"), Demo->GetStage(), ESCLDemoStage::Spawn);
			TestGate(*World, ESCLDemoStage::Training, true);
			TestGate(*World, ESCLDemoStage::MiniArena, true);
			TargetStage = ESCLDemoStage::Training; Step = 1; WalkStarted = Now; return false;
		}
		if (Step == 1)
		{
			if (Demo->GetStage() != TargetStage)
			{
				if (!Demo->GetPlayer() || !Demo->IsExploring()) { Test->AddError(TEXT("Player cannot walk to region")); return true; }
				if (Now - WalkStarted > 18.0) { Test->AddError(FString::Printf(TEXT("Walk stalled before region %d at %s"), static_cast<int32>(TargetStage), *Demo->GetPlayer()->GetActorLocation().ToString())); return true; }
			Demo->GetPlayer()->AddMovementInput(FVector::ForwardVector, 1.0F);
				return false;
			}
			Demo->GetPlayer()->GetCharacterMovement()->StopMovementImmediately();
			Demo->GetPlayer()->ConsumeMovementInputVector();
			Test->AddInfo(FString::Printf(TEXT("Walked into region %d at %s"), static_cast<int32>(TargetStage), *Demo->GetPlayer()->GetActorLocation().ToString()));
			const int32 Expected = TargetStage == ESCLDemoStage::MiniArena ? 2 : TargetStage == ESCLDemoStage::BossGate ? 0 : 1;
			CheckActors(*Demo, *World, Expected, false);
			bRegionScreenshotTaken = false;
			Step = 2; NextAction = Now + 0.4; return false;
		}
		if (Step == 2)
		{
			if (!bRegionScreenshotTaken && FParse::Param(FCommandLine::Get(), TEXT("SCLDemoScreenshots")))
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/SixRegion%d.png"), static_cast<int32>(TargetStage)), true, false);
				bRegionScreenshotTaken = true;
				// Capture a rendered frame before subsequent scripted deaths/retries change the UI.
				NextAction = Now + 0.2;
				return false;
			}
			UNavigationSystemV1* const Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			if (Nav != nullptr)
			{
				const ANavigationData* const Data = Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
				if (Data != nullptr && Data->GetRuntimeGenerationMode() != ERuntimeGenerationType::Dynamic)
				{ Test->AddError(TEXT("Demo navigation must use dynamic generation")); return true; }
			}
			const FVector From = ASCLDemoMap::Checkpoint(TargetStage);
			const FVector To = From + FVector{TargetStage == ESCLDemoStage::BossGate ? 100.0F : 650.0F, 0.0F, 0.0F};
			UNavigationPath* const Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, From, To, Demo->GetPlayer());
			if (Nav == nullptr || Path == nullptr || !Path->IsValid() || Path->IsPartial())
			{
				if (Now - NavWaitStart < 12.0) { NextAction = Now + 0.2; return false; }
				Test->AddError(FString::Printf(TEXT("No complete runtime nav path in region %d"), static_cast<int32>(TargetStage))); return true;
			}
			Test->AddInfo(FString::Printf(TEXT("Complete nav path in region %d: %d points"), static_cast<int32>(TargetStage), Path->PathPoints.Num()));
			if (TargetStage == ESCLDemoStage::BossGate)
			{
				TestGate(*World, ESCLDemoStage::MiniArena, false);
				TestGate(*World, ESCLDemoStage::BossGate, false);
				TargetStage = ESCLDemoStage::Boss; Step = 1; WalkStarted = Now; return false;
			}
			if (TargetStage == ESCLDemoStage::Boss) TestGate(*World, ESCLDemoStage::BossGate, true);
			else TestGate(*World, TargetStage, true);
			if (TargetStage == ESCLDemoStage::Training || TargetStage == ESCLDemoStage::MiniArena || TargetStage == ESCLDemoStage::Boss)
			{
				if (TargetStage == ESCLDemoStage::Boss) Kill(Demo->GetOpponent());
				OldMapping = CastChecked<ASCLDemoPlayerController>(World->GetFirstPlayerController())->GetRuntimeCombatMappingContext();
				Kill(Demo->GetPlayer()); Step = 3; NextAction = Now + 0.2; return false;
			}
			Step = 4; return false;
		}
		if (Step == 3)
		{
			Test->TestEqual(TEXT("Death (including simultaneous boss death) defeats"), Demo->GetState(), ESCLDemoState::Defeat);
			Demo->PrimaryAction();
			Test->TestEqual(TEXT("Retry retains region"), Demo->GetStage(), TargetStage);
			Test->TestTrue(TEXT("Retry returns to regional entrance"), FVector::Dist2D(Demo->GetPlayer()->GetActorLocation(), ASCLDemoMap::Checkpoint(TargetStage)) < 5.0F);
			CheckActors(*Demo, *World, TargetStage == ESCLDemoStage::MiniArena ? 2 : 1, true);
			Step = 4; NextAction = Now + 0.2; return false;
		}
		if (Step == 4)
		{
			GateWaitStarted = Now; Kill(Demo->GetOpponent()); Step = TargetStage == ESCLDemoStage::MiniArena ? 5 : 6;
			NextAction = Now + 0.2; return false;
		}
		if (Step == 5)
		{
			Test->TestEqual(TEXT("One mini-arena kill is insufficient"), Demo->GetState(), ESCLDemoState::Playing);
			Test->TestEqual(TEXT("Second opponent remains"), Demo->GetLivingOpponentCount(), 1);
			TestGate(*World, ESCLDemoStage::MiniArena, true);
			GateWaitStarted = Now; Kill(Demo->GetOpponent()); Step = 6; NextAction = Now + 0.2; return false;
		}
		if (Step == 6)
		{
			if (TargetStage == ESCLDemoStage::Boss)
			{
				Test->TestEqual(TEXT("Boss death wins"), Demo->GetState(), ESCLDemoState::Victory);
				Demo->PrimaryAction(); CheckActors(*Demo, *World, 0, false);
				Test->TestEqual(TEXT("Replay starts at spawn"), Demo->GetStage(), ESCLDemoStage::Spawn);
				TestGate(*World, ESCLDemoStage::MiniArena, true);
				Demo->GetPlayer()->Destroy(); Step = 7; NextAction = Now + 0.2; return false;
			}
			Test->TestEqual(TEXT("Clear leaves world playable"), Demo->GetState(), ESCLDemoState::StageClear);
			Test->TestFalse(TEXT("Clear does not pause"), UGameplayStatics::IsGamePaused(World));
			Demo->PrimaryAction(); Test->TestEqual(TEXT("Enter cannot advance cleared region"), Demo->GetStage(), TargetStage);
			TestGate(*World, TargetStage, false);
			FVector GatePosition = Map->GetExitGate(TargetStage)->GetComponentLocation(); GatePosition.Z = 100.0F;
			UNavigationPath* const GatePath = UNavigationSystemV1::FindPathToLocationSynchronously(World,
				GatePosition - FVector{220.0F, 0.0F, 0.0F}, GatePosition + FVector{220.0F, 0.0F, 0.0F}, Demo->GetPlayer());
			if (GatePath == nullptr || !GatePath->IsValid() || GatePath->IsPartial())
			{
				if (Now - GateWaitStarted < 5.0) { NextAction = Now + 0.2; return false; }
				Test->AddError(TEXT("Opened gate did not become navigable")); return true;
			}
			Test->AddInfo(TEXT("Opened connector has a complete navigation path"));
			if (FParse::Param(FCommandLine::Get(), TEXT("SCLDemoScreenshots")))
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/SixRegionClear%d.png"), static_cast<int32>(TargetStage)), true, false);
			TargetStage = USCLDemoSubsystem::NextStage(TargetStage); Step = 1; NavWaitStart = Now; WalkStarted = Now;
			return false;
		}
		if (Step == 7)
		{
			Test->TestEqual(TEXT("Destroyed player recovers"), Demo->GetState(), ESCLDemoState::Defeat);
			Demo->PrimaryAction(); CheckActors(*Demo, *World, 0, false);
			TargetStage = ESCLDemoStage::Training; Step = 8; return false;
		}
		if (Step == 8)
		{
			if (Demo->GetStage() != TargetStage) { Demo->GetPlayer()->AddMovementInput(FVector::ForwardVector); return false; }
			Demo->GetPlayer()->GetCharacterMovement()->StopMovementImmediately();
			Demo->PrimaryAction();
			Test->TestEqual(TEXT("Skip only clears tutorial"), Demo->GetState(), ESCLDemoState::StageClear);
			Test->TestEqual(TEXT("Skip keeps tutorial location"), Demo->GetStage(), ESCLDemoStage::Training);
			Demo->GetPlayer()->Destroy(); Step = 9; NextAction = Now + 0.2; return false;
		}
		Test->TestEqual(TEXT("Death during traversal is handled"), Demo->GetState(), ESCLDemoState::Defeat);
		Demo->PrimaryAction(); CheckActors(*Demo, *World, 1, false);
		Test->TestEqual(TEXT("Traversal retry replays region"), Demo->GetStage(), ESCLDemoStage::Training);
		Demo->RestartRun(); CheckActors(*Demo, *World, 0, false);
		Test->AddInfo(TEXT("Six-region physical walk, nav, gates, multi-enemy clear, retry and restart complete"));
		return true;
	}
private:
	void CheckActors(USCLDemoSubsystem& Demo, UWorld& World, const int32 Enemies, const bool bCheckOldMapping)
	{
		if (!Test->TestNotNull(TEXT("Player exists"), Demo.GetPlayer())) return;
		Test->TestEqual(TEXT("Player restored on entry"), Demo.GetPlayer()->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()), 300.0F);
		Test->TestEqual(TEXT("Encounter population"), Demo.GetLivingOpponentCount(), Enemies);
		int32 Count = 0, AICount = 0, MapCount = 0;
		for (TActorIterator<ASCLCharacterBase> It(&World); It; ++It) if (!It->IsActorBeingDestroyed()) ++Count;
		for (TActorIterator<AAIController> It(&World); It; ++It) if (!It->IsActorBeingDestroyed()) ++AICount;
		for (TActorIterator<ASCLDemoMap> It(&World); It; ++It) ++MapCount;
		Test->TestEqual(TEXT("No old characters"), Count, Enemies + 1);
		Test->TestEqual(TEXT("No orphan AI controllers"), AICount, Demo.GetStage() == ESCLDemoStage::Training ? 0 : Enemies);
		Test->TestEqual(TEXT("One persistent map through retries"), MapCount, 1);
		Test->TestTrue(TEXT("New player possessed"), World.GetFirstPlayerController()->GetPawn() == Demo.GetPlayer());
		AGameModeBase* const GameMode = World.GetAuthGameMode();
		if (Test->TestNotNull(TEXT("Game mode exists"), GameMode))
		{
			// 资产存在还不够：初始进入、区域重试和重开时都必须使用配置蓝图的 Controller。
			Test->TestEqual(TEXT("Runtime uses the Controller blueprint"),
				World.GetFirstPlayerController()->GetClass()->GetPathName(),
				FString(TEXT("/Game/SoulCombatLab/Blueprints/Player/BP_SCLPlayerController.BP_SCLPlayerController_C")));
			Test->TestTrue(TEXT("Controller matches GameMode configuration"),
				World.GetFirstPlayerController()->GetClass() == GameMode->PlayerControllerClass.Get());
			Test->TestTrue(TEXT("Respawn uses the configured player class"),
				Demo.GetPlayer()->GetClass() == GameMode->GetDefaultPawnClassForController(World.GetFirstPlayerController()));
		}
		if (auto* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(World.GetFirstPlayerController()->GetLocalPlayer()))
		{
			Test->TestTrue(TEXT("Mappings installed"), Input->HasMappingContext(CastChecked<ASCLDemoPlayerController>(World.GetFirstPlayerController())->GetRuntimeCombatMappingContext()));
			if (bCheckOldMapping && OldMapping.IsValid()) Test->TestTrue(TEXT("Controller mapping survives Pawn replacement without recreation"), OldMapping.Get() == CastChecked<ASCLDemoPlayerController>(World.GetFirstPlayerController())->GetRuntimeCombatMappingContext());
		}
	}
	void TestGate(UWorld& World, const ESCLDemoStage Owner, const bool bExpectedBlocked)
	{
		const UStaticMeshComponent* const Gate = Map->GetExitGate(Owner);
		if (!Test->TestNotNull(TEXT("Gate exists"), Gate)) return;
		FVector Position = Gate->GetComponentLocation(); Position.Z = 100.0F;
		FHitResult Hit;
		FCollisionQueryParams Params{SCENE_QUERY_STAT(SCLGateGeometry)};
		for (TActorIterator<ASCLCharacterBase> It(&World); It; ++It) Params.AddIgnoredActor(*It);
		const bool bBlocked = World.SweepSingleByChannel(Hit, Position - FVector{180.0F, 0.0F, 0.0F},
			Position + FVector{180.0F, 0.0F, 0.0F}, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(42.0F, 90.0F), Params);
		Test->TestEqual(TEXT("Gate capsule collision matches progression"), bBlocked, bExpectedBlocked);
	}
	FAutomationTestBase* Test;
	TWeakObjectPtr<ASCLDemoMap> Map;
	TWeakObjectPtr<UInputMappingContext> OldMapping;
	ESCLDemoStage TargetStage{ESCLDemoStage::Training};
	int32 Step{0};
	bool bScreenshotRequested{false};
	bool bRegionScreenshotTaken{false};
	double Started{FPlatformTime::Seconds()}, NextAction{Started + 1.0}, NavWaitStart{Started}, WalkStarted{Started}, GateWaitStarted{Started};
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSCLDemoRuntimeTest, "SoulCombatLabDemo.RuntimeFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FSCLDemoRuntimeTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FSCLDemoRuntimeCommand(this));
	return true;
}
#endif



