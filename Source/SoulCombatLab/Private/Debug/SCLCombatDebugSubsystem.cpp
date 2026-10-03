#include "Debug/SCLCombatDebugSubsystem.h"

#include "AI/SCLAIController.h"
#include "AI/SCLAIState.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/SCLEnemyCharacter.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "SoulCombatLab.h"
#include "Targeting/SCLTargetingComponent.h"
#include "TimerManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

bool USCLCombatDebugSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool USCLCombatDebugSubsystem::IsEnabledForWorld(const UWorld* const World)
{
	const USCLCombatDebugSubsystem* const Debug = World != nullptr
		? World->GetSubsystem<USCLCombatDebugSubsystem>() : nullptr;
	return Debug != nullptr && Debug->IsEnabled();
}

bool USCLCombatDebugSubsystem::IsRefreshTimerActive() const
{
	return GetWorld() != nullptr && GetWorld()->GetTimerManager().IsTimerActive(RefreshTimer);
}

void USCLCombatDebugSubsystem::SetEnabled(const bool bInEnabled)
{
	UWorld* const World = GetWorld();
	if (World == nullptr || bInEnabled == bEnabled || (bInEnabled && bEndingPlay))
	{
		return;
	}
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	if (bInEnabled)
	{
		return;
	}
#endif
	bEnabled = bInEnabled;
	if (bEnabled)
	{
		ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(this, &USCLCombatDebugSubsystem::HandleActorSpawned));
		for (TActorIterator<ASCLEnemyCharacter> It(World); It; ++It)
		{
			Enemies.Add(*It);
		}
		World->GetTimerManager().SetTimer(
			RefreshTimer, this, &USCLCombatDebugSubsystem::RefreshDraw, RefreshInterval, true);
		RefreshDraw();
	}
	else
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
		if (ActorSpawnedHandle.IsValid())
		{
			World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
			ActorSpawnedHandle.Reset();
		}
		Enemies.Reset();
	}
	UE_LOG(LogSoulCombatLab, Log,
		TEXT("Combat Debug Draw: Enabled=%s Refresh=%.2f MaxTrail=%.2f World=%s"),
		bEnabled ? TEXT("true") : TEXT("false"), RefreshInterval, EventLifetime, *World->GetName());
}

void USCLCombatDebugSubsystem::OnWorldEndPlay(UWorld& InWorld)
{
	SetEnabled(false);
	bEndingPlay = true;
	Super::OnWorldEndPlay(InWorld);
}

void USCLCombatDebugSubsystem::Deinitialize()
{
	SetEnabled(false);
	bEndingPlay = true;
	Super::Deinitialize();
}

void USCLCombatDebugSubsystem::HandleActorSpawned(AActor* const Actor)
{
	if (ASCLEnemyCharacter* const Enemy = Cast<ASCLEnemyCharacter>(Actor))
	{
		Enemies.AddUnique(Enemy);
	}
}

void USCLCombatDebugSubsystem::RefreshDraw()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SCL_DebugDraw);
	if (!bEnabled)
	{
		return;
	}
	const APlayerController* const Controller = GetWorld()->GetFirstPlayerController();
	const ASCLPlayerCharacter* const Player = Controller != nullptr
		? Cast<ASCLPlayerCharacter>(Controller->GetPawn()) : nullptr;
	if (Player == nullptr)
	{
		return;
	}
	if (const USCLTargetingComponent* const Targeting = Player->GetTargetingComponent())
	{
		Targeting->DrawDebugState(GeometryLifetime);
	}
	Enemies.RemoveAllSwap([](const TWeakObjectPtr<ASCLEnemyCharacter>& Enemy)
	{
		return !Enemy.IsValid();
	});
	constexpr float MaximumDrawDistance{5000.0F};
	for (const TWeakObjectPtr<ASCLEnemyCharacter>& WeakEnemy : Enemies)
	{
		const ASCLEnemyCharacter* const Enemy = WeakEnemy.Get();
		if (Enemy != nullptr && FVector::DistSquared(Player->GetActorLocation(), Enemy->GetActorLocation())
			<= FMath::Square(MaximumDrawDistance))
		{
			DrawEnemy(*Enemy);
		}
	}
}

void USCLCombatDebugSubsystem::DrawEnemy(const ASCLEnemyCharacter& Enemy) const
{
	const USCLAbilitySystemComponent* const ASC = Enemy.GetSCLAbilitySystemComponent();
	if (ASC == nullptr || ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Dead))
	{
		return;
	}
	const ASCLAIController* const Controller = Cast<ASCLAIController>(Enemy.GetController());
	const UBlackboardComponent* const Blackboard = Controller != nullptr
		? Controller->GetBlackboardComponent() : nullptr;
	const FVector Origin = Enemy.GetActorLocation();
	const bool bCanAttack = Blackboard != nullptr && Blackboard->GetValueAsBool(SCLBlackboardKeys::CanAttack);
	const FColor RangeColor = bCanAttack ? FColor::Red : FColor::Orange;
	DrawDebugCircle(GetWorld(), Origin, Enemy.GetCombatEnterDistance(), 48, RangeColor,
		false, GeometryLifetime, 0, 2.0F, FVector::ForwardVector, FVector::RightVector, false);
	DrawDebugCircle(GetWorld(), Origin, Enemy.GetCombatExitDistance(), 48, FColor::Silver,
		false, GeometryLifetime, 0, 1.0F, FVector::ForwardVector, FVector::RightVector, false);
	DrawDebugString(GetWorld(), Origin + FVector{0.0F, 0.0F, 120.0F},
		FString::Printf(TEXT("AI enter/exit %.0f/%.0f cm | CanAttack=%s\nDecision range, not weapon reach"),
			Enemy.GetCombatEnterDistance(), Enemy.GetCombatExitDistance(), bCanAttack ? TEXT("true") : TEXT("false")),
		nullptr, RangeColor, GeometryLifetime, true);
	if (Blackboard != nullptr && Blackboard->IsVectorValueSet(SCLBlackboardKeys::IdealCombatLocation))
	{
		const FVector Point = Blackboard->GetValueAsVector(SCLBlackboardKeys::IdealCombatLocation);
		if (!Point.ContainsNaN())
		{
			DrawDebugSphere(GetWorld(), Point, 20.0F, 12, FColor::Magenta, false, GeometryLifetime);
			DrawDebugLine(GetWorld(), Origin, Point, FColor::Magenta, false, GeometryLifetime);
			DrawDebugString(GetWorld(), Point + FVector{0.0F, 0.0F, 40.0F}, TEXT("Last EQS result"),
				nullptr, FColor::Magenta, GeometryLifetime, true);
		}
	}
}
