#include "Animation/AnimNotifyStates/ANS_SCLWeaponTrace.h"

#include "Combat/SCLCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/SCLCombatInterface.h"
#include "Animation/AnimNotifyLibrary.h"

namespace
{
	USCLCombatComponent* ResolveWeaponTraceCombatComponent(const USkeletalMeshComponent* const MeshComp)
	{
		AActor* const MeshOwner = MeshComp != nullptr ? MeshComp->GetOwner() : nullptr;
		if (MeshOwner == nullptr || !MeshOwner->GetClass()->ImplementsInterface(USCLCombatInterface::StaticClass()))
		{
			return nullptr;
		}

		return ISCLCombatInterface::Execute_GetCombatComponent(MeshOwner);
	}
}

/** 进入武器有效帧区间；核对当前攻击实例后开启轨迹检测和挥刀效果。 */
void UANS_SCLWeaponTrace::NotifyBegin(
	USkeletalMeshComponent* const MeshComp,
	UAnimSequenceBase* const Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// 命中窗口和连招窗口互不相同：这里开始的是刀刃轨迹检测。
	if (USCLCombatComponent* const CombatComponent = ResolveWeaponTraceCombatComponent(MeshComp))
	{
		if (CombatComponent->IsActiveAttackNotify(EventReference)) CombatComponent->BeginWeaponTrace();
	}
}

/** 有效帧登记采样请求；骨骼更新完成后才读取刀刃位置，避免根运动 Notify 读到旧姿势。 */
void UANS_SCLWeaponTrace::NotifyTick(
	USkeletalMeshComponent* const MeshComp,
	UAnimSequenceBase* const Animation,
	const float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	// 只登记当前窗口的请求；旧 Montage 的通知会被实例 ID 检查挡掉。
	if (USCLCombatComponent* const CombatComponent = ResolveWeaponTraceCombatComponent(MeshComp))
	{
		if (CombatComponent->IsActiveAttackNotify(EventReference))
			CombatComponent->TickWeaponTrace(EventReference.GetNotify()->GetEndTriggerTime());
	}
}

/** 退出有效帧区间后停止轨迹与效果，避免动画其他时段也造成武器命中。 */
void UANS_SCLWeaponTrace::NotifyEnd(
	USkeletalMeshComponent* const MeshComp,
	UAnimSequenceBase* const Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	// 离开有效帧后停止检测，避免动画其他时间仍造成武器命中。
	if (USCLCombatComponent* const CombatComponent = ResolveWeaponTraceCombatComponent(MeshComp))
	{
		if (CombatComponent->IsActiveAttackNotify(EventReference))
		{
			// UE 先发 End，再给仍活动的窗口发 Tick。卡顿跨过窗口末尾时，
			// 直接 End 会漏掉上一采样到本帧之间的最后一段挥刀。
			// 自然结束等本帧骨骼完成后再按窗口时间截断；取消/切招立即关闭，不追加伤害。
			CombatComponent->FinishWeaponTraceWindow(EventReference.GetNotify()->GetEndTriggerTime(),
				UAnimNotifyLibrary::NotifyStateReachedEnd(EventReference));
		}
	}
}

/** 编辑器时间轴显示名称，方便区分命中窗口与连招输入窗口。 */
FString UANS_SCLWeaponTrace::GetNotifyName_Implementation() const
{
	return TEXT("SCL Weapon Trace");
}
