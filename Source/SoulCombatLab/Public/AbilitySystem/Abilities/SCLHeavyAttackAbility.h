#pragma once

#include "AbilitySystem/Abilities/SCLLightAttackAbility.h"
#include "SCLHeavyAttackAbility.generated.h"

// 重击标签对应的 Ability。
// 它采用模板方法式的继承：公共流程在轻击基类中，重击只覆盖需要变化的输入类型。
UCLASS()
class SOULCOMBATLAB_API USCLHeavyAttackAbility : public USCLLightAttackAbility
{
	GENERATED_BODY()

public:
	// 配置重击专属的 Ability Tag。
	USCLHeavyAttackAbility();

protected:
	// 告诉基类本次请求应查询并执行 Heavy 连招分支。
	virtual ESCLPlayerAttackInput GetAttackInput() const override;
};
