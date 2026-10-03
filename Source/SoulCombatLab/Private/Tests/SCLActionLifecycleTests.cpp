#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Characters/Components/SCLActionMovementComponent.h"
#include "Combat/SCLCombatComponent.h"
#include "Characters/SCLEnemyCharacter.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Data/SCLAttackData.h"
#include "Animation/AnimMontage.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Demo/SCLDemoSubsystem.h"
#include "Demo/SCLDemoMap.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"

// 使用实际角色验证移动申请交叠、释放顺序，以及伤害属性回调中同步取消范围攻击。
// 这些是重构的状态所有权风险，不以“文件变少”或“能编译”代替验证。
class FSCLActionLifecycleCommand final : public IAutomationLatentCommand
{
public:
	explicit FSCLActionLifecycleCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual ~FSCLActionLifecycleCommand() override
	{
		if (Player.IsValid() && HealthHandle.IsValid())
			Player->GetSCLAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
	}
	bool Update() override
	{
		if (FPlatformTime::Seconds() - Started > 30) { Test->AddError(TEXT("Action lifecycle timed out")); return true; }
		UWorld* World = nullptr;
		for (const auto& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Game) World = Context.World();
		auto* Demo = World ? World->GetSubsystem<USCLDemoSubsystem>() : nullptr;
		if (!Demo || !Demo->IsActive()) return false;
		if (Step == 0)
		{
			Demo->RestartRun();
			Player = Demo->GetPlayer();
			Player->SetActorLocation(ASCLDemoMap::Checkpoint(ESCLDemoStage::Training));
			Demo->CheckRegion();
			auto* Target = Demo->GetOpponent();
			if (!Target || !Player->GetMesh()->GetAnimInstance()) { Test->AddError(TEXT("Action fixture missing")); return true; }
			Target->SetActorLocation(Player->GetActorLocation() + FVector{140, 0, 0});
			auto* Limits = Player->GetActionMovementComponent();
			auto* Movement = Player->GetCharacterMovement();
			const float OriginalSpeed = Movement->MaxWalkSpeed;
			const bool OriginalFacing = Movement->bOrientRotationToMovement;
			Limits->SetBaseWalkSpeed(500);
			// 先格挡后锁定，与先锁定后格挡必须得到相同结果。
			Limits->SetWalkSpeedMultiplier(TEXT("Block"), 0.5F);
			Limits->SetWalkSpeedLimit(TEXT("LockOn"), 180);
			Test->TestEqual(TEXT("Block then lock combines cap and multiplier"), Movement->MaxWalkSpeed, 90.0F);
			Limits->ClearWalkSpeedLimit(TEXT("LockOn"));
			Test->TestEqual(TEXT("Losing lock keeps block restriction"), Movement->MaxWalkSpeed, 250.0F);
			Limits->ClearWalkSpeedMultiplier(TEXT("Block"));
			Test->TestEqual(TEXT("Final release restores base"), Movement->MaxWalkSpeed, 500.0F);
			Limits->SetWalkSpeedLimit(TEXT("LockOn"), 180);
			Limits->SetWalkSpeedMultiplier(TEXT("Block"), 0.5F);
			Test->TestEqual(TEXT("Reverse request order has same speed"), Movement->MaxWalkSpeed, 90.0F);
			Limits->ClearWalkSpeedMultiplier(TEXT("Block"));
			Test->TestEqual(TEXT("Block release retains lock cap"), Movement->MaxWalkSpeed, 180.0F);
			Limits->SetBaseWalkSpeed(150);
			Test->TestEqual(TEXT("Updating base while restricted recomputes speed"), Movement->MaxWalkSpeed, 150.0F);
			Limits->ClearWalkSpeedLimit(TEXT("LockOn"));
			Limits->SetBaseWalkSpeed(OriginalSpeed);
			Limits->SetFacingLocked(TEXT("LockOn"), true);
			Limits->SetFacingLocked(TEXT("Attack"), true);
			Limits->SetFacingLocked(TEXT("Attack"), false);
			Test->TestFalse(TEXT("Attack release keeps lock facing"), Movement->bOrientRotationToMovement);
			Limits->SetFacingLocked(TEXT("LockOn"), false);
			Test->TestEqual(TEXT("Last facing release restores baseline"), Movement->bOrientRotationToMovement, OriginalFacing);
			auto* Anim = Player->GetMesh()->GetAnimInstance();
			const auto OriginalRoot = Anim->RootMotionMode;
			Limits->RequestRootMotionMode(TEXT("WeaponPresentation"), ERootMotionMode::IgnoreRootMotion, 10);
			Limits->RequestRootMotionMode(TEXT("Attack"), ERootMotionMode::RootMotionFromMontagesOnly);
			Limits->ReleaseRootMotionMode(TEXT("WeaponPresentation"));
			Test->TestTrue(TEXT("Old presentation release cannot overwrite attack root mode"), Anim->RootMotionMode == ERootMotionMode::RootMotionFromMontagesOnly);
			Limits->ReleaseRootMotionMode(TEXT("Attack"));
			Test->TestTrue(TEXT("Last root release restores baseline"), Anim->RootMotionMode == OriginalRoot);
			Limits->RequestRootMotionMode(TEXT("Attack"), ERootMotionMode::RootMotionFromMontagesOnly);
			Limits->RequestRootMotionMode(TEXT("Parry"), ERootMotionMode::IgnoreRootMotion);
			Limits->ReleaseRootMotionMode(TEXT("Attack"));
			Test->TestTrue(TEXT("Older attack release retains newer parry"), Anim->RootMotionMode == ERootMotionMode::IgnoreRootMotion);
			Limits->ReleaseRootMotionMode(TEXT("Parry"));
			Test->TestTrue(TEXT("Root policy restores after reverse release"), Anim->RootMotionMode == OriginalRoot);
			Step = 1;
			Next = World->GetTimeSeconds() + 0.05F;
			return false;
		}
		if (World->GetTimeSeconds() < Next) return false;
		if (Step == 1)
		{
			Demo->PrimaryAction();
			Player->SetActorLocation(ASCLDemoMap::Checkpoint(ESCLDemoStage::Sword));
			Demo->CheckRegion();
			Enemy = Cast<ASCLEnemyCharacter>(Demo->GetOpponent());
			if (!Enemy.IsValid()) { Test->AddError(TEXT("Enemy area fixture missing")); return true; }
			if (auto* AI = Cast<AAIController>(Enemy->GetController()))
			{
				AI->StopMovement();
				if (AI->BrainComponent) AI->BrainComponent->StopLogic(TEXT("Area lifecycle regression"));
			}
			Enemy->SetActorLocation(Player->GetActorLocation() + FVector{140, 0, 0});
			auto* ASC = Player->GetSCLAbilitySystemComponent();
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetMaxHealthAttribute(), 1000);
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetHealthAttribute(), 1000);
			auto* Combat = Enemy->GetCombatComponent_Implementation();
			Combat->CancelActiveAttack();
			FSCLRuntimeAttackProfile Profile;
			Profile.AttackName = TEXT("AreaLifecycle");
			Profile.AreaRadius = 300;
			Profile.AreaImpactDelay = 0.08F;
			Profile.bWeaponTraceEnabled = false;
			Profile.MontageStepCount = 2;
			// 属性变化发生在 ApplyPointDamage 调用栈中；回调取消攻击，模拟真实同步重入。
			HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetHealthAttribute()).AddLambda(
				[this](const FOnAttributeChangeData& Change)
				{
					if (Change.NewValue >= Change.OldValue || !Enemy.IsValid()) return;
					++HitCount;
					Enemy->GetCombatComponent_Implementation()->CancelActiveAttack();
				});
			Combat->ConfigureNextAttackProfile(Profile);
			Test->TestTrue(TEXT("Enemy starts area attack through shared orchestrator"), Combat->RequestAttack());
			auto* Data = LoadObject<USCLAttackData>(nullptr, TEXT("/Game/SoulCombatLab/Data/Attacks/DA_LightAttack.DA_LightAttack"));
			auto* Anim = Enemy->GetMesh()->GetAnimInstance();
			if (!Data || !Anim || !Data->FindComboStep(1)) { Test->AddError(TEXT("Enemy section fixture missing")); return true; }
			auto* Montage = Data->GetAttackMontage();
			const int32 First = Montage->GetSectionIndex(Data->FindComboStep(0)->MontageSection);
			const int32 Last = Montage->GetSectionIndex(Data->FindComboStep(1)->MontageSection);
			Test->TestEqual(TEXT("Automatic enemy combo connects first to second section"), Anim->Montage_GetNextSectionID(Montage, First), Last);
			Test->TestEqual(TEXT("Final enemy section terminates instead of looping"), Anim->Montage_GetNextSectionID(Montage, Last), INDEX_NONE);
			Step = 2; Next = World->GetTimeSeconds() + 0.16F; return false;
		}
		if (Step == 2)
		{
			auto* ASC = Player->GetSCLAbilitySystemComponent();
			const float Health = ASC->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			Test->TestTrue(TEXT("Enemy delayed area impact deals real damage"), Health < 1000);
			Test->TestEqual(TEXT("Damage callback synchronously cancels exactly once"), HitCount, 1);
			auto* Combat = Enemy->GetCombatComponent_Implementation();
			Test->TestFalse(TEXT("Canceled old area generation cannot revive attack state"), Combat->IsAttackActive());
			ASC->GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
			HealthHandle.Reset();
			HealthBeforeCancel = Health;
			FSCLRuntimeAttackProfile Profile;
			Profile.AreaRadius = 300;
			Profile.AreaImpactDelay = 0.08F;
			Profile.bWeaponTraceEnabled = false;
			Combat->ConfigureNextAttackProfile(Profile);
			Test->TestTrue(TEXT("Enemy restarts area attack after synchronous cancel"), Combat->RequestAttack());
			Step = 3; Next = World->GetTimeSeconds() + 0.16F; return false;
		}
		if (Step == 3)
		{
			const float Health = Player->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			Test->TestTrue(TEXT("Uninterrupted area query deals damage"), Health < HealthBeforeCancel);
			auto* Combat = Enemy->GetCombatComponent_Implementation();
			Test->TestTrue(TEXT("Completed area query retains ongoing animation cancel entry"), Combat->IsAttackActive());
			Combat->CancelActiveAttack();
			FSCLRuntimeAttackProfile Profile;
			Profile.AreaRadius = 300; Profile.AreaImpactDelay = 0.08F; Profile.bWeaponTraceEnabled = false;
			Combat->ConfigureNextAttackProfile(Profile);
			Test->TestTrue(TEXT("Enemy restarts area attack after cancel"), Combat->RequestAttack());
			Combat->CancelActiveAttack();
			HealthBeforeCancel = Health;
			Step = 4; Next = World->GetTimeSeconds() + 0.16F; return false;
		}
		Test->TestEqual(TEXT("Canceled enemy area attack has no delayed damage"),
			Player->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute()), HealthBeforeCancel);
		Test->TestFalse(TEXT("Canceled enemy retains no active attack"), Enemy->GetCombatComponent_Implementation()->IsAttackActive());
		return true;
	}
private:
	FAutomationTestBase* Test;
	TWeakObjectPtr<ASCLPlayerCharacter> Player;
	TWeakObjectPtr<ASCLEnemyCharacter> Enemy;
	FDelegateHandle HealthHandle;
	double Started{FPlatformTime::Seconds()};
	float Next{0};
	float HealthBeforeCancel{0};
	int32 Step{0}, HitCount{0};
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSCLActionLifecycleTest, "SoulCombatLabCombat.ActionLifecycle",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FSCLActionLifecycleTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FSCLActionLifecycleCommand(this));
	return true;
}
#endif
