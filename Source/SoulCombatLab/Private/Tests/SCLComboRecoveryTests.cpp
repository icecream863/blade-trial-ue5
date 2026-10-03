#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystem/Abilities/SCLLightAttackAbility.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Combat/SCLCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Demo/SCLDemoMap.h"
#include "Demo/SCLDemoSubsystem.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"

class FSCLComboRecoveryCommand final : public IAutomationLatentCommand
{
public:
	explicit FSCLComboRecoveryCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 20.0) { Test->AddError(TEXT("Combo recovery timed out")); return true; }
		if (Now < Next) return false;
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Game) World = Context.World();
		USCLDemoSubsystem* const Demo = World != nullptr ? World->GetSubsystem<USCLDemoSubsystem>() : nullptr;
		if (Demo == nullptr || !Demo->IsActive()) return false;
		if (Step == 0)
		{
			Demo->RestartRun();
			Demo->GetPlayer()->SetActorLocation(ASCLDemoMap::Checkpoint(ESCLDemoStage::Training));
			Demo->CheckRegion();
			Player = Demo->GetPlayer(); Target = Demo->GetOpponent();
			if (!Player.IsValid() || !Target.IsValid()) { Test->AddError(TEXT("Combo fixture missing")); return true; }
			if (!FApp::CanEverRender()) Player->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			Target->SetActorLocation(Player->GetActorLocation() + FVector{140.0F, 0.0F, 0.0F});
			USCLAbilitySystemComponent* const TargetASC = Target->GetSCLAbilitySystemComponent();
			TargetASC->SetNumericAttributeBase(USCLAttributeSet::GetMaxHealthAttribute(), 1000.0F);
			TargetASC->SetNumericAttributeBase(USCLAttributeSet::GetHealthAttribute(), 1000.0F);
			TargetASC->SetNumericAttributeBase(USCLAttributeSet::GetMaxPoiseAttribute(), 1000.0F);
			TargetASC->SetNumericAttributeBase(USCLAttributeSet::GetPoiseAttribute(), 1000.0F);
			SavedMode = Player->GetMesh()->GetAnimInstance()->RootMotionMode;
			Step = 1; Next = Now + 0.3; return false;
		}
		if (!Player.IsValid() || !Target.IsValid()) { Test->AddError(TEXT("Combo fixture destroyed")); return true; }
		USCLCombatComponent* const Combat = Player->GetCombatComponent_Implementation();
		USCLAbilitySystemComponent* const ASC = Player->GetSCLAbilitySystemComponent();
		UAnimInstance* const Anim = Player->GetMesh()->GetAnimInstance();
		if (Step == 1)
		{
			Test->TestTrue(TEXT("Initial Attack01 starts through GAS"), ASC->TryActivateAbilityByClass(USCLLightAttackAbility::StaticClass()));
			Test->TestEqual(TEXT("Unprepared light request uses native player attack"), Combat->GetActiveAttackName(), FName(TEXT("Attack01_1")));
			Step = 2; Next = Now + 0.2; return false;
		}
		if (Step == 2)
		{
			Test->TestTrue(TEXT("Initial second input is buffered"), ASC->TryActivateAbilityByClass(USCLLightAttackAbility::StaticClass()));
			Step = 3; Next = Now + 0.4; return false;
		}
		if (Step == 3)
		{
			Test->TestEqual(TEXT("Real incoming damage interrupts the attack"),
				UGameplayStatics::ApplyPointDamage(Player.Get(), 5.0F, FVector::BackwardVector,
					FHitResult{}, nullptr, Target.Get(), UDamageType::StaticClass()), 5.0F);
			Test->TestFalse(TEXT("Hit reaction clears active combo"), Combat->IsAttackActive());
			ASC->SetNumericAttributeBase(USCLAttributeSet::GetStaminaAttribute(), 100.0F);
			Target->GetSCLAbilitySystemComponent()->SetNumericAttributeBase(USCLAttributeSet::GetHealthAttribute(), 1000.0F);
			Target->GetSCLAbilitySystemComponent()->SetNumericAttributeBase(USCLAttributeSet::GetPoiseAttribute(), 1000.0F);
			Test->TestTrue(TEXT("Immediate attack restart after hit reaction succeeds"), ASC->TryActivateAbilityByClass(USCLLightAttackAbility::StaticClass()));
			Test->TestEqual(TEXT("Restart begins at Attack01_1"), Combat->GetActiveAttackName(), FName(TEXT("Attack01_1")));
			Step = 4; Next = Now + 0.2; return false;
		}
		if (Step == 4)
		{
			// The old interrupted instance has finished blending out, the new one is still winding up.
			if (!Test->TestTrue(TEXT("Restarted attack survives old montage end callback"), Combat->IsAttackActive())) return true;
			Test->TestTrue(TEXT("Restarted attack retains movement facing lock"), !Player->GetCharacterMovement()->bOrientRotationToMovement);
			StartHealth = Target->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			StartPoise = Target->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetPoiseAttribute());
			Test->TestTrue(TEXT("Recovered combo accepts second light input"), ASC->TryActivateAbilityByClass(USCLLightAttackAbility::StaticClass()));
			Step = 5; Next = Now + 0.8; return false;
		}
		if (Step == 5)
		{
			Test->TestEqual(TEXT("Recovered combo reaches Attack01_2"), Combat->GetActiveAttackName(), FName(TEXT("Attack01_2")));
			const float Damage = StartHealth - Target->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetHealthAttribute());
			const float Poise = StartPoise - Target->GetSCLAbilitySystemComponent()->GetNumericAttribute(USCLAttributeSet::GetPoiseAttribute());
			Test->TestTrue(TEXT("Recovered native weapon trace deals damage"), Damage > 0.0F);
			Test->TestTrue(TEXT("Recovered native weapon trace deals poise damage"), Poise > 0.0F);
			Combat->CancelActiveAttack();
			Test->TestTrue(TEXT("Cancel restores free movement"), Player->GetCharacterMovement()->bOrientRotationToMovement);
			Test->TestTrue(TEXT("Cancel restores root motion policy"), Anim->RootMotionMode == SavedMode);
			Step = 6; Next = Now + 0.4; return false;
		}
		if (Step == 6)
		{
			Test->TestFalse(TEXT("Old montage callback cannot revive canceled combo"), Combat->IsAttackActive());
			return true;
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	TWeakObjectPtr<ASCLPlayerCharacter> Player;
	TWeakObjectPtr<ASCLCharacterBase> Target;
	TEnumAsByte<ERootMotionMode::Type> SavedMode{ERootMotionMode::NoRootMotionExtraction};
	double Started{FPlatformTime::Seconds()}, Next{Started + 1.0};
	float StartHealth{0.0F}, StartPoise{0.0F};
	int32 Step{0};
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSCLComboRecoveryTest, "SoulCombatLabCombat.ComboRecovery",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FSCLComboRecoveryTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FSCLComboRecoveryCommand(this));
	return true;
}
#endif
