#include "Demo/SCLDemoSubsystem.h"

#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Characters/SCLBossCharacter.h"
#include "Characters/SCLEnemyCharacter.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Characters/SCLTrainingDummy.h"
#include "Demo/SCLDemoWidget.h"
#include "Demo/SCLDemoMap.h"
#include "Targeting/SCLTargetingComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "SoulCombatLab.h"
#include "TimerManager.h"

bool USCLDemoSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USCLDemoSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!ASCLDemoMap::ShouldUseDemo(&InWorld)) return;
	StartupTimer = InWorld.GetTimerManager().SetTimerForNextTick(this, &USCLDemoSubsystem::InitializeDemo);
}

void USCLDemoSubsystem::InitializeDemo()
{
	APlayerController* const Controller = GetWorld()->GetFirstPlayerController();
	ASCLPlayerCharacter* const ExistingPlayer = Controller != nullptr
		? Cast<ASCLPlayerCharacter>(Controller->GetPawn()) : nullptr;
	if (ExistingPlayer == nullptr || bShuttingDown)
	{
		Fail(TEXT("Player was not created by the game mode."));
		return;
	}
	Player = ExistingPlayer;
	for (TActorIterator<ASCLDemoMap> It(GetWorld()); It; ++It) { Map = *It; break; }
	if (!Map.IsValid()) { Fail(TEXT("Six-region map is missing.")); return; }
	ExistingPlayer->SetActorLocation(ASCLDemoMap::Checkpoint(ESCLDemoStage::Spawn));
	ExistingPlayer->SetActorRotation(FRotator::ZeroRotator);
	Controller->SetControlRotation(FRotator::ZeroRotator);
	// The saved laboratory dummy is replaced by an encounter-owned instance.
	for (TActorIterator<ASCLTrainingDummy> It(GetWorld()); It; ++It)
	{
		AController* const DummyController = It->GetController();
		It->Destroy();
		if (DummyController != nullptr) DummyController->Destroy();
	}
	Widget = CreateWidget<USCLDemoWidget>(Controller, USCLDemoWidget::StaticClass());
	if (Widget != nullptr)
	{
		Widget->AddToViewport(30);
	}
	GetWorld()->GetTimerManager().SetTimer(RegionTimer, this, &USCLDemoSubsystem::CheckRegion, 0.1F, true);
	State = ESCLDemoState::Title;
	ApplyPresentation();
	UE_LOG(LogSoulCombatLab, Log, TEXT("Playable Demo ready: Enter=Start Esc=Pause R=Retry"));
}

ESCLDemoStage USCLDemoSubsystem::NextStage(const ESCLDemoStage Current)
{
	switch (Current)
	{
	case ESCLDemoStage::Spawn: return ESCLDemoStage::Training;
	case ESCLDemoStage::Training: return ESCLDemoStage::Sword;
	case ESCLDemoStage::Sword: return ESCLDemoStage::Heavy;
	case ESCLDemoStage::Heavy: return ESCLDemoStage::MiniArena;
	case ESCLDemoStage::MiniArena: return ESCLDemoStage::BossGate;
	default: return ESCLDemoStage::Boss;
	}
}

void USCLDemoSubsystem::PrimaryAction()
{
	if (!IsActive() || bShuttingDown) return;
	if (bPaused) { TogglePause(); return; }
	switch (State)
	{
	case ESCLDemoState::Title: StartStage(ESCLDemoStage::Spawn); break;
	case ESCLDemoState::Defeat: StartStage(Stage); break;
	case ESCLDemoState::Victory: StartStage(ESCLDemoStage::Spawn); break;
	case ESCLDemoState::StageClear: break;
	case ESCLDemoState::Playing:
		if (Stage == ESCLDemoStage::Training)
		{ ClearOpponents(); State = ESCLDemoState::StageClear; Map->UpdateGates(Stage, true); ApplyPresentation(); }
		break;
	default: break;
	}
}

void USCLDemoSubsystem::RestartRun()
{
	if (IsActive() && !bShuttingDown) StartStage(ESCLDemoStage::Spawn);
}

