#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystem/Abilities/SCLHeavyAttackAbility.h"
#include "AbilitySystem/Abilities/SCLLightAttackAbility.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyStates/ANS_SCLWeaponTrace.h"
#include "Animation/AnimNotifyStates/ANS_SCLComboWindow.h"
#include "Characters/SCLCharacterBase.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Characters/Player/SCLPlayerComboComponent.h"
#include "Combat/SCLCombatComponent.h"
#include "Combat/SCLWeapon.h"
#include "Combat/SCLHitTraceComponent.h"
#include "Demo/SCLDemoMap.h"
#include "Demo/SCLDemoSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "Targeting/SCLTargetingComponent.h"

class FSCLGhostPlayerComboCommand final : public IAutomationLatentCommand
{
public:
	explicit FSCLGhostPlayerComboCommand(FAutomationTestBase* InTest) : Test(InTest) {}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if ((Step == 4 || Step == 41) && Player.IsValid() && Target.IsValid())
		{
			auto* Weapon = Player->GetCombatComponent_Implementation()->GetEquippedWeapon();
			if (Weapon && Weapon->GetHitTraceComponent()->IsTraceActive())
			{
				TArray<USceneComponent*> Parts; Weapon->GetComponents(Parts);
				FVector A, B;
				for (auto* Part : Parts) { if (Part->GetFName() == TEXT("TraceStart")) A = Part->GetComponentLocation(); if (Part->GetFName() == TEXT("TraceEnd")) B = Part->GetComponentLocation(); }
				auto* Anim = Player->GetMesh()->GetAnimInstance(); auto* Montage = Anim->GetCurrentActiveMontage();
				if (TraceSamples.Num() < 256) TraceSamples.Add(FString::Printf(TEXT("Trace sample montage=%s t=%.3f player=%s target=%s blade=%s..%s"), *GetNameSafe(Montage), Anim->Montage_GetPosition(Montage), *Player->GetActorLocation().ToString(), *Target->GetActorLocation().ToString(), *A.ToString(), *B.ToString()));
			}
		}
		if (Now - Started > 20.0)
		{
			Test->AddError(TEXT("Ghost player combo timed out"));
			return true;
		}
		if (Now < Next) return false;

		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			// 同一组真实动作断言也要能在 PIE 世界运行，不能用独立游戏代替 PIE 验证。
			if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) World = Context.World();
		}
		USCLDemoSubsystem* const Demo = World != nullptr ? World->GetSubsystem<USCLDemoSubsystem>() : nullptr;
		if (Demo == nullptr || !Demo->IsActive()) return false;
		if (Step == 0)
		{
			static const TCHAR* GhostMontagePaths[] = {
                TEXT("/Game/SoulCombatLab/Characters/Player/Combat/Animations/AM_GSN_Attack01_1_ALL_Root"),
                TEXT("/Game/SoulCombatLab/Characters/Player/Combat/Animations/AM_GSN_Attack01_2_Root"),
                TEXT("/Game/SoulCombatLab/Characters/Player/Combat/Animations/AM_GSN_Attack01_3_Root"),
                TEXT("/Game/SoulCombatLab/Characters/Player/Combat/Animations/AM_GSN_Attack01_4_Root"),
                TEXT("/Game/SoulCombatLab/Characters/Player/Combat/Animations/AM_PlayerHeavyJump04")
            };
			for (const TCHAR* const Path : GhostMontagePaths)
			{
				if (LoadObject<UAnimMontage>(nullptr, Path) == nullptr)
				{
					Test->AddWarning(TEXT("Local GhostSamurai assets are absent; skipping player combo test"));
					return true;
				}
			}
			Demo->RestartRun();
			Player = Demo->GetPlayer();
			if (!Player.IsValid())
			{
				Test->AddError(TEXT("Player fixture missing"));
				return true;
			}
			Player->SetActorLocation(ASCLDemoMap::Checkpoint(ESCLDemoStage::Training));
			TraceAnchor = Player->GetActorLocation();
			Demo->CheckRegion();
			Target = Demo->GetOpponent();
			if (!Target.IsValid())
			{
				Test->AddError(TEXT("Target fixture missing"));
				return true;
			}
			Target->SetActorLocation(Player->GetActorLocation() + FVector{140.0F, 0.0F, 0.0F});
			USCLAbilitySystemComponent* const TargetASC = Target->GetSCLAbilitySystemComponent();
			TargetASC->SetNumericAttributeBase(USCLAttributeSet::GetMaxHealthAttribute(), 1000.0F);
			TargetASC->SetNumericAttributeBase(USCLAttributeSet::GetHealthAttribute(), 1000.0F);
			TargetASC->SetNumericAttributeBase(USCLAttributeSet::GetMaxPoiseAttribute(), 1000.0F);
			TargetASC->SetNumericAttributeBase(USCLAttributeSet::GetPoiseAttribute(), 1000.0F);
			if (!FApp::CanEverRender())
			{
				Player->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("SCLGhostVisualScreenshots")))
			{
				RequestSideScreenshot(TEXT("GhostKatanaIdle"));
			}
			Step = 1;
			Next = Now + 0.3;
			return false;
		}
		if (!Player.IsValid())
		{
			Test->AddError(TEXT("Player fixture destroyed"));
			return true;
		}
		USCLCombatComponent* const Combat = Player->GetCombatComponent_Implementation();
		USCLPlayerComboComponent* const PlayerCombo = Player->GetPlayerComboComponent();
		USCLAbilitySystemComponent* const ASC = Player->GetSCLAbilitySystemComponent();
		UAnimInstance* const Anim = Player->GetMesh()->GetAnimInstance();
		if (Combat == nullptr || PlayerCombo == nullptr || ASC == nullptr || Anim == nullptr)
		{
			Test->AddError(TEXT("Player combat fixture incomplete"));
			return true;
		}
		const auto Activate = [ASC](ESCLPlayerAttackInput Input)
		{
			const bool bActivated = ASC->TryActivateAbilityByClass(
				Input == ESCLPlayerAttackInput::Light
					? USCLLightAttackAbility::StaticClass() : USCLHeavyAttackAbility::StaticClass());
			return bActivated;
		};
		const auto Stamina = [ASC]()
		{
			return ASC->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute());
		};
		if (Step == 1)
		{
			RestoreCamera();
			const USkeletalMesh* const PlayerMesh = Player->GetMesh()->GetSkeletalMeshAsset();
			Test->TestTrue(TEXT("Player uses bundle Manny mesh"), PlayerMesh != nullptr &&
				PlayerMesh->GetPathName().Contains(TEXT("/Game/GhostSamurai_Bundle/Demo/Characters/Mannequins/Meshes/SKM_Manny.")));
			Test->TestTrue(TEXT("Player uses bundle Manny animation Blueprint"),
				Anim->GetClass()->GetPathName().Contains(TEXT("/Game/SoulCombatLab/Characters/Player/Combat/ABP_SCLPlayerCombat.")));
			TArray<UStaticMeshComponent*> StaticMeshes;
			Player->GetComponents<UStaticMeshComponent>(StaticMeshes);
			const UStaticMeshComponent* Scabbard = nullptr;
			for (const UStaticMeshComponent* const Component : StaticMeshes)
			{
				if (Component != nullptr && Component->GetFName() == FName(TEXT("ScabbardMesh")))
				{
					Scabbard = Component;
					break;
				}
			}
			Test->TestTrue(TEXT("Player carries the bundle scabbard"), Scabbard != nullptr &&
				Scabbard->GetStaticMesh() != nullptr && Scabbard->GetStaticMesh()->GetPathName().Contains(TEXT("SM_Scabbard01")));
			if (Scabbard != nullptr)
			{
				Test->TestEqual(TEXT("Scabbard uses authored socket"), Scabbard->GetAttachSocketName(), FName(TEXT("Scabbard_Target01Socket")));
			}
			const ASCLWeapon* const Katana = Combat->GetEquippedWeapon();
			Test->TestTrue(TEXT("Katana is equipped"), Katana != nullptr);
			if (Katana != nullptr && Katana->GetRootComponent() != nullptr)
			{
				Test->TestEqual(TEXT("Katana uses authored hand socket"), Katana->GetRootComponent()->GetAttachSocketName(), FName(TEXT("weapon_rSocket")));
			}
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			// 拒绝请求不能改变招式或体力，之后的新请求仍重新检查成本。
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 5.0F);
			Test->TestFalse(TEXT("5 stamina cannot activate the 6-cost opener"), Activate(ESCLPlayerAttackInput::Light));
			Test->TestTrue(TEXT("Direct combat entry cannot bypass player stamina check"),
				Combat->RequestAttack(ESCLPlayerAttackInput::Light) == ESCLAttackRequestResult::Rejected);
			Test->TestEqual(TEXT("Rejected opener keeps stamina"), Stamina(), 5.0F);
			// 边界体力：提交成本不能误查已切换节点的下一招成本。
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 6.0F);
			Activate(ESCLPlayerAttackInput::Light);
			Test->TestEqual(TEXT("Exactly 6 stamina starts the 6-cost light opener"), Combat->GetActiveAttackName(), FName(TEXT("Attack01_1")));
			Test->TestEqual(TEXT("Exact-cost opener spends stamina down to zero"), Stamina(), 0.0F);
			Combat->CancelActiveAttack();
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 12.0F);
			Activate(ESCLPlayerAttackInput::Heavy);
			Test->TestEqual(TEXT("Exactly 12 stamina starts the 12-cost heavy opener"), Combat->GetActiveAttackName(), FName(TEXT("JumpAttack04")));
			Test->TestEqual(TEXT("Exact-cost heavy spends stamina down to zero"), Stamina(), 0.0F);
			Combat->CancelActiveAttack();
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			// 验证真实 Montage 和缓存的结果约定，防止“已接收”又被当成“已执行”。
			Test->TestTrue(TEXT("Direct opener returns Executed"),
				Combat->RequestAttack(ESCLPlayerAttackInput::Light) == ESCLAttackRequestResult::Executed);
			Test->TestEqual(TEXT("Direct opener also pays exactly once"), Stamina(), 94.0F);
			Test->TestTrue(TEXT("Input before notify returns Buffered"),
				Combat->RequestAttack(ESCLPlayerAttackInput::Heavy) == ESCLAttackRequestResult::Buffered);
			Test->TestTrue(TEXT("Occupied buffer returns Rejected"),
				Combat->RequestAttack(ESCLPlayerAttackInput::Light) == ESCLAttackRequestResult::Rejected);
			Test->TestEqual(TEXT("Buffer leaves the current montage unchanged"), Combat->GetActiveAttackName(), FName(TEXT("Attack01_1")));
			Test->TestEqual(TEXT("Buffered and rejected requests do not pay again"), Stamina(), 94.0F);
			Combat->CancelActiveAttack();

			// 体力可能在缓存等待时被其他动作消耗；Notify 必须重查，失败不能破坏当前招式。
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			Activate(ESCLPlayerAttackInput::Light);
			Activate(ESCLPlayerAttackInput::Heavy);
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 11.0F);
			Combat->OpenComboWindow();
			Test->TestEqual(TEXT("Insufficient buffered step preserves current attack"), Combat->GetActiveAttackName(), FName(TEXT("Attack01_1")));
			Test->TestEqual(TEXT("Failed buffered step does not pay"), Stamina(), 11.0F);
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			Combat->OpenComboWindow();
			Test->TestEqual(TEXT("Failed buffer is consumed once, not retried"), Combat->GetActiveAttackName(), FName(TEXT("Attack01_1")));
			Combat->CancelActiveAttack();

			// 窗口内的即时输入与 Notify 接招同价，Ability 不能再提交一次费用。
			Activate(ESCLPlayerAttackInput::Light);
			Combat->OpenComboWindow();
			Activate(ESCLPlayerAttackInput::Heavy);
			Test->TestEqual(TEXT("Input inside open window starts heavy immediately"), Combat->GetActiveAttackName(), FName(TEXT("JumpAttack04")));
			Test->TestEqual(TEXT("Immediate transition pays once through shared start"), Stamina(), 82.0F);
			Combat->CancelActiveAttack();

			// 使用临时招式表测试边界，不能改写项目 DataAsset 或其他实例共用的默认值。
			USCLPlayerMovesetData* const OriginalMoveset = PlayerCombo->PlayerMoveset;
			USCLPlayerMovesetData* const TestMoveset = DuplicateObject<USCLPlayerMovesetData>(OriginalMoveset, PlayerCombo);
			PlayerCombo->PlayerMoveset = TestMoveset;
			FSCLPlayerAttackStep& TestOpener = TestMoveset->Steps[TestMoveset->LightOpenerIndex];
			TestOpener.StaminaCost = 0.0F;
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 0.0F);
			Test->TestTrue(TEXT("Zero-cost node executes without a cost snapshot"),
				Combat->RequestAttack(ESCLPlayerAttackInput::Light) == ESCLAttackRequestResult::Executed);
			Test->TestEqual(TEXT("Zero-cost node keeps zero stamina"), Stamina(), 0.0F);
			Combat->CancelActiveAttack();
			TestOpener.StaminaCost = -1.0F;
			Test->TestTrue(TEXT("Negative cost rejects instead of granting stamina"),
				Combat->RequestAttack(ESCLPlayerAttackInput::Light) == ESCLAttackRequestResult::Rejected);
			TestOpener.StaminaCost = 12.0F;
			TestOpener.Montage = nullptr;
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			Test->TestTrue(TEXT("Missing montage rejects without paying"),
				Combat->RequestAttack(ESCLPlayerAttackInput::Light) == ESCLAttackRequestResult::Rejected);
			Test->TestEqual(TEXT("Missing montage preserves stamina"), Stamina(), 100.0F);
			Test->TestFalse(TEXT("Rejected montage does not start combo state"), PlayerCombo->IsComboActive());
			// 有 Montage 指针也不等于可播放。空 Montage 预检失败，不动当前根运动模式。
			const auto ModeBeforeFailure = Anim->RootMotionMode;
			TestOpener.Montage = NewObject<UAnimMontage>(TestMoveset);
			Test->TestTrue(TEXT("Unplayable montage rejects without paying"),
				Combat->RequestAttack(ESCLPlayerAttackInput::Light) == ESCLAttackRequestResult::Rejected);
			Test->TestEqual(TEXT("Unplayable montage preserves stamina"), Stamina(), 100.0F);
			Test->TestTrue(TEXT("Unplayable montage preserves root motion mode"), Anim->RootMotionMode == ModeBeforeFailure);
			PlayerCombo->PlayerMoveset = OriginalMoveset;
			Combat->CancelActiveAttack();
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			Test->TestTrue(TEXT("Short press activates light GAS ability"), Activate(ESCLPlayerAttackInput::Light));
			Test->TestEqual(TEXT("Short press starts Attack01_1"), Combat->GetActiveAttackName(), FName(TEXT("Attack01_1")));
			Test->TestEqual(TEXT("Light opener spends 6 stamina"), Stamina(), 94.0F);
			Step = 10;
			Next = Now + 0.1;
			return false;
		}
		if (Step == 10)
		{
			Test->TestTrue(TEXT("Second short press buffers light attack"), Activate(ESCLPlayerAttackInput::Light));
			Step = 11;
			Next = Now + 0.55;
			return false;
		}
		if (Step == 11)
		{
			if (Combat->GetActiveAttackName() != TEXT("Attack01_2") && IsBeforeComboWindow()) { Next = Now; return false; }
			Test->TestEqual(TEXT("First combo notify starts Attack01_2"), Combat->GetActiveAttackName(), FName(TEXT("Attack01_2")));
			Test->TestTrue(TEXT("Third short press buffers light attack"), Activate(ESCLPlayerAttackInput::Light));
			Step = 12;
			Next = Now + 0.55;
			return false;
		}
		if (Step == 12)
		{
			if (Combat->GetActiveAttackName() != TEXT("Attack01_3") && IsBeforeComboWindow()) { Next = Now; return false; }
			Test->TestEqual(TEXT("Second combo notify starts Attack01_3"), Combat->GetActiveAttackName(), FName(TEXT("Attack01_3")));
			for (int32 NextLight = 4; NextLight <= 4; ++NextLight)
			{
				Test->TestTrue(FString::Printf(TEXT("Light step %d input accepted"), NextLight), Activate(ESCLPlayerAttackInput::Light));
				Combat->OpenComboWindow();
				Test->TestEqual(FString::Printf(TEXT("Attack01 step %d plays"), NextLight),
					Combat->GetActiveAttackName(), FName(*FString::Printf(TEXT("Attack01_%d"), NextLight)));
			}
			Test->TestEqual(TEXT("Complete four step light chain costs 31 stamina"), Stamina(), 69.0F);
			Combat->CancelActiveAttack();
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			Test->TestTrue(TEXT("Mixed combo starts with light"), Activate(ESCLPlayerAttackInput::Light));
			Step = 2;
			Next = Now + 0.1;
			return false;
		}
		if (Step == 2)
		{
			Test->TestTrue(TEXT("Hold input buffers heavy attack"), Activate(ESCLPlayerAttackInput::Heavy));
			Test->TestEqual(TEXT("Buffered heavy does not spend stamina yet"), Stamina(), 94.0F);
			Combat->OpenComboWindow();
			Test->TestEqual(TEXT("Light can branch to Attack01 opener"), Combat->GetActiveAttackName(), FName(TEXT("JumpAttack04")));
			Test->TestEqual(TEXT("Heavy transition spends stamina once"), Stamina(), 82.0F);
			Step = 3;
			Next = Now + 0.1;
			return false;
		}
		if (Step == 3)
		{
			// 单招重击是终止节点，不允许在演出中重复排重击或跳回轻击。
            Test->TestTrue(TEXT("Heavy has no light successor"), Combat->RequestAttack(ESCLPlayerAttackInput::Light) == ESCLAttackRequestResult::Rejected);
            Combat->CancelActiveAttack();
            Test->TestTrue(TEXT("After heavy ends light restarts from opener"), Activate(ESCLPlayerAttackInput::Light));
			Test->TestEqual(TEXT("New light request restarts Attack01 opener"), Combat->GetActiveAttackName(), FName(TEXT("Attack01_1")));
			Test->TestEqual(TEXT("Light transition spends stamina once"), Stamina(), 76.0F);
			Combat->CancelActiveAttack();
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			Test->TestTrue(TEXT("Hold can open directly with heavy"), Activate(ESCLPlayerAttackInput::Heavy));
			Test->TestEqual(TEXT("Long press starts Attack01_1"), Combat->GetActiveAttackName(), FName(TEXT("JumpAttack04")));
			Test->TestEqual(TEXT("Heavy opener spends 12 stamina"), Stamina(), 88.0F);
            Test->TestTrue(TEXT("Repeated Heavy is rejected during single heavy montage"),
                Combat->RequestAttack(ESCLPlayerAttackInput::Heavy) == ESCLAttackRequestResult::Rejected);
            Combat->OpenComboWindow();
            Test->TestEqual(TEXT("A window cannot advance the terminal heavy node"), Combat->GetActiveAttackName(), FName(TEXT("JumpAttack04")));
            Test->TestEqual(TEXT("Single heavy costs 12 stamina once"), Stamina(), 88.0F);
			Combat->CancelActiveAttack();
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			// Scripted chain steps can move the player via root motion. Put the dummy
			// back in front before testing an actual montage-driven weapon hit.
			Test->AddInfo(FString::Printf(TEXT("Natural trace setup separation before reset: %.1f"),
				FVector::Dist2D(Player->GetActorLocation(), Target->GetActorLocation())));
			ResetDamageFixture(true);
			StartHealth = Target->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			Test->TestTrue(TEXT("Natural combo starts with light"), Activate(ESCLPlayerAttackInput::Light));
			Test->TestTrue(TEXT("Natural combo accepts buffered heavy"), Activate(ESCLPlayerAttackInput::Heavy));
			Step = 4;
			Next = Now + 0.7;
			return false;
		}
		if (Step == 4)
		{
			if (Combat->GetActiveAttackName() != TEXT("JumpAttack04") && IsBeforeComboWindow()) { Next = Now; return false; }
			Test->TestEqual(TEXT("Montage notify opens combo window"), Combat->GetActiveAttackName(), FName(TEXT("JumpAttack04")));
			Test->TestEqual(TEXT("Notify transition spends stamina once"), Stamina(), 82.0F);
			Step = 41;
			Next = Now;
			return false;
		}
		if (Step == 41)
		{
			// 重击有多个命中段；0.7 秒时只说明接招成功，不能提前取消并断言整招没有伤害。
			// 按实际 Montage 时间等待最后一个武器检测窗口结束，避免墙钟与动画时间脱节。
			if (UAnimMontage* Montage = Anim->GetCurrentActiveMontage())
			{
				float LastTraceEnd = 0.0F;
				for (const FAnimNotifyEvent& Event : Montage->Notifies)
					if (Cast<UANS_SCLWeaponTrace>(Event.NotifyStateClass))
						LastTraceEnd = FMath::Max(LastTraceEnd, Event.GetTime() + Event.GetDuration());
				if (Anim->Montage_GetPosition(Montage) < LastTraceEnd + 0.05F) { Next = Now; return false; }
			}
			const float TargetHealth = Target.IsValid()
				? Target->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute())
				: StartHealth;
			// 轻击只有 20 点，不能拿轻击命中冒充重击分支通过；至少要包含重击的 40 点。
			const bool bHeavyHit = StartHealth - TargetHealth >= 40.0F;
			Test->TestTrue(TEXT("Native light-to-heavy trace includes heavy damage"), bHeavyHit);
			if (!bHeavyHit) for (const FString& Sample : TraceSamples) Test->AddInfo(Sample);
			Combat->CancelActiveAttack();
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			Test->TestTrue(TEXT("Cancellation test starts light"), Activate(ESCLPlayerAttackInput::Light));
			Test->TestTrue(TEXT("Cancellation test buffers heavy"), Activate(ESCLPlayerAttackInput::Heavy));
			Combat->CancelActiveAttack();
			Step = 5;
			Next = Now + 0.7;
			return false;
		}
		if (Step == 5)
		{
			Test->TestFalse(TEXT("Canceled buffer cannot restart combat"), Combat->IsAttackActive());
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			ResetDamageFixture(false);
			HeavyStartHealth = Target->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			Test->TestTrue(TEXT("Heavy damage test starts heavy"), Activate(ESCLPlayerAttackInput::Heavy));
			const bool bTakeScreenshot = FParse::Param(FCommandLine::Get(), TEXT("SCLGhostVisualScreenshots"));
			Step = bTakeScreenshot ? 60 : 6;
			Next = Now + (bTakeScreenshot ? 0.80 : 1.45);
			return false;
		}
		if (Step == 60)
		{
			RequestSideScreenshot(TEXT("GhostKatanaHeavy"));
			Step = 6;
			Next = Now + 0.65;
			return false;
		}
		if (Step == 6)
		{
			RestoreCamera();
			const float HeavyDamage = HeavyStartHealth -
				Target->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			Test->TestTrue(TEXT("Native heavy trace damages target"), HeavyDamage > 0.0F);
			Combat->CancelActiveAttack();
			// 模拟根运动 Notify 已登记、骨骼尚未求值时取消：待采样回调不能再结算伤害。
			ResetDamageFixture(false);
			const auto OriginalBoneTick = Player->GetMesh()->VisibilityBasedAnimTickOption;
			Test->TestTrue(TEXT("Pending trace cancellation starts light"), Activate(ESCLPlayerAttackInput::Light));
			Test->TestTrue(TEXT("Pending trace window begins"), Combat->BeginWeaponTrace());
			Combat->TickWeaponTrace();
			Combat->CancelActiveAttack();
			Test->TestFalse(TEXT("Cancellation closes pending weapon trace"), Combat->GetEquippedWeapon()->GetHitTraceComponent()->IsTraceActive());
			Test->TestTrue(TEXT("Cancellation restores bone refresh policy"), Player->GetMesh()->VisibilityBasedAnimTickOption == OriginalBoneTick);
			StartHealth = Target->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			Step = 7;
			Next = Now + 0.15;
			return false;
		}
		if (Step == 7)
		{
			Test->TestEqual(TEXT("Canceled pending bone callback cannot deal late damage"),
				Target->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()), StartHealth);
			Test->TestFalse(TEXT("Old notify cannot reopen canceled trace"), Combat->GetEquippedWeapon()->GetHitTraceComponent()->IsTraceActive());
			return true;
		}
		return false;
	}

