#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AnimEnums.h"
#include "Data/SCLPlayerMovesetData.h"
#include "Combat/SCLAttackRequestResult.h"
#include "Combat/SCLMontageComboState.h"

#include "SCLCombatComponent.generated.h"

class ASCLWeapon;
class ASCLPlayerCharacter;
class UAnimMontage;
class USkeletalMeshComponent;
enum class EVisibilityBasedAnimTickOption : uint8;
class USCLAttackData;
struct FHitResult;
struct FAnimNotifyEventReference;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSCLWeaponHitSignature,
	AActor*,
	HitActor,
	const FHitResult&,
	HitResult);

// 一次攻击的运行时参数。敌人策略可在播放前覆盖整套配置，玩家招式也会写入其中的名称和倍率字段。
struct FSCLRuntimeAttackProfile
{
	FName AttackName{NAME_None};
	float DamageMultiplier{1.0F};
	float PoiseDamageMultiplier{1.0F};
	float MontagePlayRate{1.0F};
	int32 MontageStepIndex{0};
	int32 MontageStepCount{1};
	float AttackStateDuration{1.0F};
	float AreaRadius{0.0F};
	float AreaImpactDelay{0.0F};
	bool bParryable{true};
	bool bWeaponTraceEnabled{true};
};

// 战斗编排与结算组件：决定何时起播和扣费，处理武器命中及统一伤害结算。
// 一次攻击的播放、通知、范围命中与取消在本类连续阅读；共享移动规则仍向 ActionMovement 申请。
// 玩家由 PlayerComboComponent 选择 Moveset 节点，敌人由 MontageComboState 安排 AttackData 的 Montage Section。
UCLASS(ClassGroup = "SoulCombatLab", meta = (BlueprintSpawnableComponent))
class SOULCOMBATLAB_API USCLCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USCLCombatComponent();

	UFUNCTION(BlueprintCallable, Category = "Combat|Trace")
	/** WeaponTrace Notify Begin 调用；成功开启采样和挥刀效果才返回 true。 */
	bool BeginWeaponTrace();

	UFUNCTION(BlueprintCallable, Category = "Combat|Trace")
	/** 仅在命中窗口的 Notify Tick 更新扫掠，组件自身没有常驻 Tick。 */
	// Notify 提供窗口末尾；卡顿越过末尾时按时间比例截断最后一段，不延长有效帧。
	void TickWeaponTrace(float AnimationWindowEnd = -1.0F);
	// 自然结束等本帧骨骼完成后收尾；中断立即关闭，不追加检测。
	void FinishWeaponTraceWindow(float AnimationWindowEnd, bool bReachedEnd);

	UFUNCTION(BlueprintCallable, Category = "Combat|Trace")
	/** 停止检测和挥刀效果；窗口结束、取消和接招都需要调用。 */
	void EndWeaponTrace();

	UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
	/** 旧蓝图兼容入口，返回“是否接收”；正常玩家输入从角色发起 GAS 技能。 */
	bool StartLightAttack();

	// 显式攻击入口：玩家按 Input 选择连招分支，敌人使用自身攻击配置。
	// 玩家费用在本组件实际起播时统一提交；Ability 不再对玩家请求重复扣费。
	ESCLAttackRequestResult RequestAttack(ESCLPlayerAttackInput Input);
	// 玩家使用独立 Moveset 与统一起播提交；敌人保留原有 Ability/Section 成本流程。
	bool UsesPlayerMoveset() const;
	// 兼容已有敌人和直接轻击调用；玩家 GAS 路径使用上面的显式参数版本。
	bool RequestAttack();
	// 当前仍持有攻击数据，包括 Montage 已结束但仍等待范围定时命中的阶段。
	bool IsAttackActive() const { return ActiveAttackData != nullptr; }
	// 只接收当前 Montage 实例的通知；同一资产重新起播会有不同实例 ID。
	bool IsActiveAttackNotify(const FAnimNotifyEventReference& EventReference) const;
	// AI 指定前摇面向目标，弱引用避免目标销毁后留下悬空指针。
	void SetAttackTarget(AActor* Target) { AttackTarget = Target; }
	// 动画通知的统一入口：玩家消费 Moveset 缓存，敌人安排 Section 后继。
	void OpenComboWindow();
	void CloseComboWindow();

	UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
	/** 撤销动画、轨迹、缓存、攻击配置与移动限制；只停 Montage 不能完成取消。 */
	void CancelActiveAttack();

	UFUNCTION(BlueprintPure, Category = "Combat|Attack")
	float GetLightAttackStaminaCost() const;
	// 只读费用预检查；实际玩家费用直接读取已选节点，不重新查询其后继。
	float GetNextAttackStaminaCost(ESCLPlayerAttackInput Input) const;
	// 基础敌人参数与一次性 Pending 配置分开，普通攻击不会继承上次特殊招式。
	void ConfigureEnemyAttackProfile(float DamageScale, bool bParryable, float WeaponVisualScale);
	// 只设置下一次攻击的 Pending 配置；真正起播时复制为 Active，并清掉 Pending。
	void ConfigureNextAttackProfile(const FSCLRuntimeAttackProfile& AttackProfile);
	bool IsActiveAttackParryable() const;
	float GetActiveAttackStateDuration() const;
	float GetEnemyDamageScale() const { return EnemyDamageScale; }
	float GetEnemyWeaponVisualScale() const { return EnemyWeaponVisualScale; }
	FName GetActiveAttackName() const
	{
		if (bHasActiveAttackProfile && !ActiveAttackProfile.AttackName.IsNone())
		{
			return ActiveAttackProfile.AttackName;
		}
		return ActiveAttackData != nullptr ? FName{TEXT("LightAttack")} : NAME_None;
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Weapon")
	ASCLWeapon* GetEquippedWeapon() const { return EquippedWeapon; }

	UFUNCTION(BlueprintPure, Category = "Combat|Weapon")
	TSubclassOf<ASCLWeapon> GetDefaultWeaponClass() const { return DefaultWeaponClass; }
	void SetDefaultWeaponClass(const TSubclassOf<ASCLWeapon> InWeaponClass) { DefaultWeaponClass = InWeaponClass; }
	void SetWeaponAttachSocket(const FName InSocketName) { WeaponAttachSocket = InSocketName; }

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	// 共享命中处理结束后通知外部监听者；Trace 的同窗去重在武器组件内完成。
	FSCLWeaponHitSignature OnWeaponHit;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 每个命中窗口独立记录采样时间，Begin 重置；用于末帧轨迹的线性截断。
	struct FWeaponTraceSamplingState
	{
		float SampleTime{0.0F};
		float WindowEnd{-1.0F};
		int32 InstanceId{INDEX_NONE};
		bool bPending{false};
		bool bEndAfterPose{false};
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		// 仅窗口活动时保证背对镜头也更新刀刃骨骼，关闭后恢复原来的可见性优化。
		EVisibilityBasedAnimTickOption OriginalTickOption{};
		FDelegateHandle PoseReadyHandle;
	} TraceSampling;
	// 回调只在命中窗口绑定，不添加组件 Tick；骨骼完成后才读取本帧 Socket。
	void SampleWeaponTraceAfterPose(int32 InstanceId);
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Weapon")
	TSubclassOf<ASCLWeapon> DefaultWeaponClass;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Weapon")
	FName WeaponAttachSocket{TEXT("hand_r")};

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Attack")
	// 共享基础攻击数据；敌人用它查 Montage Section，玩家用它提供共享伤害参数，另用 Moveset 选择动画。
	TObjectPtr<USCLAttackData> LightAttackData;

	UPROPERTY(Transient)
	TObjectPtr<ASCLWeapon> EquippedWeapon;

	UPROPERTY(Transient)
	// 当前攻击关联的共享基础数据；范围攻击可能在 Montage 结束后继续保留到延迟命中完成。
	TObjectPtr<USCLAttackData> ActiveAttackData;

	// 敌人 Section 连段状态独立管理，不参与玩家选招。
	FSCLMontageComboState EnemyCombo;
	float EnemyDamageScale{1.0F};
	float EnemyWeaponVisualScale{1.0F};
	bool bAttacksParryable{true};
	// Pending 为下一次准备，Active 为本次执行，配套 bool 表示配置是否有效。
	FSCLRuntimeAttackProfile PendingAttackProfile;
	FSCLRuntimeAttackProfile ActiveAttackProfile;
	bool bHasPendingAttackProfile{false};
	bool bHasActiveAttackProfile{false};
	/*
	 * 当前攻击 Montage 的运行时播放状态。
	 *
	 * 这里记录的是“某一次 Montage 播放实例”，不是攻击配置本身：
	 *   - ActiveAttackData / ActiveAttackProfile：这次攻击使用什么数据；
	 *   - Playback：这次攻击当前播放到哪里、由哪个 AnimInstance 播放。
	 *
	 * 一次攻击大致经历：
	 *   Montage_Play
	 *       -> 保存 Montage、AnimInstance 和 InstanceId
	 *       -> 绑定 BlendingOut / End 回调
	 *       -> 回调到来时用 InstanceId 校验是否仍是当前攻击
	 *       -> 取消或结束时清空这些运行时信息
	 *
	 * 两次连续播放可能使用同一个 Montage 资产，所以不能只比较 Montage 指针。
	 */
	struct FAttackPlaybackState
	{
		// 当前播放的 Montage 资产。弱引用只用于查询和比较，不负责持有资源生命周期。
		TWeakObjectPtr<UAnimMontage> Montage;

		// 实际执行 Montage_Play 的动画实例；同一个角色通常来自其 SkeletalMesh。
		TWeakObjectPtr<class UAnimInstance> AnimInstance;

		// 攻击开始时记录的朝向目标，例如敌人起手时的目标；
		// 播放过程中目标变化不会自动改写这份快照。
		TWeakObjectPtr<AActor> FacingTarget;

		/*
		 * 当前 Montage 播放实例的唯一身份。
		 *
		 * INDEX_NONE 表示当前没有被 Combat 追踪的攻击播放。
		 * Montage 重新播放时，即使资产相同，AnimInstance 也会生成新的 InstanceId。
		 * 旧实例的延迟回调如果携带旧 ID，就不能清理新攻击的状态。
		 */
		int32 InstanceId{INDEX_NONE};

		/*
		 * 前摇计时器。部分攻击会在前摇期间持续朝向目标，
		 * 计时器结束后由 Combat 停止这段辅助逻辑。
		 * 它不是 Montage 的结束计时器，Montage 生命周期由动画委托负责。
		 */
		FTimerHandle WindupTimer;

		// 已经过的前摇时间，供每次 Timer/更新逻辑判断当前处于前摇的哪一段。
		float WindupElapsed{0.0F};

		// 只根据实例身份判断 Combat 是否正在追踪一段攻击播放。
		bool IsPlaying() const { return InstanceId != INDEX_NONE; }
	};

	// 当前攻击的播放快照；攻击取消或结束时由 Combat 清理。
	FAttackPlaybackState Playback;
	// 范围查询有独立结束时刻，但由同一个 Combat 拥有；Pending 在命中遍历期间也保持 true。
	struct FAreaImpactState
	{
		FTimerHandle Timer;
		float Radius{0.0F};
		bool bPending{false};
		uint64 Generation{0}; // 开始/取消改变代次，伤害回调返回后检查是否还属于本次查询。
	};
	FAreaImpactState AreaImpact;
	TWeakObjectPtr<AActor> AttackTarget; // AI 下一次起手目标，播放时复制到 Playback.FacingTarget。
	// 仅玩家锁定攻击使用：把 Warp Target 放在目标前方的停刀距离；超出校正上限时照常播放原招。
	UPROPERTY(EditDefaultsOnly, Category="Combat|Motion Warping", meta=(ClampMin="0.0", Units="cm"))
	float AttackWarpStandOff{110.0F};
	UPROPERTY(EditDefaultsOnly, Category="Combat|Motion Warping", meta=(ClampMin="0.0", Units="cm"))
	float AttackWarpMaxCorrection{150.0F};
	UPROPERTY(EditDefaultsOnly, Category="Combat|Motion Warping", meta=(ClampMin="0.0", Units="cm"))
	float AttackWarpMaxTargetDistance{260.0F};

	void SpawnAndEquipDefaultWeapon();
	void ActivatePendingAttackProfile();
	void ResetPendingAttackProfile();
	void ResetActiveAttackProfile();
	// 范围命中：私有计时器与取消代次，不向角色额外挂载组件。
	void ScheduleAreaImpact();
	void CancelAreaImpact();
	void ApplyAreaImpact();
	bool IsValidAreaTarget(AActor& Target) const;
	void ApplyAreaDamage(AActor& Target, const FHitResult& Hit);
	// 唯一结束汇合点：播放与范围查询都结束，才清理本次招式数据。
	void FinishAttackIfReady();
	void ApplyAttackDamage(
		AActor& HitActor,
		const FHitResult& HitResult,
		float SourceDamageMultiplier,
		float SourcePoiseDamage,
		const TCHAR* HitType);
	// 播放辅助函数全部为私有，外部仍从 RequestAttack / CancelActiveAttack 进入。
	bool PlayAttackMontage(UAnimMontage& Montage, float Rate, AActor* FacingTarget, bool bTrackWindup);
	void CancelAttackPlayback();
	void FinishAttackPlayback(bool bInterrupted);
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 InstanceId);
	void HandleAttackMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int32 InstanceId);
	void PrepareAttackMovement(class UAnimInstance& AnimInstance, UAnimMontage& Montage, bool bTrackWindup);
	void ReleaseAttackMovement();
	void ConfigurePlayerAttackWarp(ASCLPlayerCharacter& Player, UAnimMontage& Montage);
	void StopWindupFacing();
	void UpdateWindupFacing();
	// 玩家和敌人的选招方式不同；武器命中与伤害执行仍由本组件共享。
	ESCLAttackRequestResult RequestPlayerAttack(class ASCLPlayerCharacter& Player, ESCLPlayerAttackInput Input);
	ESCLAttackRequestResult RequestEnemyAttack();
	// 检查玩家 Moveset 和共享 AttackData 是否齐全；不负责选择节点，也不负责播放攻击。
	bool HasValidPlayerAttackData() const;
	// 玩家唯一执行入口：即时输入与 Notify 消费缓存都到这里，不存在“谁扣费”的布尔开关。
	bool StartPlayerAttackStep(int32 StepIndex);
	// 玩家实际起播所用的 Effect；提前验证 Spec，失败或只缓存时不施加。
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Player Effects")
	TSubclassOf<class UGameplayEffect> PlayerAttackCostEffectClass;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Player Effects")
	TSubclassOf<class UGameplayEffect> PlayerAttackingStateEffectClass;
	int32 ResolveCurrentComboStepIndex() const;
	void ResetComboState();

	UFUNCTION()
	// Trace 已对 Actor 去重，再调用此事件执行实际伤害和命中特效。
	void HandleTraceHit(AActor* HitActor, const FHitResult& HitResult);
};
