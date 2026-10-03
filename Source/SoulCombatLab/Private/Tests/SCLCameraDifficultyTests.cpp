#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystem/Abilities/SCLExecutionAbility.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "AI/Boss/SCLBossUtilityPolicy.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Characters/SCLBossCharacter.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Combat/SCLCombatComponent.h"
#include "Components/WidgetComponent.h"
#include "Demo/SCLDemoMap.h"
#include "Demo/SCLDemoSubsystem.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Targeting/SCLTargetingComponent.h"
#include "UnrealClient.h"
#include "UI/SCLEnemyHealthWidget.h"

// Controlled rendered fixtures, not a player playthrough or recording route.
class FSCLCameraDifficultyCommand final : public IAutomationLatentCommand
{
public:
	explicit FSCLCameraDifficultyCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 55.0) { Test->AddError(TEXT("Camera difficulty test timed out")); return true; }
		if (Now < Next) return false;
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Game) World = Context.World();
		if (World == nullptr) return false;
		USCLDemoSubsystem* const Demo = World->GetSubsystem<USCLDemoSubsystem>();
		if (Demo == nullptr || !Demo->IsActive()) return false;
		Next = Now + 0.1;
		if (Step == 0)
		{
			Demo->RestartRun();
			EnterRegion(*Demo, ESCLDemoStage::Training);
			Demo->PrimaryAction();
			EnterRegion(*Demo, ESCLDemoStage::Sword);
			PrepareEncounter(*Demo);
			PlacePair(*Demo, 150.0F);
			Step = 50; Next = Now + 0.4; return false;
		}
		if (!Player.IsValid() || !Enemy.IsValid()) { Test->AddError(TEXT("Camera fixture lost an actor")); return true; }
		APlayerController* const Controller = Cast<APlayerController>(Player->GetController());
		USCLTargetingComponent* const Targeting = Player->GetTargetingComponent();
		if (Step == 50 || Step == 52)
		{
			CheckFreeRig();
			FVector View; FRotator Rotation; Controller->GetPlayerViewPoint(View, Rotation);
			Test->TestTrue(TEXT("Free camera keeps its original unraised center pivot"), FMath::Abs(View.Z - Player->GetActorLocation().Z) < 1.0);
			Test->TestTrue(TEXT("Free camera keeps its original centered 400cm framing"), FMath::Abs(View.Y - Player->GetActorLocation().Y) < 1.0 &&
				FMath::Abs(Player->GetActorLocation().X - View.X - 400.0) < 1.0);
			Capture(Step == 50 ? TEXT("FreeBeforeLock") : TEXT("FreeAfterUnlock"));
			Step = Step == 50 ? 1 : 53; Next = Now + 0.3; return false;
		}
		if (Step == 1 || Step == 5)
		{
			Targeting->ToggleLock();
			Test->TestTrue(TEXT("Rendered target acquisition"), Targeting->GetCurrentTarget() == Enemy.Get());
			++Step; Next = Now + 1.0; return false;
		}
		if (Step == 2 || Step == 6)
		{
			CheckFrame(Step == 2 ? TEXT("SwordNear") : TEXT("BossNear"));
			++Step; Next = Now + 0.3; return false;
		}
		if (Step == 3 || Step == 7)
		{
			PlacePair(*Demo, 500.0F);
			Step = Step == 3 ? 20 : 21; Next = Now + 1.0; return false;
		}
		if (Step == 20 || Step == 21)
		{
			CheckFrame(Step == 20 ? TEXT("SwordMedium") : TEXT("BossMedium"));
			Step = Step == 20 ? 4 : 8; Next = Now + 0.3; return false;
		}
		if (Step == 4)
		{
			CheckUnlock(*Controller);
			PlacePair(*Demo, 150.0F); Controller->SetControlRotation(FRotator::ZeroRotator);
			Step = 52; Next = Now + 0.4; return false;
		}
		if (Step == 53)
		{
			KillOpponents(*Demo);
			Step = 40; Next = Now + 0.3; return false;
		}
		if (Step == 40 || Step == 41)
		{
			EnterRegion(*Demo, Step == 40 ? ESCLDemoStage::Heavy : ESCLDemoStage::MiniArena);
			PrepareEncounter(*Demo);
			if (Step == 41)
			{
				Player->SetActorLocation(ASCLDemoMap::Checkpoint(ESCLDemoStage::MiniArena) + FVector{650.0F, 0.0F, 0.0F});
				for (TActorIterator<ASCLEnemyCharacter> It(World); It; ++It)
				{
					StopEnemy(**It);
					const bool bHeavy = It->IsA<ASCLHeavyEnemyCharacter>();
					if (bHeavy) Heavy = *It; else Sword = *It;
					It->SetActorLocation(Player->GetActorLocation() + FVector{500.0F, bHeavy ? 210.0F : -210.0F, 0.0F});
				}
				if (!Sword.IsValid() || !Heavy.IsValid()) { Test->AddError(TEXT("Two health-bar owners missing")); return true; }
				Step = 70; Next = Now + 0.6; return false;
			}
			KillOpponents(*Demo);
			++Step; Next = Now + 0.3; return false;
		}
		if (Step == 70)
		{
			Test->TestFalse(TEXT("Both enemy health bars work without lock-on"), Targeting->IsLockedOn());
			CheckHealthBar(*Sword, 1.0F); CheckHealthBar(*Heavy, 1.0F);
			Capture(TEXT("MiniArenaFullHealth"));
			Step = 71; Next = Now + 0.3; return false;
		}
		if (Step == 71)
		{
			UGameplayStatics::ApplyPointDamage(Sword.Get(), 25.0F, Player->GetActorForwardVector(), FHitResult{}, Controller, Player.Get(), UDamageType::StaticClass());
			UGameplayStatics::ApplyPointDamage(Heavy.Get(), 80.0F, Player->GetActorForwardVector(), FHitResult{}, Controller, Player.Get(), UDamageType::StaticClass());
			Step = 72; Next = Now + 0.3; return false;
		}
		if (Step == 72)
		{
			CheckHealthBar(*Sword, 0.75F); CheckHealthBar(*Heavy, 0.5F);
			Capture(TEXT("MiniArenaDamaged"));
			Step = 73; Next = Now + 0.3; return false;
		}
		if (Step == 73 || Step == 75)
		{
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape, IE_Pressed, 1.0F));
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape, IE_Released, 0.0F));
			++Step; Next = Now + 0.4; return false;
		}
		if (Step == 74)
		{
			Test->TestTrue(TEXT("Esc input opens the pause/instructions menu"), Demo->IsPaused() && UGameplayStatics::IsGamePaused(World));
			Capture(TEXT("EscPauseMenu"));
			Step = 75; Next = Now + 0.3; return false;
		}
		if (Step == 76)
		{
			Test->TestFalse(TEXT("Esc input resumes while paused"), Demo->IsPaused() || UGameplayStatics::IsGamePaused(World));
			Targeting->ToggleLock();
			Test->TestTrue(TEXT("Mini-arena target acquired"), Targeting->IsLockedOn());
			Step = 77; Next = Now + 0.5; return false;
		}
		if (Step == 77)
		{
			ASCLCharacterBase* const Before = Targeting->GetCurrentTarget();
			Targeting->SwitchTargetRight();
			if (Targeting->GetCurrentTarget() == Before) Targeting->SwitchTargetLeft();
			Test->TestTrue(TEXT("Switching targets retains a separate free-camera snapshot"), Targeting->GetCurrentTarget() != Before);
			CheckUnlock(*Controller);
			DeadHealthWidget = Cast<USCLEnemyHealthWidget>(Sword->FindComponentByClass<UWidgetComponent>()->GetUserWidgetObject());
			Sword->GetSCLAbilitySystemComponent()->SetNumericAttributeBase(USCLAttributeSet::GetHealthAttribute(), 0.0F);
			Test->TestTrue(TEXT("Dead enemy health bar hides immediately"), DeadHealthWidget.IsValid() && DeadHealthWidget->GetVisibility() == ESlateVisibility::Collapsed);
			Enemy = Heavy.Get();
			Controller->SetControlRotation(FRotator::ZeroRotator);
			Step = 78; Next = Now + 2.5; return false;
		}
		if (Step == 78)
		{
			Test->TestFalse(TEXT("Dead ordinary enemy is removed"), Sword.IsValid());
			Test->TestTrue(TEXT("Removed enemy health UI releases its observer"), !DeadHealthWidget.IsValid() || DeadHealthWidget->GetObservedEnemy() == nullptr);
			CheckHealthBar(*Heavy, 0.5F);
			Capture(TEXT("MiniArenaSurvivor"));
			Step = 79; Next = Now + 0.3; return false;
		}
		if (Step == 79)
		{
			KillOpponents(*Demo); Step = 42; Next = Now + 0.3; return false;
		}
		if (Step == 42)
		{
			EnterRegion(*Demo, ESCLDemoStage::BossGate);
			EnterRegion(*Demo, ESCLDemoStage::Boss);
			PrepareEncounter(*Demo);
			PlacePair(*Demo, 150.0F);
			ASCLBossCharacter* const Boss = Cast<ASCLBossCharacter>(Enemy.Get());
			if (Boss == nullptr) { Test->AddError(TEXT("Boss fixture missing")); return true; }
			Test->TestEqual(TEXT("Real Boss starts at 400 HP"), Boss->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()), 400.0F);
			Step = 5; Next = Now + 0.4; return false;
		}
		if (Step == 8)
		{
			// The closed entrance shortens the spring arm; pitch must not feed back from that displacement.
			PlacePair(*Demo, 150.0F, 0.0F);
			Step = 9; Next = Now + 1.0; return false;
		}
		if (Step == 9)
		{
			CheckFrame(TEXT("BossGateNear"));
			Step = 10; Next = Now + 0.3; return false;
		}
		if (Step == 10)
		{
			CheckUnlock(*Controller);
			PlacePair(*Demo, 140.0F);
			Step = 11; Next = Now + 0.4; return false;
		}
		if (Step == 11)
		{
			ASCLBossCharacter* const Boss = CastChecked<ASCLBossCharacter>(Enemy.Get());
			const FSCLBossAttackDecision Decision = Boss->SelectNextExecutableAttack(140.0F);
			Test->TestTrue(TEXT("Fresh Boss chooses light slash at melee range"), Decision.Attack == ESCLBossAttack::LightSlash);
			Test->TestTrue(TEXT("Real Boss prepares attack profile"), Boss->PrepareCurrentAttack());
			Test->TestTrue(TEXT("Prepared light attack leaves a generous counterattack opening"), Boss->GetCurrentAttackRecoveryDuration() >= 1.2F);
			HealthBefore = Player->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			Test->TestTrue(TEXT("Real Boss weapon attack starts"), Boss->GetCombatComponent_Implementation()->RequestAttack());
			Step = 12; Next = Now + 1.5; return false;
		}
		if (Step == 12)
		{
			const float Damage = HealthBefore - Player->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			Test->TestEqual(TEXT("Easy Boss light trace deals 18 damage"), Damage, 18.0F);
			ASCLBossCharacter* const Boss = CastChecked<ASCLBossCharacter>(Enemy.Get());
			UGameplayStatics::ApplyPointDamage(Boss, 199.0F, Player->GetActorForwardVector(), FHitResult{}, Controller, Player.Get(), UDamageType::StaticClass());
			Test->TestTrue(TEXT("201/400 HP remains phase one"), Boss->GetBossPhase() == ESCLBossPhase::PhaseOne);
			Boss->GetCombatComponent_Implementation()->CancelActiveAttack();
			Boss->GetSCLAbilitySystemComponent()->ApplyPoiseDamageToSelf(1000.0F, Player.Get());
			Test->TestTrue(TEXT("Boss poise break opens a real execution"),
				Boss->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Executable));
			Test->TestTrue(TEXT("Boss execution ability starts"),
				Player->GetSCLAbilitySystemComponent()->TryActivateAbilitiesByTag(FGameplayTagContainer{SCLGameplayTags::Ability_Execution}));
			// 处决伤害由 Montage Notify 触发，不能把旧的 0.7 秒延时当成命中时刻。
			// 等真实生命变化，同时保留有界等待：丢失 Notify 或技能中断都必须失败。
			ExecutionDeadline = World->GetTimeSeconds() + 5.0F;
			Step = 13; Next = Now + 0.1; return false;
		}
		if (Step == 13)
		{
			ASCLBossCharacter* const Boss = CastChecked<ASCLBossCharacter>(Enemy.Get());
			const float RemainingHealth = Boss->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			const FGameplayAbilitySpec* const Execution = Player->GetSCLAbilitySystemComponent()
				->FindAbilitySpecByBaseClass(USCLExecutionAbility::StaticClass());
			if (FMath::IsNearlyEqual(RemainingHealth, 201.0F, 0.05F) &&
				Execution != nullptr && Execution->IsActive() && World->GetTimeSeconds() < ExecutionDeadline)
			{
				return false;
			}
			Test->TestTrue(TEXT("Boss execution removes exactly one fifth of remaining health"),
				FMath::IsNearlyEqual(RemainingHealth, 160.8F, 0.05F));
			Test->TestTrue(TEXT("Boss execution crosses into phase two without killing"),
				Boss->GetBossPhase() == ESCLBossPhase::PhaseTwo &&
				!Boss->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Dead));
			for (const ESCLBossAttack Attack : {ESCLBossAttack::LightSlash, ESCLBossAttack::HeavySlash, ESCLBossAttack::DashSlash,
				ESCLBossAttack::Combo, ESCLBossAttack::AreaAttack, ESCLBossAttack::DelayedAttack})
			{
				const FSCLBossAttackExecutionProfile Profile = SCLBossUtilityPolicy::ResolveExecutionProfile(Attack, ESCLBossPhase::PhaseTwo);
				Test->TestTrue(FString::Printf(TEXT("%s retains at least 0.85s phase-two recovery"), SCLBossUtilityPolicy::GetAttackName(Attack)), Profile.RecoveryDuration >= 0.85F);
			}
			Test->AddInfo(FString::Printf(TEXT("Boss execution observed: HP=%.1f Phase=%d"),
				RemainingHealth, static_cast<int32>(Boss->GetBossPhase())));
			ExecutionDeadline = World->GetTimeSeconds() + 5.0F;
			Step = 14; Next = Now + 0.1; return false;
		}
		if (Step == 14)
		{
			const FGameplayAbilitySpec* const Execution = Player->GetSCLAbilitySystemComponent()
				->FindAbilitySpecByBaseClass(USCLExecutionAbility::StaticClass());
			if (Execution != nullptr && Execution->IsActive() && World->GetTimeSeconds() < ExecutionDeadline)
			{
				return false;
			}
			Test->TestTrue(TEXT("Boss execution animation completes normally"), Execution != nullptr && !Execution->IsActive());
			return true;
		}
		return false;
	}
