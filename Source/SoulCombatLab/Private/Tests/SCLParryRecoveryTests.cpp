#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystem/Abilities/SCLParryAbility.h"
#include "AbilitySystem/Abilities/SCLLightAttackAbility.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Demo/SCLDemoSubsystem.h"
#include "Engine/Engine.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

// 使用实际授予的 GA_PlayerParry：检查空弹、成功、接攻击以及旧定时器不会结束新动作。
class FSCLParryRecoveryCommand final : public IAutomationLatentCommand
{
public:
 explicit FSCLParryRecoveryCommand(FAutomationTestBase* InTest) : Test(InTest) {}
 bool Update() override
 {
  if (FPlatformTime::Seconds()-Started > 45) { Test->AddError(TEXT("Parry recovery timeout")); return true; }
  UWorld* World=nullptr;
  for (const auto& Context : GEngine->GetWorldContexts()) if(Context.WorldType==EWorldType::Game) World=Context.World();
  auto* Demo=World ? World->GetSubsystem<USCLDemoSubsystem>() : nullptr;
  if (!Demo || !Demo->IsActive()) return false;
  if (!Player.IsValid()) { Demo->RestartRun(); Demo->PrimaryAction(); Player=Demo->GetPlayer(); return false; }
  auto* ASC=Player->GetSCLAbilitySystemComponent();
  const double Now=World->GetTimeSeconds();
  auto* Spec=ASC->FindAbilitySpecByBaseClass(USCLParryAbility::StaticClass());
  if (Step==0)
  {
   Test->TestTrue(TEXT("Whiff starts"), ASC->TryActivateAbility(Spec->Handle)); Begin=Now; Step=1; return false;
  }
  auto* Parry=Cast<USCLParryAbility>(Spec->GetPrimaryInstance());
  if (Step==1)
  {
   if (ASC->HasMatchingGameplayTag(SCLGameplayTags::State_ParryAction))
   { if (Now-Begin>0.95) { Test->AddError(TEXT("Whiff still locks controls past .95 seconds")); return true; } return false; }
   Test->TestFalse(TEXT("Whiff removes effective window"), ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Parrying));
   Test->TestFalse(TEXT("Whiff does not report success"), Parry->WasRecentlySuccessful());
   Test->TestTrue(TEXT("Next parry starts"), ASC->TryActivateAbility(Spec->Handle)); Step=2; return false;
  }
  if (Step==2)
  {
   if (!ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Parrying)) return false;
   // 成功结算和方向判定由 RuntimePolish 检查；本例直接发送已确认成功的结果，隔离恢复生命周期。
   Parry->NotifySuccessfulParry(); Begin=Now; Step=3; return false;
  }
  if (Step==3)
  {
   if (ASC->HasMatchingGameplayTag(SCLGameplayTags::State_ParryAction))
   { if(Now-Begin>0.25) { Test->AddError(TEXT("Successful parry recovery exceeded .25 seconds")); return true; } return false; }
   Test->TestTrue(TEXT("Confirmed success reports feedback"), Parry->WasRecentlySuccessful());
   Test->TestFalse(TEXT("Success closes defensive window"), ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Parrying));
   Player->MoveInViewDirection(FVector2D(0,1));
   Test->TestTrue(TEXT("Movement input accepted immediately after recovery"), !Player->GetPendingMovementInputVector().IsNearlyZero());
   Player->ConsumeMovementInputVector(); Player->ClearMovementInput();
   Test->TestTrue(TEXT("Attack starts after successful parry recovery"), ASC->TryActivateAbilityByClass(USCLLightAttackAbility::StaticClass()));
   Begin=Now; Step=4; return false;
  }
  if(Now-Begin<0.15) return false;
  Test->TestTrue(TEXT("Old recovery does not cancel follow-up attack"), ASC->HasMatchingGameplayTag(SCLGameplayTags::State_Attacking));
  FGameplayTagContainer Tags(SCLGameplayTags::Ability_Attack_Light); ASC->CancelAbilities(&Tags);
  return true;
 }
private:
 FAutomationTestBase* Test;
 double Started{FPlatformTime::Seconds()}, Begin{0};
 int32 Step{0};
 TWeakObjectPtr<ASCLPlayerCharacter> Player;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSCLParryRecoveryTest,"SoulCombatLabCombat.ParryRecovery",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FSCLParryRecoveryTest::RunTest(const FString& Parameters) { ADD_LATENT_AUTOMATION_COMMAND(FSCLParryRecoveryCommand(this)); return true; }
#endif