void USCLDemoSubsystem::StartStage(const ESCLDemoStage NewStage, const bool bRespawnPlayer)
{
	// bRespawnPlayer=false 是正常步行转区：只换敌人；失败重试才销毁并重建玩家 Pawn。
	APlayerController* const Controller = GetWorld()->GetFirstPlayerController();
	if (Controller == nullptr || bShuttingDown || !Map.IsValid()) return;
	if (bRespawnPlayer) ClearEncounter();
	else ClearOpponents();
	GetWorld()->GetTimerManager().ClearTimer(OutcomeTimer);
	bPaused = false;
	UGameplayStatics::SetGamePaused(GetWorld(), false);
	Stage = NewStage;
	State = ESCLDemoState::Playing;
	Map->UpdateGates(Stage, false);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (bRespawnPlayer)
	{
		// 重试时沿用 GameMode 配置的玩家蓝图类，才能保留网格、动画蓝图等资产设置。
		AGameModeBase* const GameMode = GetWorld()->GetAuthGameMode();
		UClass* const PlayerClass = GameMode != nullptr
			? GameMode->GetDefaultPawnClassForController(Controller) : nullptr;
		if (PlayerClass == nullptr || !PlayerClass->IsChildOf(ASCLPlayerCharacter::StaticClass()))
		{
			Fail(TEXT("Game mode has no SCL player class for respawn."));
			return;
		}
		Player = GetWorld()->SpawnActor<ASCLPlayerCharacter>(PlayerClass,
			ASCLDemoMap::Checkpoint(Stage), FRotator::ZeroRotator, Params);
		if (!Player.IsValid()) { Fail(TEXT("Cannot spawn player.")); return; }
		Player->OnDestroyed.AddDynamic(this, &USCLDemoSubsystem::HandlePlayerDestroyed);
		Controller->Possess(Player.Get());
		Controller->SetControlRotation(FRotator::ZeroRotator);
		PlayerDeadHandle = Player->GetSCLAbilitySystemComponent()->RegisterGameplayTagEvent(SCLGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved).AddUObject(this, &USCLDemoSubsystem::HandleDeathTag);
	}
	if (!Player.IsValid()) { Fail(TEXT("Player lost during region transition.")); return; }
	USCLAbilitySystemComponent* const ASC = Player->GetSCLAbilitySystemComponent();
	ASC->SetNumericAttributeBase(USCLAttributeSet::GetMaxHealthAttribute(), 300.0F);
	ASC->SetNumericAttributeBase(USCLAttributeSet::GetHealthAttribute(), 300.0F);
	ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
	TArray<TSubclassOf<ASCLCharacterBase>> Classes;
	switch (Stage)
	{
	case ESCLDemoStage::Training: Classes.Add(ASCLTrainingDummy::StaticClass()); break;
	case ESCLDemoStage::Sword: Classes.Add(ASCLSwordEnemyCharacter::StaticClass()); break;
	case ESCLDemoStage::Heavy: Classes.Add(ASCLHeavyEnemyCharacter::StaticClass()); break;
	case ESCLDemoStage::MiniArena:
		Classes.Add(ASCLSwordEnemyCharacter::StaticClass()); Classes.Add(ASCLHeavyEnemyCharacter::StaticClass()); break;
	case ESCLDemoStage::Boss: Classes.Add(ASCLBossCharacter::StaticClass()); break;
	default: break;
	}
	for (int32 Index = 0; Index < Classes.Num(); ++Index)
	{
		const FVector Offset{650.0F, Classes.Num() > 1 ? (Index == 0 ? -250.0F : 250.0F) : 0.0F, 0.0F};
		ASCLCharacterBase* const Enemy = GetWorld()->SpawnActor<ASCLCharacterBase>(Classes[Index],
			ASCLDemoMap::Checkpoint(Stage) + Offset, FRotator{0.0F, 180.0F, 0.0F}, Params);
		if (Enemy == nullptr) { Fail(TEXT("Cannot spawn region encounter.")); return; }
		FOpponentObserver Observer;
		Observer.Character = Enemy;
		Observer.Controller = Enemy->GetController();
		Observer.DeadHandle = Enemy->GetSCLAbilitySystemComponent()->RegisterGameplayTagEvent(SCLGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved).AddUObject(this, &USCLDemoSubsystem::HandleDeathTag);
		Enemy->OnDestroyed.AddDynamic(this, &USCLDemoSubsystem::HandleOpponentDestroyed);
		Opponents.Add(Observer);
	}
	ApplyPresentation();
	UE_LOG(LogSoulCombatLab, Log, TEXT("Demo region entered: Stage=%d Enemies=%d Respawn=%d Player=%s"),
		static_cast<int32>(Stage), Opponents.Num(), bRespawnPlayer, *Player->GetActorLocation().ToString());
}

