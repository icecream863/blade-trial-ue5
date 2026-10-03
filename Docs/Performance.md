# 性能记录

采样工具为 Unreal Insights，环境为 UE 5.8、Windows 11、i9-13900H、16 GB 内存。以下数据来自 2026-09-04 的 Editor PIE 样本，采集早于后续镜头和 Boss 数值调整。

## 实际战斗样本

CPU 耗时以每次调用平均 inclusive 毫秒计，包含子调用，嵌套项不能直接相加。

| 样本 | 标记 | 调用数 | 平均耗时 |
| --- | --- | ---: | ---: |
| Boss 二阶段战斗 | WeaponTrace | 1825 | 0.024 ms |
| Boss 二阶段战斗 | AIContext | 418 | 0.010 ms |
| Boss 二阶段战斗 | AIAttackDecision | 29 | 0.207 ms |
| Boss 二阶段战斗 | VisualRoll | 290 | 0.025 ms |
| Boss 锁定 | LockOnTick | 1846 | 0.037 ms |

Boss 战斗的 70 秒分析区间包含 4197 个完整游戏帧：平均 16.665 ms，p99 19.292 ms，最大 40.737 ms。唯一超过 33.333 ms 的帧包含 23.864 ms 的 `ConditionalCollectGarbage`，主要来自引擎对象可达性和编辑器插件 GC。

锁定样本有五次约 330 ms 的长帧，其中约 324–327 ms 位于引擎帧率限制等待；锁定更新本身最大 0.2079 ms。

这些样本没有显示当前单 Boss 规模存在明显的战斗 CPU 热点，因此没有据此调整采样密度或 AI 更新频率。数据不是独立打包版本的帧率保证，也不覆盖大量敌人、联网和内存分配成本。

## 更新频率

| 路径 | 触发方式 |
| --- | --- |
| Weapon Trace | Montage Notify 有效窗口内更新 |
| 锁定候选查询 | 获取和切换目标时触发 |
| 锁定朝向与视线 | 锁定期间每帧更新 |
| AI 上下文 | BT Service 0.2 秒及感知事件 |
| 开发 HUD | 可见时每 0.25 秒刷新 |
| Debug Draw | 启用时每 0.1 秒刷新 |

## 复现入口

`Debug/SCLProfilingSubsystem` 提供显式启用的固定场景采集。以 `-SCLLab -SCLProfile=Idle`、`-SCLLab -SCLProfile=Boss` 或 `-SCLLab -SCLProfile=BossDebug` 启动 Game 实例，使用 `-trace=cpu,frame,bookmark,region -statnamedevents` 记录，并在 Insights 中筛选 `SCL.Measure` 区间。

固定 Boss 采集场景会提高测试玩家生命以维持测量窗口，应与实际游玩样本分开分析。它只在传入相应命令行参数时启用。
