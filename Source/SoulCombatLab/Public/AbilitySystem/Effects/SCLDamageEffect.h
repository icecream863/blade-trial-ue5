#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "SCLDamageEffect.generated.h"

// 即时生命伤害效果；伤害数值由调用方通过 SetByCaller 传入。
UCLASS()
class SOULCOMBATLAB_API USCLDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USCLDamageEffect();
};
