#include "UI/SCLBossWidget.h"

#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "Characters/SCLBossCharacter.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "SoulCombatLab.h"
#include "TimerManager.h"
#include "UI/SCLBossUIModel.h"

namespace
{
constexpr float PhaseTransitionDurationSeconds{1.75F};
constexpr float DeathDisplayDurationSeconds{1.25F};
const FLinearColor BossHealthColor{0.72F, 0.02F, 0.03F, 1.0F};
}

void USCLBossWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
	SetVisibility(ESlateVisibility::Collapsed);
}

void USCLBossWidget::NativeDestruct()
{
	UnbindBoss();
	Super::NativeDestruct();
}

void USCLBossWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UCanvasPanel* const RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("BossUIRoot"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* const Backdrop = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("BossUIBackdrop"));
	Backdrop->SetBrushColor(FLinearColor{0.015F, 0.01F, 0.01F, 0.82F});
	Backdrop->SetPadding(FMargin{18.0F, 10.0F, 18.0F, 10.0F});
	UCanvasPanelSlot* const BackdropSlot = RootCanvas->AddChildToCanvas(Backdrop);
	BackdropSlot->SetAnchors(FAnchors{0.5F, 0.0F});
	BackdropSlot->SetAlignment(FVector2D{0.5F, 0.0F});
	BackdropSlot->SetPosition(FVector2D{0.0F, 36.0F});
	BackdropSlot->SetSize(FVector2D{680.0F, 142.0F});

	UVerticalBox* const Content = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("BossUIContent"));
	Backdrop->SetContent(Content);

	BossNameText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("BossName"));
	BossNameText->SetText(FText::FromString(TEXT("BOSS PROTOTYPE")));
	BossNameText->SetJustification(ETextJustify::Center);
	BossNameText->SetColorAndOpacity(FSlateColor{FLinearColor::White});
	FSlateFontInfo NameFont = BossNameText->GetFont();
	NameFont.Size = 22;
	BossNameText->SetFont(NameFont);
	UVerticalBoxSlot* const NameSlot = Content->AddChildToVerticalBox(BossNameText);
	NameSlot->SetHorizontalAlignment(HAlign_Fill);
	NameSlot->SetPadding(FMargin{0.0F, 0.0F, 0.0F, 2.0F});

	PhaseText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("BossPhase"));
	PhaseText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* const PhaseSlot = Content->AddChildToVerticalBox(PhaseText);
	PhaseSlot->SetHorizontalAlignment(HAlign_Fill);

	USizeBox* const HealthBarSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("BossHealthBarSize"));
	HealthBarSize->SetHeightOverride(22.0F);
	UVerticalBoxSlot* const HealthBarSlot = Content->AddChildToVerticalBox(HealthBarSize);
	HealthBarSlot->SetHorizontalAlignment(HAlign_Fill);
	HealthBarSlot->SetPadding(FMargin{0.0F, 5.0F, 0.0F, 2.0F});

	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(),
		TEXT("BossHealthBar"));
	HealthBar->SetFillColorAndOpacity(BossHealthColor);
	HealthBarSize->SetContent(HealthBar);

	HealthValueText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("BossHealthValue"));
	HealthValueText->SetJustification(ETextJustify::Center);
	HealthValueText->SetColorAndOpacity(FSlateColor{FLinearColor{0.85F, 0.85F, 0.85F, 1.0F}});
	UVerticalBoxSlot* const HealthValueSlot = Content->AddChildToVerticalBox(HealthValueText);
	HealthValueSlot->SetHorizontalAlignment(HAlign_Fill);

	PhaseTransitionText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("BossPhaseTransition"));
	PhaseTransitionText->SetText(FText::FromString(TEXT("PHASE II - ENRAGED")));
	PhaseTransitionText->SetJustification(ETextJustify::Center);
	PhaseTransitionText->SetColorAndOpacity(
		FSlateColor{FLinearColor{1.0F, 0.16F, 0.02F, 1.0F}});
	FSlateFontInfo TransitionFont = PhaseTransitionText->GetFont();
	TransitionFont.Size = 18;
	PhaseTransitionText->SetFont(TransitionFont);
	PhaseTransitionText->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* const TransitionSlot = Content->AddChildToVerticalBox(PhaseTransitionText);
	TransitionSlot->SetHorizontalAlignment(HAlign_Fill);
	TransitionSlot->SetPadding(FMargin{0.0F, 3.0F, 0.0F, 0.0F});
}

void USCLBossWidget::BindBoss(ASCLBossCharacter& NewBoss)
{
	if (ObservedBoss.Get() == &NewBoss)
	{
		RefreshHealth();
		RefreshPhase();
		return;
	}

	UnbindBoss();
	ObservedBoss = &NewBoss;
	USCLAbilitySystemComponent* const AbilitySystem = NewBoss.GetSCLAbilitySystemComponent();
	if (AbilitySystem == nullptr)
	{
		ObservedBoss.Reset();
		return;
	}

	HealthChangedDelegateHandle = AbilitySystem
		->GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetHealthAttribute())
		.AddUObject(this, &USCLBossWidget::HandleHealthChanged);
	MaxHealthChangedDelegateHandle = AbilitySystem
		->GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &USCLBossWidget::HandleMaxHealthChanged);
	DeadTagDelegateHandle = AbilitySystem->RegisterGameplayTagEvent(
		SCLGameplayTags::State_Dead,
		EGameplayTagEventType::NewOrRemoved).AddUObject(
			this,
			&USCLBossWidget::HandleDeadTagChanged);
	NewBoss.OnBossPhaseChanged.AddDynamic(this, &USCLBossWidget::HandleBossPhaseChanged);
	NewBoss.OnDestroyed.AddDynamic(this, &USCLBossWidget::HandleBossDestroyed);

	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshHealth();
	RefreshPhase();
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Boss UI bound: Widget=%s Boss=%s Phase=%d"),
		*GetNameSafe(this),
		*GetNameSafe(&NewBoss),
		static_cast<int32>(NewBoss.GetBossPhase()));
}

