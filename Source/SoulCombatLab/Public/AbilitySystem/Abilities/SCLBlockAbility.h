#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"

#include "SCLBlockAbility.generated.h"

class UGameplayEffect;
class UAnimMontage;

// 格挡技能逻辑基类：按住时维护状态和低速移动，松开/取消时清理；收势动画可选。
// GA_PlayerBlock 配置蓝图只改动画和速度；持续防御姿势/步伐仍在 AnimBP 的格挡 Blend Space。
UCLASS(Blueprintable)
class SOULCOMBATLAB_API USCLBlockAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USCLBlockAbility();
	/** 输入松开是正常结束；受击或处决取消不播放松手动画。 */
	void ReleaseBlock();

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
	UPROPERTY(EditDefaultsOnly, Category = "Ability|Effects")
	TSubclassOf<UGameplayEffect> BlockingStateEffectClass;

	// 在技能蓝图 Class Defaults 配置；只播放一次，随后由格挡 Blend Space 维持防御。
	UPROPERTY(EditDefaultsOnly, Category="Ability|Animation", meta=(DisplayName="格挡起手 Montage"))
	TSoftObjectPtr<UAnimMontage> BlockStartMontage;

	// 可选：正常松开时播放；设为 None 就直接交回 AnimBP。当前 GA_PlayerBlock 已清空。
	// 受击、死亡或其他动作取消格挡时不播放这段收势。
	UPROPERTY(EditDefaultsOnly, Category="Ability|Animation", meta=(DisplayName="格挡收势 Montage"))
	TSoftObjectPtr<UAnimMontage> BlockEndMontage;

	UPROPERTY(EditDefaultsOnly, Category="Ability|Movement", meta=(ClampMin="0.1", ClampMax="1.0", DisplayName="格挡移动速度倍率"))
	float BlockingSpeedMultiplier{0.55F};

	FActiveGameplayEffectHandle BlockingStateHandle;
};
