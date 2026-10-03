#pragma once

#include "CoreMinimal.h"
#include "Characters/SCLCharacterBase.h"

#include "SCLPlayerCharacter.generated.h"

class UCameraComponent;
class UAnimInstance;
class USpringArmComponent;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;
class ASCLWeapon;
class USCLPlayerDeveloperComponent;
class ASCLTrainingDummy;
class USCLPlayerComboComponent;
class USCLWeaponPresentationComponent;
class USCLTargetingComponent;
class UMotionWarpingComponent;

// 玩家角色：装配模型、镜头、武器和组件，执行移动与动作请求，保存本角色的移动意图。
// 输入资产、映射生命周期和按键解释看 Controller；连招选择看 ComboComponent；开发实验看 DeveloperComponent。
UCLASS(Blueprintable)
class SOULCOMBATLAB_API ASCLPlayerCharacter : public ASCLCharacterBase
{
	GENERATED_BODY()

public:
	ASCLPlayerCharacter();
	// Controller 管理输入配置与按键解释；角色将动作应用到自身移动、GAS 和组件。
	/** X=左右、Y=前后；缓存意图供躲闪使用，再按视角 Yaw 添加普通移动。 */
	void MoveInViewDirection(FVector2D MovementInput);
	// 移动松开/取消时清除意图，避免躲闪仍使用旧方向。
	void ClearMovementInput();
	// 以下动作仅请求/取消 GAS 技能，具体费用、状态、位移和伤害由技能处理。
	void RequestDodge();
	void StartBlock();
	void StopBlock();
	void RequestParry();
	void RequestExecution();
	UFUNCTION(Exec)
	void DebugToggleDeveloperHUD();

	UFUNCTION(BlueprintPure, Category = "Combat|Dodge")
	/** 返回视角平面的单位方向；无移动输入时使用角色正前方。 */
	FVector GetDesiredDodgeDirection() const;
	// 动画需要区分“无按键”和“无按键时默认向前闪避”，因此不能直接用上面的兜底方向。
	FVector GetWorldMovementIntent() const;

	// Controller 判定输入后调用这里；通过轻/重标签尝试激活 GAS，不直接选 Montage。
	UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
	void RequestLightAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
	void RequestHeavyAttack();

	UFUNCTION(BlueprintPure, Category = "Combat|Targeting")
	USCLTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }
	UMotionWarpingComponent* GetMotionWarpingComponent() const { return MotionWarpingComponent; }

	UFUNCTION(BlueprintPure, Category = "Combat|Combo")
	USCLPlayerComboComponent* GetPlayerComboComponent() const { return PlayerComboComponent; }


	// 武器表现读取当前角色的实际握刀挂点，不重复保存一份装备配置。
	FName GetWeaponHandSocket() const { return PlayerWeaponAttachSocket; }
	USCLWeaponPresentationComponent* GetWeaponPresentationComponent() const { return WeaponPresentationComponent; }
protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	// 仅供控制台触发的测试与调试命令，不属于普通玩家输入流程。

	UFUNCTION(Exec)
	void DebugToggleCombatDraw();

	UFUNCTION(Exec)
	void DebugTestDodgeIFrame();

	UFUNCTION(Exec)
	void DebugTestBlock();

	UFUNCTION(Exec)
	void DebugTestParry();

	UFUNCTION(Exec)
	void DebugTestExecution();

	UFUNCTION(Exec)
	void DebugPrepareExecution();

	UFUNCTION(Exec)
	void DebugPrepareLockOnSwitch();

	UFUNCTION(Exec)
	void DebugSpawnPerceptionEnemy();

	UFUNCTION(Exec)
	void DebugSpawnHeavyEnemy();

	UFUNCTION(Exec)
	void DebugSpawnEnemyVariants();

	UFUNCTION(Exec)
	void DebugSpawnBossPrototype(float DistanceToTarget = 800.0F);

	UFUNCTION(Exec)
	void DebugSetBossPhaseTwo();

	UFUNCTION(Exec)
	void DebugEvaluateBossUtility(float DistanceToTarget);

	UFUNCTION(Exec)
	void DebugTestBossFoundation();

	UFUNCTION(Exec)
	void DebugPrepareBossCombatTest(bool bStartInPhaseTwo = false);

	UFUNCTION(Exec)
	void DebugKillLockedTarget();

	// 玩家蓝图可配置的外观、装备和组件；连招表保存在 PlayerComboComponent 上。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USCLTargetingComponent> TargetingComponent;

	// 攻击和处决仅在有效目标附近设置 Warp Target；组件不决定选敌规则。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USCLPlayerComboComponent> PlayerComboComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ScabbardMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Appearance", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMesh> PlayerSkeletalMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> ScabbardAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Equipment", meta = (AllowPrivateAccess = "true"))
	FName ScabbardAttachSocket{TEXT("Scabbard_Target01Socket")};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Equipment", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ASCLWeapon> PlayerWeaponClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Equipment", meta = (AllowPrivateAccess = "true"))
	FName PlayerWeaponAttachSocket{TEXT("weapon_rSocket")};


	// 武器表现组件负责空闲收刀、动画通知换挂点和动作打断。
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USCLWeaponPresentationComponent> WeaponPresentationComponent;

	// 开发组件以此角色为 Owner，读取位置、GAS 与战斗组件；调试实现留在组件内部。
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USCLPlayerDeveloperComponent> DeveloperComponent;
	// 当前二维移动意图，不是速度；攻击中也会更新，用于随后躲闪方向选择。
	FVector2D CachedMovementInput{FVector2D::ZeroVector};
};
