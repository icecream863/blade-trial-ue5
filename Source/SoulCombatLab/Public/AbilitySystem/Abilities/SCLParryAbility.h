#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "Animation/AnimEnums.h"

#include "SCLParryAbility.generated.h"

class UGameplayEffect;
class UAnimMontage;
class UAnimInstance;
struct FAnimNotifyEventReference;

// 弹反技能逻辑基类：GA_PlayerParry 配置动画，Montage 的 ANS_SCLParryWindow 决定有效帧。
// 动作持续时间与成功判定窗口不同；动画通知只交给实际激活的技能实例。
UCLASS(Blueprintable)
class SOULCOMBATLAB_API USCLParryAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USCLParryAbility();
	// 仅当前 Montage 实例的通知可以改变弹反判定，混出的旧动画不能重新开窗。
	void HandleParryWindowNotify(bool bBegin, const FAnimNotifyEventReference& Reference);
	// 受击端确认弹反成功后调用。一次动作只结算首个成功弹反目标，不能靠多段命中重复扣血。
	float ApplyCounterDamage(AActor* Attacker);
	// 只由受击端确认成功后调用；伤害为 0 时仍应缩短成功后的恢复。
	void NotifySuccessfulParry();
	bool WasRecentlySuccessful(float Seconds = 0.5F) const;

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
	void BeginParryWindow();

	UFUNCTION()
	void EndParryWindow();

	UFUNCTION()
	void FinishParry();
	UFUNCTION()
	void HandleMontageInterrupted();

	void RemoveParryingState();

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Effects")
	TSubclassOf<UGameplayEffect> ParryingStateEffectClass;

	// 换动画时保留 ANS_SCLParryWindow，并把窗口移动到新动作的有效防御帧。
	UPROPERTY(EditDefaultsOnly, Category="Ability|Animation", meta=(DisplayName="弹反 Montage"))
	TSoftObjectPtr<UAnimMontage> ParryMontage;

	// 在 GA_PlayerParry 的 Class Defaults 调整；0 表示只弹开、失衡，不造成生命伤害。
	UPROPERTY(EditDefaultsOnly, Category="Ability|Damage", meta=(DisplayName="成功弹反伤害", ClampMin="0.0"))
	float CounterDamage{20.0F};
	bool bCounterDamageResolved{false};
	// 成功后不必等待动画里与弹反无关的反击尾段；参数在 GA_PlayerParry 类默认值中调。
	UPROPERTY(EditDefaultsOnly, Category="Ability|Recovery", meta=(DisplayName="成功后恢复秒数", ClampMin="0.01"))
	float SuccessfulRecoverySeconds{0.08F};
	UPROPERTY(EditDefaultsOnly, Category="Ability|Recovery", meta=(DisplayName="空弹窗口结束后恢复秒数", ClampMin="0.01"))
	float MissRecoverySeconds{0.18F};
	bool bParrySucceeded{false};
	double LastSuccessTime{-1.0};
	FTimerHandle RecoveryTimer;
	void ScheduleRecovery(float Seconds);

	FActiveGameplayEffectHandle ParryingStateHandle;
	TWeakObjectPtr<UAnimInstance> ParryAnimInstance;
	int32 ActiveMontageInstanceId{INDEX_NONE};
};
