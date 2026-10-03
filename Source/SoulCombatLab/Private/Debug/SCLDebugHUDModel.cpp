#include "Debug/SCLDebugHUDModel.h"

namespace SCLDebugHUDFormatter
{
	FString BuildText(const FSCLDebugHUDSnapshot& Snapshot)
	{
		const FString EQSDescription = Snapshot.bHasEQSPoint
			? FString::Printf(
				TEXT("(%.0f, %.0f, %.0f)  Delta=%.0f cm"),
				Snapshot.EQSPoint.X,
				Snapshot.EQSPoint.Y,
				Snapshot.EQSPoint.Z,
				Snapshot.DistanceToEQSPoint)
			: TEXT("None");
		return FString::Printf(
			TEXT(
				"FPS: %.1f\n"
				"\nPLAYER\n"
				"  State Tags: %s\n"
				"  Health: %.0f / %.0f\n"
				"  Stamina: %.0f / %.0f\n"
				"  Current Ability: %s\n"
				"  Locked Target: %s\n"
				"\nAI: %s\n"
				"  Perception: Target=%s  LOS=%s\n"
				"  Behavior Tree: %s\n"
				"  Blackboard State: %s\n"
				"  Distance: %.1f cm  CanAttack=%s\n"
				"  Current Attack: %s\n"
				"  Reposition: %s\n"
				"  EQS Point: %s\n"
				"  Boss Phase: %s"),
			FMath::Max(Snapshot.FramesPerSecond, 0.0F),
			*Snapshot.PlayerStateTags,
			FMath::Max(Snapshot.PlayerHealth, 0.0F),
			FMath::Max(Snapshot.PlayerMaxHealth, 0.0F),
			FMath::Max(Snapshot.PlayerStamina, 0.0F),
			FMath::Max(Snapshot.PlayerMaxStamina, 0.0F),
			*Snapshot.CurrentAbility,
			*Snapshot.LockedTarget,
			*Snapshot.EnemyName,
			Snapshot.bHasPerceivedTarget ? TEXT("true") : TEXT("false"),
			Snapshot.bHasLineOfSight ? TEXT("true") : TEXT("false"),
			Snapshot.bBehaviorTreeRunning ? TEXT("Running") : TEXT("Stopped"),
			*Snapshot.EnemyState,
			FMath::Max(Snapshot.EnemyDistance, 0.0F),
			Snapshot.bCanAttack ? TEXT("true") : TEXT("false"),
			*Snapshot.CurrentAttack,
			*Snapshot.PathFollowingStatus,
			*EQSDescription,
			*Snapshot.BossPhase);
	}
}
