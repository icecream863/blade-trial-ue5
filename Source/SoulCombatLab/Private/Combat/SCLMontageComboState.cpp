#include "Combat/SCLMontageComboState.h"
#include "Data/SCLAttackData.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

/*
 * 敌人连段不是“播放一个 Montage 就结束”，而是动态修改 Montage Section
 * 的跳转关系：
 *
 *   ActiveStep   当前正在播放的 Section
 *   BufferedStep 玩家/AI 已输入、等待窗口消费的下一段
 *   QueuedStep   已写入 Montage_SetNextSection、等待实际播放的下一段
 *
 * 这里不负责选择攻击成本、伤害或 Ability 生命周期，只负责维护 Section 图。
 */

void FSCLMontageComboState::Reset()
{
	// Reset 必须同时清掉三种节点，否则旧的 QueuedStep 可能污染下一次起手。
	ActiveStep = BufferedStep = QueuedStep = INDEX_NONE;
	bWindowOpen = false;
}

void FSCLMontageComboState::Start(const USCLAttackData& Data, UAnimInstance& Anim, int32 FirstStep, int32 StepCount)
{
	// 开始新一轮攻击前先彻底清空上一轮状态；FirstStep 无效时退回第 0 段。
	Reset();
	ActiveStep = Data.FindComboStep(FirstStep) ? FirstStep : 0;
	UAnimMontage* Montage = Data.GetAttackMontage();
	const auto* First = Data.FindComboStep(ActiveStep);
	if (!Montage || !First) return;
	// 先跳到首段，再重建本次允许使用的 Section 链。
	Anim.Montage_JumpToSection(First->MontageSection, Montage);
	// 每次重新安排末段 None，清掉同一个 Montage 播放实例中上一次动态连接。
	// 这一步很重要：Montage 的 Section 跳转是运行时状态，不会自动按新攻击重置。
	const int32 Last = FMath::Min(ActiveStep + FMath::Max(StepCount, 1) - 1, Data.GetComboStepCount() - 1);
	for (int32 Index = ActiveStep; Index <= Last; ++Index)
	{
		const auto* Current = Data.FindComboStep(Index);
		const auto* Next = Index < Last ? Data.FindComboStep(Index + 1) : nullptr;
		if (Current) Anim.Montage_SetNextSection(Current->MontageSection, Next ? Next->MontageSection : NAME_None, Montage);
	}
}

int32 FSCLMontageComboState::Resolve(const USCLAttackData* Data, const UAnimInstance* Anim) const
{
	// 优先从动画实例的当前 Section 反查真实播放位置；
	// 动画尚未同步、Montage 不存在或 Section 不匹配时，退回本地记录。
	if (!Data) return INDEX_NONE;
	if (!Anim || !Data->GetAttackMontage()) return ActiveStep;
	const int32 Step = Data->FindComboStepIndex(Anim->Montage_GetCurrentSection(Data->GetAttackMontage()));
	return Step != INDEX_NONE ? Step : ActiveStep;
}

void FSCLMontageComboState::Refresh(const USCLAttackData& Data, const UAnimInstance& Anim)
{
	// 所有依赖 ActiveStep 的操作前先同步一次，避免混出/跳段后仍使用旧节点。
	const int32 Step = Resolve(&Data, &Anim);
	if (Step != INDEX_NONE) ActiveStep = Step;
	if (QueuedStep == Step) QueuedStep = INDEX_NONE;
}

ESCLAttackRequestResult FSCLMontageComboState::Buffer(const USCLAttackData& Data, UAnimInstance& Anim)
{
	// Buffer 的 Buffered 只表示“输入被接受”，不表示下一段已经开始播放。
	Refresh(Data, Anim);
	if (BufferedStep != INDEX_NONE || QueuedStep != INDEX_NONE || !Data.FindComboStep(ActiveStep + 1))
		return ESCLAttackRequestResult::Rejected;
	BufferedStep = ActiveStep + 1;
	// 窗口未开时等待 Notify；窗口已开时尝试立即写入下一段跳转。
	return !bWindowOpen || Advance(Data, Anim) ? ESCLAttackRequestResult::Buffered : ESCLAttackRequestResult::Rejected;
}

void FSCLMontageComboState::OpenWindow(const USCLAttackData& Data, UAnimInstance& Anim)
{
	// ComboWindow Notify Begin 到达后，才允许把缓存写入 Montage 的下一段关系。
	Refresh(Data, Anim);
	bWindowOpen = true;
	Advance(Data, Anim);
}

bool FSCLMontageComboState::Advance(const USCLAttackData& Data, UAnimInstance& Anim)
{
	// 这是“消费缓存”的唯一入口：写入跳转关系后，缓存转为 QueuedStep。
	// 真正何时进入下一段仍由 Montage 播放到当前 Section 末尾决定。
	if (!bWindowOpen || BufferedStep == INDEX_NONE) return false;
	const int32 CurrentIndex = Resolve(&Data, &Anim);
	const auto* Current = Data.FindComboStep(CurrentIndex);
	const auto* Next = Data.FindComboStep(BufferedStep);
	if (!Current || !Next || !Data.GetAttackMontage()) return false;
	Anim.Montage_SetNextSection(Current->MontageSection, Next->MontageSection, Data.GetAttackMontage());
	ActiveStep = CurrentIndex;
	QueuedStep = BufferedStep;
	BufferedStep = INDEX_NONE;
	return true;
}
