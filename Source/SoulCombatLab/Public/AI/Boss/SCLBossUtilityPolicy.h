#pragma once

#include "AI/Boss/SCLBossTypes.h"
#include "CoreMinimal.h"

// Boss 选招策略：按阶段、距离和历史评分，输出战斗组件可执行的攻击配置。
namespace SCLBossUtilityPolicy
{
	inline constexpr float PhaseTwoHealthFraction{0.50F};

	SOULCOMBATLAB_API ESCLBossPhase ResolvePhase(float CurrentHealth, float MaxHealth);
	SOULCOMBATLAB_API bool IsAttackAvailable(ESCLBossAttack Attack, ESCLBossPhase Phase);
	SOULCOMBATLAB_API float ScoreAttack(ESCLBossAttack Attack, const FSCLBossUtilityContext& Context);
	SOULCOMBATLAB_API FSCLBossAttackDecision SelectAttack(const FSCLBossUtilityContext& Context);
	SOULCOMBATLAB_API FSCLBossAttackDecision SelectExecutableAttack(const FSCLBossUtilityContext& Context);
	SOULCOMBATLAB_API FSCLBossAttackExecutionProfile ResolveExecutionProfile(
		ESCLBossAttack Attack,
		ESCLBossPhase Phase);
	SOULCOMBATLAB_API const TCHAR* GetPhaseName(ESCLBossPhase Phase);
	SOULCOMBATLAB_API const TCHAR* GetAttackName(ESCLBossAttack Attack);
}
