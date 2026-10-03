#include "Debug/SCLProfilingSubsystem.h"

#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Debug/SCLCombatDebugSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "SoulCombatLab.h"
#include "TimerManager.h"
#include "UI/SCLHUD.h"

bool USCLProfilingSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	return false;
#else
	return WorldType == EWorldType::Game;
#endif
}

void USCLProfilingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!FParse::Value(FCommandLine::Get(), TEXT("SCLProfile="), Scenario))
	{
		return;
	}
	if (Scenario != TEXT("Idle") && Scenario != TEXT("Boss") && Scenario != TEXT("BossDebug"))
	{
		UE_LOG(LogSoulCombatLab, Error, TEXT("Unknown SCLProfile scenario: %s"), *Scenario);
		FinishMeasurement();
		return;
	}
	InWorld.GetTimerManager().SetTimer(StageTimer, this,
		&USCLProfilingSubsystem::PrepareScenario, 2.0F, false);
}

void USCLProfilingSubsystem::PrepareScenario()
{
	APlayerController* const Controller = GetWorld()->GetFirstPlayerController();
	ASCLPlayerCharacter* const Player = Controller != nullptr
		? Cast<ASCLPlayerCharacter>(Controller->GetPawn()) : nullptr;
	if (Player == nullptr)
	{
		UE_LOG(LogSoulCombatLab, Error, TEXT("SCLProfile could not find the player after startup."));
		FinishMeasurement();
		return;
	}
	if (Scenario != TEXT("Idle"))
	{
		// Reuse the real gameplay setup command after Pawn creation, not startup ExecCmds.
		Controller->ConsoleCommand(TEXT("DebugPrepareBossCombatTest true"), false);
		if (USCLAbilitySystemComponent* const ASC = Player->GetSCLAbilitySystemComponent())
		{
			constexpr float ProfilingHealth{100000.0F};
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetMaxHealthAttribute(), ProfilingHealth);
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetHealthAttribute(), ProfilingHealth);
		}
	}
	if (Scenario == TEXT("BossDebug"))
	{
		GetWorld()->GetSubsystem<USCLCombatDebugSubsystem>()->SetEnabled(true);
		if (ASCLHUD* const HUD = Cast<ASCLHUD>(Controller->GetHUD()))
		{
			HUD->ToggleDeveloperDebugHUD();
		}
	}
	UE_LOG(LogSoulCombatLab, Log, TEXT("SCLProfile prepared: Scenario=%s Warmup=3s Sample=20s"), *Scenario);
	GetWorld()->GetTimerManager().SetTimer(StageTimer, this,
		&USCLProfilingSubsystem::BeginMeasurement, 3.0F, false);
}

void USCLProfilingSubsystem::BeginMeasurement()
{
	bMeasuring = true;
	TRACE_BEGIN_REGION(TEXT("SCL.Measure"));
	TRACE_BOOKMARK(TEXT("SCL profile start: %s"), *Scenario);
	UE_LOG(LogSoulCombatLab, Log, TEXT("SCLProfile measurement began: %s"), *Scenario);
	GetWorld()->GetTimerManager().SetTimer(StageTimer, this,
		&USCLProfilingSubsystem::FinishMeasurement, 20.0F, false);
}

void USCLProfilingSubsystem::FinishMeasurement()
{
	const bool bCompleted = bMeasuring;
	Cleanup();
	UE_LOG(LogSoulCombatLab, Log, TEXT("SCLProfile finished: Scenario=%s Completed=%s"),
		*Scenario, bCompleted ? TEXT("true") : TEXT("false"));
	// Only terminate this explicitly requested unattended profiling instance.
	if (FParse::Param(FCommandLine::Get(), TEXT("SCLProfileAutoExit")))
	{
		FPlatformMisc::RequestExitWithStatus(false, bCompleted ? 0 : 1);
	}
}

void USCLProfilingSubsystem::Cleanup()
{
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(StageTimer);
	}
	if (bMeasuring)
	{
		TRACE_END_REGION(TEXT("SCL.Measure"));
		bMeasuring = false;
	}
}

void USCLProfilingSubsystem::OnWorldEndPlay(UWorld& InWorld)
{
	Cleanup();
	Super::OnWorldEndPlay(InWorld);
}

void USCLProfilingSubsystem::Deinitialize()
{
	Cleanup();
	Super::Deinitialize();
}
