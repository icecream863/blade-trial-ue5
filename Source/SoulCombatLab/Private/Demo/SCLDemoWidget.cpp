#include "Demo/SCLDemoWidget.h"
#include "Demo/SCLDemoSubsystem.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/SCLParryAbility.h"
#include "AbilitySystem/Abilities/SCLExecutionAbility.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Characters/SCLEnemyCharacter.h"
#include "Characters/SCLBossCharacter.h"
#include "Targeting/SCLTargetingComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "TimerManager.h"
#include "GameplayTags/SCLGameplayTags.h"

void USCLDemoWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* const Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const auto AddText = [this](UVerticalBox* Box, const TCHAR* Text, int32 Size)
	{
		UTextBlock* const Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Text));
		Label->SetAutoWrapText(true);
		FSlateFontInfo Font = Label->GetFont(); Font.Size = Size; Label->SetFont(Font);
		Box->AddChildToVerticalBox(Label)->SetPadding(FMargin{0.0F, 5.0F});
		return Label;
	};
	UBorder* const Status = WidgetTree->ConstructWidget<UBorder>();
	Status->SetBrushColor(FLinearColor{0.015F, 0.025F, 0.035F, 0.88F});
	Status->SetPadding(FMargin{16.0F});
	Status->SetVisibility(ESlateVisibility::HitTestInvisible);
	auto* StatusSlot = Root->AddChildToCanvas(Status);
	StatusSlot->SetAnchors(FAnchors{0.0F, 1.0F});
	StatusSlot->SetAlignment(FVector2D{0.0F, 1.0F});
	StatusSlot->SetPosition(FVector2D{24.0F, -24.0F});
	StatusSlot->SetSize(FVector2D{420.0F, 340.0F});
	UVerticalBox* const StatusBox = WidgetTree->ConstructWidget<UVerticalBox>(); Status->SetContent(StatusBox);
	Objective = AddText(StatusBox, TEXT("刀术试炼"), 17);
	Vitals = AddText(StatusBox, TEXT(""), 15);
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>();
	HealthBar->SetFillColorAndOpacity(FLinearColor{0.8F, 0.12F, 0.12F});
	USizeBox* const HPSize = WidgetTree->ConstructWidget<USizeBox>(); HPSize->SetHeightOverride(18.0F); HPSize->SetContent(HealthBar);
	StatusBox->AddChildToVerticalBox(HPSize)->SetPadding(FMargin{0.0F, 4.0F});
	StaminaBar = WidgetTree->ConstructWidget<UProgressBar>();
	StaminaBar->SetFillColorAndOpacity(FLinearColor{0.15F, 0.75F, 0.35F});
	USizeBox* const SPSize = WidgetTree->ConstructWidget<USizeBox>(); SPSize->SetHeightOverride(14.0F); SPSize->SetContent(StaminaBar);
	StatusBox->AddChildToVerticalBox(SPSize)->SetPadding(FMargin{0.0F, 4.0F});
	AddText(StatusBox, TEXT("Esc 暂停 / 操作说明"), 14);
	OpponentVitals = AddText(StatusBox, TEXT(""), 15);
	PoiseBar = WidgetTree->ConstructWidget<UProgressBar>();
	PoiseBar->SetFillColorAndOpacity(FLinearColor{0.95F, 0.65F, 0.1F});
	USizeBox* const PoiseSize = WidgetTree->ConstructWidget<USizeBox>();
	PoiseSize->SetHeightOverride(14.0F); PoiseSize->SetContent(PoiseBar);
	StatusBox->AddChildToVerticalBox(PoiseSize);
	CombatHint = AddText(StatusBox, TEXT("削韧至零可失衡，靠近按 E 处决"), 16);
	// 常驻速查放在右下角，主状态卡继续显示血量/体力；不盖住中央战斗画面。
	QuickHelp = WidgetTree->ConstructWidget<UBorder>();
	QuickHelp->SetBrushColor(FLinearColor{0.015F, 0.025F, 0.035F, 0.88F});
	QuickHelp->SetPadding(FMargin{14.0F});
	QuickHelp->SetVisibility(ESlateVisibility::HitTestInvisible);
	auto* HelpSlot = Root->AddChildToCanvas(QuickHelp);
	HelpSlot->SetAnchors(FAnchors{1.0F, 1.0F});
	HelpSlot->SetAlignment(FVector2D{1.0F, 1.0F});
	HelpSlot->SetPosition(FVector2D{-24.0F, -24.0F});
	HelpSlot->SetAutoSize(true);
	USizeBox* HelpSize = WidgetTree->ConstructWidget<USizeBox>();
	HelpSize->SetWidthOverride(350.0F); QuickHelp->SetContent(HelpSize);
	UVerticalBox* HelpBox = WidgetTree->ConstructWidget<UVerticalBox>(); HelpSize->SetContent(HelpBox);
	AddText(HelpBox, TEXT("战斗速查"), 17);
	AddText(HelpBox, TEXT("左键短按轻击 / 长按重击 · 连按接招\n右键按住格挡 · 松开解除\nQ 弹反：正面来刀命中前按\nE 处决：敌人失衡后靠近并面向它\n左 Shift 八向闪避 · 中键锁定\n滚轮切换目标 · 空格跳跃 · Esc 暂停"), 14);
	StaminaHint = AddText(HelpBox, TEXT("松开格挡，停手片刻可恢复体力"), 14);
	Menu = WidgetTree->ConstructWidget<UBorder>();
	Menu->SetBrushColor(FLinearColor{0.012F, 0.02F, 0.03F, 0.97F});
	Menu->SetPadding(FMargin{30.0F});
	auto* MenuSlot = Root->AddChildToCanvas(Menu);
	MenuSlot->SetAnchors(FAnchors{0.5F, 0.5F}); MenuSlot->SetAlignment(FVector2D{0.5F, 0.5F});
	MenuSlot->SetSize(FVector2D{660.0F, 560.0F});
	UVerticalBox* const MenuBox = WidgetTree->ConstructWidget<UVerticalBox>(); Menu->SetContent(MenuBox);
	Heading = AddText(MenuBox, TEXT("刀术试炼"), 30);
	Description = AddText(MenuBox, TEXT(""), 18);
	AddText(MenuBox, TEXT("WASD 移动  ·  鼠标转视角  ·  中键锁定\n左键短按轻击 / 长按重击，连按接招\n右键按住格挡  ·  正面来刀命中前按 Q 弹反\n敌人失衡后，靠近并面向它按 E 处决\n左 Shift 八向闪避  ·  空格跳跃\n滚轮切换目标  ·  Esc 暂停 / 继续"), 17);
	AddText(MenuBox, TEXT("进入区域恢复生命与体力；死亡按 R 从本区入口重试。"), 15);
	const auto AddButton = [this, MenuBox](const TCHAR* Text, UTextBlock*& OutLabel)
	{
		UButton* const Button = WidgetTree->ConstructWidget<USCLDemoButton>();
		OutLabel = WidgetTree->ConstructWidget<UTextBlock>(); OutLabel->SetText(FText::FromString(Text));
		OutLabel->SetJustification(ETextJustify::Center);
		OutLabel->SetColorAndOpacity(FSlateColor{FLinearColor{0.02F, 0.025F, 0.035F}});
		Button->SetContent(OutLabel);
		MenuBox->AddChildToVerticalBox(Button)->SetPadding(FMargin{0.0F, 8.0F});
		return Button;
	};
	UTextBlock* Label = nullptr;
	PrimaryButton = AddButton(TEXT("开始试炼"), Label); PrimaryText = Label;
	PrimaryButton->OnClicked.AddDynamic(this, &USCLDemoWidget::HandlePrimary);
	AddButton(TEXT("重新挑战"), Label)->OnClicked.AddDynamic(this, &USCLDemoWidget::HandleRestart);
	AddButton(TEXT("退出试炼"), Label)->OnClicked.AddDynamic(this, &USCLDemoWidget::HandleQuit);
	GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &USCLDemoWidget::Refresh, 0.1F, true);
}

