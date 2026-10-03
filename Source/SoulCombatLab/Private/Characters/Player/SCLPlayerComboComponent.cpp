#include "Characters/Player/SCLPlayerComboComponent.h"

#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

/**
 * 创建无 Tick 的连招状态机，并提供默认 Moveset 资产；推进依赖输入和 Notify。
 */
USCLPlayerComboComponent::USCLPlayerComboComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// C++ 提供默认招式表；组件上的 PlayerMoveset 属性仍可在玩家蓝图中配置。
	static ConstructorHelpers::FObjectFinder<USCLPlayerMovesetData> MovesetAsset(
		TEXT("/Game/SoulCombatLab/Characters/Player/Combat/Data/DA_GhostSamurai_PlayerMoveset.DA_GhostSamurai_PlayerMoveset"));
	PlayerMoveset = MovesetAsset.Object;
}

/**
 * 按本次 Input 查待选节点的成本；本函数只查询，不改变连招状态。
 * 无节点返回 0 不代表一定能出招，请求仍需校验后继和资产。
 */
float USCLPlayerComboComponent::GetNextStaminaCost(const ESCLPlayerAttackInput Input) const
{
	// GAS 在激活前询问“本次输入将去的节点”成本，而不是当前正在播放的招式成本。
	const FSCLPlayerAttackStep* const Step = GetStep(ResolveNextStep(Input));
	return Step != nullptr ? Step->StaminaCost : 0.0F;
}

/**
 * 安全查询 Moveset 数组下标；没有资产或索引越界时返回 nullptr。
 */
const FSCLPlayerAttackStep* USCLPlayerComboComponent::GetStep(const int32 StepIndex) const
{
	return PlayerMoveset != nullptr ? PlayerMoveset->FindStep(StepIndex) : nullptr;
}

/**
 * 检查起手索引、各招 Montage/名称和所有后继索引。
 * INDEX_NONE 是合法的终止边；任何非终止的无效边都使整张表不能用于攻击。
 */
bool USCLPlayerComboComponent::HasValidMoveset() const
{
	// 整张表有无效索引或缺失 Montage 时，玩家攻击请求会被拒绝。
	if (PlayerMoveset == nullptr || PlayerMoveset->Steps.IsEmpty())
	{
		return false;
	}
	if (!PlayerMoveset->Steps.IsValidIndex(PlayerMoveset->LightOpenerIndex) ||
		!PlayerMoveset->Steps.IsValidIndex(PlayerMoveset->HeavyOpenerIndex))
	{
		return false;
	}
	for (const FSCLPlayerAttackStep& Step : PlayerMoveset->Steps)
	{
		if (Step.Montage == nullptr || Step.AttackName.IsNone() ||
			!FMath::IsFinite(Step.StaminaCost) || Step.StaminaCost < 0.0F)
		{
			return false;
		}
		if ((Step.NextLightStepIndex != INDEX_NONE && !PlayerMoveset->Steps.IsValidIndex(Step.NextLightStepIndex)) ||
			(Step.NextHeavyStepIndex != INDEX_NONE && !PlayerMoveset->Steps.IsValidIndex(Step.NextHeavyStepIndex)))
		{
			return false;
		}
	}
	return true;
}

/**
 * 把一次 Light/Heavy 输入变成“待执行节点”或“一格缓存”。
 * 注意：选中节点不等于起播，Combat 必须完成动画与费用提交才能确认当前节点。
 * 因此本函数完全不依赖 Combat，阅读连招规则只需看本类和 Moveset。
 */
FSCLPlayerComboSelection USCLPlayerComboComponent::SelectAttack(const ESCLPlayerAttackInput Input)
{
	if (!HasValidMoveset())
	{
		return {};
	}
	if (ActiveStepIndex == INDEX_NONE)
	{
		// 起手直接交出节点；当前节点留到 Combat 成功起播后再确认。
		return {ResolveNextStep(Input), false};
	}
	if (bHasBufferedInput || GetWorld() == nullptr || ResolveNextStep(Input) == INDEX_NONE)
	{
		// 一格缓存已占用，或招式表没有对应后继，不接受新的接招请求。
		return {};
	}

	BufferedInput = Input;
	BufferedAttackTime = GetWorld()->GetTimeSeconds();
	bHasBufferedInput = true;
	if (bComboWindowOpen)
	{
		// 窗口内输入可以立即消费，但仍由调用者决定能否真正起播。
		return {ConsumeBufferedStep(), false};
	}
	return {INDEX_NONE, true};
}

/**
 * Combat 收到当前攻击的 Notify Begin 后调用；只交出之前缓存的待接节点。
 * 原 Ability 是否还在运行不影响费用规则，Combat 对所有玩家起播使用同一条提交路径。
 */
int32 USCLPlayerComboComponent::OpenComboWindow()
{
	bComboWindowOpen = true;
	// 窗口打开时推进之前缓存的输入；若等待超过 InputBufferLifetime，则丢弃输入且不扣费。
	return ConsumeBufferedStep();
}

/**
 * Notify End 关闭窗口并丢弃未消费输入，避免缓存自动带到后面的窗口。
 */
void USCLPlayerComboComponent::CloseComboWindow()
{
	// 窗口关闭后不保留尚未用掉的按键；下次接招需要玩家重新输入。
	bComboWindowOpen = false;
	bHasBufferedInput = false;
}

/**
 * 攻击结束、取消或切换招式时清除节点、缓存时间和窗口标记。
 * 下一次新输入会重新按起手索引选招。
 */
void USCLPlayerComboComponent::ResetCombo()
{
	// 攻击结束或取消后回到“无当前招式”，下一次输入重新走起手索引。
	ActiveStepIndex = INDEX_NONE;
	BufferedInput = ESCLPlayerAttackInput::Light;
	BufferedAttackTime = 0.0F;
	bHasBufferedInput = false;
	bComboWindowOpen = false;
}

/**
 * 纯粹查连招图：没有当前节点查轻/重起手；否则查当前节点的对应后继边。
 */
int32 USCLPlayerComboComponent::ResolveNextStep(const ESCLPlayerAttackInput Input) const
{
	if (PlayerMoveset == nullptr)
	{
		return INDEX_NONE;
	}
	// 没有当前招式选起手；有当前招式则按轻/重输入查该节点的后继。
	return ActiveStepIndex == INDEX_NONE
		? PlayerMoveset->GetOpenerIndex(Input)
		: PlayerMoveset->GetNextStepIndex(ActiveStepIndex, Input);
}

/**
 * 只有窗口已开、缓存存在且未超时才交出节点。
 * 无论后续起播是否成功，这笔缓存都只消费一次，不能反复触发免费重试。
 */
int32 USCLPlayerComboComponent::ConsumeBufferedStep()
{
	// 缓存最多等 InputBufferLifetime 秒；超时只丢输入，不启动下一招，也不扣下一招的体力。
	if (!bComboWindowOpen || !bHasBufferedInput || GetWorld() == nullptr ||
		GetWorld()->GetTimeSeconds() - BufferedAttackTime > InputBufferLifetime)
	{
		bHasBufferedInput = false;
		return INDEX_NONE;
	}
	const int32 StepIndex = ResolveNextStep(BufferedInput);
	bHasBufferedInput = false;
	return StepIndex;
}
