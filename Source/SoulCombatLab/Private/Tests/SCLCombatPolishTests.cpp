#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "AbilitySystem/Abilities/SCLExecutionAbility.h"
#include "AbilitySystem/Abilities/SCLBlockAbility.h"
#include "AbilitySystem/Abilities/SCLParryAbility.h"
#include "AbilitySystem/Abilities/SCLLightAttackAbility.h"
#include "Animation/AnimInstance.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Characters/SCLEnemyCharacter.h"
#include "Combat/SCLCombatComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Demo/SCLDemoSubsystem.h"
#include "Demo/SCLDemoMap.h"
#include "Demo/SCLDemoWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "UnrealClient.h"
#include "Targeting/SCLTargetingComponent.h"

class FSCLCombatPolishCommand final : public IAutomationLatentCommand
{
public:
	explicit FSCLCombatPolishCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		// 总超时看墙钟；游戏定时器、Montage 和验收等待使用同一个世界时间。
		if (FPlatformTime::Seconds() - Started > 90.0) { Test->AddError(TEXT("Combat polish timed out")); return true; }
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) World = Context.World();
		if (World == nullptr) return false;
		USCLDemoSubsystem* const Demo = World->GetSubsystem<USCLDemoSubsystem>();
		if (Demo == nullptr || !Demo->IsActive()) return false;
		const double Now = World->GetTimeSeconds();
		if (Now < Next) return false;
		Next = Now + 0.10;
		if (Step == 0)
		{
			if (!bTrainingPrepared)
			{
				PrepareTraining(*Demo);
				Player = Demo->GetPlayer();
				// ACharacter defaults to AlwaysTickPose, which leaves weapon socket bones stale in NullRHI.
				// Rendered runs retain the production policy; only this headless fixture forces evaluation.
				if (!FApp::CanEverRender()) Player->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
				Demo->GetOpponent()->SetActorLocation(Player->GetActorLocation() + Player->GetActorForwardVector() * 140.0F);
				bTrainingPrepared = true;
				// Let mesh/bone transforms settle after the fixture moves between spatial regions.
				Next = Now + 0.3;
				return false;
			}
			// 按住与松开的技能生命周期：状态、低速移动和恢复必须配对。
			const float OriginalWalkSpeed = Player->GetCharacterMovement()->MaxWalkSpeed;
			FreeMoveSpeed = OriginalWalkSpeed;

			// 测试真实玩家蓝图已经授予配置子类，防止只改查找函数却忘记接入资产。
			USCLAbilitySystemComponent* ConfigASC = Player->GetSCLAbilitySystemComponent();
			for (UClass* Base : {USCLBlockAbility::StaticClass(), USCLParryAbility::StaticClass(), USCLExecutionAbility::StaticClass()})
			{
				const FGameplayAbilitySpec* Configured = ConfigASC->FindAbilitySpecByBaseClass(Base);
				Test->TestTrue(TEXT("Player grants configured Blueprint ability instead of native parent"),
					Configured && Configured->Ability && Configured->Ability->GetClass() != Base && Configured->Ability->IsA(Base));
				Test->TestTrue(TEXT("Startup ability query recognizes configured subclass"), Player->HasStartupAbility(Base));
				Test->TestNull(TEXT("Native parent is not granted alongside configuration Blueprint"), ConfigASC->FindAbilitySpecFromClass(Base));
			}
			Player->StartBlock();
			Test->TestTrue(TEXT("Block hold grants blocking state"),
				Player->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Blocking));
			Test->TestTrue(TEXT("Block hold slows walking"),
				Player->GetCharacterMovement()->MaxWalkSpeed < OriginalWalkSpeed);
			CheckHint(*World, TEXT("格挡中"));
			const float GuardStaminaBefore = ConfigASC->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute());
			const float GuardHealthBefore = ConfigASC->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			UGameplayStatics::ApplyPointDamage(Player.Get(), 20.0F, -Player->GetActorForwardVector(),
				FHitResult{}, nullptr, Demo->GetOpponent(), UDamageType::StaticClass());
			Test->TestTrue(TEXT("Player guard spends 16 stamina for a 20-damage hit"), FMath::IsNearlyEqual(
				ConfigASC->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()), GuardStaminaBefore - 16.0F));
			Test->TestEqual(TEXT("Guard still prevents health damage"),
				ConfigASC->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()), GuardHealthBefore);
			Player->StopBlock();
			ConfigASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 10.0F);
			CheckHint(*World, TEXT("体力偏低"));
			ConfigASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			Test->TestFalse(TEXT("Block release clears blocking state"),
				Player->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Blocking));
			Test->TestEqual(TEXT("Block release restores walking speed"),
				Player->GetCharacterMovement()->MaxWalkSpeed, OriginalWalkSpeed);
			Player->GetMesh()->GetAnimInstance()->StopAllMontages(0.0F);
			StartPosition = Player->GetActorLocation();
			RootStart = Player->GetMesh()->GetBoneLocation(TEXT("root"));
			Player->GetCharacterMovement()->Velocity = FVector{400.0F, 0.0F, 0.0F};
			SavedMode = Player->GetMesh()->GetAnimInstance()->RootMotionMode;
			Test->TestTrue(TEXT("Real player light attack starts"),
				Player->GetSCLAbilitySystemComponent()->TryActivateAbilityByClass(USCLLightAttackAbility::StaticClass()));
			Test->TestTrue(TEXT("Attack immediately stops residual velocity"), Player->GetVelocity().Size2D() < 1.0F);
			AttackStarted = Now;
			Step = 1;
			return false;
		}
		if (!Player.IsValid()) { Test->AddError(TEXT("Test player missing")); return true; }
		if (Step == 1)
		{
			MaxDrift = FMath::Max(MaxDrift, static_cast<float>(FVector::Dist2D(StartPosition, Player->GetActorLocation())));
			MaxRootDrift = FMath::Max(MaxRootDrift, static_cast<float>(FVector::Dist2D(RootStart, Player->GetMesh()->GetBoneLocation(TEXT("root")))));
			if (Now - AttackStarted < 1.3) return false;
			// 玩家攻击现在由 Montage 根运动推动胶囊；旧“原地不动”断言已不适用。
			Test->TestTrue(TEXT("Attack root motion moves capsule within one short lunge"), MaxDrift > 3.0F && MaxDrift < 150.0F);
			// 基础 Blend Space 正常输出后，骨骼根点还包含姿势/混合偏移，不能拿它的世界位移等同于胶囊位移。
			Test->TestTrue(TEXT("Attack animation root changes during lunge"), MaxRootDrift > 3.0F);
			Test->AddInfo(FString::Printf(TEXT("Attack capsule drift %.3f cm, root drift %.3f cm"), MaxDrift, MaxRootDrift));
			Player->GetCombatComponent_Implementation()->CancelActiveAttack();
			Test->TestTrue(TEXT("Cancelled attack restores root motion policy"), Player->GetMesh()->GetAnimInstance()->RootMotionMode == SavedMode);
			CheckWalls(*World);
			PrepareSword(*Demo);
			// Free camera looks at the enemy while the pawn still faces away from it.
			Player->SetActorRotation(FRotator{0.0F, 150.0F, 0.0F});
			Player->GetController()->SetControlRotation(FRotator::ZeroRotator);
			Player->StartBlock();
            Test->TestTrue(TEXT("Guard active before parry conversion"), Player->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Blocking));
            Test->TestTrue(TEXT("Parry converts an active guard"), Player->GetSCLAbilitySystemComponent()->TryActivateAbilitiesByTag(FGameplayTagContainer{SCLGameplayTags::Ability_Parry}));
			Test->TestTrue(TEXT("Unlocked parry faces the camera's horizontal aim"), Player->GetActorForwardVector().X > 0.99F);
			Test->TestFalse(TEXT("Parry conversion removes the old guard state"), Player->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Blocking));
            AttackStarted = Now;
			Step = 2; Next = Now; return false;
		}
		USCLAbilitySystemComponent* const PlayerASC = Player->GetSCLAbilitySystemComponent();
		if (Step == 2)
		{
			if (!Sword.IsValid()) { Test->AddError(TEXT("Sword missing")); return true; }
			// 不靠墙钟延时猜有效帧：运行中直接等待动画通知授予判定标签。
			if (!PlayerASC->HasMatchingGameplayTag(SCLGameplayTags::State_Parrying))
			{
				if (Now - AttackStarted > 1.0) { Test->AddError(TEXT("Parry notify window was never observed")); return true; }
				Next = Now;
				return false;
			}
            // 以 Montage 时间采样，不把墙钟时间当动画时间；两者在渲染启动时可能不一致。
            // 0.35 秒已超过旧窗口结束 0.34，新的 0.47 秒窗口应仍有判定。
            UAnimInstance* ParryAnim = Player->GetMesh()->GetAnimInstance();
            if (ParryAnim->Montage_GetPosition(ParryAnim->GetCurrentActiveMontage()) < 0.35F) return false;
            Test->TestTrue(TEXT("Expanded parry window remains active after old 0.34 second end"), PlayerASC->HasMatchingGameplayTag(SCLGameplayTags::State_Parrying));
			const float SwordHealthBefore = Sword->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			const float PlayerHealthBefore = PlayerASC->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			const float Damage = UGameplayStatics::ApplyPointDamage(Player.Get(), 20.0F, -Player->GetActorForwardVector(),
				FHitResult{}, Sword->GetController(), Sword.Get(), UDamageType::StaticClass());
			Test->TestEqual(TEXT("Sword hit is parried"), Damage, 0.0F);
			Test->TestEqual(TEXT("Successful parry deals 20 health damage to attacker"),
				Sword->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()), SwordHealthBefore - 20.0F);
			Test->TestEqual(TEXT("Successful parry leaves defender health intact"),
				PlayerASC->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()), PlayerHealthBefore);
			Test->TestTrue(TEXT("Parried Sword is executable"), Sword->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Executable));
			CheckHint(*World, TEXT("弹反成功！靠近并面向敌人，按 E 处决"));
			// 背后受击可能打断弹反动作，所以先验证正面有效帧，再测各来向。
			CheckParryDirections();
			Test->TestEqual(TEXT("Repeated hits in one parry action do not repeat counter damage"),
				Sword->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()), SwordHealthBefore - 20.0F);
			// 弹反动作拥有完整 Montage；处决会取消它并接管朝向与根运动。
			const FVector BeforeExecution = Player->GetActorLocation();
			Test->TestTrue(TEXT("Execution starts during own parry window with cooked animation"),
				PlayerASC->TryActivateAbilitiesByTag(FGameplayTagContainer{SCLGameplayTags::Ability_Execution}));
			Test->TestTrue(TEXT("Execution start does not teleport capsule"),
				FVector::Dist2D(BeforeExecution, Player->GetActorLocation()) < 3.0F);
			CheckHint(*World, TEXT("处决中"));
			AttackStarted = Now;
			// 新处决以 Montage 的 1.024 秒命中通知结算，而非旧的 0.45 秒定时器。
			Step = 3; Next = Now + 1.35; return false;
		}
		if (Step == 3)
		{
			Test->TestTrue(TEXT("Execution root motion ends near victim"), Sword.IsValid() &&
				FVector::Dist2D(Player->GetActorLocation(), Sword->GetActorLocation()) < 180.0F);
			Test->TestTrue(TEXT("Execution impact kills the Sword through actual damage"),
				Sword.IsValid() && Sword->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Dead));
			Step = 15; return false;
		}
		if (Step == 15)
		{
			const FGameplayAbilitySpec* const Execution = PlayerASC->FindAbilitySpecByBaseClass(USCLExecutionAbility::StaticClass());
			if (Execution != nullptr && Execution->IsActive() && Now - AttackStarted < 5.0) return false;
			Test->TestTrue(TEXT("Execution completes within five seconds"), Execution != nullptr && !Execution->IsActive());
			Test->TestTrue(TEXT("Free execution after parry restores movement facing"), Player->GetCharacterMovement()->bOrientRotationToMovement);
			Test->TestTrue(TEXT("Free execution restores animation root policy"), Player->GetMesh()->GetAnimInstance()->RootMotionMode == SavedMode);
			CheckFreeCamera();
			PrepareSword(*Demo);
			Sword->GetSCLAbilitySystemComponent()->SetNumericAttributeBase(USCLAttributeSet::GetHealthAttribute(), 10.0F);
			Test->TestTrue(TEXT("Fresh parry action starts for lethal counter"),
				Player->GetSCLAbilitySystemComponent()->TryActivateAbilitiesByTag(FGameplayTagContainer{SCLGameplayTags::Ability_Parry}));
			Step = 16; AttackStarted = Now; Next = Now; return false;
		}
		if (Step == 16)
		{
			if (!PlayerASC->HasMatchingGameplayTag(SCLGameplayTags::State_Parrying))
			{
				if (Now - AttackStarted > 1.0) { Test->AddError(TEXT("Lethal parry window missing")); return true; }
				Next = Now; return false;
			}
			UGameplayStatics::ApplyPointDamage(Player.Get(), 20.0F, -Player->GetActorForwardVector(),
				FHitResult{}, Sword->GetController(), Sword.Get(), UDamageType::StaticClass());
			Test->TestTrue(TEXT("Counter damage kills low-health attacker through normal death handling"),
				Sword->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Dead));
			PrepareSword(*Demo);
			const float DamageValues[]{20.0F, 22.0F, 20.0F};
			const float PoiseValues[]{10.0F, 12.0F, 10.0F};
			for (int32 Hit = 0; Hit < 3; ++Hit)
			{
				UGameplayStatics::ApplyPointDamage(Sword.Get(), DamageValues[Hit], Player->GetActorForwardVector(),
					FHitResult{}, Player->GetController(), Player.Get(), UDamageType::StaticClass());
				Sword->GetSCLAbilitySystemComponent()->ApplyPoiseDamageToSelf(PoiseValues[Hit], Player.Get());
			}
			Test->TestTrue(TEXT("Sword poise breaks before health reaches zero"),
				Sword->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()) > 0.0F);
			Test->TestTrue(TEXT("Poise break opens execution"), Sword->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Executable));
			Step = 4; Next = Now + 2.0; return false;
		}
		if (Step == 4)
		{
			CheckHint(*World, TEXT("敌人失衡！靠近并面向敌人，按 E 处决"));
			if (FApp::CanEverRender())
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/CombatHints-Execution.png"), true, false);
			Test->TestTrue(TEXT("Victim stays staggered beyond old one-second recovery"),
				Sword->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Staggered));
			Test->TestFalse(TEXT("Executable victim cannot restart attacking"),
				Sword->GetSCLAbilitySystemComponent()->TryActivateAbilityByClass(USCLLightAttackAbility::StaticClass()));
			if (FParse::Param(FCommandLine::Get(), TEXT("SCLCombatScreenshots")))
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/CombatPoise.png"), true, false);
			Step = 5; Next = Now + 2.3; return false;
		}
		if (Step == 5)
		{
			Test->TestFalse(TEXT("Execution window expires normally"), Sword->GetSCLAbilitySystemComponent()->HasMatchingGameplayTag(SCLGameplayTags::State_Executable));
			Test->TestEqual(TEXT("Poise replenishes after opportunity ends"),
				Sword->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetPoiseAttribute()), 30.0F);
			EnterSword(*Demo);
			Player = Demo->GetPlayer();
			Step = 6; AttackStarted = Now; return false;
		}
		if (Step == 7)
		{
			EnterRegion(*Demo, ESCLDemoStage::Heavy); Player = Demo->GetPlayer();
			Demo->GetOpponent()->GetSCLAbilitySystemComponent()->SetNumericAttributeBase(USCLAttributeSet::GetHealthAttribute(), 0.0F);
			Step = 8; Next = Now + 0.3; return false;
		}
		if (Step == 8)
		{
			EnterRegion(*Demo, ESCLDemoStage::MiniArena);
			for (TActorIterator<ASCLEnemyCharacter> It(World); It; ++It)
			{
				StopEnemy(**It);
				if (It->IsA<ASCLHeavyEnemyCharacter>()) Heavy = *It;
				else Sword = *It;
			}
			if (!Sword.IsValid() || !Heavy.IsValid()) { Test->AddError(TEXT("Mini arena pair missing")); return true; }
			DeadController = Sword->GetController();
			Sword->GetSCLAbilitySystemComponent()->SetNumericAttributeBase(USCLAttributeSet::GetHealthAttribute(), 0.0F);
			Step = 11; Next = Now + 2.4; return false;
		}
		if (Step == 11)
		{
			Test->TestFalse(TEXT("First mini arena corpse disappears without leaving region"), Sword.IsValid());
			Test->TestFalse(TEXT("Corpse cleanup also removes its AI controller"), DeadController.IsValid());
			Test->TestEqual(TEXT("Cleanup preserves surviving enemy and locked exit"), Demo->GetLivingOpponentCount(), 1);
			Test->TestEqual(TEXT("First corpse cleanup does not clear arena"), Demo->GetState(), ESCLDemoState::Playing);
			Player->SetActorRotation(FRotator::ZeroRotator);
			Player->GetController()->SetControlRotation(FRotator::ZeroRotator);
			Heavy->SetActorLocation(Player->GetActorLocation() + FVector{140.0F, 0.0F, 0.0F});
			Heavy->GetSCLAbilitySystemComponent()->ApplyPoiseDamageToSelf(1000.0F, Player.Get());
			Test->TestTrue(TEXT("Parry starts against Heavy fixture"), PlayerASC->TryActivateAbilitiesByTag(FGameplayTagContainer{SCLGameplayTags::Ability_Parry}));
			Step = 12; Next = Now + 0.13; return false;
		}
		if (Step == 12)
		{
			Test->TestTrue(TEXT("Heavy check occurs inside parry window"), PlayerASC->HasMatchingGameplayTag(SCLGameplayTags::State_Parrying));
			Test->TestEqual(TEXT("Unparryable Heavy attack still deals damage from the front"),
				UGameplayStatics::ApplyPointDamage(Player.Get(), 5.0F, -Player->GetActorForwardVector(),
					FHitResult{}, Heavy->GetController(), Heavy.Get(), UDamageType::StaticClass()), 5.0F);
			USCLTargetingComponent* const Targeting = Player->FindComponentByClass<USCLTargetingComponent>();
			Targeting->ToggleLock();
			// Screen projection is unavailable in NullRHI; the required rendered run checks acquisition.
			if (FApp::CanEverRender()) Test->TestTrue(TEXT("Heavy acquired before execution"), Targeting->GetCurrentTarget() == Heavy.Get());
			Test->TestTrue(TEXT("Locked Heavy execution starts"), PlayerASC->TryActivateAbilitiesByTag(FGameplayTagContainer{SCLGameplayTags::Ability_Execution}));
			DeadController = Heavy->GetController();
			// 命中通知在约 1.024 秒，死亡后的尸体清理还需要约 2 秒。
			Step = 13; Next = Now + 3.6; return false;
		}
		if (Step == 13)
		{
			Test->TestFalse(TEXT("Executed Heavy corpse is removed in current arena"), Heavy.IsValid());
			Test->TestFalse(TEXT("Executed Heavy AI is removed"), DeadController.IsValid());
			Test->TestFalse(TEXT("Dead executed target no longer owns camera"), Player->FindComponentByClass<USCLTargetingComponent>()->IsLockedOn());
			Test->TestTrue(TEXT("Locked execution restores free movement after target death"), Player->GetCharacterMovement()->bOrientRotationToMovement);
			Test->TestTrue(TEXT("Target death restores configured free speed"), FMath::IsNearlyEqual(Player->GetCharacterMovement()->MaxWalkSpeed, FreeMoveSpeed));
			Test->TestEqual(TEXT("Last corpse cleanup preserves clear result"), Demo->GetState(), ESCLDemoState::StageClear);
			CheckFreeCamera();
			Player->AddMovementInput(FVector::RightVector, 1.0F);
			AttackStarted = Now;
			Step = 14; Next = Now; return false;
		}
		if (Step == 14)
		{
			if (Now - AttackStarted < 0.4)
			{
				Player->AddMovementInput(FVector::RightVector, 1.0F);
				Next = Now;
				return false;
			}
			Test->TestTrue(TEXT("Movement turns pawn away from executed Heavy without relocking"), Player->GetActorForwardVector().Y > 0.1F);
			Step = 10; return false;
		}
		if (Step == 10)
		{
			EnterRegion(*Demo, ESCLDemoStage::BossGate);
			EnterRegion(*Demo, ESCLDemoStage::Boss); Player = Demo->GetPlayer();
			Test->TestEqual(TEXT("Boss encounter uses normal region setup"), Demo->GetStage(), ESCLDemoStage::Boss);
			Step = 9; AttackStarted = Now; return false;
		}
		if (PlayerASC->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()) < 300.0F)
		{
			if (Step == 9)
			{
				Test->AddInfo(TEXT("Normal Boss AI approached from spawn and landed a real hit"));
				return true;
			}
			Test->AddInfo(TEXT("Normal Sword AI approached from spawn and landed a real weapon hit"));
			Demo->GetOpponent()->GetSCLAbilitySystemComponent()->SetNumericAttributeBase(USCLAttributeSet::GetHealthAttribute(), 0.0F);
			Step = 7; Next = Now + 0.3; return false;
		}
		if (Now - AttackStarted < 8.0) return false;
		Test->AddError(Step == 9 ? TEXT("Boss AI did not reach and hit a stationary player within 8 seconds")
			: TEXT("Sword AI did not reach and hit a stationary player within 8 seconds"));
		return true;
	}
