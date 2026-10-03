#include "AI/Boss/SCLBossUtilityPolicy.h"

#include <array>
#include <limits>

namespace
{
constexpr float CloseRangeCentimeters{200.0F};
constexpr float MediumRangeCentimeters{600.0F};
constexpr float MeleeAttackMaximumRangeCentimeters{180.0F};
constexpr float RepeatedAttackPenalty{40.0F};
constexpr float RecentAttackPenalty{25.0F};
constexpr float UnavailableScore{-10000.0F};

constexpr std::array<ESCLBossAttack, 6> CandidateAttacks{
	ESCLBossAttack::LightSlash,
	ESCLBossAttack::HeavySlash,
	ESCLBossAttack::Combo,
	ESCLBossAttack::DashSlash,
	ESCLBossAttack::AreaAttack,
	ESCLBossAttack::DelayedAttack};

constexpr std::array<ESCLBossAttack, 6> ExecutableAttacks{
	ESCLBossAttack::LightSlash,
	ESCLBossAttack::HeavySlash,
	ESCLBossAttack::Combo,
	ESCLBossAttack::DashSlash,
	ESCLBossAttack::AreaAttack,
	ESCLBossAttack::DelayedAttack};

template <std::size_t CandidateCount>
FSCLBossAttackDecision SelectFromCandidates(
	const FSCLBossUtilityContext& Context,
	const std::array<ESCLBossAttack, CandidateCount>& Candidates)
{
	FSCLBossAttackDecision BestDecision;
	BestDecision.Score = -std::numeric_limits<float>::max();
	for (const ESCLBossAttack Candidate : Candidates)
	{
		const float CandidateScore = SCLBossUtilityPolicy::ScoreAttack(Candidate, Context);
		if (CandidateScore > UnavailableScore && CandidateScore > BestDecision.Score)
		{
			BestDecision.Attack = Candidate;
			BestDecision.Score = CandidateScore;
		}
	}
	return BestDecision;
}
}

ESCLBossPhase SCLBossUtilityPolicy::ResolvePhase(const float CurrentHealth, const float MaxHealth)
{
	if (MaxHealth <= UE_SMALL_NUMBER)
	{
		return ESCLBossPhase::PhaseOne;
	}

	const float HealthFraction = FMath::Clamp(CurrentHealth / MaxHealth, 0.0F, 1.0F);
	return HealthFraction <= PhaseTwoHealthFraction
		? ESCLBossPhase::PhaseTwo
		: ESCLBossPhase::PhaseOne;
}

bool SCLBossUtilityPolicy::IsAttackAvailable(const ESCLBossAttack Attack, const ESCLBossPhase Phase)
{
	if (Attack == ESCLBossAttack::None)
	{
		return false;
	}

	return Attack != ESCLBossAttack::AreaAttack || Phase == ESCLBossPhase::PhaseTwo;
}

float SCLBossUtilityPolicy::ScoreAttack(
	const ESCLBossAttack Attack,
	const FSCLBossUtilityContext& Context)
{
	if (!IsAttackAvailable(Attack, Context.Phase))
	{
		return UnavailableScore;
	}
	const float Distance = FMath::Max(Context.DistanceToTarget, 0.0F);
	const float MaximumRange = Attack == ESCLBossAttack::DashSlash ? 350.0F
		: Attack == ESCLBossAttack::AreaAttack ? 240.0F : MeleeAttackMaximumRangeCentimeters;
	if (Distance > MaximumRange)
	{
		return UnavailableScore;
	}

	float Score = 0.0F;
	switch (Attack)
	{
	case ESCLBossAttack::LightSlash:
		Score = 20.0F;
		break;
	case ESCLBossAttack::HeavySlash:
		Score = 15.0F;
		break;
	case ESCLBossAttack::Combo:
		Score = 10.0F;
		break;
	case ESCLBossAttack::DashSlash:
		Score = 5.0F;
		break;
	case ESCLBossAttack::AreaAttack:
		Score = 15.0F;
		break;
	case ESCLBossAttack::DelayedAttack:
		Score = 8.0F;
		break;
	case ESCLBossAttack::None:
	default:
		return UnavailableScore;
	}

	if (Distance < CloseRangeCentimeters)
	{
		switch (Attack)
		{
		case ESCLBossAttack::LightSlash:
			Score += 30.0F;
			break;
		case ESCLBossAttack::HeavySlash:
			Score += 20.0F;
			break;
		case ESCLBossAttack::Combo:
			Score += 20.0F;
			break;
		case ESCLBossAttack::AreaAttack:
			Score += 45.0F;
			break;
		default:
			break;
		}
	}
	else if (Distance <= MediumRangeCentimeters)
	{
		switch (Attack)
		{
		case ESCLBossAttack::HeavySlash:
			Score += 10.0F;
			break;
		case ESCLBossAttack::DashSlash:
			Score += 50.0F;
			break;
		case ESCLBossAttack::AreaAttack:
			Score += 25.0F;
			break;
		case ESCLBossAttack::DelayedAttack:
			Score += 20.0F;
			break;
		default:
			break;
		}
	}
	else
	{
		if (Attack == ESCLBossAttack::DashSlash)
		{
			Score += 70.0F;
		}
		else if (Attack == ESCLBossAttack::DelayedAttack)
		{
			Score += 15.0F;
		}
	}

	if (Context.Phase == ESCLBossPhase::PhaseTwo)
	{
		switch (Attack)
		{
		case ESCLBossAttack::HeavySlash:
			Score += 5.0F;
			break;
		case ESCLBossAttack::Combo:
			Score += 20.0F;
			break;
		case ESCLBossAttack::DelayedAttack:
			Score += 25.0F;
			break;
		default:
			break;
		}
	}

	if (Attack == Context.PreviousAttack)
	{
		Score -= RepeatedAttackPenalty;
	}
	if (Attack != ESCLBossAttack::None && Attack == Context.AttackBeforePrevious)
	{
		Score -= RecentAttackPenalty;
	}
	return Score;
}

