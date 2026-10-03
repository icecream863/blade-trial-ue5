#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"

#include "SCLAbilitySystemComponent.generated.h"

/*
 * 项目的 GAS 组件，负责角色的“数值和状态层”：
 *
 *   Ability/Combat 请求效果
 *       -> GameplayEffect 修改 Health、Stamina、Poise
 *       -> 本类监听属性变化
 *       -> 触发死亡、失衡、破防、处决窗口或体力恢复
 *
 * 它不负责选择连招节点，也不直接播放攻击 Montage；玩家连招由
 * SCLPlayerComboComponent 选择，真正的动画生命周期由 SCLCombatComponent 管理。
 */
UCLASS(ClassGroup = "SoulCombatLab")
class SOULCOMBATLAB_API USCLAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	USCLAbilitySystemComponent();

	// 初始化 GAS 上下文并绑定属性/标签回调；重复初始化会先解除旧回调，避免重复触发。
	void InitializeAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor);
	// 按继承关系查找已授予技能：SCLBlockAbility 也能找到 GA_PlayerBlock 等蓝图子类。
	// 返回的是技能记录，不是类默认对象；调用方仍需检查 IsActive，再取 GetPrimaryInstance。
	// 优先返回正在运行的实例对应记录；无活动技能时返回首个已授予记录，便于检查配置。
	// 同一种动作只配置一个技能类：在 StartupAbilities 中替换父类，不同时添加父类和子类。
	FGameplayAbilitySpec* FindAbilitySpecByBaseClass(TSubclassOf<UGameplayAbility> AbilityBaseClass);
	// 消耗体力后重置恢复延时；不要在每个 Ability 中各自管理恢复计时器。
	void RestartStaminaRegenerationDelay();
	// 以下入口通过 GameplayEffect 修改自身属性，并返回实际应用的数值。
	// 传入的是正数“要扣多少”，内部会通过 SetByCaller 以负值应用到目标属性。
	float ApplyDamageToSelf(float DamageAmount, AActor* DamageCauser);
	float ApplyPoiseDamageToSelf(float PoiseDamageAmount, AActor* DamageCauser);
	float ApplyGuardDamageToSelf(float GuardDamageAmount, AActor* DamageCauser);
	void ApplyStaggeredState(FName Reason);
	void ApplyExecutableState();
	void ConsumeExecutableState();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Ability System|Stamina", meta = (ClampMin = "0.0", Units = "s"))
	float StaminaRegenDelay{1.5F};

	UPROPERTY(EditDefaultsOnly, Category = "Ability System|Stamina")
	TSubclassOf<UGameplayEffect> StaminaRegenEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ability System|Health")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ability System|Poise")
	TSubclassOf<UGameplayEffect> PoiseDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ability System|Poise")
	TSubclassOf<UGameplayEffect> StaggeredStateEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ability System|Guard")
	TSubclassOf<UGameplayEffect> GuardDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ability System|Execution")
	TSubclassOf<UGameplayEffect> ExecutableStateEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ability System|Health")
	TSubclassOf<UGameplayEffect> DeadStateEffectClass;

	// 体力恢复分成两个阶段：先等 Delay Timer，再保存并管理持续恢复 Effect。
	FTimerHandle StaminaRegenDelayHandle;
	FActiveGameplayEffectHandle ActiveStaminaRegenHandle;
	FDelegateHandle StaminaChangedDelegateHandle;
	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle PoiseChangedDelegateHandle;
	FDelegateHandle ExecutableTagChangedDelegateHandle;

	// Start/Stop 成对使用：Stop 同时清 Timer 和活动 Effect。
	void StartStaminaRegeneration();
	void StopStaminaRegeneration();
	// 属性回调把 GAS 数值变化转换为战斗规则，而不是每个攻击 Ability 各自判断。
	void HandleStaminaChanged(const FOnAttributeChangeData& ChangeData);
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandlePoiseChanged(const FOnAttributeChangeData& ChangeData);
	void HandleExecutableTagChanged(const FGameplayTag Tag, int32 NewCount);
	void CancelActiveCombatAction();
	float ApplyAttributeReductionToSelf(
		float ReductionAmount,
		AActor* EffectCauser,
		TSubclassOf<UGameplayEffect> EffectClass,
		const FGameplayTag& SetByCallerTag,
		const FGameplayAttribute& ReducedAttribute);
};
