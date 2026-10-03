#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"

#include "SCLDodgeAbility.generated.h"

class UAnimMontage;
class UGameplayEffect;

// 方向按角色局部坐标顺时针排列：0° 前、90° 右；不能拿镜头坐标直接查动画。
UENUM(BlueprintType)
enum class ESCLDodgeDirection : uint8
{
	Forward UMETA(DisplayName="前 F"),
	ForwardRight UMETA(DisplayName="右前 FR"),
	Right UMETA(DisplayName="右 R"),
	BackwardRight UMETA(DisplayName="右后 BR"),
	Backward UMETA(DisplayName="后 B"),
	BackwardLeft UMETA(DisplayName="左后 BL"),
	Left UMETA(DisplayName="左 L"),
	ForwardLeft UMETA(DisplayName="左前 FL")
};

// 闪避技能：选择八向 Montage、支付体力、申请位移并在结束时清理状态。
// GA_PlayerDodge 配置方向动画；无敌时间在各 Montage 的 SCL Invincible 通知中调整。
UCLASS(Blueprintable)
class SOULCOMBATLAB_API USCLDodgeAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USCLDodgeAbility();
	/** 世界输入转为角色局部八向；零输入向前，侧闪/后闪不会为了选动画而转身。 */
	UFUNCTION(BlueprintPure, Category="Ability|Dodge")
	static ESCLDodgeDirection SelectDodgeDirection(FVector WorldDirection, FRotator Facing);

protected:
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual bool CheckCost(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ApplyCost(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) const override;

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
	void HandleDodgeCompleted();

	UFUNCTION()
	void HandleDodgeInterrupted();

	void FinishDodge(bool bWasCancelled);
	// 配了方向表就必须找到对应项；缺项拒绝动作，不能用前滚冒充侧闪。
	UAnimMontage* ResolveMontage(ESCLDodgeDirection Direction) const;

	UPROPERTY(EditDefaultsOnly, Category="Ability|Animation", meta=(DisplayName="八向闪避 Montage"))
	TMap<ESCLDodgeDirection, TSoftObjectPtr<UAnimMontage>> DirectionalMontages;

	// 兼容没有方向表的旧角色；当前玩家使用 GA_PlayerDodge 的八项配置。

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Animation")
	TSoftObjectPtr<UAnimMontage> DodgeMontage{
		FSoftObjectPath{TEXT("/Game/SoulCombatLab/Animations/AM_Dodge.AM_Dodge")}};

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Effects")
	TSubclassOf<UGameplayEffect> StaminaCostEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Effects")
	TSubclassOf<UGameplayEffect> DodgingStateEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Cost", meta = (ClampMin = "0.0"))
	float StaminaCost{25.0F};

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Movement", meta = (ClampMin = "0.0", Units = "cm"))
	float DodgeDistance{450.0F};

	FActiveGameplayEffectHandle DodgingStateHandle;

};