void USCLBossWidget::UnbindBoss()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTransitionTimerHandle);
		World->GetTimerManager().ClearTimer(DeathHideTimerHandle);
	}
	PhaseTransitionTimerHandle.Invalidate();
	DeathHideTimerHandle.Invalidate();

	ASCLBossCharacter* const Boss = ObservedBoss.Get();
	USCLAbilitySystemComponent* const AbilitySystem = Boss != nullptr
		? Boss->GetSCLAbilitySystemComponent()
		: nullptr;
	if (AbilitySystem != nullptr)
	{
		if (HealthChangedDelegateHandle.IsValid())
		{
			AbilitySystem
				->GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetHealthAttribute())
				.Remove(HealthChangedDelegateHandle);
		}
		if (MaxHealthChangedDelegateHandle.IsValid())
		{
			AbilitySystem
				->GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetMaxHealthAttribute())
				.Remove(MaxHealthChangedDelegateHandle);
		}
		if (DeadTagDelegateHandle.IsValid())
		{
			AbilitySystem->RegisterGameplayTagEvent(
				SCLGameplayTags::State_Dead,
				EGameplayTagEventType::NewOrRemoved).Remove(DeadTagDelegateHandle);
		}
	}
	if (Boss != nullptr)
	{
		Boss->OnBossPhaseChanged.RemoveDynamic(this, &USCLBossWidget::HandleBossPhaseChanged);
		Boss->OnDestroyed.RemoveDynamic(this, &USCLBossWidget::HandleBossDestroyed);
	}

	HealthChangedDelegateHandle.Reset();
	MaxHealthChangedDelegateHandle.Reset();
	DeadTagDelegateHandle.Reset();
	ObservedBoss.Reset();
	if (PhaseTransitionText != nullptr)
	{
		PhaseTransitionText->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void USCLBossWidget::RefreshHealth()
{
	const ASCLBossCharacter* const Boss = ObservedBoss.Get();
	const USCLAbilitySystemComponent* const AbilitySystem = Boss != nullptr
		? Boss->GetSCLAbilitySystemComponent()
		: nullptr;
	if (AbilitySystem == nullptr)
	{
		return;
	}

	const float CurrentHealth =
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
	const float MaxHealth =
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetMaxHealthAttribute());
	const FSCLBossUIState State = SCLBossUIPolicy::ResolveState(
		CurrentHealth,
		MaxHealth,
		Boss->GetBossPhase());
	if (HealthBar != nullptr)
	{
		HealthBar->SetPercent(State.HealthFraction);
	}
	if (HealthValueText != nullptr)
	{
		HealthValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%.0f / %.0f"),
			FMath::Max(CurrentHealth, 0.0F),
			FMath::Max(MaxHealth, 0.0F))));
	}
}

void USCLBossWidget::RefreshPhase()
{
	const ASCLBossCharacter* const Boss = ObservedBoss.Get();
	const USCLAbilitySystemComponent* const AbilitySystem = Boss != nullptr
		? Boss->GetSCLAbilitySystemComponent()
		: nullptr;
	if (Boss == nullptr || AbilitySystem == nullptr || PhaseText == nullptr)
	{
		return;
	}

	const FSCLBossUIState State = SCLBossUIPolicy::ResolveState(
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()),
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetMaxHealthAttribute()),
		Boss->GetBossPhase());
	PhaseText->SetText(FText::FromName(State.PhaseLabel));
	PhaseText->SetColorAndOpacity(FSlateColor{State.PhaseColor});
}

void USCLBossWidget::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshHealth();
}

void USCLBossWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshHealth();
}

void USCLBossWidget::HandleDeadTagChanged(
	const FGameplayTag Tag,
	const int32 NewCount)
{
	if (Tag != SCLGameplayTags::State_Dead || NewCount <= 0)
	{
		return;
	}

	RefreshHealth();
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DeathHideTimerHandle,
			this,
			&USCLBossWidget::HideAfterDeath,
			DeathDisplayDurationSeconds,
			false);
	}
}

void USCLBossWidget::HandleBossPhaseChanged(
	const ESCLBossPhase PreviousPhase,
	const ESCLBossPhase NewPhase)
{
	RefreshHealth();
	RefreshPhase();
	if (!SCLBossUIPolicy::ShouldShowPhaseTransition(PreviousPhase, NewPhase) ||
		PhaseTransitionText == nullptr)
	{
		return;
	}

	PhaseTransitionText->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PhaseTransitionTimerHandle,
			this,
			&USCLBossWidget::HidePhaseTransition,
			PhaseTransitionDurationSeconds,
			false);
	}
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Boss UI phase feedback: Boss=%s Previous=%d Current=%d Duration=%.2f"),
		*GetNameSafe(ObservedBoss.Get()),
		static_cast<int32>(PreviousPhase),
		static_cast<int32>(NewPhase),
		PhaseTransitionDurationSeconds);
}

void USCLBossWidget::HandleBossDestroyed(AActor* const DestroyedActor)
{
	UnbindBoss();
}

void USCLBossWidget::HidePhaseTransition()
{
	PhaseTransitionTimerHandle.Invalidate();
	if (PhaseTransitionText != nullptr)
	{
		PhaseTransitionText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USCLBossWidget::HideAfterDeath()
{
	DeathHideTimerHandle.Invalidate();
	SetVisibility(ESlateVisibility::Collapsed);
}
