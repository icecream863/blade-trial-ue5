#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Data/SCLPlayerMovesetData.h"
#include "Misc/Optional.h"

#include "SCLLightAttackAbility.generated.h"

class UGameplayEffect;

// 轻击 Ability 同时也是轻/重攻击 Ability 的公共基类。
// 玩家只做 GAS 资格预检查并提交输入；播放/费用/攻击状态统一归 Combat，连招选择归 Combo。
// 敌人暂时保留本类原有的 Section 成本提交流程，不与玩家缓存共用费用快照。
UCLASS()
class SOULCOMBATLAB_API USCLLightAttackAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	// 构造时配置 GAS 的实例化策略、网络预测策略和默认 GameplayEffect。
	USCLLightAttackAbility();

protected:
	// 返回本 Ability 代表的攻击分支。
	// 基类返回 Light，重击子类只需覆盖这里即可复用其余流程。
	virtual ESCLPlayerAttackInput GetAttackInput() const;

	// 激活资格检查：先执行 GAS 自身规则，再检查角色当前是否处于禁止攻击的状态。
	// 这里只回答“能不能激活”，不会修改体力，也不会播放攻击动画。
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// 费用预检查不产生副作用；玩家实际扣费归 Combat，敌人提交时还会再检查。
	// 因此不能假设这个函数只会被调用一次，也不能在这里产生持久化副作用。
	virtual bool CheckCost(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// 仅用于敌人的 CommitAbilityCost；玩家不能在这里重复扣费。
	// GameplayEffect 只提供“如何修改体力”，具体数值通过 SetByCaller 在运行时注入。
	virtual void ApplyCost(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) const override;

	// 玩家提交一次输入就结束；敌人执行后沿用 Ability 费用/状态提交。
	// Executed 表示本次已经起播；Buffered 表示只收下输入，本次 Ability 可以结束，后续由 Notify 推进。
	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	// 通过 AvatarActor 的战斗接口取得组件，避免 Ability 直接依赖某个具体角色类。
	class USCLCombatComponent* ResolveCombatComponent(const FGameplayAbilityActorInfo* ActorInfo) const;

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Effects")
	// 敌人旧流程的成本 Effect；玩家成本 Effect 位于 Combat|Player Effects。
	TSubclassOf<UGameplayEffect> StaminaCostEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Effects")
	// 敌人旧流程的状态 Effect；玩家攻击状态由 Combat 统一施加。
	TSubclassOf<UGameplayEffect> AttackingStateEffectClass;

	// 仅敌人使用：起播会改变 Section 节点，提交时使用起播前的成本。
	// Optional 区分“没有快照”与“合法零成本”；玩家不再需要这个临时状态。
	TOptional<float> EnemyAttackCostSnapshot;
};
