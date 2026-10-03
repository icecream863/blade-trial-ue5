#include "AbilitySystem/Abilities/SCLLightAttackAbility.h"

#include "AbilitySystem/Effects/SCLLightAttackCostEffect.h"
#include "AbilitySystem/Effects/SCLAttackingStateEffect.h"
#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "Combat/SCLCombatComponent.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "Interfaces/SCLCombatInterface.h"
#include "SoulCombatLab.h"

/*
 * 轻/重攻击 Ability 的职责边界：
 *
 *   1. GAS 调用 CanActivateAbility / CheckCost，判断这次输入是否有资格进入攻击流程。
 *   2. ActivateAbility 把输入交给 CombatComponent。
 *   3. CombatComponent 决定玩家连招节点或敌人 Montage Section，并尝试起播。
 *   4. 玩家和敌人的扣费路径不同：
 *        - 玩家：真正起播时由 CombatComponent 扣费；
 *        - 敌人：起播后由本 Ability CommitAbilityCost -> ApplyCost 扣费。
 *
 * 玩家输入可能只返回 Buffered：这表示按键已保存到 PlayerComboComponent，
 * 当前 Ability 可以结束，但此时不能扣费；之后由 ComboWindow Notify 重新进入 Combat。
 */

/**
 * 配置轻击标签、实例化/网络执行策略和默认 Effect；重击子类复用执行流程。
 */
USCLLightAttackAbility::USCLLightAttackAbility()
{
	// HeavyAbility 继承本类，复用以下状态检查、成本检查和攻击请求流程。
	// 每个角色使用独立实例；敌人成本快照不会与其他角色共用。
	// InstancedPerActor 让每个角色拥有自己的 Ability 实例，敌人的费用快照不会互相覆盖。
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 本地玩家可以先响应输入，服务器随后对预测结果进行验证。
	// 本地玩家可以先响应输入，服务器随后对预测结果进行验证。
	// 这里描述的是 GAS 执行策略，不等于“攻击一定成功起播”。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	// 以下 Effect 仅供敌人的旧 Section 流程；玩家 Effect 配置在 CombatComponent。
	StaminaCostEffectClass = USCLLightAttackCostEffect::StaticClass();
	AttackingStateEffectClass = USCLAttackingStateEffect::StaticClass();

	FGameplayTagContainer DefaultAssetTags;
	DefaultAssetTags.AddTag(SCLGameplayTags::Ability_Attack_Light);
	SetAssetTags(DefaultAssetTags);
}

/**
 * 轻击固定返回 Light，重击子类覆盖为 Heavy，类型无需临时写进其他组件。
 */
ESCLPlayerAttackInput USCLLightAttackAbility::GetAttackInput() const
{
	return ESCLPlayerAttackInput::Light;
}

/**
 * GAS 激活前先执行父类检查，再排除躲闪、格挡、弹反、失衡和死亡状态。
 */
bool USCLLightAttackAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	const FGameplayTagContainer* const SourceTags,
	const FGameplayTagContainer* const TargetTags,
	FGameplayTagContainer* const OptionalRelevantTags) const
{
	// 第一关：先让 GAS 检查冷却、标签阻塞、授予条件等通用规则。
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* const AbilitySystem =
		ActorInfo != nullptr ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (AbilitySystem == nullptr)
	{
		// 没有 ASC 就无法读取状态标签，因此不能安全地放行攻击。
		return false;
	}

	// 第二关：这些是本项目额外规定的“攻击期间不能切入”的角色状态。
	// 例如处于格挡时不能同时激活攻击 Ability。
	FGameplayTagContainer BlockingStates;
	BlockingStates.AddTag(SCLGameplayTags::State_Dodging);
	BlockingStates.AddTag(SCLGameplayTags::State_Blocking);
	BlockingStates.AddTag(SCLGameplayTags::State_Parrying);
	BlockingStates.AddTag(SCLGameplayTags::State_ParryAction);
	BlockingStates.AddTag(SCLGameplayTags::State_Staggered);
	BlockingStates.AddTag(SCLGameplayTags::State_Dead);
	// 只要命中其中任意一个状态，HasAnyMatchingGameplayTags 就会返回 true。
	// 这里只读状态，不修改标签、体力或连招节点。
	return !AbilitySystem->HasAnyMatchingGameplayTags(BlockingStates);
}

