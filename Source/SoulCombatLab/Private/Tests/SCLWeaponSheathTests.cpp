#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Characters/Player/SCLWeaponPresentationComponent.h"
#include "Combat/SCLCombatComponent.h"
#include "Combat/SCLWeapon.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Demo/SCLDemoSubsystem.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// 用真实玩家、Montage 时间轴与 Notify 验证挂点，不直接调用 CommitSheath 伪造成功。
class FSCLWeaponSheathCommand final : public IAutomationLatentCommand
{
public:
	explicit FSCLWeaponSheathCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds() - Started > 70.0) { Test->AddError(TEXT("Weapon sheath test timed out")); return true; }
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Game) World = Context.World();
		if (!World) return false;
		// 动画与 Timer 使用游戏时间；截图/渲染停顿不应让断言提前越过 Notify。
		const double Now = World->GetTimeSeconds();
		auto* Demo = World->GetSubsystem<USCLDemoSubsystem>();
		if (!Demo || !Demo->IsActive()) return false;
		if (Demo->GetState() == ESCLDemoState::Title) Demo->PrimaryAction();
		auto* Player = Cast<ASCLPlayerCharacter>(World->GetFirstPlayerController()->GetPawn());
		if (!Player) return false;
		auto* Presentation = Player->GetWeaponPresentationComponent();
		auto* Combat = Player->GetCombatComponent_Implementation();
		auto* ASC = Player->GetSCLAbilitySystemComponent();
		auto* Anim = Player->GetMesh()->GetAnimInstance();
		auto* Weapon = Combat->GetEquippedWeapon();
		if (!Presentation || !Anim || !ASC || !Weapon)
		{ Test->AddError(TEXT("Missing player weapon presentation fixture")); return true; }
		if (!Presentation->IsSheathDrawEnabled())
		{
			// 暂停表现流程时，闲置和移动都不应收刀，攻击立即走原技能入口。
			if (Step == 0)
			{
				Test->TestNull(TEXT("Disabled sheath does not load its montage"), Presentation->GetSheathMontage());
				Test->TestNull(TEXT("Disabled draw does not load its montage"), Presentation->GetDrawMontage());
				StartLocation = Player->GetActorLocation();
				Next = Now + Presentation->GetIdleDelay() + 0.5;
				bWalking = true;
				Step = 1;
				return false;
			}
			if (bWalking) Player->MoveInViewDirection(FVector2D(0.0F, 0.3F));
			if (Now < Next) return false;
			bWalking = false;
			Test->TestTrue(TEXT("Player can move while sheath is disabled"),
				FVector::Dist2D(StartLocation, Player->GetActorLocation()) > 25.0F);
			Test->TestFalse(TEXT("Idle does not start sheath"), Presentation->IsSheathing());
			Test->TestFalse(TEXT("Sword remains unsheathed"), Presentation->IsSheathed());
			Test->TestFalse(TEXT("Draw is inactive"), Presentation->IsDrawing());
			Test->TestEqual(TEXT("Sword stays in hand socket"),
				Weapon->GetRootComponent()->GetAttachSocketName(), Player->GetWeaponHandSocket());
			Player->RequestLightAttack();
			Test->TestTrue(TEXT("Light attack starts without draw delay"), Combat->IsAttackActive());
			Test->TestFalse(TEXT("Attack does not start draw montage"), Presentation->IsDrawing());
			return true;
		}
		if (!Presentation->GetSheathMontage() || !Presentation->GetDrawMontage())
		{ Test->AddError(TEXT("Missing enabled sheath/draw animation")); return true; }
		// 每帧送移动输入，验证收刀动画实际播放时角色仍能前进。
		if (bWalking) Player->MoveInViewDirection(FVector2D(0.0F, 0.3F));
		if (Now < Next) return false;
		const double Idle = Presentation->GetIdleDelay() + 0.25;
		switch (Step)
		{
		case 0:
			Combat->CancelActiveAttack(); Anim->StopAllMontages(0.0F);
			Presentation->PrepareForAction();
			StartLocation = Player->GetActorLocation(); OriginalRootMode = Anim->RootMotionMode;
			Test->TestEqual(TEXT("Authored sheath sequence length"), Presentation->GetSheathMontage()->GetPlayLength(), 2.7F);
			Test->TestTrue(TEXT("Fast draw montage is available"), Presentation->GetDrawMontage()->GetPlayLength() > 1.3F);
			bWalking = true;
			Next = Now + Idle; break;
		case 1:
			Test->TestTrue(TEXT("Idle automatically starts sheath while moving"), Presentation->IsSheathing());
			Test->TestTrue(TEXT("Character can move before sheath"), FVector::Dist(StartLocation, Player->GetActorLocation()) > 25.0F);
			Test->TestFalse(TEXT("Sword stays in hand before authored notify"), Presentation->IsSheathed());
			Next = Now + 0.65; break;
		case 2:
			Screenshot(TEXT("Sheath-Animation")); Next = Now + 0.85; break;
		case 3:
			Test->TestTrue(TEXT("Actual animation notify attaches sword to sheath"), Presentation->IsSheathed());
			Test->TestEqual(TEXT("Sheath uses authored katana socket"), Weapon->GetRootComponent()->GetAttachSocketName(), FName(TEXT("katana_Targer01Socket")));
			Screenshot(TEXT("Sheath-Commit")); Next = Now + 1.35; break;
		case 4:
			Test->TestFalse(TEXT("Sheath montage finishes"), Presentation->IsSheathing());
			Test->TestTrue(TEXT("Finished sword remains sheathed"), Presentation->IsSheathed());
			Test->TestTrue(TEXT("Root motion mode restored"), Anim->RootMotionMode == OriginalRootMode);
			Test->TestTrue(TEXT("Sheath does not freeze moving capsule"), FVector::Dist(StartLocation, Player->GetActorLocation()) > 100.0F);
			// 无处决目标、零体力格挡都是被拒绝的请求，不得暗中拔刀。
			Player->RequestExecution();
			Test->TestTrue(TEXT("Rejected execution leaves sword sheathed"), Presentation->IsSheathed());
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 0.0F);
			Player->StartBlock();
			Test->TestTrue(TEXT("Rejected block leaves sword sheathed"), Presentation->IsSheathed());
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			Screenshot(TEXT("Sheath-Idle")); bWalking = false; Next = Now + 0.2; break;
		case 5:
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			Player->RequestLightAttack();
			Test->TestTrue(TEXT("Sheathed light attack starts draw montage"), Presentation->IsDrawing());
			Test->TestFalse(TEXT("Attack waits until draw finishes"), Combat->IsAttackActive());
			Test->TestEqual(TEXT("No stamina charge during draw"), ASC->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()), 100.0F);
			Next = Now + 0.18; break;
		case 6:
			Test->TestEqual(TEXT("Sword remains in sheath before draw notify"), Weapon->GetRootComponent()->GetAttachSocketName(), FName(TEXT("katana_Targer01Socket")));
			Screenshot(TEXT("Draw-BeforeGrip")); Next = Now + 0.35; break;
		case 7:
			Test->TestTrue(TEXT("Draw montage still playing after grip notify"), Presentation->IsDrawing());
			Test->TestEqual(TEXT("Real draw notify attaches sword to hand"), Weapon->GetRootComponent()->GetAttachSocketName(), Player->GetWeaponHandSocket());
			Test->TestFalse(TEXT("Grip notify does not skip rest of draw animation"), Combat->IsAttackActive());
			Screenshot(TEXT("Draw-AfterGrip")); Next = Now + 1.1; break;
		case 8:
			Test->TestFalse(TEXT("Draw montage ends before queued attack"), Presentation->IsDrawing());
			Test->TestTrue(TEXT("Queued light attack starts after draw"), Combat->IsAttackActive());
			Test->TestEqual(TEXT("Light attack charges exactly once"), ASC->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()), 88.0F);
			Combat->CancelActiveAttack(); Anim->StopAllMontages(0.0F);
			Presentation->PrepareForAction(); Next = Now + Idle + 3.2; break;
		case 9:
			Test->TestTrue(TEXT("Sword auto sheathed again"), Presentation->IsSheathed());
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			Player->RequestHeavyAttack();
			Test->TestTrue(TEXT("Heavy input also queues while drawing"), Presentation->IsDrawing());
			// 首个起播帧就读费用；再生效果可能在下一帧恢复体力。
			Next = Now + 1.20; break;
		case 10:
			if (Presentation->IsDrawing() || !Combat->IsAttackActive()) return false;
			Test->TestTrue(TEXT("Queued heavy attack starts"), Combat->IsAttackActive());
			Test->TestEqual(TEXT("Queued heavy chooses its opener"), Combat->GetActiveAttackName(), FName(TEXT("Attack01_1")));
			Test->TestEqual(TEXT("Heavy cost charged only after draw"), ASC->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()), 80.0F);
			Combat->CancelActiveAttack(); Anim->StopAllMontages(0.0F);
			Presentation->PrepareForAction(); Next = Now + Idle + 3.2; break;
		case 11:
			// 重击的状态结束时间会改变下一次收刀起点，等真实入鞘通知而非猜固定秒数。
			if (!Presentation->IsSheathed()) return false;
			Test->TestTrue(TEXT("Sword sheathes before interrupted draw"), Presentation->IsSheathed());
			Player->RequestLightAttack();
			Test->TestTrue(TEXT("Draw starts before stagger"), Presentation->IsDrawing());
			ASC->AddLooseGameplayTag(SCLGameplayTags::State_Staggered);
			Test->TestFalse(TEXT("Stagger cancels draw"), Presentation->IsDrawing());
			Test->TestEqual(TEXT("Canceled draw restores hand socket"), Weapon->GetRootComponent()->GetAttachSocketName(), Player->GetWeaponHandSocket());
			ASC->RemoveLooseGameplayTag(SCLGameplayTags::State_Staggered);
			Next = Now + 1.6; break;
		case 12:
			Test->TestFalse(TEXT("Canceled draw does not issue delayed attack"), Combat->IsAttackActive());
			return true;
		default:
			Test->AddError(TEXT("Unexpected sheath test step"));
			return true;
		}
		++Step; return false;
	}
private:
	void Screenshot(const TCHAR* Name)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("SheathScreenshots")))
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Automation") / Name, false, false);
	}
	FAutomationTestBase* Test;
	double Started{FPlatformTime::Seconds()}, Next{0.0};
	int32 Step{0};
	bool bWalking{false};
	FVector StartLocation;
	ERootMotionMode::Type OriginalRootMode{ERootMotionMode::NoRootMotionExtraction};
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSCLWeaponSheathTest, "SoulCombatLabCombat.WeaponSheath",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FSCLWeaponSheathTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FSCLWeaponSheathCommand(this));
	return true;
}
#endif
