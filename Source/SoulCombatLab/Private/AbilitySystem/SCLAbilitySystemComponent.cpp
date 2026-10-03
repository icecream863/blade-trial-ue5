#include "AbilitySystem/SCLAbilitySystemComponent.h"
#include "Characters/Player/SCLPlayerCharacter.h"
#include "Characters/Components/SCLHitReactionComponent.h"

#include "AbilitySystem/Effects/SCLDamageEffect.h"
#include "AbilitySystem/Effects/SCLDeadStateEffect.h"
#include "AbilitySystem/Effects/SCLExecutableStateEffect.h"
#include "AbilitySystem/Effects/SCLGuardDamageEffect.h"
#include "AbilitySystem/Effects/SCLPoiseDamageEffect.h"
#include "AbilitySystem/Effects/SCLStaggeredStateEffect.h"
#include "AbilitySystem/Effects/SCLStaminaRegenEffect.h"
#include "AbilitySystem/SCLAttributeSet.h"
#include "AbilitySystem/SCLAbilitySystemPolicy.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Characters/SCLCharacterBase.h"
#include "Combat/SCLCombatComponent.h"
#include "Engine/World.h"
#include "GameplayTags/SCLGameplayTags.h"
#include "Interfaces/SCLCombatInterface.h"
#include "SoulCombatLab.h"
#include "TimerManager.h"

/*
 * 这个组件是角色 GAS 层与战斗层之间的“状态总线”：
 *
 *   GameplayEffect 修改 Health/Stamina/Poise
 *             ↓
 *   属性变化回调在这里统一收口
 *             ↓
 *   死亡、失衡、破防、处决窗口和体力恢复等状态被触发
 *
 * 攻击动画本身仍由 SCLCombatComponent 管理；本类只在属性或状态标签
 * 发生变化时通知/打断战斗，避免每个 Ability 各自监听同一套属性。
 */

USCLAbilitySystemComponent::USCLAbilitySystemComponent()
{
	// 这些默认类也可以在蓝图中替换；代码只保存“要应用哪一种 Effect”。
	SetIsReplicatedByDefault(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	StaminaRegenEffectClass = USCLStaminaRegenEffect::StaticClass();
	DamageEffectClass = USCLDamageEffect::StaticClass();
	PoiseDamageEffectClass = USCLPoiseDamageEffect::StaticClass();
	StaggeredStateEffectClass = USCLStaggeredStateEffect::StaticClass();
	GuardDamageEffectClass = USCLGuardDamageEffect::StaticClass();
	ExecutableStateEffectClass = USCLExecutableStateEffect::StaticClass();
	DeadStateEffectClass = USCLDeadStateEffect::StaticClass();
}

FGameplayAbilitySpec* USCLAbilitySystemComponent::FindAbilitySpecByBaseClass(
	const TSubclassOf<UGameplayAbility> AbilityBaseClass)
{
	if (AbilityBaseClass == nullptr) return nullptr;
	// 与引擎 FindAbilitySpecFromClass 的区别在 IsA：引擎入口只匹配完全相同的类。
	// 原生父类只描述动作接口，实际执行对象可以来自任意层级的配置蓝图子类。
	ABILITYLIST_SCOPE_LOCK();
	FGameplayAbilitySpec* FirstMatch = nullptr;
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.Ability != nullptr && !Spec.PendingRemove && Spec.Ability->IsA(AbilityBaseClass))
		{
			// 同一动作可能同时存在未激活的 Spec 和正在运行的 Spec；
			// 优先返回活动项，调用者可以继续读取这一次动作的实例状态。
			if (Spec.IsActive()) return &Spec;
			if (FirstMatch == nullptr) FirstMatch = &Spec;
		}
	}
	return FirstMatch;
}