private:
	void CheckFreeCamera()
	{
		const USpringArmComponent* const Boom = Player->FindComponentByClass<USpringArmComponent>();
		Test->TestEqual(TEXT("Execution/death unlock restores original camera arm"), Boom->TargetArmLength, 400.0F);
		Test->TestTrue(TEXT("Execution/death unlock removes lock-only camera offsets"), Boom->TargetOffset.IsNearlyZero() && Boom->SocketOffset.IsNearlyZero());
		APlayerController* const Controller = Cast<APlayerController>(Player->GetController());
		const float Before = Controller->GetControlRotation().Yaw;
		Test->TestFalse(TEXT("Gameplay look input is not ignored after execution"), Controller->IsLookInputIgnored());
		Controller->AddYawInput(20.0F);
		Controller->UpdateRotation(0.016F);
		Test->TestTrue(TEXT("Look input changes camera yaw without another lock-on"),
			FMath::Abs(FMath::FindDeltaAngleDegrees(Before, Controller->GetControlRotation().Yaw)) > 1.0F);
	}
	void CheckParryDirections()
	{
		const float Angles[]{-75.0F, 75.0F, 100.0F, 180.0F};
		for (const float Angle : Angles)
		{
			const FVector Direction = FRotator{0.0F, Angle, 0.0F}.Vector();
			Sword->SetActorLocation(Player->GetActorLocation() + Direction * 140.0F);
			const float Damage = UGameplayStatics::ApplyPointDamage(Player.Get(), 5.0F, -Direction,
				FHitResult{}, Sword->GetController(), Sword.Get(), UDamageType::StaticClass());
			Test->TestEqual(FString::Printf(TEXT("Parry damage at %.0f degrees"), Angle), Damage, FMath::Abs(Angle) < 80.0F ? 0.0F : 5.0F);
		}
		Sword->SetActorLocation(Player->GetActorLocation() + FVector{140.0F, 0.0F, 0.0F});
	}
	static void StopEnemy(ASCLEnemyCharacter& Enemy)
	{
		if (AAIController* const AI = Cast<AAIController>(Enemy.GetController()))
		{
			AI->StopMovement();
			if (AI->BrainComponent != nullptr) AI->BrainComponent->StopLogic(TEXT("Controlled combat regression"));
		}
		Enemy.GetCombatComponent_Implementation()->CancelActiveAttack();
	}
	void EnterRegion(USCLDemoSubsystem& Demo, const ESCLDemoStage Region)
	{
		Demo.GetPlayer()->SetActorLocation(ASCLDemoMap::Checkpoint(Region));
		Demo.CheckRegion();
	}
	void PrepareTraining(USCLDemoSubsystem& Demo)
	{
		Demo.RestartRun(); EnterRegion(Demo, ESCLDemoStage::Training);
	}
	void EnterSword(USCLDemoSubsystem& Demo)
	{
		PrepareTraining(Demo); Demo.PrimaryAction(); EnterRegion(Demo, ESCLDemoStage::Sword);
	}
	void PrepareSword(USCLDemoSubsystem& Demo)
	{
		EnterSword(Demo);
		Player = Demo.GetPlayer(); Sword = Cast<ASCLEnemyCharacter>(Demo.GetOpponent());
		if (!Sword.IsValid()) return;
		if (AAIController* const AI = Cast<AAIController>(Sword->GetController()))
		{
			AI->StopMovement();
			if (AI->BrainComponent != nullptr) AI->BrainComponent->StopLogic(TEXT("Controlled combat regression"));
		}
		Sword->GetCombatComponent_Implementation()->CancelActiveAttack();
		Sword->SetActorLocation(Player->GetActorLocation() + Player->GetActorForwardVector() * 140.0F);
		Sword->SetActorRotation((-Player->GetActorForwardVector()).Rotation());
	}
	void CheckWalls(UWorld& World)
	{
		int32 Count = 0;
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			if (!It->ActorHasTag(TEXT("SCLArenaBoundary"))) continue;
			TArray<UBoxComponent*> Walls; It->GetComponents(Walls);
			for (UBoxComponent* Wall : Walls)
			{
				++Count;
				const FVector Extent = Wall->GetScaledBoxExtent();
				const FVector Normal = Extent.X < Extent.Y ? FVector::ForwardVector : FVector::RightVector;
				const FVector Center{Wall->GetComponentLocation().X, Wall->GetComponentLocation().Y, 700.0F};
				FHitResult Hit;
				FCollisionQueryParams Params{SCENE_QUERY_STAT(SCLArenaWallTest), false, Player.Get()};
				const bool Blocked = World.SweepSingleByChannel(Hit, Center - Normal * 180.0F,
					Center + Normal * 180.0F, FQuat::Identity, ECC_Pawn,
					FCollisionShape::MakeCapsule(42.0F, 90.0F), Params);
				Test->TestTrue(TEXT("Arena wall blocks character capsule"), Blocked && Hit.GetActor() == *It);
			}
		}
		Test->TestEqual(TEXT("Four arena walls exist"), Count, 4);
	}
	// 在真正创建的 Demo Widget 中读取提示，防止只测一段未接入界面的字符串。
	void CheckHint(UWorld& World, const TCHAR* Expected)
	{
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(&World, Widgets, USCLDemoWidget::StaticClass(), false);
		bool bFound = false;
		for (auto* Widget : Widgets)
		{
			auto* DemoWidget = Cast<USCLDemoWidget>(Widget);
			DemoWidget->Refresh();
			TArray<UWidget*> Children;
			DemoWidget->WidgetTree->GetAllWidgets(Children);
			for (auto* Child : Children)
				if (const auto* Text = Cast<UTextBlock>(Child)) bFound |= Text->GetText().ToString().Contains(Expected);
		}
		Test->TestTrue(FString::Printf(TEXT("Actual combat UI shows %s"), Expected), bFound);
	}
	FAutomationTestBase* Test;
	TWeakObjectPtr<ASCLPlayerCharacter> Player;
	TWeakObjectPtr<ASCLEnemyCharacter> Sword;
	TWeakObjectPtr<ASCLEnemyCharacter> Heavy;
	TWeakObjectPtr<AController> DeadController;
	FVector StartPosition{ForceInit}, RootStart{ForceInit};
	TEnumAsByte<ERootMotionMode::Type> SavedMode{ERootMotionMode::NoRootMotionExtraction};
	double Started{FPlatformTime::Seconds()}, Next{0.0}, AttackStarted{0.0};
	float MaxDrift{0.0F}, MaxRootDrift{0.0F};
	float FreeMoveSpeed{0.0F};
	int32 Step{0};
	bool bTrainingPrepared{false};
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSCLCombatPolishRuntimeTest, "SoulCombatLabCombat.RuntimePolish",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSCLCombatPolishRuntimeTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FSCLCombatPolishCommand(this));
	return true;
}
#endif