private:
	void Capture(const TCHAR* Label)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("SCLCameraScreenshots")))
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/Camera%s.png"), Label), true, false);
	}
	void CheckHealthBar(ASCLEnemyCharacter& Character, const float ExpectedFraction)
	{
		UWidgetComponent* const Component = Character.FindComponentByClass<UWidgetComponent>();
		USCLEnemyHealthWidget* const Widget = Component != nullptr ? Cast<USCLEnemyHealthWidget>(Component->GetUserWidgetObject()) : nullptr;
		Test->TestNotNull(TEXT("Enemy owns a native overhead health widget"), Widget);
		if (Widget == nullptr) return;
		Test->TestTrue(TEXT("Health widget observes its own enemy"), Widget->GetObservedEnemy() == &Character);
		Test->TestEqual(TEXT("Visible bar matches actual independent damage"), Widget->GetDisplayedHealthFraction(), ExpectedFraction);
		Test->TestTrue(TEXT("Living enemy bar is visible"), Widget->GetVisibility() == ESlateVisibility::HitTestInvisible);
	}
	void CheckFreeRig()
	{
		const USpringArmComponent* const Boom = Player->FindComponentByClass<USpringArmComponent>();
		Test->TestEqual(TEXT("Unlocked camera restores original 400cm arm"), Boom->TargetArmLength, 400.0F);
		Test->TestTrue(TEXT("Unlocked camera has no height or shoulder modification"), Boom->TargetOffset.IsNearlyZero() && Boom->SocketOffset.IsNearlyZero());
	}
	static void StopEnemy(ASCLEnemyCharacter& Character)
	{
		if (AAIController* const AI = Cast<AAIController>(Character.GetController()))
		{
			AI->StopMovement();
			if (AI->BrainComponent != nullptr) AI->BrainComponent->StopLogic(TEXT("Camera visual fixture"));
		}
		Character.GetCombatComponent_Implementation()->CancelActiveAttack();
	}
	static void EnterRegion(USCLDemoSubsystem& Demo, const ESCLDemoStage Region)
	{
		Demo.GetPlayer()->SetActorLocation(ASCLDemoMap::Checkpoint(Region));
		Demo.CheckRegion();
	}
	static void KillOpponents(USCLDemoSubsystem& Demo)
	{
		while (Demo.GetLivingOpponentCount() > 0)
			Demo.GetOpponent()->GetSCLAbilitySystemComponent()->SetNumericAttributeBase(USCLAttributeSet::GetHealthAttribute(), 0.0F);
	}
	void PrepareEncounter(USCLDemoSubsystem& Demo)
	{
		Player = Demo.GetPlayer(); Enemy = Demo.GetOpponent();
		if (AAIController* const AI = Cast<AAIController>(Enemy->GetController()))
		{
			AI->StopMovement();
			if (AI->BrainComponent != nullptr) AI->BrainComponent->StopLogic(TEXT("Camera visual fixture"));
		}
		Enemy->GetCombatComponent_Implementation()->CancelActiveAttack();
		Player->GetController()->SetControlRotation(FRotator::ZeroRotator);
	}
	void PlacePair(USCLDemoSubsystem& Demo, const float Distance, const float EntryOffset = 700.0F)
	{
		Player->GetCharacterMovement()->StopMovementImmediately();
		Enemy->GetCharacterMovement()->StopMovementImmediately();
		Player->SetActorLocation(ASCLDemoMap::Checkpoint(Demo.GetStage()) + FVector{EntryOffset, 0.0F, 0.0F});
		Player->SetActorRotation(FRotator::ZeroRotator);
		Enemy->SetActorLocation(Player->GetActorLocation() + FVector{Distance, 0.0F, 0.0F});
		Enemy->SetActorRotation(FRotator{0.0F, 180.0F, 0.0F});
	}
	void CheckFrame(const TCHAR* Label)
	{
		APlayerController* const Controller = CastChecked<APlayerController>(Player->GetController());
		FVector View; FRotator Rotation;
		Controller->GetPlayerViewPoint(View, Rotation);
		const float Pitch = FRotator::NormalizeAxis(Rotation.Pitch);
		Test->TestTrue(TEXT("Lock stays acquired after camera settles"), Player->GetTargetingComponent()->GetCurrentTarget() == Enemy.Get());
		Test->TestTrue(TEXT("Locked camera maintains a mild downward view"), Pitch >= -25.5F && Pitch <= -15.5F);
		Test->TestTrue(TEXT("Camera is above the player's head"), View.Z > Player->GetActorLocation().Z + 95.0F);
		int32 Width = 0, Height = 0; Controller->GetViewportSize(Width, Height);
		FVector2D PlayerScreen, EnemyScreen;
		Controller->ProjectWorldLocationToScreen(Player->GetActorLocation(), PlayerScreen, true);
		Controller->ProjectWorldLocationToScreen(Enemy->GetActorLocation(), EnemyScreen, true);
		Test->TestTrue(TEXT("Shoulder framing separates target from player silhouette"), FMath::Abs(PlayerScreen.X - EnemyScreen.X) > Width * 0.02F);
		for (const ASCLCharacterBase* Character : {static_cast<ASCLCharacterBase*>(Player.Get()), Enemy.Get()})
		{
			for (const float Offset : {-80.0F, 85.0F})
			{
				FVector2D Screen;
				const bool Projected = Controller->ProjectWorldLocationToScreen(Character->GetActorLocation() + FVector{0.0F, 0.0F, Offset}, Screen, true);
				Test->TestTrue(TEXT("Both characters' head and feet fit in frame"), Projected && Screen.X > 0 && Screen.X < Width && Screen.Y > 0 && Screen.Y < Height);
			}
		}
		Test->AddInfo(FString::Printf(TEXT("%s: Camera=%s Pitch=%.2f Distance=%.0f"), Label, *View.ToString(), Pitch,
			FVector::Dist2D(Player->GetActorLocation(), Enemy->GetActorLocation())));
		if (FParse::Param(FCommandLine::Get(), TEXT("SCLCameraScreenshots")))
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/Camera%s.png"), Label), true, false);
	}
	void CheckUnlock(APlayerController& Controller)
	{
		Player->GetTargetingComponent()->ToggleLock();
		Test->TestFalse(TEXT("Middle-button toggle releases camera"), Player->GetTargetingComponent()->IsLockedOn());
		CheckFreeRig();
		const FRotator Before = Controller.GetControlRotation();
		Controller.AddYawInput(20.0F); Controller.AddPitchInput(10.0F); Controller.UpdateRotation(0.016F);
		const FRotator After = Controller.GetControlRotation();
		Test->TestTrue(TEXT("Unlock restores free yaw and pitch"), FMath::Abs(FMath::FindDeltaAngleDegrees(Before.Yaw, After.Yaw)) > 1.0F &&
			FMath::Abs(FMath::FindDeltaAngleDegrees(Before.Pitch, After.Pitch)) > 1.0F);
		// This fixture called UpdateRotation manually; consume the injected axes as PlayerTick normally does.
		Controller.RotationInput = FRotator::ZeroRotator;
		Test->TestTrue(TEXT("Unlock releases movement facing"), Player->GetCharacterMovement()->bOrientRotationToMovement);
	}
	FAutomationTestBase* Test;
	TWeakObjectPtr<ASCLPlayerCharacter> Player;
	TWeakObjectPtr<ASCLCharacterBase> Enemy;
	TWeakObjectPtr<ASCLEnemyCharacter> Sword, Heavy;
	TWeakObjectPtr<USCLEnemyHealthWidget> DeadHealthWidget;
	double Started{FPlatformTime::Seconds()}, Next{Started + 1.0};
	int32 Step{0};
	float HealthBefore{0.0F};
	// 使用游戏时间，截图/调试导致的现实时间停顿不应当提前结束动画检查。
	float ExecutionDeadline{0.0F};
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSCLCameraDifficultyRuntimeTest, "SoulCombatLabCombat.CameraDifficulty",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FSCLCameraDifficultyRuntimeTest::RunTest(const FString& Parameters)
{
	if (!FApp::CanEverRender()) { AddError(TEXT("Camera framing needs a rendered viewport")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FSCLCameraDifficultyCommand(this));
	return true;
}
#endif
