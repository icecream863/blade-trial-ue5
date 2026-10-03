#include "UI/SCLDeveloperDebugWidget.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "AI/SCLAIController.h"
#include "AI/SCLAIState.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Blueprint/WidgetTree.h"
#include "BrainComponent.h"
#include "Characters/SCLBossCharacter.h"
#include "Characters/SCLEnemyCharacter.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Combat/SCLCombatComponent.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Debug/SCLDebugHUDModel.h"
#include "GameFramework/PlayerController.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "Navigation/PathFollowingComponent.h"
#include "Targeting/SCLTargetingComponent.h"
#include "TimerManager.h"

namespace
{
FString ResolveActiveAbilityName(const USCLAbilitySystemComponent& AbilitySystem)
{
	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystem.GetActivatableAbilities())
	{
		if (AbilitySpec.IsActive() && AbilitySpec.Ability != nullptr)
		{
			return AbilitySpec.Ability->GetClass()->GetName();
		}
	}
	return TEXT("None");
}

FString ResolvePathStatus(const EPathFollowingStatus::Type Status)
{
	switch (Status)
	{
	case EPathFollowingStatus::Paused:
		return TEXT("Paused");
	case EPathFollowingStatus::Moving:
		return TEXT("Moving");
	case EPathFollowingStatus::Waiting:
		return TEXT("Waiting");
	case EPathFollowingStatus::Idle:
	default:
		return TEXT("Idle");
	}
}
}

void USCLDeveloperDebugWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
	SetVisibility(ESlateVisibility::Collapsed);
}

void USCLDeveloperDebugWidget::NativeDestruct()
{
	StopRefreshing();
	ObservedPlayer.Reset();
	KnownEnemies.Reset();
	Super::NativeDestruct();
}

void USCLDeveloperDebugWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UCanvasPanel* const RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("DeveloperDebugRoot"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* const Backdrop = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("DeveloperDebugBackdrop"));
	Backdrop->SetBrushColor(FLinearColor{0.005F, 0.015F, 0.02F, 0.88F});
	Backdrop->SetPadding(FMargin{14.0F});
	UCanvasPanelSlot* const BackdropSlot = RootCanvas->AddChildToCanvas(Backdrop);
	BackdropSlot->SetAnchors(FAnchors{0.0F, 0.0F});
	BackdropSlot->SetAlignment(FVector2D::ZeroVector);
	BackdropSlot->SetPosition(FVector2D{24.0F, 24.0F});
	BackdropSlot->SetSize(FVector2D{570.0F, 520.0F});

	UVerticalBox* const Content = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("DeveloperDebugContent"));
	Backdrop->SetContent(Content);

	UTextBlock* const Header = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("DeveloperDebugHeader"));
	Header->SetText(FText::FromString(TEXT("DEVELOPER DEBUG HUD  [F1]")));
	Header->SetColorAndOpacity(FSlateColor{FLinearColor{0.1F, 0.85F, 1.0F, 1.0F}});
	FSlateFontInfo HeaderFont = Header->GetFont();
	HeaderFont.Size = 19;
	Header->SetFont(HeaderFont);
	UVerticalBoxSlot* const HeaderSlot = Content->AddChildToVerticalBox(Header);
	HeaderSlot->SetHorizontalAlignment(HAlign_Fill);
	HeaderSlot->SetPadding(FMargin{0.0F, 0.0F, 0.0F, 8.0F});

	DebugBodyText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("DeveloperDebugBody"));
	DebugBodyText->SetColorAndOpacity(FSlateColor{FLinearColor{0.9F, 0.95F, 0.95F, 1.0F}});
	FSlateFontInfo BodyFont = DebugBodyText->GetFont();
	BodyFont.Size = 14;
	DebugBodyText->SetFont(BodyFont);
	UVerticalBoxSlot* const BodySlot = Content->AddChildToVerticalBox(DebugBodyText);
	BodySlot->SetHorizontalAlignment(HAlign_Fill);
}

