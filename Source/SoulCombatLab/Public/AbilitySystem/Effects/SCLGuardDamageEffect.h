#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "SCLGuardDamageEffect.generated.h"

// 即时扣除格挡体力的 GameplayEffect，格挡伤害由调用方传入。
UCLASS()
class SOULCOMBATLAB_API USCLGuardDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USCLGuardDamageEffect();
};
