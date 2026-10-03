#pragma once

#include "CoreMinimal.h"

namespace SCLAbilitySystemPolicy
{
	constexpr bool ShouldCompleteStaminaRegeneration(
		const float OldStamina,
		const float NewStamina,
		const float MaxStamina)
	{
		return MaxStamina > 0.0F &&
			OldStamina < MaxStamina &&
			NewStamina >= MaxStamina;
	}
}
