#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "SCLDodgingStateEffect.generated.h"

// 持续授予 State.Dodging 的 GameplayEffect，标记闪避状态。
UCLASS()
class SOULCOMBATLAB_API USCLDodgingStateEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit USCLDodgingStateEffect(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Gameplay Effect")
	bool GrantsDodgingState() const;
};
