#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "SCLAttackingStateEffect.generated.h"

// 限时授予 State.Attacking 的 GameplayEffect，标记正在攻击。
UCLASS()
class SOULCOMBATLAB_API USCLAttackingStateEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit USCLAttackingStateEffect(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Gameplay Effect")
	bool GrantsAttackingState() const;
};
