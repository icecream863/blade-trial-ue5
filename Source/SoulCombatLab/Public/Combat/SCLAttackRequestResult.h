#pragma once

#include "CoreMinimal.h"

// 一次攻击请求的直接结果；调用者不必再读取组件的“上次执行”状态。
// 玩家请求与敌人的 Montage 分段请求共用此约定。
//
// 典型的玩家路径：
//   Rejected -> Ability 结束并标记失败
//   Buffered -> Ability 可以结束，但本次不能扣费；以后由 ComboWindow Notify 继续
//   Executed  -> Montage 已经成功起播；由当前攻击路径负责提交费用
enum class ESCLAttackRequestResult : uint8
{
	Rejected, // 无有效招式、缓存已占用或播放失败；这次请求没有被接受。
	Buffered, // 接招输入已接收，等待动画窗口或下一段；只缓存时不扣下一招费用。
	Executed  // 已启动本次攻击；玩家已由 Combat 完成扣费，敌人沿用 Ability 提交。
};
