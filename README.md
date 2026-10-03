# 刀术试炼 / Blade Trial

基于 Unreal Engine 5.8、C++ 和 Gameplay Ability System 的个人第三人称近战战斗 Demo。六个相连区域串起训练、敌人战斗与双阶段 Boss，包含轻重连招、格挡、弹反、处决、闪避和目标锁定。

## 技术要点

- 使用 GAS 管理战斗技能、体力消耗与角色状态；连招数据支持轻重攻击分支和输入缓存。
- 通过 Montage/NotifyState 控制接招、无敌与命中窗口，使用武器 Sweep 判定伤害，并用 Motion Warping 调整攻击和处决的贴近位移。
- 使用 AI Perception、Behavior Tree、EQS 与 Boss 出招评分实现敌人决策；关卡流程支持区域推进、失败重试和 Boss 阶段切换。

项目使用 Epic 模板与 GhostSamurai 第三方素材；仓库未包含完整 GhostSamurai 素材包，完整运行需要自行取得授权资源。详见[资源说明](THIRD_PARTY_NOTICES.md)。