void USCLDemoWidget::Refresh()
{
	USCLDemoSubsystem* const Demo = GetWorld()->GetSubsystem<USCLDemoSubsystem>();
	if (Demo == nullptr || Heading == nullptr) return;
	Objective->SetText(Demo->GetObjective());
	if (ASCLPlayerCharacter* const Player = Demo->GetPlayer())
	{
		const USCLAbilitySystemComponent* const ASC = Player->GetSCLAbilitySystemComponent();
		const float HP = ASC->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
		const float MaxHP = ASC->GetNumericAttribute(USCLAttributeSet::GetMaxHealthAttribute());
		const float SP = ASC->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute());
		const float MaxSP = ASC->GetNumericAttribute(USCLAttributeSet::GetMaxStaminaAttribute());
		HealthBar->SetPercent(FMath::Clamp(HP / FMath::Max(MaxHP, 1.0F), 0.0F, 1.0F));
		StaminaBar->SetPercent(FMath::Clamp(SP / FMath::Max(MaxSP, 1.0F), 0.0F, 1.0F));
		// 这是资源预警，不修改体力或恢复逻辑；按住格挡时当前规则暂停体力恢复。
		const bool bLowStamina = SP <= MaxSP * 0.20F;
		StaminaBar->SetFillColorAndOpacity(bLowStamina ? FLinearColor{1.0F, 0.55F, 0.10F} : FLinearColor{0.15F, 0.75F, 0.35F});
		StaminaHint->SetText(FText::FromString(bLowStamina ? TEXT("体力偏低！松开格挡，停手片刻恢复") : TEXT("松开格挡，停手片刻可恢复体力")));
		StaminaHint->SetColorAndOpacity(FSlateColor{bLowStamina ? FLinearColor{1.0F, 0.65F, 0.25F} : FLinearColor::White});
		Vitals->SetText(FText::FromString(FString::Printf(TEXT("生命 %.0f / %.0f   体力 %.0f / %.0f"), HP, MaxHP, SP, MaxSP)));
	}
	const ESCLDemoState State = Demo->GetState();
	if (const ASCLCharacterBase* const Opponent = Demo->GetOpponent())
	{
		const USCLAbilitySystemComponent* const ASC = Opponent->GetSCLAbilitySystemComponent();
		const float Poise = ASC->GetNumericAttribute(USCLAttributeSet::GetPoiseAttribute());
		const float MaxPoise = ASC->GetNumericAttribute(USCLAttributeSet::GetMaxPoiseAttribute());
		const ASCLPlayerCharacter* const Player = Demo->GetPlayer();
		const bool bLocked = Player != nullptr && Player->GetTargetingComponent()->GetCurrentTarget() == Opponent;
		const TCHAR* Name = Opponent->IsA<ASCLBossCharacter>() ? TEXT("Boss")
			: Opponent->IsA<ASCLHeavyEnemyCharacter>() ? TEXT("重兵")
			: Opponent->IsA<ASCLEnemyCharacter>() ? TEXT("剑兵") : TEXT("训练目标");
		OpponentVitals->SetText(FText::FromString(FString::Printf(TEXT("%s：%s  生命 %.0f\n韧性 %.0f / %.0f"),
			bLocked ? TEXT("锁定") : TEXT("最近"), Name, ASC->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()), Poise, MaxPoise)));
		PoiseBar->SetPercent(Poise / FMath::Max(MaxPoise, 1.0F));
	}
	else { OpponentVitals->SetText(FText::GetEmpty()); PoiseBar->SetPercent(0.0F); }
	RefreshCombatHint(Demo->GetPlayer(), Demo->GetOpponent());
	Menu->SetVisibility(!Demo->IsExploring() || Demo->IsPaused() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	QuickHelp->SetVisibility(Demo->IsExploring() && !Demo->IsPaused() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	PrimaryText->SetText(Demo->GetPrimaryLabel()); PrimaryButton->SetIsEnabled(State != ESCLDemoState::Error);
	const TCHAR* Title = TEXT("刀术试炼");
	const TCHAR* Detail = TEXT("观察对手，抓住破绽。\n以连招、弹反和闪避完成试炼，最终击败 Boss。\n按 Enter 开始挑战。");
	if (Demo->IsPaused()) { Title = TEXT("已暂停"); Detail = TEXT("按 Esc 或 Enter 继续当前战斗。"); }
	else if (State == ESCLDemoState::Defeat) { Title = TEXT("挑战失败"); Detail = TEXT("当前区域已保留。按 R 或 Enter 从本区入口重试。"); }
	else if (State == ESCLDemoState::StageClear) { Title = TEXT("关卡完成"); Detail = TEXT("通道已开启，沿金色路线步行前进。"); }
	else if (State == ESCLDemoState::Victory) { Title = TEXT("试炼完成"); Detail = TEXT("你已完成全部试炼并击败 Boss。可以重新挑战。"); }
	else if (State == ESCLDemoState::Error) { Title = TEXT("关卡加载失败"); Detail = TEXT("请尝试重新挑战。"); }
	Heading->SetText(FText::FromString(Title)); Description->SetText(FText::FromString(Detail));
}

void USCLDemoWidget::RefreshCombatHint(ASCLPlayerCharacter* Player, const ASCLCharacterBase* Opponent)
{
	const TCHAR* Hint = TEXT("沿金色路线前进，清场后通道开启");
	FLinearColor Color = FLinearColor::White;
	if (Player)
	{
		auto* ASC = Player->GetSCLAbilitySystemComponent();
		const auto* EnemyASC = Opponent ? Opponent->GetSCLAbilitySystemComponent() : nullptr;
		const auto* Execution = ASC->FindAbilitySpecByBaseClass(USCLExecutionAbility::StaticClass());
		const auto* ParrySpec = ASC->FindAbilitySpecByBaseClass(USCLParryAbility::StaticClass());
		const auto* Parry = ParrySpec ? Cast<USCLParryAbility>(ParrySpec->GetPrimaryInstance()) : nullptr;
		const bool bRecentlyParried = Parry && Parry->WasRecentlySuccessful();
		const bool bExecutable = EnemyASC && EnemyASC->HasMatchingGameplayTag(SCLGameplayTags::State_Executable);
		// 只在实际活动技能中显示“处决中”；失衡提示不承诺距离、朝向、路径已通过预检。
		if (ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Dead)) Hint = TEXT("挑战失败 · R 从本区入口重试");
		else if (Execution && Execution->IsActive()) Hint = TEXT("处决中 · 命中后等待动作结束");
		else if (ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Staggered)) Hint = TEXT("失衡中 · 等待恢复，避免背对敌人");
		else if (bExecutable)
		{
			Hint = bRecentlyParried ? TEXT("弹反成功！靠近并面向敌人，按 E 处决") : TEXT("敌人失衡！靠近并面向敌人，按 E 处决");
			Color = FLinearColor::Yellow;
		}
		else if (bRecentlyParried) { Hint = TEXT("弹反成功 · 可以接攻击或移动"); Color = FLinearColor::Yellow; }
		else if (ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Blocking)) Hint = TEXT("格挡中 · 防住正面来刀；松开右键恢复体力");
		else if (ASC->HasMatchingGameplayTag(SCLGameplayTags::State_ParryAction)) Hint = TEXT("弹反动作中 · 只有有效帧接住来刀才算成功");
		else if (EnemyASC && EnemyASC->HasMatchingGameplayTag(SCLGameplayTags::State_Attacking))
			Hint = TEXT("敌人出招 · 右键格挡，或命中前按 Q 弹反");
		else if (Opponent) Hint = TEXT("中键锁定 · 攻击削韧或成功弹反，创造处决机会");
	}
	CombatHint->SetText(FText::FromString(Hint));
	CombatHint->SetColorAndOpacity(FSlateColor{Color});
}
void USCLDemoWidget::HandlePrimary() { if (auto* Demo = GetWorld()->GetSubsystem<USCLDemoSubsystem>()) Demo->PrimaryAction(); }
void USCLDemoWidget::HandleRestart() { if (auto* Demo = GetWorld()->GetSubsystem<USCLDemoSubsystem>()) Demo->RestartRun(); }
void USCLDemoWidget::HandleQuit() { if (auto* Demo = GetWorld()->GetSubsystem<USCLDemoSubsystem>()) Demo->Quit(); }
void USCLDemoWidget::NativeDestruct()
{
	if (GetWorld() != nullptr) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	Super::NativeDestruct();
}