FSCLBossAttackDecision SCLBossUtilityPolicy::SelectAttack(const FSCLBossUtilityContext& Context)
{
	return SelectFromCandidates(Context, CandidateAttacks);
}

FSCLBossAttackDecision SCLBossUtilityPolicy::SelectExecutableAttack(
	const FSCLBossUtilityContext& Context)
{
	return SelectFromCandidates(Context, ExecutableAttacks);
}

FSCLBossAttackExecutionProfile SCLBossUtilityPolicy::ResolveExecutionProfile(
	const ESCLBossAttack Attack,
	const ESCLBossPhase Phase)
{
	FSCLBossAttackExecutionProfile Profile;
	Profile.Attack = Attack;
	if (Attack == ESCLBossAttack::LightSlash)
	{
		Profile.DamageMultiplier = 1.0F;
		Profile.PoiseDamageMultiplier = 1.0F;
		Profile.MontagePlayRate = 1.10F;
		Profile.AttackStateDuration = 0.90F;
		Profile.RecoveryDuration = 1.20F;
	}
	else if (Attack == ESCLBossAttack::HeavySlash)
	{
		Profile.DamageMultiplier = 1.60F;
		Profile.PoiseDamageMultiplier = 2.0F;
		Profile.MontagePlayRate = 0.80F;
		Profile.MontageStepIndex = 2;
		Profile.AttackStateDuration = 1.25F;
		Profile.RecoveryDuration = 1.90F;
		Profile.bParryable = false;
	}
	else if (Attack == ESCLBossAttack::DashSlash)
	{
		Profile.DamageMultiplier = 1.25F;
		Profile.PoiseDamageMultiplier = 1.25F;
		Profile.MontagePlayRate = 1.20F;
		Profile.MontageStepIndex = 1;
		Profile.AttackStateDuration = 0.90F;
		Profile.RecoveryDuration = 1.50F;
		Profile.DashSpeed = 900.0F;
		Profile.DashDuration = 0.22F;
	}
	else if (Attack == ESCLBossAttack::Combo)
	{
		Profile.DamageMultiplier = 0.85F;
		Profile.PoiseDamageMultiplier = 0.80F;
		Profile.MontagePlayRate = 1.15F;
		Profile.MontageStepCount = 3;
		Profile.AttackStateDuration = 2.40F;
		Profile.RecoveryDuration = 1.90F;
	}
	else if (Attack == ESCLBossAttack::AreaAttack)
	{
		Profile.DamageMultiplier = 1.35F;
		Profile.PoiseDamageMultiplier = 2.50F;
		Profile.MontagePlayRate = 0.85F;
		Profile.MontageStepIndex = 2;
		Profile.AttackStateDuration = 1.35F;
		Profile.RecoveryDuration = 2.10F;
		Profile.AreaRadius = 260.0F;
		Profile.AreaImpactDelay = 0.72F;
		Profile.bParryable = false;
		Profile.bWeaponTraceEnabled = false;
	}
	else if (Attack == ESCLBossAttack::DelayedAttack)
	{
		Profile.DamageMultiplier = 1.45F;
		Profile.PoiseDamageMultiplier = 1.50F;
		Profile.MontagePlayRate = 0.55F;
		Profile.MontageStepIndex = 2;
		Profile.AttackStateDuration = 1.75F;
		Profile.RecoveryDuration = 1.60F;
	}

	if (Phase == ESCLBossPhase::PhaseTwo)
	{
		constexpr float PhaseTwoDamageScale{1.0F};
		constexpr float PhaseTwoSpeedScale{1.05F};
		constexpr float PhaseTwoRecoveryScale{1.0F};
		Profile.DamageMultiplier *= PhaseTwoDamageScale;
		Profile.MontagePlayRate *= PhaseTwoSpeedScale;
		Profile.AttackStateDuration /= PhaseTwoSpeedScale;
		Profile.RecoveryDuration *= PhaseTwoRecoveryScale;
		Profile.DashSpeed *= PhaseTwoSpeedScale;
		Profile.AreaImpactDelay /= PhaseTwoSpeedScale;
	}
	return Profile;
}

const TCHAR* SCLBossUtilityPolicy::GetPhaseName(const ESCLBossPhase Phase)
{
	return Phase == ESCLBossPhase::PhaseTwo ? TEXT("PhaseTwo") : TEXT("PhaseOne");
}

const TCHAR* SCLBossUtilityPolicy::GetAttackName(const ESCLBossAttack Attack)
{
	switch (Attack)
	{
	case ESCLBossAttack::LightSlash:
		return TEXT("LightSlash");
	case ESCLBossAttack::HeavySlash:
		return TEXT("HeavySlash");
	case ESCLBossAttack::Combo:
		return TEXT("Combo");
	case ESCLBossAttack::DashSlash:
		return TEXT("DashSlash");
	case ESCLBossAttack::AreaAttack:
		return TEXT("AreaAttack");
	case ESCLBossAttack::DelayedAttack:
		return TEXT("DelayedAttack");
	case ESCLBossAttack::None:
	default:
		return TEXT("None");
	}
}
