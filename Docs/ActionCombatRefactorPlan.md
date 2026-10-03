# SoulCombatLab 玩家刀术实施记录

## 目标与现状

主要目标是让玩家直接使用 GhostSamurai Bundle 的角色骨骼、刀、攻击动画和原版特效，并形成按素材命名组织的连击。玩家和 GameMode 蓝图放在 `Blueprints/Player`、`Blueprints/GameModes`，玩家战斗 Montage 与 Moveset 数据留在 `Characters/Player/Combat`。2026-09-24 的 UE 5.8 完整编译、资产审计和离屏自动化测试曾通过。2026-09-25 将玩家连招状态拆为专用组件并调整模块目录后，完整编译和资产脚本重新保存已通过；这次自动化游戏测试没有重跑。Editor 内连续动作、实际鼠标手感、PIE 的整体战斗回归仍待试玩。

先前把不同 `AttackXX` 的累计动画重定向到项目 Manny，并混成六招的方案已停止使用。运行时的玩家攻击引用 `/Game/SoulCombatLab/Characters/Player/Combat/Animations/` 下的十个原骨骼 Montage。

## 资产事实与分组

素材包的 `GhostSamurai_Katana` 蓝图使用 `/Game/GhostSamurai_Bundle/Demo/Characters/Mannequins/Meshes/SKM_Manny` 和 `/Game/GhostSamurai_Bundle/GhostSamurai/Blueprints/ABP_GhostSamurai_Katana`，二者都基于素材包的 `SK_Mannequin`。它的攻击 Montage 也在该骨骼上；这个 `SK_Mannequin` 与原 SoulCombatLab Manny 所用的是不同资产。玩家直接使用素材包原版网格、骨骼和刀术 AnimBP，不再运行时重定向攻击。

原蓝图没有可复用的输入或连击逻辑。原 Montage 只有 `Default` Section，但已写入逐招 Niagara 通知；连击选择由玩家专属 `PlayerComboComponent` 管理，Montage 播放、武器轨迹与伤害继续由共享 `CombatComponent` 执行。编号的证据是资源本身：`Attack01_1_ALL_Root` 与 `Attack02_1_ALL_Root` 是两组起手，后续选用对应的 `Attack01_2_Root` 等单招 Montage。若连续使用 `_2_ALL`、`_3_ALL`，会重复播放前面的累计动作。

| 鼠标输入 | 原素材组 | 运行时段号 | 总体力消耗 |
| --- | --- | --- | ---: |
| 左键短按，在 0.25 秒内松开 | `Attack02`，`NS_Slash_S` 系列 | `Attack02_1` → `2` → `3` → `4` → `5` → `6` | 88 |
| 左键长按，达到 0.25 秒 | `Attack01`，`NS_Slash_L` 系列 | `Attack01_1` → `2` → `3` → `4` | 95 |

每次后续输入在当前 Montage 的 `ANS_SCLComboWindow` 内推进同组下一招；切换短按或长按时从另一组第一招起手。两组最后一招没有接招窗口。一条提前输入缓存最多保留 0.7 秒，攻击取消时清除。右键仍为格挡。`Attack02` 对应轻击、`Attack01` 对应重击，是依据特效命名和动作力度采用的当前操作映射，不是素材包蓝图提供的游戏规则。

## 原版特效、武器与命中

`Scripts/CreateNativeGhostCombos.py` 从原 Montage 复制十个 Montage 到 `/Game/SoulCombatLab/Characters/Player/Combat/Animations/`。副本保留 Niagara 通知、原骨骼及源动画引用，并按斩击特效时间增加本项目的 `ANS_SCLWeaponTrace`；前八招还加 `ANS_SCLComboWindow`。`Attack01_3` 的两个斩击有两段扫掠。原包资产不改写。

玩家的 `ASCLGhostKatanaWeapon` 使用 `SM_Katana01`，按原 `GhostSamurai_Katana` 蓝图挂在素材包 Manny 的 `weapon_rSocket`；`SM_Scabbard01` 按原蓝图挂在 `Scabbard_Target01Socket`，两者相对变换均为零。刀刃扫掠使用刀上的两个端点，命中时触发 `NS_Slash_Hit_L`。原 Montage 的 `NS_Slash_L`、`NS_Slash_S`、`NS_Slash_Stab_01`、`NS_Slash_SP01`、`NS_Slash_Trail_01` 等通知仍按原时间播放。截图可用于核对模型、刀和特效的单帧位置，连续衔接须在 PIE 中判断。

包内也有 `SM_Quiver_01`。资产检查确认它挂在 `GhostSamurai_Bow` 蓝图的 `Quiver_Socket`，而 `GhostSamurai_Katana` 蓝图没有箭袋；因此当前纯刀术玩家不显示箭袋。以后增加弓箭状态时，可按原弓箭蓝图同时装配箭袋、弓和箭。

素材包和十个副本 `.uasset` 在本机项目中；第三方包目录被 `.gitignore` 排除。可复现的复制规则在上述脚本中，其他机器需要先合法取得并导入同一素材包。不要将本地测试通过误称为公开仓库可独立运行。

## 验证记录

- `CreateNativeGhostCombos.py`：十个 Montage 生成成功；资产审计确认十个都指向素材包 `SK_Mannequin`，各自保留 1～3 个原版特效，前八个有连击窗口、每个至少一个武器扫掠窗口。
- `Build.bat SoulCombatLabEditor Win64 Development`：2026-09-24 完整编译成功。
- `SoulCombatLabCombat.GhostPlayerCombo`：2026-09-24 前一版离屏游戏测试通过，覆盖 6 段轻击、4 段重击、跨组起手、体力扣除、取消缓存、自然 Montage 通知推进、原骨骼武器伤害以及实际角色模型和动画蓝图。2026-09-25 拆分玩家连招组件后仅重新编译了测试源码，尚未重跑测试。
- `SoulCombatLabCombat.ComboRecovery`：2026-09-24 前一版通过。覆盖受击打断、立即重新起手、旧 Montage 回调隔离、恢复后接招命中和削韧，以及取消后的移动与 Root Motion 恢复；2026-09-25 组件拆分后尚未重跑。
- 2026-09-25 `Build.bat SoulCombatLabEditor Win64 Development`：`Public/` 与 `Private/` 目录迁移及玩家连招组件拆分后完整编译成功（88 个构建动作）。
- 2026-09-25 `Scripts/OrganizePlayerCombatAssets.py`：Unreal Python commandlet 执行成功（0 个错误）；玩家蓝图默认招式配置写入 `PlayerComboComponent`。
- `Saved/Screenshots/GhostKatanaIdle.png`、`GhostKatanaHeavy.png`：离屏截图已检查，玩家持刀与斩击特效可见。截图不足以判断连续动作和手感。

## 下一步试玩检查

打开 `SoulCombatLab.uproject`，在训练区实际短按和长按左键，依次检查两组动作是否流畅、刀刃与 Niagara 轨迹是否重合、接招窗口是否适合操作、Root Motion 与移动锁定是否自然。再检查受击、翻滚、格挡、弹反、处决和重试后的缓存清理。发现具体问题时按对应 Montage 的 `SCL_Trace`、`SCL_Combo` 轨道和刀具挂点调整，并重新运行自动化测试。
