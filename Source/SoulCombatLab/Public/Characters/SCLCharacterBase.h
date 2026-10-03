#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/SCLCombatInterface.h"

#include "SCLCharacterBase.generated.h"

class USCLAbilitySystemComponent;
class USCLAttributeSet;
class USCLCombatComponent;
class USCLHitReactionComponent;
class USCLActionMovementComponent;
class UGameplayAbility;
class UStaticMeshComponent;

// 玩家与敌人的共同角色基类：持有 GAS、属性、战斗和受击组件，并处理进入角色的伤害。
UCLASS(Abstract)
class SOULCOMBATLAB_API ASCLCharacterBase : public ACharacter, public IAbilitySystemInterface, public ISCLCombatInterface
{
	GENERATED_BODY()

public:
	ASCLCharacterBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// 伤害统一入口：按无敌、弹反、格挡、生命伤害的顺序处理，实际数值交给 ASC。
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Ability System")
	USCLAbilitySystemComponent* GetSCLAbilitySystemComponent() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category = "Ability System")
	USCLAttributeSet* GetSCLAttributeSet() const { return AbilityAttributes; }

	// 查询默认技能配置是否包含该动作，允许实际配置为它的蓝图子类。
	bool HasStartupAbility(TSubclassOf<UGameplayAbility> AbilityClass) const;

	UFUNCTION(BlueprintPure, Category = "Combat|Hit Reaction")
	USCLHitReactionComponent* GetHitReactionComponent() const { return HitReactionComponent; }

	void SetGuardVisualActive(bool bActive);
	void PlayGuardImpactFeedback();
	void SetExecutionMarkerActive(bool bActive);
	void SetLockOnMarkerActive(bool bActive);
	// 攻击、锁定等系统分别用自己的名称申请朝向锁；最后一个释放者才恢复原设置。
	USCLActionMovementComponent* GetActionMovementComponent() const { return ActionMovementComponent; }
	void SetMovementFacingLocked(FName FacingOwner, bool bLocked);

	virtual USCLCombatComponent* GetCombatComponent_Implementation() const override { return CombatComponent; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability System")
	// 玩家蓝图在这里选择实际授予的技能类；动画等可调参数在相应技能蓝图 Class Defaults。
	// 更换动作配置时替换原数组项，不把同一个动作的父类和子类重复加入。
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

private:
	// 授予角色在游戏开始时配置的初始技能。
	void GrantStartupAbilities();
	// 判断这次伤害是否被防御方向覆盖。
	bool IsDamageBlocked(const FDamageEvent& DamageEvent, const AActor* DamageCauser) const;
	// 判断这次攻击是否满足弹反条件。
	bool IsDamageParried(const FDamageEvent& DamageEvent, AActor* DamageCauser) const;
	// 判断点伤害的来源方向是否落在指定角度范围内。
	bool IsIncomingPointDamageWithinArc(
		const FDamageEvent& DamageEvent,
		const AActor* DamageCauser,
		float ArcDegrees) const;
	// 弹反成功后的后续处理，例如通知攻击者并播放反馈。
	void HandleSuccessfulParry(const FDamageEvent& DamageEvent, AActor* DamageCauser);
	// 将格挡受击反馈恢复为普通状态。
	void ResetGuardImpactFeedback();

	// 格挡有效的扇形角度，角色正前方左右各 BlockArcDegrees / 2 度。
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0", ClampMax = "360.0", Units = "deg"))
	float BlockArcDegrees{120.0F};

	// 格挡时从生命伤害换算为防御体力伤害的倍率。
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0"))
	float GuardStaminaDamageMultiplier{1.5F};

	// 弹反有效的扇形角度，角色正前方左右各 ParryArcDegrees / 2 度。
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Parry", meta = (ClampMin = "0.0", ClampMax = "360.0", Units = "deg"))
	float ParryArcDegrees{160.0F};

	// GAS 能力系统组件：管理技能、Gameplay Effect 和角色的能力状态。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USCLAbilitySystemComponent> AbilitySystemComponent;

	// 属性组件：保存生命、体力等可被 GAS 修改的角色属性。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USCLAttributeSet> AbilityAttributes;

	// 战斗编排与命中结算组件。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USCLCombatComponent> CombatComponent;

	// 受击反应组件：负责播放受击、硬直等反馈。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USCLHitReactionComponent> HitReactionComponent;

	// 格挡状态的可视化网格；通常在角色格挡时显示。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> GuardVisual;

	// 处决状态提示网格；目标可被处决时显示。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ExecutionMarker;

	// 锁定目标提示网格；角色被其他角色锁定时显示。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> LockOnMarker;

	// 格挡受击放大反馈的计时器，到期后恢复 GuardVisual 的普通大小。
	FTimerHandle GuardImpactFeedbackTimerHandle;

	// 所有动作通过同一个组件申请移动限制，角色只提供访问入口。
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USCLActionMovementComponent> ActionMovementComponent;
};