void USCLAbilitySystemComponent::InitializeAbilityActorInfo(AActor* const InOwnerActor, AActor* const InAvatarActor)
{
	// OwnerActor 通常是拥有 ASC 的对象，AvatarActor 是实际执行动作的角色。
	// 先初始化 GAS 的基础上下文，再注册属性/标签回调，否则回调可能读不到正确 Avatar。
	InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	// 可能在重生、重新附身或测试中重复初始化，因此每次绑定前先解除旧句柄。
	FOnGameplayAttributeValueChange& StaminaChanged =
		GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetStaminaAttribute());
	if (StaminaChangedDelegateHandle.IsValid())
	{
		StaminaChanged.Remove(StaminaChangedDelegateHandle);
	}
	StaminaChangedDelegateHandle = StaminaChanged.AddUObject(
		this,
		&USCLAbilitySystemComponent::HandleStaminaChanged);

	FOnGameplayAttributeValueChange& HealthChanged =
		GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetHealthAttribute());
	if (HealthChangedDelegateHandle.IsValid())
	{
		HealthChanged.Remove(HealthChangedDelegateHandle);
	}
	HealthChangedDelegateHandle = HealthChanged.AddUObject(
		this,
		&USCLAbilitySystemComponent::HandleHealthChanged);

	FOnGameplayAttributeValueChange& PoiseChanged =
		GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetPoiseAttribute());
	if (PoiseChangedDelegateHandle.IsValid())
	{
		PoiseChanged.Remove(PoiseChangedDelegateHandle);
	}
	PoiseChangedDelegateHandle = PoiseChanged.AddUObject(
		this,
		&USCLAbilitySystemComponent::HandlePoiseChanged);

	FOnGameplayEffectTagCountChanged& ExecutableTagChanged = RegisterGameplayTagEvent(
		SCLGameplayTags::State_Executable,
		EGameplayTagEventType::NewOrRemoved);
	if (ExecutableTagChangedDelegateHandle.IsValid())
	{
		ExecutableTagChanged.Remove(ExecutableTagChangedDelegateHandle);
	}
	ExecutableTagChangedDelegateHandle = ExecutableTagChanged.AddUObject(
		this,
		&USCLAbilitySystemComponent::HandleExecutableTagChanged);
}

void USCLAbilitySystemComponent::RestartStaminaRegenerationDelay()
{
	// 任何消耗体力的行为都会调用这里：先移除正在恢复的 Effect，
	// 再重新等待一段时间，形成“最后一次消耗后延迟恢复”的效果。
	StopStaminaRegeneration();

	UWorld* const World = GetWorld();
	if (World == nullptr ||
		StaminaRegenEffectClass == nullptr ||
		GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()) >=
			GetNumericAttribute(USCLAttributeSet::GetMaxStaminaAttribute()))
	{
		return;
	}

	if (StaminaRegenDelay <= 0.0F)
	{
		// 允许把延迟配置为 0，便于测试或实现立即恢复。
		StartStaminaRegeneration();
		return;
	}

	// Timer 只负责延迟，不负责恢复数值；真正的恢复由 StaminaRegenEffect 完成。
	World->GetTimerManager().SetTimer(
		StaminaRegenDelayHandle,
		this,
		&USCLAbilitySystemComponent::StartStaminaRegeneration,
		StaminaRegenDelay,
		false);
}

float USCLAbilitySystemComponent::ApplyDamageToSelf(const float DamageAmount, AActor* const DamageCauser)
{
	// 三个公开入口只负责选择不同的 Effect、SetByCaller 标签和目标属性；
	// 实际的 GAS Spec 创建与应用集中在 ApplyAttributeReductionToSelf。
	return ApplyAttributeReductionToSelf(
		DamageAmount,
		DamageCauser,
		DamageEffectClass,
		SCLGameplayTags::Data_Damage,
		USCLAttributeSet::GetHealthAttribute());
}

float USCLAbilitySystemComponent::ApplyPoiseDamageToSelf(
	const float PoiseDamageAmount,
	AActor* const DamageCauser)
{
	return ApplyAttributeReductionToSelf(
		PoiseDamageAmount,
		DamageCauser,
		PoiseDamageEffectClass,
		SCLGameplayTags::Data_Damage_Poise,
		USCLAttributeSet::GetPoiseAttribute());
}

float USCLAbilitySystemComponent::ApplyGuardDamageToSelf(
	const float GuardDamageAmount,
	AActor* const DamageCauser)
{
	return ApplyAttributeReductionToSelf(
		GuardDamageAmount,
		DamageCauser,
		GuardDamageEffectClass,
		SCLGameplayTags::Data_Damage_Guard,
		USCLAttributeSet::GetStaminaAttribute());
}

void USCLAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 先清理 Timer、活动 Effect 和所有委托，避免对象销毁后仍有回调访问 this。
	StopStaminaRegeneration();
	if (StaminaChangedDelegateHandle.IsValid())
	{
		GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetStaminaAttribute())
			.Remove(StaminaChangedDelegateHandle);
		StaminaChangedDelegateHandle.Reset();
	}
	if (HealthChangedDelegateHandle.IsValid())
	{
		GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedDelegateHandle);
		HealthChangedDelegateHandle.Reset();
	}
	if (PoiseChangedDelegateHandle.IsValid())
	{
		GetGameplayAttributeValueChangeDelegate(USCLAttributeSet::GetPoiseAttribute())
			.Remove(PoiseChangedDelegateHandle);
		PoiseChangedDelegateHandle.Reset();
	}
	if (ExecutableTagChangedDelegateHandle.IsValid())
	{
		RegisterGameplayTagEvent(
			SCLGameplayTags::State_Executable,
			EGameplayTagEventType::NewOrRemoved)
			.Remove(ExecutableTagChangedDelegateHandle);
		ExecutableTagChangedDelegateHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void USCLAbilitySystemComponent::StartStaminaRegeneration()
{
	// Blocking 状态下不启动恢复；Timer 触发后仍要再次检查，因为状态可能在等待期间改变。
	if (StaminaRegenEffectClass == nullptr || HasMatchingGameplayTag(SCLGameplayTags::State_Blocking))
	{
		return;
	}

	const FGameplayEffectSpecHandle RegenSpec = MakeOutgoingSpec(
		StaminaRegenEffectClass,
		1.0F,
		MakeEffectContext());
	if (!RegenSpec.IsValid())
	{
		return;
	}

	// 保存句柄是为了在满体力、再次消耗或 EndPlay 时精准移除这一个持续 Effect。
	ActiveStaminaRegenHandle = ApplyGameplayEffectSpecToSelf(*RegenSpec.Data.Get());
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("GAS stamina regeneration started: Owner=%s Stamina=%.2f/%.2f"),
		*GetNameSafe(GetAvatarActor()),
		GetNumericAttribute(USCLAttributeSet::GetStaminaAttribute()),
		GetNumericAttribute(USCLAttributeSet::GetMaxStaminaAttribute()));
}

void USCLAbilitySystemComponent::StopStaminaRegeneration()
{
	// Stop 同时取消“尚未触发”的延迟 Timer 和“已经生效”的恢复 Effect。
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaminaRegenDelayHandle);
	}

	if (ActiveStaminaRegenHandle.IsValid())
	{
		const FActiveGameplayEffectHandle EffectHandle = ActiveStaminaRegenHandle;
		ActiveStaminaRegenHandle.Invalidate();
		RemoveActiveGameplayEffect(EffectHandle);
	}
}

void USCLAbilitySystemComponent::HandleStaminaChanged(const FOnAttributeChangeData& ChangeData)
{
	// 体力同时承担普通资源和格挡耐力的角色，因此一个回调处理两条独立规则：
	//   1. 格挡中降到 0：Guard Break；
	//   2. 体力恢复到上限：移除恢复 Effect。
	if (ChangeData.OldValue > 0.0F &&
		ChangeData.NewValue <= 0.0F &&
		HasMatchingGameplayTag(SCLGameplayTags::State_Blocking))
	{
		// 破防必须先取消格挡 Ability，否则 State.Blocking 可能继续存在。
		ApplyStaggeredState(TEXT("GuardBreak"));

		FGameplayTagContainer BlockAbilityTags;
		BlockAbilityTags.AddTag(SCLGameplayTags::Ability_Block);
		CancelAbilities(&BlockAbilityTags);
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("Guard broken: Owner=%s Stamina=%.2f State=State.Staggered"),
			*GetNameSafe(GetAvatarActor()),
			ChangeData.NewValue);
	}

	const float MaxStamina = GetNumericAttribute(USCLAttributeSet::GetMaxStaminaAttribute());
	if (SCLAbilitySystemPolicy::ShouldCompleteStaminaRegeneration(
		ChangeData.OldValue,
		ChangeData.NewValue,
		MaxStamina))
	{
		StopStaminaRegeneration();
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("GAS stamina regeneration completed: Owner=%s Stamina=%.2f/%.2f"),
			*GetNameSafe(GetAvatarActor()),
			ChangeData.NewValue,
			MaxStamina);
	}
}

