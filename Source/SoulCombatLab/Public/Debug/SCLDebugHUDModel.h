#pragma once

#include "CoreMinimal.h"

// 开发 HUD 一次刷新所需的数据快照；格式化函数只读取这份快照。
struct FSCLDebugHUDSnapshot
{
	float FramesPerSecond{0.0F};
	FString PlayerStateTags{TEXT("None")};
	float PlayerHealth{0.0F};
	float PlayerMaxHealth{0.0F};
	float PlayerStamina{0.0F};
	float PlayerMaxStamina{0.0F};
	FString CurrentAbility{TEXT("None")};
	FString LockedTarget{TEXT("None")};
	FString EnemyName{TEXT("None")};
	FString EnemyState{TEXT("None")};
	float EnemyDistance{0.0F};
	bool bHasPerceivedTarget{false};
	bool bHasLineOfSight{false};
	bool bBehaviorTreeRunning{false};
	bool bCanAttack{false};
	FString CurrentAttack{TEXT("None")};
	FString PathFollowingStatus{TEXT("Idle")};
	bool bHasEQSPoint{false};
	FVector EQSPoint{FVector::ZeroVector};
	float DistanceToEQSPoint{0.0F};
	FString BossPhase{TEXT("N/A")};
};

namespace SCLDebugHUDFormatter
{
	SOULCOMBATLAB_API FString BuildText(const FSCLDebugHUDSnapshot& Snapshot);
}
