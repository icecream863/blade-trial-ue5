#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/SCLAbilitySystemPolicy.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSCLStaminaRegenerationCompletionPolicyTest,
	"SoulCombatLab.AbilitySystem.StaminaRegenerationCompletionPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSCLStaminaRegenerationCompletionPolicyTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Crossing from below maximum to maximum completes regeneration"),
		SCLAbilitySystemPolicy::ShouldCompleteStaminaRegeneration(97.5F, 100.0F, 100.0F));
	TestFalse(
		TEXT("A reentrant unchanged maximum notification does not complete twice"),
		SCLAbilitySystemPolicy::ShouldCompleteStaminaRegeneration(100.0F, 100.0F, 100.0F));
	TestFalse(
		TEXT("A notification already above maximum does not complete twice"),
		SCLAbilitySystemPolicy::ShouldCompleteStaminaRegeneration(100.0F, 102.0F, 100.0F));
	TestFalse(
		TEXT("Regeneration below maximum remains active"),
		SCLAbilitySystemPolicy::ShouldCompleteStaminaRegeneration(80.0F, 90.0F, 100.0F));
	TestFalse(
		TEXT("An invalid maximum cannot complete regeneration"),
		SCLAbilitySystemPolicy::ShouldCompleteStaminaRegeneration(0.0F, 0.0F, 0.0F));
	return true;
}

#endif
