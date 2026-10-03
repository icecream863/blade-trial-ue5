#pragma once
#include "CoreMinimal.h"
#include "Combat/SCLAttackRequestResult.h"
class USCLAttackData;
class UAnimInstance;

/*
 * 敌人 Section 连段状态：当前段、缓存段与已排队段是三个不同时间点。
 *
 * 输入进入后的状态变化：
 *   1. Buffer：保存 BufferedStep，不修改 Montage。
 *   2. OpenWindow：ComboWindow 打开后调用 Advance。
 *   3. Advance：调用 Montage_SetNextSection，把缓存变成 QueuedStep。
 *   4. Montage 播放到当前段结尾后，动画系统才真正进入下一段。
 *
 * 因此 QueuedStep 不是“已经播放”，而是“已经安排好跳转”。
 */
// 普通 C++ 对象即可，不需要组件或 Tick；资源和播放实例由调用方显式传入。
// 与玩家 Moveset 的轻重分支分开，不能把玩家节点下标当成敌人 Section 下标。
class FSCLMontageComboState
{
public:
	// 清空本轮 Section 链和所有等待中的输入。
	void Reset();
	// 从首段开始播放，并重建本次允许使用的 Section 跳转链。
	void Start(const USCLAttackData& Data, UAnimInstance& Anim, int32 FirstStep, int32 StepCount);
	// 保存下一段输入；返回 Buffered 不代表下一段已经起播。
	ESCLAttackRequestResult Buffer(const USCLAttackData& Data, UAnimInstance& Anim);
	// 由 ComboWindow Notify Begin 调用，尝试消费 BufferedStep。
	void OpenWindow(const USCLAttackData& Data, UAnimInstance& Anim);
	// 由 ComboWindow Notify End 调用；关闭后不能再写入下一段跳转。
	void CloseWindow() { bWindowOpen = false; }
	// 从动画当前 Section 反查节点，动画信息不可用时回退到 ActiveStep。
	int32 Resolve(const USCLAttackData* Data, const UAnimInstance* Anim) const;
	int32 GetActiveStep() const { return ActiveStep; }
private:
	// 当前正在播放或最近一次同步到的 Section 节点。
	int32 ActiveStep{INDEX_NONE};
	// 已接受但尚未写入 Montage 跳转关系的输入。
	int32 BufferedStep{INDEX_NONE};
	// 已写入 Montage_SetNextSection、等待动画实际播放的节点。
	int32 QueuedStep{INDEX_NONE};
	// 只有窗口打开时，BufferedStep 才能转为 QueuedStep。
	bool bWindowOpen{false};
	// 同步动画实际位置，并清理已经抵达的排队节点。
	void Refresh(const USCLAttackData& Data, const UAnimInstance& Anim);
	// 把 BufferedStep 写入 Montage 的下一段关系。
	bool Advance(const USCLAttackData& Data, UAnimInstance& Anim);
};