void USCLDemoSubsystem::CheckRegion()
{
	if (!IsExploring() || bPaused || !Player.IsValid() || bShuttingDown) return;
	const FVector Position = Player->GetActorLocation();
	if (Position.Z < -200.0F)
	{
		Player->GetSCLAbilitySystemComponent()->SetNumericAttributeBase(USCLAttributeSet::GetHealthAttribute(), 0.0F);
		return;
	}
	if (Player->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Dead)) return;
	const bool bCanAdvance = State == ESCLDemoState::StageClear || Stage == ESCLDemoStage::Spawn || Stage == ESCLDemoStage::BossGate;
	// 正常过区域只换当前遭遇，不重新生成 Pawn。
	if (bCanAdvance && Stage != ESCLDemoStage::Boss && ASCLDemoMap::IsInsideEntry(NextStage(Stage), Position))
		StartStage(NextStage(Stage), false);
}

int32 USCLDemoSubsystem::GetLivingOpponentCount() const
{
	int32 Count = 0;
	for (const FOpponentObserver& Observer : Opponents)
		if (Observer.Character.IsValid() && !Observer.Character->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Dead)) ++Count;
	return Count;
}

ASCLCharacterBase* USCLDemoSubsystem::GetOpponent() const
{
	// Multi-enemy HUD follows lock-on; otherwise display the nearest living opponent.
	if (Player.IsValid())
		if (const USCLTargetingComponent* const Targeting = Player->FindComponentByClass<USCLTargetingComponent>())
			if (ASCLCharacterBase* const Target = Targeting->GetCurrentTarget()) return Target;
	ASCLCharacterBase* Nearest = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (const FOpponentObserver& Observer : Opponents)
	{
		ASCLCharacterBase* const Enemy = Observer.Character.Get();
		if (Enemy == nullptr || Enemy->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Dead)) continue;
		const double Distance = Player.IsValid() ? FVector::DistSquared(Player->GetActorLocation(), Enemy->GetActorLocation()) : 0.0;
		if (Nearest == nullptr || Distance < BestDistance) { Nearest = Enemy; BestDistance = Distance; }
	}
	return Nearest;
}

void USCLDemoSubsystem::HandleDeathTag(const FGameplayTag Tag, const int32 Count)
{
	if (Tag == SCLGameplayTags::State_Dead && Count > 0 && IsExploring() && !bShuttingDown)
	{
		// GAS death callbacks can be reentrant inside damage/trace; resolve after the stack unwinds.
		GetWorld()->GetTimerManager().ClearTimer(OutcomeTimer);
		OutcomeTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &USCLDemoSubsystem::ResolveOutcome);
	}
}

