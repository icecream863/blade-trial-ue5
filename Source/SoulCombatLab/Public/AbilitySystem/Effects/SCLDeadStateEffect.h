#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "SCLDeadStateEffect.generated.h"

// 持续授予 State.Dead 的 GameplayEffect，标记角色死亡。
UCLASS()
class SOULCOMBATLAB_API USCLDeadStateEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit USCLDeadStateEffect(const FObjectInitializer& ObjectInitializer);
};