/**
 * 只读费用预检查，不扣费、不推进连招。
 * 玩家起播时由 Combat 再检查一次，防止输入缓存等待期间体力发生变化。
 * 敌人提交时使用起播前快照，保留原 Section 流程的行为。
 */
bool USCLLightAttackAbility::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	FGameplayTagContainer* const OptionalRelevantTags) const
{
	// GAS 可能多次调用 CheckCost，因此这里必须是纯查询：
	// 不能推进连招、缓存输入、应用 Effect 或启动 Montage。
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	const USCLCombatComponent* const CombatComponent = ResolveCombatComponent(ActorInfo);
	const UAbilitySystemComponent* const AbilitySystem =
		ActorInfo != nullptr ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	/*
	 * 玩家通常读取 Combat 当前计算出的下一招成本。
	 * 敌人如果已经保存了快照，则优先使用快照：
	 *
	 *   读取成本 -> RequestAttack 推进 Section -> CommitAbilityCost
	 *
	 * 如果第二步之后重新查询，当前节点可能已经变了，读到的就会是下一段成本。
	 */
	const float StaminaCost = EnemyAttackCostSnapshot.IsSet()
		? EnemyAttackCostSnapshot.GetValue()
		: (CombatComponent != nullptr ? CombatComponent->GetNextAttackStaminaCost(GetAttackInput()) : 0.0F);
	// 体力始终从 ASC 的属性集中读取，避免 Ability 保存一份过期体力值。
	// 注意：CheckCost 返回 true 只表示“检查时够用”，不是最终扣费成功保证。
	return CombatComponent != nullptr && AbilitySystem != nullptr &&
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()) >= StaminaCost;
}

/**
 * 仅用于敌人的 Ability 成本提交。玩家费用已经由 Combat 起播函数负责，不能在这里再扣一次。
 * 保留 Ability 的 Spec 创建/应用入口，维持敌人原有的 AbilityLevel 与预测上下文。
 */
void USCLLightAttackAbility::ApplyCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// ApplyCost 是 GAS 提交 Ability 费用时的入口。
	// 玩家不走这里：玩家的输入可能在 Ability 结束后才由 ComboWindow 消费，
	// 因此玩家费用必须绑定在 Combat 真正起播的时刻。
	const USCLCombatComponent* const CombatComponent = ResolveCombatComponent(ActorInfo);
	if (CombatComponent == nullptr || CombatComponent->UsesPlayerMoveset())
	{
		return;
	}
	// 保留父类调用，允许 GAS 的通用费用配置正常执行；本项目的敌人具体体力值
	// 则由下面的 SetByCaller 注入。
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	USCLAbilitySystemComponent* const AbilitySystem = ActorInfo != nullptr
		? Cast<USCLAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get())
		: nullptr;
	// Optional 没有值表示本次没有成功建立敌人攻击费用快照，
	// 不能猜测当前节点再扣费，否则可能扣错攻击段的成本。
	const float StaminaCost = EnemyAttackCostSnapshot.IsSet() ? EnemyAttackCostSnapshot.GetValue() : 0.0F;
	if (AbilitySystem == nullptr || StaminaCostEffectClass == nullptr || StaminaCost <= 0.0F)
	{
		// 缺少执行对象，或者本次攻击不需要扣费时，不创建无效的 Effect Spec。
		return;
	}

	const FGameplayEffectSpecHandle CostSpec = MakeOutgoingGameplayEffectSpec(
		Handle,
		ActorInfo,
		ActivationInfo,
		StaminaCostEffectClass,
		GetAbilityLevel(Handle, ActorInfo));
	if (!CostSpec.IsValid())
	{
		// Spec 创建失败时不能继续 Apply，否则会造成攻击与扣费状态不一致。
		return;
	}

	// Cost Effect 以加法修改体力，传负值才是扣除；不直接写属性基值。
	// 通过 GameplayEffect 扣费还能触发 ASC 的体力变化回调和恢复延迟逻辑。
	CostSpec.Data->SetSetByCallerMagnitude(SCLGameplayTags::Data_Cost_Stamina, -StaminaCost);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CostSpec);
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("GAS stamina cost applied: Owner=%s Cost=%.2f RemainingStamina=%.2f"),
		*GetNameSafe(ActorInfo->AvatarActor.Get()),
		StaminaCost,
		AbilitySystem->GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()));
	AbilitySystem->RestartStaminaRegenerationDelay();
}