void USCLDemoSubsystem::ResolveOutcome()
{
	if (!IsExploring() || bShuttingDown) return;
	// Give death feedback a brief lifetime even if the player stays in this region.
	for (const FOpponentObserver& Observer : Opponents)
		if (Observer.Character.IsValid() && Observer.Character->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Dead))
		{
			Observer.Character->SetActorEnableCollision(false);
			if (Observer.Character->GetLifeSpan() == 0.0F) Observer.Character->SetLifeSpan(2.0F);
		}
	if (!Player.IsValid() || Player->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Dead))
	{
		State = ESCLDemoState::Defeat;
	}
	else if (Opponents.Num() > 0 && GetLivingOpponentCount() == 0)
	{
		State = Stage == ESCLDemoStage::Boss ? ESCLDemoState::Victory : ESCLDemoState::StageClear;
		if (Map.IsValid()) Map->UpdateGates(Stage, true);
	}
	// Clearing one or both enemies does not change the active gameplay input mode.
	if (IsExploring()) { if (Widget != nullptr) Widget->Refresh(); }
	else ApplyPresentation();
	UE_LOG(LogSoulCombatLab, Log, TEXT("Demo outcome: State=%d Stage=%d"), static_cast<int32>(State), static_cast<int32>(Stage));
}

void USCLDemoSubsystem::HandlePlayerDestroyed(AActor* DestroyedActor)
{
	if (IsExploring() && !bShuttingDown)
	{
		Player.Reset();
		OutcomeTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &USCLDemoSubsystem::ResolveOutcome);
	}
}

void USCLDemoSubsystem::TogglePause()
{
	if (!IsExploring() || bShuttingDown) return;
	bPaused = !bPaused;
	if (bPaused && Player.IsValid()) Player->GetSCLAbilitySystemComponent()->CancelAllAbilities();
	ApplyPresentation();
}

void USCLDemoSubsystem::ApplyPresentation()
{
	APlayerController* const Controller = GetWorld()->GetFirstPlayerController();
	if (Controller == nullptr) return;
	const bool bMenu = !IsExploring() || bPaused;
	UGameplayStatics::SetGamePaused(GetWorld(), bMenu);
	Controller->bShowMouseCursor = bMenu;
	Controller->ResetIgnoreMoveInput();
	Controller->ResetIgnoreLookInput();
	Controller->SetIgnoreMoveInput(bMenu);
	Controller->SetIgnoreLookInput(bMenu);
	if (Player.IsValid())
	{
		if (bMenu) { Player->DisableInput(Controller); Player->GetCharacterMovement()->StopMovementImmediately(); }
		else Player->EnableInput(Controller);
	}
	if (bMenu)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Controller->SetInputMode(Mode);
	}
	else Controller->SetInputMode(FInputModeGameOnly{});
	if (Widget != nullptr) Widget->Refresh();
}

void USCLDemoSubsystem::ClearEncounter()
{
	GetWorld()->GetTimerManager().ClearTimer(OutcomeTimer);
	if (Player.IsValid())
	{
		Player->OnDestroyed.RemoveDynamic(this, &USCLDemoSubsystem::HandlePlayerDestroyed);
		Player->GetSCLAbilitySystemComponent()->RegisterGameplayTagEvent(SCLGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved).Remove(PlayerDeadHandle);
		Player->Destroy();
	}
	ClearOpponents();
	Player.Reset(); PlayerDeadHandle.Reset();
}

void USCLDemoSubsystem::ClearOpponents()
{
	for (const FOpponentObserver& Observer : Opponents)
	{
		if (ASCLCharacterBase* const Enemy = Observer.Character.Get())
		{
			Enemy->OnDestroyed.RemoveDynamic(this, &USCLDemoSubsystem::HandleOpponentDestroyed);
			Enemy->GetSCLAbilitySystemComponent()->RegisterGameplayTagEvent(SCLGameplayTags::State_Dead,
				EGameplayTagEventType::NewOrRemoved).Remove(Observer.DeadHandle);
			Enemy->Destroy();
		}
		if (Observer.Controller.IsValid()) Observer.Controller->Destroy();
	}
	Opponents.Reset();
}

void USCLDemoSubsystem::HandleOpponentDestroyed(AActor* DestroyedActor)
{
	for (const FOpponentObserver& Observer : Opponents)
		if (Observer.Character == DestroyedActor && Observer.Controller.IsValid())
			Observer.Controller->Destroy();
	if (IsExploring() && !bShuttingDown)
		OutcomeTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &USCLDemoSubsystem::ResolveOutcome);
}

