#pragma once

#include "Animation/AnimInstance.h"
#include "SCLPlayerAnimInstance.generated.h"

/** 玩家动画图的输入：把世界速度换算到角色自身的前后、左右方向。 */
UCLASS(Blueprintable)
class SOULCOMBATLAB_API USCLPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** CharacterMovement 的实际离地状态；跳跃与走下台阶都会为真，不能只看空格按键。 */
	UPROPERTY(BlueprintReadOnly, Category="Locomotion|Jump")
	bool bIsFalling{false};

	/** 厘米/秒：正数上升、负数下降，用于区分起跳和直接下落。 */
	UPROPERTY(BlueprintReadOnly, Category="Locomotion|Jump")
	float VerticalSpeed{0.0F};

	/** 实际水平速度；落地后正在移动时尽快混回步伐，避免长时间滑着播落地。 */
	UPROPERTY(BlueprintReadOnly, Category="Locomotion|Jump")
	float GroundSpeed{0.0F};

	/** -1 后退、+1 前进；Blend Space 的纵轴。 */
	UPROPERTY(BlueprintReadOnly, Category="Locomotion")
	float LocalForwardSpeed{0.0F};

	/** -1 左移、+1 右移；Blend Space 的横轴。 */
	UPROPERTY(BlueprintReadOnly, Category="Locomotion")
	float LocalRightSpeed{0.0F};

	/** 锁定仅决定朝向；移动动画仍按角色局部速度选择。 */
	UPROPERTY(BlueprintReadOnly, Category="Locomotion")
	bool bLockedOn{false};

	UPROPERTY(BlueprintReadOnly, Category="Locomotion")
	bool bWeaponSheathed{false};

	UPROPERTY(BlueprintReadOnly, Category="Locomotion")
	bool bBlocking{false};
private:
	// 只跨一帧隔离闪避位移清理，不改变胶囊移动、无敌帧或技能持续时间。
	bool bWasDodging{false};
};
