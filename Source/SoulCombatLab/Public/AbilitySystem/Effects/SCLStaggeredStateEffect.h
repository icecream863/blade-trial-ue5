#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "SCLStaggeredStateEffect.generated.h"

// 限时授予 State.Staggered 的 GameplayEffect，标记失衡。
UCLASS()
class SOULCOMBATLAB_API USCLStaggeredStateEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit USCLStaggeredStateEffect(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Gameplay Effect")
	bool GrantsStaggeredState() const;
};