private:
	// 首次特效编译会停住游戏线程：墙钟过了 0.55 秒不代表动画到达连招窗口。
	// 只等待实际窗口起点（加一小段通知分发余量）；窗口已过仍未切招会照常断言失败。
	bool IsBeforeComboWindow() const
	{
		const auto* Anim = Player->GetMesh()->GetAnimInstance();
		const auto* Montage = Anim->GetCurrentActiveMontage();
		if (!Montage) return false;
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
			if (Cast<UANS_SCLComboWindow>(Event.NotifyStateClass))
				return Anim->Montage_GetPosition(Montage) < Event.GetTriggerTime() + 0.05F;
		return false;
	}
	// 费用测试会在同一帧多次强制切招，并在跳跃重击中途取消。
	// 自然命中测试必须从同一地面姿态开始，不能把残留高度、速度或混出姿势当成命中结果。
	void ResetDamageFixture(const bool bLockTarget)
	{
		Test->AddInfo(FString::Printf(TEXT("Trace fixture before reset: player=%s target=%s falling=%d"),
			*Player->GetActorLocation().ToString(), *Target->GetActorLocation().ToString(),
			Player->GetCharacterMovement()->IsFalling()));
		Player->GetCombatComponent_Implementation()->CancelActiveAttack();
		Player->GetTargetingComponent()->ClearTarget();
		Player->GetMesh()->GetAnimInstance()->StopAllMontages(0.0F);
		Player->GetCharacterMovement()->StopMovementImmediately();
		Player->ClearMovementInput(); Player->ConsumeMovementInputVector();
		Player->SetActorLocation(TraceAnchor);
		Player->SetActorRotation(FRotator::ZeroRotator);
		Player->GetController()->SetControlRotation(FRotator::ZeroRotator);
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Target->GetCharacterMovement()->StopMovementImmediately();
		Target->SetActorLocation(TraceAnchor + FVector{140.0F, 0.0F, 0.0F});
		// 自然连招的根位移会改变双方位置；锁定后实际朝向/校正链路才负责对齐当前目标。
		// 后面的单独重击继续在未锁定场景检查，不能把每次未锁定挥刀都当成必中。
		if (bLockTarget)
		{
			Player->GetTargetingComponent()->ToggleLock();
			Test->TestTrue(TEXT("Natural damage fixture locks the actual dummy"),
				Player->GetTargetingComponent()->GetCurrentTarget() == Target.Get());
		}
	}
	FVector TraceAnchor{ForceInit};
	// 失败时才输出刀刃位置证据；正常通过不刷屏，也不改生产命中规则。
	TArray<FString> TraceSamples;
	void RequestSideScreenshot(const TCHAR* const Name)
	{
		if (!Player.IsValid()) return;
		if (AController* const Controller = Player->GetController())
		{
			SavedControlRotation = Controller->GetControlRotation();
			Controller->SetControlRotation(FRotator{-10.0F, SavedControlRotation.Yaw + 90.0F, 0.0F});
			bCameraAdjusted = true;
		}
		if (USpringArmComponent* const Boom = Player->FindComponentByClass<USpringArmComponent>())
		{
			SavedCameraArmLength = Boom->TargetArmLength;
			Boom->TargetArmLength = 240.0F;
			bCameraAdjusted = true;
		}
		FScreenshotRequest::RequestScreenshot(
			FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/%s.png"), Name), true, false);
	}

	void RestoreCamera()
	{
		if (!bCameraAdjusted || !Player.IsValid()) return;
		if (AController* const Controller = Player->GetController()) Controller->SetControlRotation(SavedControlRotation);
		if (USpringArmComponent* const Boom = Player->FindComponentByClass<USpringArmComponent>())
		{
			Boom->TargetArmLength = SavedCameraArmLength;
		}
		bCameraAdjusted = false;
	}

	FAutomationTestBase* Test;
	TWeakObjectPtr<ASCLPlayerCharacter> Player;
	TWeakObjectPtr<ASCLCharacterBase> Target;
	double Started{FPlatformTime::Seconds()};
	double Next{Started + 1.0};
	float StartHealth{0.0F};
	float HeavyStartHealth{0.0F};
	FRotator SavedControlRotation{ForceInit};
	float SavedCameraArmLength{400.0F};
	bool bCameraAdjusted{false};
	int32 Step{0};
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSCLGhostPlayerComboTest, "SoulCombatLabCombat.GhostPlayerCombo",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSCLGhostPlayerComboTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FSCLGhostPlayerComboCommand(this));
	return true;
}

#endif
