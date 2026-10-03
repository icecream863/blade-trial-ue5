#if WITH_DEV_AUTOMATION_TESTS

#include "AI/Boss/SCLBossUtilityPolicy.h"
#include "AI/SCLAIState.h"
#include "AbilitySystem/Abilities/SCLExecutionAbility.h"
#include "Characters/SCLBossCharacter.h"
#include "Data/SCLEnemyArchetypeData.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSCLBossPhaseAndUtilityPolicyTest,
	"SoulCombatLab.Boss.PhaseAndUtilityPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSCLBossPhaseAndUtilityPolicyTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Ordinary enemy execution still consumes all remaining health"),
		USCLExecutionAbility::CalculateExecutionDamage(75.0F, false),
		75.0F);
	TestEqual(
		TEXT("Boss execution consumes one fifth of remaining health"),
		USCLExecutionAbility::CalculateExecutionDamage(301.0F, true),
		60.2F);
	TestEqual(
		TEXT("Boss execution damage remains safe at zero health"),
		USCLExecutionAbility::CalculateExecutionDamage(0.0F, true),
		0.0F);

	TestTrue(
		TEXT("Health above 50 percent remains Phase One"),
		SCLBossUtilityPolicy::ResolvePhase(501.0F, 1000.0F) == ESCLBossPhase::PhaseOne);
	TestTrue(
		TEXT("Health exactly at 50 percent enters Phase Two"),
		SCLBossUtilityPolicy::ResolvePhase(500.0F, 1000.0F) == ESCLBossPhase::PhaseTwo);
	TestTrue(
		TEXT("Invalid maximum health safely defaults to Phase One"),
		SCLBossUtilityPolicy::ResolvePhase(0.0F, 0.0F) == ESCLBossPhase::PhaseOne);

	TestFalse(
		TEXT("Area Attack is locked during Phase One"),
		SCLBossUtilityPolicy::IsAttackAvailable(
			ESCLBossAttack::AreaAttack,
			ESCLBossPhase::PhaseOne));
	TestTrue(
		TEXT("Area Attack unlocks during Phase Two"),
		SCLBossUtilityPolicy::IsAttackAvailable(
			ESCLBossAttack::AreaAttack,
			ESCLBossPhase::PhaseTwo));

	FSCLBossUtilityContext Context;
	Context.DistanceToTarget = 150.0F;
	Context.Phase = ESCLBossPhase::PhaseOne;
	FSCLBossAttackDecision Decision = SCLBossUtilityPolicy::SelectAttack(Context);
	TestTrue(TEXT("Phase One close range selects Light Slash"), Decision.Attack == ESCLBossAttack::LightSlash);
	TestEqual(TEXT("Phase One close-range winning score"), Decision.Score, 50.0F);

	Context.DistanceToTarget = 320.0F;
	Decision = SCLBossUtilityPolicy::SelectAttack(Context);
	TestTrue(TEXT("Phase One medium range selects Dash Slash"), Decision.Attack == ESCLBossAttack::DashSlash);
	TestEqual(TEXT("Phase One medium-range winning score"), Decision.Score, 55.0F);

	Context.DistanceToTarget = 150.0F;
	Context.Phase = ESCLBossPhase::PhaseTwo;
	Decision = SCLBossUtilityPolicy::SelectAttack(Context);
	TestTrue(TEXT("Phase Two close range selects newly unlocked Area Attack"), Decision.Attack == ESCLBossAttack::AreaAttack);
	TestEqual(TEXT("Phase Two close-range winning score"), Decision.Score, 60.0F);

	Context.Phase = ESCLBossPhase::PhaseOne;
	Context.PreviousAttack = ESCLBossAttack::LightSlash;
	Decision = SCLBossUtilityPolicy::SelectAttack(Context);
	TestTrue(TEXT("Repeat penalty changes the next close attack"), Decision.Attack == ESCLBossAttack::HeavySlash);
	TestEqual(TEXT("Repeat-aware winning score"), Decision.Score, 35.0F);

	Decision = SCLBossUtilityPolicy::SelectExecutableAttack(Context);
	TestTrue(
		TEXT("Executable selector retains the repeat-aware Heavy Slash"),
		Decision.Attack == ESCLBossAttack::HeavySlash);
	TestEqual(TEXT("Executable Heavy Slash score"), Decision.Score, 35.0F);

	Context.Phase = ESCLBossPhase::PhaseTwo;
	Context.AttackBeforePrevious = ESCLBossAttack::AreaAttack;
	Decision = SCLBossUtilityPolicy::SelectExecutableAttack(Context);
	TestTrue(
		TEXT("Phase Two Combo weight overtakes Heavy Slash after Light Slash"),
		Decision.Attack == ESCLBossAttack::Combo);
	TestEqual(TEXT("Phase Two repeat-aware Combo score"), Decision.Score, 50.0F);
	TestEqual(
		TEXT("Recent Area Attack receives a two-choice cooldown penalty"),
		SCLBossUtilityPolicy::ScoreAttack(ESCLBossAttack::AreaAttack, Context),
		35.0F);

	Context.DistanceToTarget = 320.0F;
	Context.Phase = ESCLBossPhase::PhaseOne;
	Context.AttackBeforePrevious = ESCLBossAttack::None;
	Context.PreviousAttack = ESCLBossAttack::None;
	Decision = SCLBossUtilityPolicy::SelectExecutableAttack(Context);
	TestTrue(
		TEXT("Executable subset retains Dash Slash at medium range"),
		Decision.Attack == ESCLBossAttack::DashSlash);
	Context.PreviousAttack = ESCLBossAttack::DashSlash;
	Decision = SCLBossUtilityPolicy::SelectExecutableAttack(Context);
	TestTrue(
		TEXT("At medium range repeat penalty never permits an unreachable melee attack"),
		Decision.Attack == ESCLBossAttack::DashSlash);

	const FSCLBossAttackExecutionProfile LightProfile =
		SCLBossUtilityPolicy::ResolveExecutionProfile(
			ESCLBossAttack::LightSlash,
			ESCLBossPhase::PhaseOne);
	Context.DistanceToTarget = 500.0F;
	TestTrue(TEXT("No attack selected beyond physical dash reach"),
		SCLBossUtilityPolicy::SelectExecutableAttack(Context).Attack == ESCLBossAttack::None);
	Context.DistanceToTarget = 320.0F;
	Context.Phase = ESCLBossPhase::PhaseTwo;
	TestTrue(TEXT("AOE cannot be selected beyond its radius"),
		SCLBossUtilityPolicy::ScoreAttack(ESCLBossAttack::AreaAttack, Context) < -1000.0F);
	TestTrue(TEXT("Delayed slash cannot be selected at dash range"),
		SCLBossUtilityPolicy::ScoreAttack(ESCLBossAttack::DelayedAttack, Context) < -1000.0F);
	const FSCLBossAttackExecutionProfile HeavyProfile =
		SCLBossUtilityPolicy::ResolveExecutionProfile(
			ESCLBossAttack::HeavySlash,
			ESCLBossPhase::PhaseOne);
	const FSCLBossAttackExecutionProfile DashProfile =
		SCLBossUtilityPolicy::ResolveExecutionProfile(
			ESCLBossAttack::DashSlash,
			ESCLBossPhase::PhaseOne);
	const FSCLBossAttackExecutionProfile ComboProfile =
		SCLBossUtilityPolicy::ResolveExecutionProfile(
			ESCLBossAttack::Combo,
			ESCLBossPhase::PhaseOne);
	const FSCLBossAttackExecutionProfile AreaProfile =
		SCLBossUtilityPolicy::ResolveExecutionProfile(
			ESCLBossAttack::AreaAttack,
			ESCLBossPhase::PhaseTwo);
	const FSCLBossAttackExecutionProfile DelayedProfile =
		SCLBossUtilityPolicy::ResolveExecutionProfile(
			ESCLBossAttack::DelayedAttack,
			ESCLBossPhase::PhaseOne);
	TestTrue(TEXT("Light Slash remains parryable"), LightProfile.bParryable);
	TestFalse(TEXT("Heavy Slash is unparryable"), HeavyProfile.bParryable);
	TestTrue(
		TEXT("Heavy Slash has the strongest damage multiplier"),
		HeavyProfile.DamageMultiplier > DashProfile.DamageMultiplier &&
			DashProfile.DamageMultiplier > LightProfile.DamageMultiplier);
	TestTrue(
		TEXT("Heavy Slash has the slowest Montage rate"),
		HeavyProfile.MontagePlayRate < LightProfile.MontagePlayRate);
	TestEqual(TEXT("Light Slash uses the first Montage section"), LightProfile.MontageStepIndex, 0);
	TestEqual(TEXT("Dash Slash uses the second Montage section"), DashProfile.MontageStepIndex, 1);
	TestEqual(TEXT("Heavy Slash uses the third Montage section"), HeavyProfile.MontageStepIndex, 2);
	TestTrue(TEXT("Dash Slash owns forward movement"), DashProfile.DashSpeed > 0.0F);
	TestEqual(TEXT("Combo automatically links all three Montage sections"), ComboProfile.MontageStepCount, 3);
	TestTrue(
		TEXT("Combo attacking state spans longer than one Light Slash"),
		ComboProfile.AttackStateDuration > LightProfile.AttackStateDuration);
	TestTrue(TEXT("Area Attack owns a real radius"), AreaProfile.AreaRadius > 0.0F);
	TestTrue(TEXT("Area Attack has a readable impact delay"), AreaProfile.AreaImpactDelay > 0.0F);
	TestFalse(TEXT("Area Attack suppresses the weapon trace"), AreaProfile.bWeaponTraceEnabled);
	TestFalse(TEXT("Area Attack is not parryable"), AreaProfile.bParryable);
	TestTrue(
		TEXT("Delayed Attack has a slower release than Heavy Slash"),
		DelayedProfile.MontagePlayRate < HeavyProfile.MontagePlayRate);
	TestTrue(TEXT("Delayed Attack remains parryable"), DelayedProfile.bParryable);

	const FSCLBossAttackExecutionProfile PhaseTwoDashProfile =
		SCLBossUtilityPolicy::ResolveExecutionProfile(
			ESCLBossAttack::DashSlash,
			ESCLBossPhase::PhaseTwo);
	TestTrue(
		TEXT("Phase Two increases attack playback speed"),
		PhaseTwoDashProfile.MontagePlayRate > DashProfile.MontagePlayRate);
	TestEqual(
		TEXT("Phase Two keeps the full counterattack recovery window"),
		PhaseTwoDashProfile.RecoveryDuration,
		DashProfile.RecoveryDuration);
	TestEqual(
		TEXT("Phase Two no longer adds extra damage"),
		PhaseTwoDashProfile.DamageMultiplier,
		DashProfile.DamageMultiplier);
	const FSCLBossAttackExecutionProfile PhaseOneAreaProfile =
		SCLBossUtilityPolicy::ResolveExecutionProfile(
			ESCLBossAttack::AreaAttack,
			ESCLBossPhase::PhaseOne);
	TestTrue(
		TEXT("Phase Two accelerates Area Attack impact timing"),
		AreaProfile.AreaImpactDelay < PhaseOneAreaProfile.AreaImpactDelay);

	const ASCLBossCharacter* const Boss = GetDefault<ASCLBossCharacter>();
	TestNotNull(TEXT("Boss Character CDO exists"), Boss);
	if (Boss == nullptr)
	{
		return false;
	}

	const USCLEnemyArchetypeData* const BossData = Boss->GetArchetypeData();
	TestTrue(
		TEXT("Boss Character selects the Boss archetype"),
		BossData != nullptr && BossData->IsA<USCLBossArchetypeData>());
	if (BossData != nullptr)
	{
		TestEqual(TEXT("Boss maximum health"), BossData->GetMaxHealth(), 400.0F);
		TestEqual(TEXT("Boss maximum poise"), BossData->GetMaxPoise(), 60.0F);
		TestEqual(TEXT("Boss damage scale"), BossData->GetDamageScale(), 0.90F);
	}
	TestTrue(TEXT("Boss starts in Phase One"), Boss->GetBossPhase() == ESCLBossPhase::PhaseOne);
	TestTrue(TEXT("Boss starts without a selected attack"), Boss->GetCurrentAttack() == ESCLBossAttack::None);
	TestEqual(TEXT("Boss enters combat at Utility medium range"), Boss->GetCombatEnterDistance(), 350.0F);
	TestEqual(TEXT("Boss combat exit preserves hysteresis"), Boss->GetCombatExitDistance(), 380.0F);
	TestTrue(
		TEXT("Boss range policy enters Combat at 350 centimeters"),
		SCLAIStatePolicy::ResolveTargetRangeState(
			350.0F,
			ESCLEnemyAIState::Chase,
			Boss->GetCombatEnterDistance(),
			Boss->GetCombatExitDistance()) == ESCLEnemyAIState::Combat);
	TestTrue(
		TEXT("Boss range policy remains in Combat through 380 centimeters"),
		SCLAIStatePolicy::ResolveTargetRangeState(
			380.0F,
			ESCLEnemyAIState::Combat,
			Boss->GetCombatEnterDistance(),
			Boss->GetCombatExitDistance()) == ESCLEnemyAIState::Combat);
	return true;
}

#endif