void USCLAbilitySystemComponent::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	// 只在“从正数跨到 0 或以下”的瞬间处理死亡，避免后续重复写入 0 时重复执行清理。
	if (ChangeData.OldValue > 0.0F && ChangeData.NewValue <= 0.0F)
	{
		StopStaminaRegeneration();
		if (DeadStateEffectClass != nullptr &&
			!HasMatchingGameplayTag(SCLGameplayTags::State_Dead))
		{
			// 死亡状态由 Effect 提供标签、持续时间和其他 GAS 行为，不直接写状态字段。
			const FGameplayEffectSpecHandle DeadStateSpec = MakeOutgoingSpec(
				DeadStateEffectClass,
				1.0F,
				MakeEffectContext());
			if (DeadStateSpec.IsValid())
			{
				ApplyGameplayEffectSpecToSelf(*DeadStateSpec.Data.Get());
			}
		}
		// 三步分别处理 GAS Ability、CombatComponent 动画/状态和处决窗口。
		CancelAllAbilities();
		CancelActiveCombatAction();
		ConsumeExecutableState();
		UE_LOG(
			LogSoulCombatLab,
			Log,
			TEXT("GAS death: Owner=%s Health=%.2f State=State.Dead"),
			*GetNameSafe(GetAvatarActor()),
			ChangeData.NewValue);
	}
}

void USCLAbilitySystemComponent::HandlePoiseChanged(const FOnAttributeChangeData& ChangeData)
{
	// Poise 只有在正数跨到 0 或以下时才算真正失衡；普通的削韧过程不打断动作。
	if (ChangeData.OldValue <= 0.0F || ChangeData.NewValue > 0.0F)
	{
		return;
	}

	ApplyStaggeredState(TEXT("PoiseBreak"));
	const float MaxPoise = GetNumericAttribute(USCLAttributeSet::GetMaxPoiseAttribute());
	// 敌人失衡后进入短暂处决窗口；玩家失衡后直接恢复 Poise，等待下一次削韧。
	if (GetAvatarActor() != nullptr && !GetAvatarActor()->IsA<ASCLPlayerCharacter>()) ApplyExecutableState();
	else SetNumericAttributeBase(USCLAttributeSet::GetPoiseAttribute(), MaxPoise);
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("GAS poise broken: Owner=%s State=State.Staggered MaxPoise=%.2f"),
		*GetNameSafe(GetAvatarActor()),
		MaxPoise);
}

void USCLAbilitySystemComponent::HandleExecutableTagChanged(
	const FGameplayTag Tag,
	const int32 NewCount)
{
	// GameplayTag 是 GAS 中的事实来源；角色表现层只订阅这里同步执行标记。
	if (ASCLCharacterBase* const Character = Cast<ASCLCharacterBase>(GetAvatarActor()))
	{
		Character->SetExecutionMarkerActive(NewCount > 0);
	}
	if (NewCount == 0 && !HasMatchingGameplayTag(SCLGameplayTags::State_Dead))
	{
		// 处决窗口结束后恢复 Poise；死亡时不恢复，避免死角色情况被重新激活。
		SetNumericAttributeBase(USCLAttributeSet::GetPoiseAttribute(), GetNumericAttribute(USCLAttributeSet::GetMaxPoiseAttribute()));
	}
}

void USCLAbilitySystemComponent::ApplyStaggeredState(const FName Reason)
{
	// 失衡可能由削韧或破防触发。两者都共享同一个 Staggered Effect，
	// 只有“削韧失衡”额外播放受击反应，Guard Break 由格挡流程表现。
	if (StaggeredStateEffectClass == nullptr ||
		HasMatchingGameplayTag(SCLGameplayTags::State_Staggered))
	{
		return;
	}

	// 失衡是战斗中断事件，必须先停掉 Montage、连招和 Attacking Effect。
	CancelActiveCombatAction();

	const FGameplayEffectSpecHandle StaggeredSpec = MakeOutgoingSpec(
		StaggeredStateEffectClass,
		1.0F,
		MakeEffectContext());
	if (!StaggeredSpec.IsValid())
	{
		return;
	}

	ApplyGameplayEffectSpecToSelf(*StaggeredSpec.Data.Get());
	if (Reason == TEXT("PoiseBreak"))
	{
		if (ASCLCharacterBase* const Character = Cast<ASCLCharacterBase>(GetAvatarActor()))
		{
			if (auto* Reaction = Character->GetHitReactionComponent()) Reaction->PlayStaggerReaction();
		}
	}
	UE_LOG(
		LogSoulCombatLab,
		Verbose,
		TEXT("GAS staggered state applied: Owner=%s Reason=%s"),
		*GetNameSafe(GetAvatarActor()),
		*Reason.ToString());
}

