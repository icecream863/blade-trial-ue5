#include "UI/SCLHUD.h"

#include "Blueprint/UserWidget.h"
#include "Characters/SCLBossCharacter.h"
#include "Characters/SCLEnemyCharacter.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SoulCombatLab.h"
#include "UI/SCLBossWidget.h"
#include "UI/SCLDeveloperDebugWidget.h"

void ASCLHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* const PlayerController = GetOwningPlayerController();
	UWorld* const World = GetWorld();
	if (PlayerController == nullptr || !PlayerController->IsLocalController() || World == nullptr)
	{
		return;
	}

	BossWidget = CreateWidget<USCLBossWidget>(
		PlayerController,
		USCLBossWidget::StaticClass(),
		TEXT("SCLBossWidget"));
	DeveloperDebugWidget = CreateWidget<USCLDeveloperDebugWidget>(
		PlayerController,
		USCLDeveloperDebugWidget::StaticClass(),
		TEXT("SCLDeveloperDebugWidget"));
	if (BossWidget == nullptr || DeveloperDebugWidget == nullptr)
	{
		UE_LOG(
			LogSoulCombatLab,
			Error,
			TEXT("Native HUD widget creation failed: HUD=%s BossWidget=%s DebugWidget=%s"),
			*GetNameSafe(this),
			BossWidget != nullptr ? TEXT("true") : TEXT("false"),
			DeveloperDebugWidget != nullptr ? TEXT("true") : TEXT("false"));
		return;
	}

	BossWidget->AddToViewport(10);
	DeveloperDebugWidget->AddToViewport(100);
	if (ASCLPlayerCharacter* const Player = Cast<ASCLPlayerCharacter>(PlayerController->GetPawn()))
	{
		DeveloperDebugWidget->BindPlayer(*Player);
	}
	ActorSpawnedDelegateHandle = World->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &ASCLHUD::HandleActorSpawned));
	for (TActorIterator<ASCLEnemyCharacter> EnemyIterator(World); EnemyIterator; ++EnemyIterator)
	{
		ASCLEnemyCharacter& Enemy = **EnemyIterator;
		DeveloperDebugWidget->RegisterEnemy(Enemy);
		if (ASCLBossCharacter* const Boss = Cast<ASCLBossCharacter>(&Enemy))
		{
			BossWidget->BindBoss(*Boss);
		}
	}

	UE_LOG(LogSoulCombatLab, Log, TEXT("Boss UI initialized: HUD=%s"), *GetNameSafe(this));
}

void ASCLHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld(); World != nullptr && ActorSpawnedDelegateHandle.IsValid())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedDelegateHandle);
	}
	ActorSpawnedDelegateHandle.Reset();
	if (BossWidget != nullptr)
	{
		BossWidget->UnbindBoss();
		BossWidget->RemoveFromParent();
		BossWidget = nullptr;
	}
	if (DeveloperDebugWidget != nullptr)
	{
		DeveloperDebugWidget->RemoveFromParent();
		DeveloperDebugWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ASCLHUD::HandleActorSpawned(AActor* const SpawnedActor)
{
	ASCLBossCharacter* const Boss = Cast<ASCLBossCharacter>(SpawnedActor);
	if (Boss != nullptr && BossWidget != nullptr)
	{
		BossWidget->BindBoss(*Boss);
	}
	if (ASCLEnemyCharacter* const Enemy = Cast<ASCLEnemyCharacter>(SpawnedActor);
		Enemy != nullptr && DeveloperDebugWidget != nullptr)
	{
		DeveloperDebugWidget->RegisterEnemy(*Enemy);
	}
}

void ASCLHUD::ToggleDeveloperDebugHUD()
{
	if (DeveloperDebugWidget == nullptr)
	{
		return;
	}
	if (APlayerController* const PlayerController = GetOwningPlayerController())
	{
		if (ASCLPlayerCharacter* const Player = Cast<ASCLPlayerCharacter>(PlayerController->GetPawn()))
		{
			DeveloperDebugWidget->BindPlayer(*Player);
		}
	}
	DeveloperDebugWidget->ToggleDebugDisplay();
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Developer Debug HUD toggled: HUD=%s Visible=%s Refresh=%.2f"),
		*GetNameSafe(this),
		DeveloperDebugWidget->IsDebugDisplayVisible() ? TEXT("true") : TEXT("false"),
		DeveloperDebugWidget->GetRefreshInterval());
}
