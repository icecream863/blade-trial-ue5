#include "Animation/AnimNotifyStates/ANS_SCLComboWindow.h"

#include "Combat/SCLCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/SCLCombatInterface.h"

namespace
{
USCLCombatComponent* ResolveComboWindowCombatComponent(const USkeletalMeshComponent* const MeshComp)
{
	AActor* const MeshOwner = MeshComp != nullptr ? MeshComp->GetOwner() : nullptr;
	if (MeshOwner == nullptr || !MeshOwner->GetClass()->ImplementsInterface(USCLCombatInterface::StaticClass()))
	{
		return nullptr;
	}

	return ISCLCombatInterface::Execute_GetCombatComponent(MeshOwner);
}
}

/** Montage 时间轴进入接招区间；EventReference 用于核对当前实例，随后打开窗口并尝试消费缓存。 */
void UANS_SCLComboWindow::NotifyBegin(
	USkeletalMeshComponent* const MeshComp,
	UAnimSequenceBase* const Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	// 动画时间轴走到接招区间，才通知 CombatComponent 打开窗口并尝试消费缓存。
	if (USCLCombatComponent* const CombatComponent = ResolveComboWindowCombatComponent(MeshComp))
	{
		// 只接受当前攻击 Montage 实例的通知，避免上一招混出时的旧通知推进新连招。
		if (CombatComponent->IsActiveAttackNotify(EventReference)) CombatComponent->OpenComboWindow();
	}
}

/** Montage 时间轴离开接招区间；仅当前实例才能关闭窗口，旧混出通知不能影响新一刀。 */
void UANS_SCLComboWindow::NotifyEnd(
	USkeletalMeshComponent* const MeshComp,
	UAnimSequenceBase* const Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	// 离开区间就关闭窗口；未接出的输入会在连招组件里清掉。
	if (USCLCombatComponent* const CombatComponent = ResolveComboWindowCombatComponent(MeshComp))
	{
		if (CombatComponent->IsActiveAttackNotify(EventReference)) CombatComponent->CloseComboWindow();
	}
}

/** 编辑器时间轴显示名称；名称本身不会驱动连招，真正执行的是 Begin/End。 */
FString UANS_SCLComboWindow::GetNotifyName_Implementation() const
{
	return TEXT("SCL Combo Window");
}
