#pragma once

#include "CoreMinimal.h"

#include "SCLAIState.generated.h"

// 行为树和黑板共用的敌人状态；状态名是决策标签，不等于某个动画已经播放。
UENUM(BlueprintType)
enum class ESCLEnemyAIState : uint8
{
	Idle,
	Patrol,
	Investigate,
	Chase,
	Combat,
	Attack,
	Retreat,
	Staggered,
	Dead
};

// 根据距离和前一状态计算追击/战斗状态；进入和退出阈值不同，避免边界抖动。
namespace SCLAIStatePolicy
{
	inline constexpr float CombatEnterDistance{220.0F};
	inline constexpr float CombatExitDistance{280.0F};

	SOULCOMBATLAB_API ESCLEnemyAIState ResolveTargetRangeState(
		float DistanceToTarget,
		ESCLEnemyAIState PreviousState);
	SOULCOMBATLAB_API ESCLEnemyAIState ResolveTargetRangeState(
		float DistanceToTarget,
		ESCLEnemyAIState PreviousState,
		float EnterDistance,
		float ExitDistance);
	SOULCOMBATLAB_API bool ShouldRetainUnseenTarget(
		float DistanceToTarget,
		float AwarenessDistance);
}

// 原生行为树与 AI Controller 共用的黑板键名。
namespace SCLBlackboardKeys
{
	SOULCOMBATLAB_API extern const FName TargetActor;
	SOULCOMBATLAB_API extern const FName DistanceToTarget;
	SOULCOMBATLAB_API extern const FName HasLineOfSight;
	SOULCOMBATLAB_API extern const FName CombatState;
	SOULCOMBATLAB_API extern const FName CanAttack;
	SOULCOMBATLAB_API extern const FName IdealCombatLocation;
}
