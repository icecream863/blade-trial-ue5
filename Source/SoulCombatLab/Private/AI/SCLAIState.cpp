#include "AI/SCLAIState.h"

namespace SCLAIStatePolicy
{
	ESCLEnemyAIState ResolveTargetRangeState(
		const float DistanceToTarget,
		const ESCLEnemyAIState PreviousState)
	{
		return ResolveTargetRangeState(
			DistanceToTarget,
			PreviousState,
			CombatEnterDistance,
			CombatExitDistance);
	}

	ESCLEnemyAIState ResolveTargetRangeState(
		const float DistanceToTarget,
		const ESCLEnemyAIState PreviousState,
		const float EnterDistance,
		const float ExitDistance)
	{
		const bool bWasInCloseCombat = PreviousState == ESCLEnemyAIState::Combat ||
			PreviousState == ESCLEnemyAIState::Attack;
		const float SafeEnterDistance = FMath::Max(EnterDistance, 0.0F);
		const float SafeExitDistance = FMath::Max(ExitDistance, SafeEnterDistance);
		const float ChaseThreshold = bWasInCloseCombat
			? SafeExitDistance
			: SafeEnterDistance;

		return DistanceToTarget <= ChaseThreshold
			? ESCLEnemyAIState::Combat
			: ESCLEnemyAIState::Chase;
	}

	bool ShouldRetainUnseenTarget(
		const float DistanceToTarget,
		const float AwarenessDistance)
	{
		return AwarenessDistance > 0.0F &&
			DistanceToTarget >= 0.0F &&
			DistanceToTarget <= AwarenessDistance;
	}
}

namespace SCLBlackboardKeys
{
	const FName TargetActor{TEXT("TargetActor")};
	const FName DistanceToTarget{TEXT("DistanceToTarget")};
	const FName HasLineOfSight{TEXT("HasLineOfSight")};
	const FName CombatState{TEXT("CombatState")};
	const FName CanAttack{TEXT("CanAttack")};
	const FName IdealCombatLocation{TEXT("IdealCombatLocation")};
}
