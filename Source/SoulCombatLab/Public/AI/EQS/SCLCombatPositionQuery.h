#pragma once

#include "CoreMinimal.h"

class UEnvQuery;

namespace SCLCombatPositionQuery
{
	inline constexpr float PreferredDistance{185.0F};
	inline constexpr float MinimumDistance{150.0F};
	inline constexpr float MaximumDistance{230.0F};
	inline constexpr int32 CandidatePointCount{12};

	SOULCOMBATLAB_API UEnvQuery* Build(UObject& Outer);
}
