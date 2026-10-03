#include "AbilitySystem/Abilities/SCLHeavyAttackAbility.h"

#include "GameplayTags/SCLGameplayTags.h"

/**
 * 覆盖激活标签为重击；状态、成本和执行逻辑沿用轻击基类。
 */
USCLHeavyAttackAbility::USCLHeavyAttackAbility()
{
	// 重击只改变“技能身份”和“输入分支”，不复制轻击的激活、扣费和执行代码。
	// 这样轻重攻击的公共规则只维护一份，后续修复也会同时作用于两种攻击。
	FGameplayTagContainer Tags;
	Tags.AddTag(SCLGameplayTags::Ability_Attack_Heavy);
	SetAssetTags(Tags);
}

/**
 * 重击明确返回 Heavy，基类的成本查询和攻击请求都使用这个覆盖结果。
 */
ESCLPlayerAttackInput USCLHeavyAttackAbility::GetAttackInput() const
{
	// 基类的 CheckCost 和 ActivateAbility 都会调用这个函数，
	// 因此重击会自动查询重击节点并向 ComboComponent 请求 Heavy 分支。
	return ESCLPlayerAttackInput::Heavy;
}
