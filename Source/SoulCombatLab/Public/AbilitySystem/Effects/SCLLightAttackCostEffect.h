#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "SCLLightAttackCostEffect.generated.h"

// 即时扣除攻击体力的 GameplayEffect，成本由调用方传入。
UCLASS()
class SOULCOMBATLAB_API USCLLightAttackCostEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USCLLightAttackCostEffect();
};
