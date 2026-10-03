#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "SCLPoiseDamageEffect.generated.h"

// 即时扣除韧性的 GameplayEffect，削韧值由调用方传入。
UCLASS()
class SOULCOMBATLAB_API USCLPoiseDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USCLPoiseDamageEffect();
};
