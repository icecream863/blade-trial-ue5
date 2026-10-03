#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "SCLExecutableStateEffect.generated.h"

// 限时授予可处决与失衡状态的 GameplayEffect。
UCLASS()
class SOULCOMBATLAB_API USCLExecutableStateEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit USCLExecutableStateEffect(const FObjectInitializer& ObjectInitializer);
};
