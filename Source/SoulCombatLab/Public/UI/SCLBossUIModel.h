#pragma once

#include "AI/Boss/SCLBossTypes.h"
#include "CoreMinimal.h"

// Boss UI 一次刷新所需的状态数据。
struct FSCLBossUIState
{
	float HealthFraction{0.0F};
	FName PhaseLabel{TEXT("PHASE I")};
	FLinearColor PhaseColor{FLinearColor::White};
};

namespace SCLBossUIPolicy
{
	SOULCOMBATLAB_API FSCLBossUIState ResolveState(
		float CurrentHealth,
		float MaxHealth,
		ESCLBossPhase Phase);
	SOULCOMBATLAB_API bool ShouldShowPhaseTransition(
		ESCLBossPhase PreviousPhase,
		ESCLBossPhase NewPhase);
}