void USCLAbilitySystemComponent::CancelActiveCombatAction()
{
	// GAS 状态和 Combat 动画不是同一套系统，取消时必须显式通知两边。
	AActor* const CombatAvatar = GetAvatarActor();
	if (CombatAvatar != nullptr &&
		CombatAvatar->GetClass()->ImplementsInterface(USCLCombatInterface::StaticClass()))
	{
		if (USCLCombatComponent* const CombatComponent =
			ISCLCombatInterface::Execute_GetCombatComponent(CombatAvatar))
		{
			CombatComponent->CancelActiveAttack();
		}
	}

	FGameplayTagContainer AttackingTags;
	AttackingTags.AddTag(SCLGameplayTags::State_Attacking);
	// 移除残留的攻击状态，防止动画已取消但角色仍被视为攻击中。
	RemoveActiveEffectsWithGrantedTags(AttackingTags);
}

void USCLAbilitySystemComponent::ApplyExecutableState()
{
	// 处决状态开始前也要中断当前攻击；它代表“目标可被处决”，不是普通受击。
	if (ExecutableStateEffectClass == nullptr ||
		HasMatchingGameplayTag(SCLGameplayTags::State_Dead))
	{
		return;
	}

	CancelActiveCombatAction();
	// 同一个执行窗口不叠加旧 Effect，重新开始时以这次应用为准。
	ConsumeExecutableState();
	const FGameplayEffectSpecHandle ExecutableStateSpec = MakeOutgoingSpec(
		ExecutableStateEffectClass,
		1.0F,
		MakeEffectContext());
	if (!ExecutableStateSpec.IsValid())
	{
		return;
	}

	ApplyGameplayEffectSpecToSelf(*ExecutableStateSpec.Data.Get());
	UE_LOG(
		LogSoulCombatLab,
		Log,
		TEXT("Execution window started: Owner=%s Duration=4.00 State=State.Executable"),
		*GetNameSafe(GetAvatarActor()));
}

void USCLAbilitySystemComponent::ConsumeExecutableState()
{
	// 通过授予标签移除所有处决 Effect；调用者不需要保存某个具体 Effect 句柄。
	FGameplayTagContainer ExecutableTags;
	ExecutableTags.AddTag(SCLGameplayTags::State_Executable);
	RemoveActiveEffectsWithGrantedTags(ExecutableTags);
}

float USCLAbilitySystemComponent::ApplyAttributeReductionToSelf(
	const float ReductionAmount,
	AActor* const EffectCauser,
	const TSubclassOf<UGameplayEffect> EffectClass,
	const FGameplayTag& SetByCallerTag,
	const FGameplayAttribute& ReducedAttribute)
{
	/*
	 * 伤害/削韧/破防都走同一条 GAS 路径：
	 *
	 *   计算好的正数 ReductionAmount
	 *        ↓（SetByCaller 写入负值）
	 *   GameplayEffect 修改目标属性
	 *        ↓
	 *   属性回调触发死亡、失衡或破防
	 *
	 * EffectCauser 的 ASC 作为 Source，用于保留来源和触发正确的上下文；
	 * 如果攻击者没有 ASC（例如环境伤害），则退回由目标自己的 ASC 创建 Spec。
	 */
	if (ReductionAmount <= 0.0F || EffectClass == nullptr)
	{
		return 0.0F;
	}

	const float PreviousValue = GetNumericAttribute(ReducedAttribute);
	if (PreviousValue <= 0.0F)
	{
		// 目标属性已经耗尽时不再创建 Effect，避免重复触发边界状态。
		return 0.0F;
	}

	UAbilitySystemComponent* SourceAbilitySystem =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(EffectCauser);
	if (SourceAbilitySystem == nullptr)
	{
		// 没有攻击者 ASC 时仍允许环境伤害或测试调用生效。
		SourceAbilitySystem = this;
	}

	FGameplayEffectContextHandle EffectContext = SourceAbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(EffectCauser);
	FGameplayEffectSpecHandle ReductionSpec = SourceAbilitySystem->MakeOutgoingSpec(
		EffectClass,
		1.0F,
		EffectContext);
	if (!ReductionSpec.IsValid())
	{
		return 0.0F;
	}

	// 这里传负值，因为目标属性的 GameplayEffect 通常以加法修改属性。
	ReductionSpec.Data->SetSetByCallerMagnitude(SetByCallerTag, -ReductionAmount);
	SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*ReductionSpec.Data.Get(), this);
	// 返回“实际扣掉的最大值”，而不是盲目返回请求值。
	return FMath::Min(ReductionAmount, PreviousValue);
}