void USCLDeveloperDebugWidget::BindPlayer(ASCLPlayerCharacter& Player)
{
	ObservedPlayer = &Player;
	if (IsDebugDisplayVisible())
	{
		RefreshDebugText();
	}
}

void USCLDeveloperDebugWidget::RegisterEnemy(ASCLEnemyCharacter& Enemy)
{
	const bool bAlreadyKnown = KnownEnemies.ContainsByPredicate(
		[&Enemy](const TWeakObjectPtr<ASCLEnemyCharacter>& KnownEnemy)
		{
			return KnownEnemy.Get() == &Enemy;
		});
	if (!bAlreadyKnown)
	{
		KnownEnemies.Add(&Enemy);
	}
}

void USCLDeveloperDebugWidget::ToggleDebugDisplay()
{
	if (IsDebugDisplayVisible())
	{
		StopRefreshing();
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshDebugText();
	StartRefreshing();
}

bool USCLDeveloperDebugWidget::IsDebugDisplayVisible() const
{
	return GetVisibility() != ESlateVisibility::Collapsed &&
		GetVisibility() != ESlateVisibility::Hidden;
}

void USCLDeveloperDebugWidget::StartRefreshing()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		RefreshTimerHandle,
		this,
		&USCLDeveloperDebugWidget::RefreshDebugText,
		RefreshIntervalSeconds,
		true);
}

void USCLDeveloperDebugWidget::StopRefreshing()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	RefreshTimerHandle.Invalidate();
}

ASCLEnemyCharacter* USCLDeveloperDebugWidget::ResolveObservedEnemy()
{
	KnownEnemies.RemoveAll(
		[](const TWeakObjectPtr<ASCLEnemyCharacter>& Enemy)
		{
			return !Enemy.IsValid();
		});

	ASCLPlayerCharacter* const Player = ObservedPlayer.Get();
	if (Player == nullptr)
	{
		return nullptr;
	}

	if (const USCLTargetingComponent* const Targeting = Player->GetTargetingComponent())
	{
		if (ASCLEnemyCharacter* const LockedEnemy =
			Cast<ASCLEnemyCharacter>(Targeting->GetCurrentTarget()))
		{
			return LockedEnemy;
		}
	}

	ASCLEnemyCharacter* NearestEnemy = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<ASCLEnemyCharacter>& EnemyReference : KnownEnemies)
	{
		ASCLEnemyCharacter* const Enemy = EnemyReference.Get();
		const USCLAbilitySystemComponent* const EnemyAbilitySystem = Enemy != nullptr
			? Enemy->GetSCLAbilitySystemComponent()
			: nullptr;
		if (Enemy == nullptr || EnemyAbilitySystem == nullptr ||
			EnemyAbilitySystem->HasMatchingGameplayTag(SCLGameplayTags::State_Dead))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			Player->GetActorLocation(),
			Enemy->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestEnemy = Enemy;
		}
	}
	return NearestEnemy;
}

