#include "UI/SCLEnemyHealthWidget.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "Characters/SCLEnemyCharacter.h"
#include "CollisionQueryParams.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void USCLEnemyHealthWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(false);
	UBorder* const Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(FLinearColor{0.015F, 0.02F, 0.025F, 0.88F});
	Backdrop->SetPadding(FMargin{5.0F, 3.0F});
	WidgetTree->RootWidget = Backdrop;
	UVerticalBox* const Content = WidgetTree->ConstructWidget<UVerticalBox>();
	Backdrop->SetContent(Content);
	HealthText = WidgetTree->ConstructWidget<UTextBlock>();
	HealthText->SetJustification(ETextJustify::Center);
	HealthText->SetColorAndOpacity(FSlateColor{FLinearColor::White});
	FSlateFontInfo Font = HealthText->GetFont(); Font.Size = 13; HealthText->SetFont(Font);
	Content->AddChildToVerticalBox(HealthText)->SetPadding(FMargin{0.0F, 0.0F, 0.0F, 3.0F});
	USizeBox* const BarSize = WidgetTree->ConstructWidget<USizeBox>();
	BarSize->SetHeightOverride(8.0F);
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>();
	HealthBar->SetFillColorAndOpacity(FLinearColor{0.85F, 0.06F, 0.045F});
	BarSize->SetContent(HealthBar);
	Content->AddChildToVerticalBox(BarSize);
	SetVisibility(ESlateVisibility::Collapsed);
}

void USCLEnemyHealthWidget::BindEnemy(ASCLEnemyCharacter& Enemy)
{
	UnbindEnemy();
	ObservedEnemy = &Enemy;
	USCLAbilitySystemComponent* const ASC = Enemy.GetSCLAbilitySystemComponent();
	HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetHealthAttribute())
		.AddUObject(this, &USCLEnemyHealthWidget::HandleHealthChanged);
	MaxHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &USCLEnemyHealthWidget::HandleHealthChanged);
	RefreshHealth();
	// Screen-space bars stay readable; only nearby visible enemies may display them.
	GetWorld()->GetTimerManager().SetTimer(VisibilityTimer, this, &USCLEnemyHealthWidget::RefreshVisibility, 0.15F, true);
}

void USCLEnemyHealthWidget::UnbindEnemy()
{
	if (GetWorld() != nullptr) GetWorld()->GetTimerManager().ClearTimer(VisibilityTimer);
	if (ASCLEnemyCharacter* const Enemy = ObservedEnemy.Get())
	{
		USCLAbilitySystemComponent* const ASC = Enemy->GetSCLAbilitySystemComponent();
		ASC->GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
	}
	HealthChangedHandle.Reset(); MaxHealthChangedHandle.Reset(); ObservedEnemy.Reset();
	SetVisibility(ESlateVisibility::Collapsed);
}

void USCLEnemyHealthWidget::NativeDestruct()
{
	UnbindEnemy();
	Super::NativeDestruct();
}

void USCLEnemyHealthWidget::HandleHealthChanged(const FOnAttributeChangeData& Change)
{
	RefreshHealth();
}

void USCLEnemyHealthWidget::RefreshHealth()
{
	const ASCLEnemyCharacter* const Enemy = ObservedEnemy.Get();
	if (Enemy == nullptr || HealthBar == nullptr || HealthText == nullptr) return;
	const USCLAbilitySystemComponent* const ASC = Enemy->GetSCLAbilitySystemComponent();
	const float HP = ASC->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
	const float MaxHP = ASC->GetNumericAttribute(USCLAttributeSet::GetMaxHealthAttribute());
	HealthBar->SetPercent(FMath::Clamp(HP / FMath::Max(MaxHP, 1.0F), 0.0F, 1.0F));
	HealthText->SetText(FText::FromString(FString::Printf(TEXT("%s  %.0f / %.0f"),
		Enemy->IsA<ASCLHeavyEnemyCharacter>() ? TEXT("重兵") : TEXT("剑兵"), HP, MaxHP)));
	RefreshVisibility();
}

float USCLEnemyHealthWidget::GetDisplayedHealthFraction() const
{
	return HealthBar != nullptr ? HealthBar->GetPercent() : 0.0F;
}

void USCLEnemyHealthWidget::RefreshVisibility()
{
	UWorld* const World = GetWorld();
	const ASCLEnemyCharacter* const Enemy = ObservedEnemy.Get();
	const APlayerController* const Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	bool bVisible = Enemy != nullptr && Controller != nullptr && Controller->GetPawn() != nullptr &&
		Enemy->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()) > 0.0F &&
		FVector::DistSquared(Enemy->GetActorLocation(), Controller->GetPawn()->GetActorLocation()) < FMath::Square(2000.0F);
	if (bVisible)
	{
		FVector View; FRotator Rotation;
		Controller->GetPlayerViewPoint(View, Rotation);
		const FVector Aim = Enemy->GetActorLocation() + FVector{0.0F, 0.0F, 70.0F};
		FCollisionQueryParams Query{SCENE_QUERY_STAT(SCLEnemyHealthVisibility), false, Controller->GetPawn()};
		Query.AddIgnoredActor(Enemy);
		FHitResult Hit;
		bVisible = FVector::DotProduct(Rotation.Vector(), Aim - View) > 0.0F &&
			!World->LineTraceSingleByChannel(Hit, View, Aim, ECC_Visibility, Query);
	}
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
