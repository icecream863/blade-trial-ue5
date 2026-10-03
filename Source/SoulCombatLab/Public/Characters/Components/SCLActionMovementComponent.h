#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AnimEnums.h"
#include "SCLActionMovementComponent.generated.h"

class UAnimInstance;

// 动作移动限制的唯一管理者：按申请者名称组合步速、朝向和根运动规则。
// 锁定只申请速度上限，格挡只申请速度倍率；结束时各自撤销自己的申请，不恢复别人的快照。
// 不播放动画、不选目标、不判断技能资格，没有常驻 Tick。所有申请由对应动作的结束/取消路径撤销。
UCLASS(ClassGroup = "SoulCombatLab")
class SOULCOMBATLAB_API USCLActionMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USCLActionMovementComponent();
	// 普通移动参数变更用这个入口；有限制时更新基础值，再重新计算最终值。
	void SetBaseWalkSpeed(float Speed);
	void SetWalkSpeedLimit(FName Requester, float Limit);
	void ClearWalkSpeedLimit(FName Requester);
	void SetWalkSpeedMultiplier(FName Requester, float Multiplier);
	void ClearWalkSpeedMultiplier(FName Requester);
	// 同名重复申请不会叠加；最后一个朝向申请撤销后才恢复原值。
	void SetFacingLocked(FName Requester, bool bLocked);
	// 优先级越大越优先；同级取最后申请。武器表现使用 10，战斗动作使用 100。
	// 新动作与旧动作短暂交叠时，旧动作结束只能撤销自身，不能覆盖新动作的根运动模式。
	bool RequestRootMotionMode(FName Requester, ERootMotionMode::Type Mode, int32 Priority = 100);
	void ReleaseRootMotionMode(FName Requester);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 以下都是本身体的临时申请，不写入蓝图或存档；配置值仍在申请动作自己的属性中。
	TMap<FName, float> SpeedLimits;
	TMap<FName, float> SpeedMultipliers;
	TSet<FName> FacingLocks;
	float BaseWalkSpeed{0.0F};
	bool bSpeedBaselineSaved{false};
	bool bOriginalOrientToMovement{true};
	struct FRootMotionRequest
	{
		ERootMotionMode::Type Mode{ERootMotionMode::NoRootMotionExtraction};
		int32 Priority{100};
		uint64 Order{0};
	};
	TMap<FName, FRootMotionRequest> RootMotionRequests;
	TWeakObjectPtr<UAnimInstance> RootMotionInstance;
	ERootMotionMode::Type OriginalRootMotionMode{ERootMotionMode::NoRootMotionExtraction};
	uint64 RequestOrder{0};
	void CaptureSpeedBaseline();
	void ApplyWalkSpeed();
	void ApplyRootMotionMode();
};
