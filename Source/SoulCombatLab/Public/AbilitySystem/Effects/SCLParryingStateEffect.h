#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "SCLParryingStateEffect.generated.h"

// 持续授予 State.Parrying 的 GameplayEffect，供弹反判定读取。
UCLASS()
class SOULCOMBATLAB_API USCLParryingStateEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit USCLParryingStateEffect(const FObjectInitializer& ObjectInitializer);
};