void USCLDeveloperDebugWidget::RefreshDebugText()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SCL_DebugHUD);
	ASCLPlayerCharacter* const Player = ObservedPlayer.Get();
	if (Player == nullptr || DebugBodyText == nullptr)
	{
		return;
	}

	FSCLDebugHUDSnapshot Snapshot;
	const float DeltaSeconds = GetWorld() != nullptr ? GetWorld()->GetDeltaSeconds() : 0.0F;
	Snapshot.FramesPerSecond = DeltaSeconds > UE_SMALL_NUMBER ? 1.0F / DeltaSeconds : 0.0F;

	const USCLAbilitySystemComponent* const PlayerAbilitySystem =
		Player->GetSCLAbilitySystemComponent();
	if (PlayerAbilitySystem != nullptr)
	{
		FGameplayTagContainer PlayerTags;
		PlayerAbilitySystem->GetOwnedGameplayTags(PlayerTags);
		Snapshot.PlayerStateTags = PlayerTags.IsEmpty()
			? TEXT("None")
			: PlayerTags.ToStringSimple();
		Snapshot.PlayerHealth = PlayerAbilitySystem->GetNumericAttribute(
			USCLAttributeSet::GetHealthAttribute());
		Snapshot.PlayerMaxHealth = PlayerAbilitySystem->GetNumericAttribute(
			USCLAttributeSet::GetMaxHealthAttribute());
		Snapshot.PlayerStamina = PlayerAbilitySystem->GetNumericAttribute(
			USCLAttributeSet::GetStaminaAttribute());
		Snapshot.PlayerMaxStamina = PlayerAbilitySystem->GetNumericAttribute(
			USCLAttributeSet::GetMaxStaminaAttribute());
		Snapshot.CurrentAbility = ResolveActiveAbilityName(*PlayerAbilitySystem);
	}
	if (const USCLTargetingComponent* const Targeting = Player->GetTargetingComponent())
	{
		Snapshot.LockedTarget = GetNameSafe(Targeting->GetCurrentTarget());
	}

	ASCLEnemyCharacter* const Enemy = ResolveObservedEnemy();
	if (Enemy != nullptr)
	{
		Snapshot.EnemyName = GetNameSafe(Enemy);
		Snapshot.EnemyDistance = FVector::Dist(
			Player->GetActorLocation(),
			Enemy->GetActorLocation());
		if (const USCLCombatComponent* const Combat = Enemy->GetCombatComponent_Implementation())
		{
			const FName AttackName = Combat->GetActiveAttackName();
			Snapshot.CurrentAttack = AttackName.IsNone()
				? TEXT("None")
				: AttackName.ToString();
		}
		if (const ASCLBossCharacter* const Boss = Cast<ASCLBossCharacter>(Enemy))
		{
			Snapshot.BossPhase = Boss->GetBossPhase() == ESCLBossPhase::PhaseTwo
				? TEXT("PhaseTwo")
				: TEXT("PhaseOne");
		}

		const ASCLAIController* const AIController = Cast<ASCLAIController>(Enemy->GetController());
		const UBlackboardComponent* const Blackboard = AIController != nullptr
			? AIController->GetBlackboardComponent()
			: nullptr;
		if (AIController != nullptr)
		{
			Snapshot.bHasPerceivedTarget = AIController->HasPerceivedTarget();
			Snapshot.bBehaviorTreeRunning = AIController->GetBrainComponent() != nullptr &&
				AIController->GetBrainComponent()->IsRunning();
			Snapshot.PathFollowingStatus = ResolvePathStatus(AIController->GetMoveStatus());
			Snapshot.EnemyState = StaticEnum<ESCLEnemyAIState>()->GetNameStringByValue(
				static_cast<int64>(AIController->GetCurrentCombatState()));
		}
		if (Blackboard != nullptr)
		{
			Snapshot.bHasLineOfSight = Blackboard->GetValueAsBool(
				SCLBlackboardKeys::HasLineOfSight);
			Snapshot.bCanAttack = Blackboard->GetValueAsBool(SCLBlackboardKeys::CanAttack);
			const FVector EQSPoint = Blackboard->GetValueAsVector(
				SCLBlackboardKeys::IdealCombatLocation);
			Snapshot.bHasEQSPoint = !EQSPoint.ContainsNaN() && !EQSPoint.IsNearlyZero();
			Snapshot.EQSPoint = EQSPoint;
			Snapshot.DistanceToEQSPoint = Snapshot.bHasEQSPoint
				? FVector::Dist(Enemy->GetActorLocation(), EQSPoint)
				: 0.0F;
		}
	}

	DebugBodyText->SetText(FText::FromString(SCLDebugHUDFormatter::BuildText(Snapshot)));
}
