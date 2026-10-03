#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "SCLBlockingStateEffect.generated.h"

// 持续授予 State.Blocking 的 GameplayEffect，标记正在格挡。
UCLASS()
class SOULCOMBATLAB_API USCLBlockingStateEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit USCLBlockingStateEffect(const FObjectInitializer& ObjectInitializer);
};
