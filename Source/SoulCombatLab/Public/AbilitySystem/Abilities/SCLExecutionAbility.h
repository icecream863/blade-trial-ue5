#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimEnums.h"

#include "SCLExecutionAbility.generated.h"

class UAnimMontage;
struct FAnimNotifyEventReference;
class UAnimInstance;
class UCharacterMovementComponent;
class UGameplayEffect;

// 处决技能逻辑基类：验证目标与路径，播放 GA_PlayerExecution 配置的动画，在通知处结算。
// 蓝图可调整动画与距离；命中通知和 SCL_Execution 校正窗口必须与动画时间轴配套。
UCLASS(Blueprintable)
class SOULCOMBATLAB_API USCLExecutionAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USCLExecutionAbility();
	void HandleImpactNotify(const FAnimNotifyEventReference& Reference);

	static float CalculateExecutionDamage(float CurrentHealth, bool bBossTarget);

protected:
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	UFUNCTION()
	void ResolveExecutionImpact();

	UFUNCTION()
	void HandleExecutionCompleted();

	UFUNCTION()
	void HandleExecutionInterrupted();

	AActor* FindExecutionTarget(const FGameplayAbilityActorInfo* ActorInfo) const;
	void FinishExecution(bool bWasCancelled);
	void RestoreExecutionMovementOrientation();
	void RestoreExecutionRootMotionMode();

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Animation")
	// 技能蓝图 Class Defaults 配置；需要 AN_SCLExecutionImpact 和 SCL_Execution 窗口。
	TSoftObjectPtr<UAnimMontage> ExecutionMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Effects")
	TSubclassOf<UGameplayEffect> AttackingStateEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumExecutionDistance{250.0F};

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Targeting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumExecutionFacingDot{0.25F};

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float ExecutionStandOffDistance{105.0F};

	// 与处决 Montage 中 SCL 处决命中通知配套；缺通知时不结算伤害。
	UPROPERTY()
	TObjectPtr<UAnimMontage> ActiveExecutionMontage;

	UPROPERTY()
	TWeakObjectPtr<AActor> ExecutionTarget;

	bool bImpactResolved{false};
	int32 ActiveMontageInstanceId{INDEX_NONE};
	FActiveGameplayEffectHandle ExecutionStateHandle;
};
