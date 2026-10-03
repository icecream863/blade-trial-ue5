#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "SCLDodgeCostEffect.generated.h"

// 即时扣除闪避体力的 GameplayEffect，成本由调用方传入。
UCLASS()
class SOULCOMBATLAB_API USCLDodgeCostEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USCLDodgeCostEffect();
};
