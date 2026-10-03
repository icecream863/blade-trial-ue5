#include "Data/SCLPlayerMovesetData.h"

/**
 * 只做安全数组查询；返回的是数据资产内的节点地址，调用者只读使用。
 */
const FSCLPlayerAttackStep* USCLPlayerMovesetData::FindStep(const int32 StepIndex) const
{
	return Steps.IsValidIndex(StepIndex) ? &Steps[StepIndex] : nullptr;
}

/**
 * 没有当前招式时按 Light/Heavy 选择起手下标，不在此检查动画和输入窗口。
 */
int32 USCLPlayerMovesetData::GetOpenerIndex(const ESCLPlayerAttackInput Input) const
{
	return Input == ESCLPlayerAttackInput::Heavy ? HeavyOpenerIndex : LightOpenerIndex;
}

/**
 * 按当前节点和本次输入查一条后继边；INDEX_NONE 表示没有可接的节点。
 */
int32 USCLPlayerMovesetData::GetNextStepIndex(
	const int32 CurrentStepIndex,
	const ESCLPlayerAttackInput Input) const
{
	const FSCLPlayerAttackStep* const Step = FindStep(CurrentStepIndex);
	// 只按当前节点和本次输入查一条边；这里不检查窗口时机，也不播放动画。
	return Step == nullptr ? INDEX_NONE :
		(Input == ESCLPlayerAttackInput::Heavy ? Step->NextHeavyStepIndex : Step->NextLightStepIndex);
}
