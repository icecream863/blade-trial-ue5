#include "UI/SCLBossUIModel.h"

namespace SCLBossUIPolicy
{
	FSCLBossUIState ResolveState(
		const float CurrentHealth,
		const float MaxHealth,
		const ESCLBossPhase Phase)
	{
		FSCLBossUIState State;
		State.HealthFraction = MaxHealth > UE_SMALL_NUMBER
			? FMath::Clamp(CurrentHealth / MaxHealth, 0.0F, 1.0F)
			: 0.0F;
		if (Phase == ESCLBossPhase::PhaseTwo)
		{
			State.PhaseLabel = TEXT("PHASE II");
			State.PhaseColor = FLinearColor{1.0F, 0.24F, 0.05F, 1.0F};
		}
		return State;
	}

	bool ShouldShowPhaseTransition(
		const ESCLBossPhase PreviousPhase,
		const ESCLBossPhase NewPhase)
	{
		return PreviousPhase != NewPhase && NewPhase == ESCLBossPhase::PhaseTwo;
	}
}
