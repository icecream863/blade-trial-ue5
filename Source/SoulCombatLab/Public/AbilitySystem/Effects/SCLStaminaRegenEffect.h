#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "SCLStaminaRegenEffect.generated.h"

// 持续恢复体力的 GameplayEffect。
UCLASS()
class SOULCOMBATLAB_API USCLStaminaRegenEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USCLStaminaRegenEffect();
};
