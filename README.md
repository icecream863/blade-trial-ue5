# UE5 Melee Combat Demo

> Unreal Engine 5.8 · C++ · GAS · AnimMontage · Motion Warping

第三人称近战动作 Demo，重点实现连招、命中检测、格挡弹反、八向闪避、目标锁定、处决和战斗动作状态管理。

**Demo：待补充**

## 核心内容

### 连招与输入缓存
- 使用 DataAsset 配置轻 / 重攻击与后继关系
- 通过 AnimNotify 控制连招窗口，并支持单格输入缓存
- 仅在攻击真正起播后扣除体力，避免缓存输入提前或重复扣费

### 武器命中
- 使用 AnimNotifyState 控制武器判定窗口
- 对刀刃多采样点执行跨帧 Sphere Sweep
- 单次攻击窗口内进行目标去重，降低高速攻击漏判与重复伤害

### 战斗动作
- 支持格挡、弹反、八向闪避、目标锁定和失衡处决
- 使用 Motion Warping 对齐处决目标
- 集中管理速度、朝向和 Root Motion 限制，避免不同动作互相覆盖状态

### 中断恢复
- 对攻击中断场景增加旧回调失效保护
- 使用检测代次与 Montage 实例校验过滤失效逻辑
- 避免上一动作在中断后继续修改当前状态

### 敌人与 Boss
- 使用 AI Perception、Blackboard、Behavior Tree 与 EQS 组织敌人行为
- Boss 根据距离、阶段和近期攻击历史选择不同招式

## 技术栈

- Unreal Engine 5.8
- C++
- Gameplay Ability System
- Enhanced Input
- AnimMontage / AnimNotifyState
- Motion Warping
- AI Perception
- Behavior Tree / EQS

## 资源说明

仓库不包含完整第三方角色、动画和特效资源。相关资源依赖与授权说明见：

`THIRD_PARTY_NOTICES.md`

## 环境

- Unreal Engine 5.8
- Windows
- Visual Studio 2022 / Rider