void USCLDemoSubsystem::Fail(const TCHAR* const Reason)
{
	State = ESCLDemoState::Error;
	UE_LOG(LogSoulCombatLab, Error, TEXT("Demo setup failed: %s"), Reason);
	ApplyPresentation();
}

void USCLDemoSubsystem::Quit()
{
	if (IsActive()) UKismetSystemLibrary::QuitGame(GetWorld(), GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
}

FText USCLDemoSubsystem::GetObjective() const
{
	if (State == ESCLDemoState::StageClear)
		return FText::FromString(Stage == ESCLDemoStage::MiniArena ? TEXT("清场完成：金色 Boss 门已解锁，沿路前进") : TEXT("区域完成：通道已开启，沿金线步行前进"));
	switch (Stage)
	{
	case ESCLDemoStage::Spawn: return FText::FromString(TEXT("1 / 6  出生区：沿金线走入训练区"));
	case ESCLDemoStage::Training: return FText::FromString(TEXT("2 / 6  训练区：击败假人，或 Enter 跳过练习"));
	case ESCLDemoStage::Sword: return FText::FromString(TEXT("3A / 6  剑兵庭院：观察前摇，尝试弹反"));
	case ESCLDemoStage::Heavy: return FText::FromString(TEXT("3B / 6  重兵庭院：用翻滚躲避重击"));
	case ESCLDemoStage::MiniArena: return FText::FromString(FString::Printf(TEXT("4 / 6  小竞技场：击败两名敌人（剩余 %d）"), GetLivingOpponentCount()));
	case ESCLDemoStage::BossGate: return FText::FromString(TEXT("5 / 6  Boss 门厅：继续前进，进入最终战"));
	default: return FText::FromString(TEXT("6 / 6  Boss 竞技场：半血后进入第二阶段"));
	}
}

FText USCLDemoSubsystem::GetPrimaryLabel() const
{
	if (bPaused) return FText::FromString(TEXT("继续战斗"));
	switch (State)
	{
	case ESCLDemoState::Title: return FText::FromString(TEXT("开始试炼"));
	case ESCLDemoState::Defeat: return FText::FromString(TEXT("重试当前关卡"));
	case ESCLDemoState::StageClear: return FText::FromString(TEXT("沿通道前进"));
	case ESCLDemoState::Victory: return FText::FromString(TEXT("再玩一次"));
	default: return FText::FromString(TEXT("继续"));
	}
}

void USCLDemoSubsystem::Shutdown()
{
	if (bShuttingDown) return;
	bShuttingDown = true;
	GetWorld()->GetTimerManager().ClearTimer(StartupTimer);
	GetWorld()->GetTimerManager().ClearTimer(OutcomeTimer);
	// World teardown owns actor destruction; only release observers here.
	if (Player.IsValid()) Player->GetSCLAbilitySystemComponent()->RegisterGameplayTagEvent(
		SCLGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved).Remove(PlayerDeadHandle);
	GetWorld()->GetTimerManager().ClearTimer(RegionTimer);
	for (const FOpponentObserver& Observer : Opponents)
	{
		if (!Observer.Character.IsValid()) continue;
		Observer.Character->OnDestroyed.RemoveDynamic(this, &USCLDemoSubsystem::HandleOpponentDestroyed);
		Observer.Character->GetSCLAbilitySystemComponent()->RegisterGameplayTagEvent(SCLGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved).Remove(Observer.DeadHandle);
	}
	if (Widget != nullptr) { Widget->RemoveFromParent(); Widget = nullptr; }
	if (Player.IsValid()) Player->OnDestroyed.RemoveDynamic(this, &USCLDemoSubsystem::HandlePlayerDestroyed);
	State = ESCLDemoState::Inactive;
}

void USCLDemoSubsystem::OnWorldEndPlay(UWorld& InWorld) { Shutdown(); Super::OnWorldEndPlay(InWorld); }
void USCLDemoSubsystem::Deinitialize() { Shutdown(); Super::Deinitialize(); }