/**
 * 玩家：资格检查已由 GAS 完成，这里只提交输入；Combat 在真正起播时统一扣费和施加状态。
 * 敌人：沿用原来的 Section 攻击与 Ability 成本提交，避免本轮重构改变 AI 战斗规则。
 * 两条路径显式分开，玩家不再保存费用快照，也不再执行 CommitAbilityCost。
 */
void USCLLightAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* const ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* const TriggerEventData)
{
	// Super 只完成 GAS Ability 的基础激活流程；真正的攻击动画仍由 Combat 决定。
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	USCLCombatComponent* const Combat = ResolveCombatComponent(ActorInfo);
	if (Combat == nullptr)
	{
		// 没有战斗组件时，Ability 没有办法把输入变成攻击请求，必须以失败结束。
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (Combat->UsesPlayerMoveset())
	{
		/*
		 * 玩家路径：
		 *   RequestAttack
		 *      ├─ Rejected  -> 没有起播，也没有缓存
		 *      ├─ Buffered  -> 缓存输入，等待 ComboWindow，当前 Ability 结束
		 *      └─ Executed  -> Combat 已起播并完成玩家费用/状态提交
		 *
		 * 因为 Buffered 的后续执行发生在当前 Ability 结束之后，
		 * 这里不能调用 CommitAbilityCost，否则会出现“尚未攻击就扣费”。
		 */
		const ESCLAttackRequestResult Result = Combat->RequestAttack(GetAttackInput());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, Result == ESCLAttackRequestResult::Rejected);
		return;
	}

	/*
	 * 敌人路径：
	 *   1. 先读取当前 Section 对应的成本；
	 *   2. 再请求 Combat 推进并播放 Section；
	 *   3. 起播成功后 CommitAbilityCost；
	 *   4. GAS 进入 ApplyCost，使用第 1 步保存的快照。
	 *
	 * 敌人目前沿用 Ability 持有费用/状态提交的旧流程，
	 * 所以不能把玩家的“窗口延迟接招”规则直接套到这里。
	 */
	EnemyAttackCostSnapshot = Combat->GetNextAttackStaminaCost(GetAttackInput());
	const ESCLAttackRequestResult Result = Combat->RequestAttack(GetAttackInput());
	if (Result == ESCLAttackRequestResult::Executed)
	{
		// RequestAttack 已经起播后才提交费用；提交失败必须取消刚刚开始的攻击，
		// 否则会出现“动画播放了但没有扣费”的免费攻击。
		if (!CommitAbilityCost(Handle, ActorInfo, ActivationInfo))
		{
			Combat->CancelActiveAttack();
			EnemyAttackCostSnapshot.Reset();
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		if (AttackingStateEffectClass != nullptr)
		{
			// 敌人的 Attacking 标签由 Ability Effect 维护，持续时间与 Combat 的攻击状态一致。
			const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
				Handle, ActorInfo, ActivationInfo, AttackingStateEffectClass, GetAbilityLevel(Handle, ActorInfo));
			if (Spec.IsValid())
			{
				Spec.Data->SetDuration(Combat->GetActiveAttackStateDuration(), true);
				ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
			}
		}
	}
	// 快照只服务于这一次敌人 Ability 激活；无论成功、拒绝还是提交失败都必须清除。
	EnemyAttackCostSnapshot.Reset();
	// Rejected 才是 Ability 意义上的失败；Buffered 是成功接收输入，只是延迟执行。
	EndAbility(Handle, ActorInfo, ActivationInfo, true, Result == ESCLAttackRequestResult::Rejected);
}

/**
 * 从 GAS AvatarActor 的战斗接口取得组件；ActorInfo 可为空，查询失败返回 nullptr。
 */
USCLCombatComponent* USCLLightAttackAbility::ResolveCombatComponent(
	const FGameplayAbilityActorInfo* const ActorInfo) const
{
	AActor* const AvatarActor = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	return AvatarActor != nullptr && AvatarActor->GetClass()->ImplementsInterface(USCLCombatInterface::StaticClass())
		? ISCLCombatInterface::Execute_GetCombatComponent(AvatarActor)
		: nullptr;
}
