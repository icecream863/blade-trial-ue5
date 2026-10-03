# 测试

## 2026-10-02 玩家攻击职责与教学课程整理（当前结果）

- 玩家连招组件只选节点和保存/消费输入，不再包含 Combat 引用或反向执行调用。Combat 编排请求，所有玩家起手、窗口内接招和 Notify 延迟接招统一进入 `StartPlayerAttackStep(StepIndex)`；不再用布尔参数决定提交费用的类。玩家 Ability 只做资格预检查和输入提交；敌人原有 Ability/Section 流程保留。下方历史记录中的玩家成本快照和两处扣费分工已被本次实现替代。
- 中文注释解释类边界、选招结果与执行结果的差别、四段起播顺序、实例身份和失败清理。玩家 Effect 配置在 Combat 的 `Combat|Player Effects`，每招成本仍在 Moveset；收刀/拔刀默认仍暂停。
- 最终 Editor Win64 Development 构建成功，日志 `Saved/BuildLogs/SoulCombatLabEditor-Win64-Development.log`。曾尝试调用 Engine 未导出的 `HasValidSlotSetup`，链接失败；现预检查动画时长、骨架和 Slot 数据，完整起播结果仍以 `Montage_Play` 为准，最终构建已修正通过。
- 七项真实游戏回归的最终结果均为 `Result={Success}`，进程退出码均为 0：RuntimePolish、GhostPlayerCombo、WeaponSheath、ComboRecovery、PlayerInput、RuntimeFlow 的日志为 `Saved/Logs/AttackBoundary-<Name>.log`；CameraDifficulty 最终日志为 `Saved/Logs/AttackBoundary-Final-CameraDifficulty.log`。命令为 `Scripts/ValidateCombatAnimation.ps1 -Only <所需测试标签> -LogPrefix AttackBoundary`；脚本验证测试标签，避免拼错标签却执行零项。
- GhostPlayerCombo 新增直接入口不能绕过体力、直接起手只扣一次、缓存/拒绝不额外扣费、缓存后体力不足不破坏当前一刀且不自动重试、窗口内即时接招同价、零成本、负成本拒绝、缺失及不可播放 Montage 不扣费/不改变根运动模式等实际断言。既有六轻四重、跨组分支、真实 Notify 和武器伤害检查继续通过。
- CameraDifficulty 初次失败是旧测试固定等 0.7 秒便检查处决伤害；同一日志随后实际 Notify 正常结算 40.2 伤害。测试改为等待真实生命变化和技能结束，保留游戏时间超时及精确生命/阶段断言。最终确认 Boss 为 160.8 HP、进入二阶段，动画正常完成；保留最初失败日志，未把退出码 0 当作测试通过。
- HTML 主文与折叠说明同步修改，包含连续阅读路线、选招/执行区别、单一起播费用流程、参数修改位置和失败排查。Playwright 浏览器检查通过：15 章、87 类型、159 本地链接无缺失，页面无脚本错误；轻重接招、过期缓存、进度保存和类搜索通过。390 px 页面宽度无溢出，桌面/手机截图已检查。报告 `Saved/DocumentationQA/AttackBoundary-BrowserReport.txt`；截图 `output/playwright/AttackBoundary-*.png`。课程与类目录连续重新生成的字节哈希一致。
- 边界：本轮确认上述自动化游戏路径和课程交互，没有新增网络客户端验证或完整人工 PIE 手感验收。Combat 仍包含敌人 Section 与范围攻击等职责，不把本轮攻击主线整理称为整个工程已完成职责拆分。全仓 `git diff --check` 仍指出既有 `Config/DefaultGame.ini` 文件尾空行；本轮所改源码的新增行空白检查通过。

自动化测试源码位于 `Source/SoulCombatLab/Private/Tests`，包括纯策略断言、World 生命周期和实际地图运行三类。可在 Unreal Editor 的 Automation 窗口按 `SoulCombatLab` 前缀查找。

## 2026-10-01 防御、处决与锁定移动动画

当前玩法后来暂停了自动收刀与拔刀；本节的早期 `WeaponSheath` 结果记录的是暂停前的行为。最新持刀验证见下方“暂停收刀与拔刀”小节。

- `SoulCombatLabEditor / Win64 / Development` 最终增量编译通过。`Scripts/VerifyCombatAnimationAssets.py` 在重新加载后确认：弹反 Montage 有一个有效窗口，处决 Montage 有命中通知与 Motion Warping 窗口；持刀、收刀、防御三套 Blend Space 各有九个非空方向采样，项目 AnimBP 的三个播放器实际引用这些资产。
- 在可渲染 `UnrealEditor-Cmd.exe -game` 中运行 `Scripts/ValidateCombatAnimation.ps1`：`RuntimePolish`、`GhostPlayerCombo`、`WeaponSheath`、`RuntimeFlow` 最终均为 `Result={Success}` 且进程退出码 0。日志位于 `Saved/Logs/CombatAnimation-*.log`。测试包含弹反 Notify 有效帧、处决起手无胶囊瞬移及 Notify 后落点距离、处决伤害、格挡按住/松开速度恢复、失败的格挡和处决请求不拔刀、连招真实武器命中及关卡重建。
- 单独运行 `Scripts/ValidateCombatAnimation.ps1 -Only LockedLocomotion`：锁定八方向测试 `Result={Success}`、退出码 0；八个方向及前进、右移的额外相位抽样图位于 `Saved/Automation/LockedMove-*.png`。测试断言运行中的玩家 AnimInstance 是项目类、锁定持续有效、每个方向的角色局部速度符号正确，以及锁定步速不超过 180 cm/s。源持刀步行动画的根位移采样约 190 cm / 1.13 s，锁定步速据此默认设为 180 cm/s。格挡中丢锁、松开格挡后恢复自由步速也通过。抽样帧能显示后退、左移等步伐差异，但离散截图不能证明连续脚底接触或完全无滑步；这仍需在 PIE 连续移动观察。
- 2026-10-01 用户发现“脚上没有动画，全在平移”后，新增足部骨骼断言，旧资产立即失败：前进、右移时 `foot_l` 相对 Mesh 的位移都是 `0.00 cm`。根因是脚本仅写入九个 `SampleData`，三套 Blend Space 的运行时插值数据均为空。调用 `ResampleData()` 并保存资产后，三套数据均非空；同一回归通过，前进位移 `54.27 cm`，右移 `48.58 cm`，新抽样图可见真实迈步。这个断言验证骨骼在动，连续脚底贴合仍需 PIE 观察。
- 首轮旧断言假设攻击原地不动、按墙钟猜弹反窗口及处决尸体清理时机，已按新的根运动和动画通知修正。另一次运行发现玩家蓝图仍引用素材包 AnimBP，现已保存为项目 `ABP_SCLPlayerCombat`，并由 `GhostPlayerCombo` 的实际 AnimInstance 断言复验。

## 2026-10-01 暂停收刀与拔刀

- `USCLWeaponPresentationComponent::bEnableSheathDraw` 默认 `false`。运行时不加载两段 Montage、不启动空闲计时器；轻重攻击直接进入原 GAS/Combo 路径。项目动画资源和原实现保留，之后可在玩家蓝图组件默认值中重新启用。
- 编辑器 Development 完整构建通过。可渲染游戏回归 `WeaponSheath`、`GhostPlayerCombo`、`LockedLocomotion`、`RuntimePolish` 均为 `Result={Success}`、退出码 0。`WeaponSheath` 的关闭模式分支等待超过原空闲阈值，验证持续移动、未收刀、武器仍挂手部，以及轻击不等待拔刀直接起播。日志在 `Saved/Logs/CombatAnimation-*.log`。

## 打包回归

完成 Development 打包后，在项目根目录执行：

```powershell
.\Scripts\ValidateSixRegion.ps1 -Screenshots
```

脚本串行启动离屏 D3D12 实例，使用独立的 `Saved/Validation` 设置目录。每项同时检查测试成功标记和进程退出码。

| 测试 | 覆盖内容 |
| --- | --- |
| RuntimeFlow | 六区域推进、清场开门、区域重试和重开 |
| RuntimePolish | 受击、弹反、失衡、处决与死亡清理 |
| ComboRecovery | 受击中断后立即重启三连的伤害和削韧 |
| CameraDifficulty | 自由/锁定镜头恢复、双敌血条、暂停与 Boss 参数 |

2026-09-05 的 Win64 Development 包通过上述四项测试，退出码均为 0。连段恢复场景总伤害为 67、总削韧为 40；Boss 以 400 生命开场，轻击实测 18 伤害，201 生命时处决扣除 40.2，剩余 160.8 并进入二阶段。

测试使用显式的场景摆位与伤害输入检查状态。镜头遮挡、操作手感和其他硬件兼容性仍需要实际运行检查。

## 2026-09-28 重构回归

- UE 5.8 `SoulCombatLabEditor / Win64 / Development` 完整编译通过。
- 使用新编译的编辑器启动 PIE，经 MCP 初始化与 `tools/list` 确认连接；PIE 日志显示加载 `BP_SCLPlayerGameMode_C`，Demo 完成初始化。
- 离屏 `-game` 运行 `SoulCombatLabDemo.RuntimeFlow`：测试结果 `Success`，退出码 0；其中断言重生 Pawn 的实际类等于 GameMode 为 Controller 选择的类。
- 离屏 `-game` 运行 `SoulCombatLabCombat.GhostPlayerCombo`：测试结果 `Success`，退出码 0；本地 GhostSamurai 素材存在，测试未跳过。首次运行在脚本驱动多段动作后未重新放置目标，两个自然命中断言失败；修正测试场景摆位并重新编译后通过。

上述结果验证了重生类、连招与自动化武器命中，不代表人工操作手感、镜头遮挡或正式打包已复验。

## 2026-09-30 输入职责整理回归

- `SoulCombatLabEditor / Win64 / Development` 完整编译通过；新增的 `SCLPlayerCharacter.Attack.cpp` 进入编译。
- 使用新编译的 `UnrealEditor-Cmd.exe -game` 离屏运行 `GhostPlayerCombo`、`ComboRecovery`、`RuntimeFlow`，三项日志均出现 `Test Completed. Result={Success}`，退出码均为 0。日志保存在 `Saved/Logs/Refactor-*.log`。
- 这些自动化场景验证连招、受击恢复与重生输入映射生命周期；尚未用真实鼠标操作验证短按、长按和手感。

## 2026-09-30 实现文件与学习入口整理

- UE 5.8 Editor Development 完整编译通过（39 个构建动作）。角色输入与调试、玩家与敌人攻击实现拆分后的文件均参与编译。
- 再次运行 `GhostPlayerCombo`、`ComboRecovery`、`RuntimeFlow`，三项均为 `Success`，退出码均为 0。日志为 `Saved/Logs/Structure-*.log`。
- HTML 的 159 个本地链接和 27 个 ID 检查通过；本机 Edge 检查 82 个类型的中英文搜索、无结果提示、搜索重置、4 个场景切换，无页面脚本错误。390 px 窄屏未产生整页横向溢出，宽表格在自身容器内横向滚动。
- 页面截图保存在 `Saved/DocumentationQA`。本轮没有进行人工鼠标手感验证或重新打包。

## 2026-09-30 Controller 蓝图与 UBT trace 修复

- 创建 `Blueprints/Player/BP_SCLPlayerController`，继承原生 `ASCLDemoPlayerController`，并保存 `BP_SCLPlayerGameMode` 的 `Player Controller Class` 引用。资产脚本成功退出，0 个错误。
- 新增 RuntimeFlow 断言：初次进入、区域重试与重开后，实际 Controller 类路径必须为 `BP_SCLPlayerController_C`，且与 GameMode 配置一致。回归结果 `Success`，退出码 0，日志 `Saved/Logs/Controller-RuntimeFlow.log`。
- Windows 2026-09-15 的 `.NET Runtime / 1026` 记录显示 UBT 在 `Log.BackupLogFile` 移动 `Trace.uba` 时抛出 `FileNotFoundException`。并发探测还复现旧备份删除的 `UnauthorizedAccessException`。
- UE 5.8 本机 UBT 启动 trace 轮换加入跨进程互斥，并将这一诊断备份步骤的 I/O 异常记录为警告；正常构建互斥保留。四个并发 `ValidatePlatforms -WaitMutex` 探测均退出 0，检查期间无新的 dotnet 崩溃事件。
- 原始源码和 UBT 二进制保存在 `Saved/Repairs/UbtTrace-20260930-162133`；后续迭代备份也在 `Saved/Repairs`。这是本机引擎补丁，引擎升级或校验可能覆盖它；未以短时验证承诺以后所有 dotnet 程序均不再崩溃。

修复后的 UBT 执行正常 Editor 构建也成功（目标已是最新，0 个编译动作），未记录 trace 启动异常。

## 2026-09-30 职责纠正（当前结构）

本节取代前面按 `.Input.cpp`、`.Attack.cpp`、`.Debug.cpp` 拆分同一个类的组织方式。当前每个类只有一份头文件和实现；具体职责见 `Architecture.md`。

- Character 的移动、动作请求与映射生命周期回到 `SCLPlayerCharacter.cpp`。Controller 统一绑定和解释按键；开发场景实现及临时 Boss 状态迁入 `USCLPlayerDeveloperComponent`。
- 蓝图迁移前读取到重击阈值为 0.25 秒，迁移后保存在 `BP_SCLPlayerController`。玩家、Controller 和 GameMode 蓝图编译保存成功，玩家 CDO 的开发组件校验通过。
- Editor Development 编译通过。新增 `PlayerInput` 回归覆盖 Controller 短按、长按、重击后的松开及取消定时器；与 `GhostPlayerCombo`、`ComboRecovery`、`RuntimeFlow` 共四项均为 `Success`，退出码均为 0。日志为 `Saved/Logs/ClearStructure-*.log`。
- HTML 的 153 个源码链接有效，83 个类型职责目录更新成功；开发组件搜索及页面脚本检查通过。未进行真实鼠标手感验证或重新打包。


## 2026-09-30 攻击请求显式传参

- 长按阈值归 `SCLDemoPlayerController`；UE 资产检查确认 `BP_SCLPlayerController` 的实际阈值为 0.25 秒，玩家 GameMode 使用此 Controller。
- `SCLLightAttackAbility::GetAttackInput()` 返回 Light，重击覆盖为 Heavy；成本查询和攻击请求显式传入同一类型。删除 ComboComponent 的 PreparedInput 及 Prepare/Clear/Get 接口。
- 保留 BufferedInput：它是等待动画窗口的真实连招缓存。保留 PendingStaminaCost：它是起播前的成本快照，避免节点切换后扣错费用。
- 调试场景实现、定时器和临时 Boss 引用归 `SCLPlayerDeveloperComponent`，Character 控制台命令只转发。UE 资产检查确认玩家蓝图实际具有该组件。
- Editor 编译成功；HTML 浏览器无 pageerror，83 个类说明，136 个本地文件链接存在。
- 资产证据：`Saved/Logs/ExplicitResponsibilities-Assets.log`。游戏自动化结果见下。

- 游戏自动化均 Success，进程退出码均为 0：`SoulCombatLabCombat.PlayerInput`、`SoulCombatLabCombat.GhostPlayerCombo`、`SoulCombatLabCombat.ComboRecovery`、`SoulCombatLabDemo.RuntimeFlow`。
- 测试覆盖 Controller 短按/长按/取消、轻重起手与分支、连招扣费与缓存、中断恢复，以及 Demo 重生/重开后的 Controller 配置。前三项日志为 `Saved/Logs/ExplicitResponsibilities-<Name>.log`，流程最终成功日志为 `Saved/Logs/ExplicitResponsibilities-RuntimeFlow-Final.log`。
- 验证过程：第一次流程命令使用错误分类，执行了 0 项测试，未计为通过；随后 Local DDC 尝试在测试前因用户目录的着色器传输文件写入失败而退出，未计为通过。恢复原缓存配置并使用正确测试分类后完成测试。
- 这些结果证明本次职责迁移与攻击传参的自动化路径；没有替代人工按键手感评估，也未逐个运行所有开发场景命令。


## 2026-09-30 攻击请求结果与成本边界

### 本次改动

- 用 ESCLAttackRequestResult 明确区分 Rejected / Buffered / Executed，Combo 和 Combat 直接返回结果。删除 bOutExecuted、WasLastAttackRequestExecuted 和 bLastAttackRequestExecuted，避免结果通过两处状态传递。
- 兼容的 StartLightAttack() 与无参数 RequestAttack() 仍返回是否接收的 bool。敌人 SetNextSection 只安排后继段，返回 Buffered；初次播放返回 Executed。
- 将玩家输入缓存有效期从固定 0.7 秒改为 PlayerComboComponent 的 InputBufferLifetime 蓝图配置，默认值保持 0.7。
- 发现并复现提交成本的错误：起播切换当前节点后，UE CommitAbilityCost 再次 CheckCost，旧实现查到了后继成本。12 点体力无法启动 12 点轻击。
- 用 TOptional<float> 保存本次成本快照；激活前查询待选节点，提交检查与 ApplyCost 共用快照，结束后清除。Optional 区分未保存和合法零成本。

### 证据

- 修复前真实游戏测试：Saved/Logs/AttackCost-BeforeFix.log，GhostPlayerCombo 为 Fail。轻击起手名为 None，体力仍为 12，精确成本两条断言失败。UE 进程即使退出码为 0，也不能据此当作测试通过。
- 修复后 Editor 编译成功。测试加入 11 点不足、12 点轻击、20 点重击，以及 Executed / Buffered / Rejected 对真实 Montage 和缓存状态的断言。
- HTML 浏览器无 pageerror，结果表与配置说明存在，类搜索通过；137 个本地文件链接存在。截图 Saved/DocumentationQA/AttackResultContract.png。

- 修复后游戏回归均 Success，进程退出码为 0：GhostPlayerCombo（Saved/Logs/AttackCost-AfterFix.log）、ComboRecovery（Saved/Logs/AttackCost-ComboRecovery.log）、CameraDifficulty（Saved/Logs/AttackResult-CameraDifficulty.log）。
- UE 资产读取确认 BP_SCLPlayerSamurai 的 PlayerComboComponent 暴露 input_buffer_lifetime，实际值为浮点表示的 0.7 秒（Saved/Logs/AttackResult-BufferConfiguration.log）；未改写用户配置资产。
- 本次证明了所列攻击、扣费、中断和 Boss 回归路径；缓存自定义值的手感仍由人工调试评估。


## 2026-09-30 中文代码阅读注释

- 在玩家角色、Controller、连招、Combat、轻重 Ability、Moveset、武器采样、动画通知和开发调试组件共 19 个文件补充中文说明。重点是调用来源、参数/结果含义、状态生命周期、扣费分工、取消顺序与同步回调保护。
- 关键实现使用函数说明和编号步骤：构造装配、GAS 成本快照与执行请求；状态字段说明其有效时段、所有者和清理位置。
- 与修改前快照比较，去除注释后的代码 token 一致；报告保存在 Saved/CommentPassVerification.json。本次验证是注释与可执行代码一致性检查，没有新增游戏运行验证结论。
- HTML 类速查由 Scripts/UpdateLearningCatalog.py 从头文件重新生成，仍为 83 个类型。

## 2026-09-30 完整 HTML 学习课程

- 15 章新增连续讲解、术语和 C++ 写法速查、当前源码节选、十招表、修改步骤、排错表与自测答案；原有详细证据保留在各章折叠区域。
- 第 04 章加入连招教学模型，显示节点、输入缓存、窗口、虚拟时间、体力与操作记录；不驱动 UE，不模拟动画、碰撞、网络或体力再生。图和费用是本页配置快照，不会随资产自动更新。
- 浏览器验证：15 章正文 / 15 道自测 / 83 个类型 / 143 个本地文件链接，未发现缺失文件或页面脚本错误。验证轻轻、轻重、缓存占用、超时、窗口内输入、取消与结束后起手、关闭窗口清缓存、12/20 点精确成本和 11 点不足，以及学习进度刷新保存、类搜索与原有四个时序页签。
- 1440 px 桌面与 390 px 手机截图已检查，手机正文没有页面横向溢出。报告：Saved/DocumentationQA/CompleteCourse-Report.json；截图：CompleteCourse-Desktop.png、CompleteCourse-Combo.png、CompleteCourse-Mobile.png，均位于同一目录。
- 分章讲义保存在 Docs/CourseLessons.json；运行 python Scripts/BuildLearningCourse.py 更新正文并重新提取源码节选。连续运行生成器的文件哈希一致。本轮修改仅涉及课程、文档与生成脚本，没有新增 UE 游戏运行验证。

## 2026-09-30 输入配置与生命周期归 Controller

- 删除 Character 中全部 InputAction / MappingContext 字段、输入安装/卸载、SetupPlayerInputComponent 转发和 friend 访问。Controller 持有默认资产和动作、绑定自身输入组件，通过 GetPawn 操作当前角色。
- Controller 占有时安装本地映射，失去 Pawn 时移除映射并清长按、移动意图、跳跃和格挡；退出世界时收尾。运行战斗映射使用 Transient + VisibleInstanceOnly，在 PostInitializeComponents 中创建。
- 迁移前通过当前 UE MCP 读取玩家蓝图 CDO 的七项可配置输入引用，保存为 Saved/LegacyPlayerInputAssets.json；CreatePlayerControllerBlueprint.py 迁移并核对 Controller 蓝图值，保存标记 Saved/ControllerInputAssetsMigrated.json。原蓝图引用没有按默认值直接覆盖。
- 保留 DeveloperComponent 和 CachedMovementInput：前者以角色为 Owner 实现实验，后者是该身体供移动/躲闪共享的意图。它们不是按键资产或映射配置。
- 增加 PlayerInput 回归的模拟键注入及占有/卸载检查。早期用构造函数默认映射的版本失败：动作值变为 1，但映射指向蓝图 CDO 的 AttackHoldAction，回调绑定的运行对象不同，不能启动攻击。失败证据保留在 ControllerInput-PlayerInput-Final.log；改为运行实例建图后，日志动作路径属于实际 Controller，真实 Montage 起播断言通过。
- 最终 Editor Development 编译成功。游戏回归均找到各自一项测试且 Result={Success}、退出码为 0：PlayerInput、RuntimeFlow、ComboRecovery。日志分别为 Saved/Logs/ControllerInput-Verified-PlayerInput.log、ControllerInput-Verified-RuntimeFlow.log、ControllerInput-Verified-ComboRecovery.log。
- 模拟键注入从 PlayerInput 开始，验证映射、Controller 回调和起播，不证明 Windows 物理鼠标设备分发。RuntimeFlow 覆盖六区域、死亡重试与胜利重开映射；本轮没有网络客户端验证。
- HTML 与 Architecture 同步更新归属、初始化和重生流程。浏览器验证仍为 15 章 / 83 类型 / 143 个有效本地链接、无 pageerror，模拟器与学习进度检查通过。
- 最终蓝图编译保存成功（ControllerInput-BlueprintFinal.log），编辑器已重新启动。重连 MCP 后读取 Controller 蓝图 CDO，七项引用与迁移前快照一致，证据 Saved/ControllerInputAssetsFinalVerified.json。


## 2026-09-30 自动收刀与动作中断

- 新增 WeaponPresentationComponent 与 AN_SCLSheathWeapon；生成 AS_PlayerSheath / AM_PlayerSheath 项目副本。原始 Unarm_1 时长 2.7 秒，实际骨骼采样显示 1.5 秒时握刀点与入鞘点已对齐；在 Montage 放置此刻的入鞘 Notify。
- Editor Development 全量/增量构建成功，最终构建日志 Saved/BuildLogs/SoulCombatLabEditor-Win64-Development.log。
- 最终三项回归均找到测试且 Result={Success}、进程退出码 0：WeaponSheath、ComboRecovery、RuntimeFlow。日志 Saved/Logs/Sheath-Final-WeaponSheath.log、Sheath-Final-ComboRecovery.log、Sheath-Final-RuntimeFlow.log。
- WeaponSheath 使用实际关卡玩家、动画和真实 Notify，验证空闲起播、通知前持刀、通知后挂到 katana_Targer01Socket、结束保留入鞘、胶囊没有根位移、根运动模式恢复、入鞘后攻击立即起播且只扣 12 点、移动阻止/取消收刀、受击状态和攻击中断。移动/受击采用意图/状态注入，不代表实际敌人命中的完整流程。
- WeaponSheath 使用真实 GPU 渲染，检查 Saved/Automation/Sheath-Animation.png、Sheath-Commit.png、Sheath-Idle.png 三帧；不是 NullRHI 的画面推断，也不等同于逐帧动画/穿模审查。另两项使用 NullRHI，覆盖连招恢复和实际死亡重生/关卡循环。
- 初次按现实时间检查动画，在截图停顿时提前断言；改为游戏时间。随后向主角注入死亡状态会让演示关卡暂停，不能继续靠游戏计时结束测试；删除这段注入，使用 RuntimeFlow 已有的真实死亡/重生回归。早期失败日志保留，不作为成功证据。
- 再次攻击目前立即换回握刀挂点，没有独立拔刀动画。空闲收刀默认要求静止，不抢其他 Montage。尚未验证网络同步；当前组件定位是本地玩家表现。
- HTML 第 07 章加入组件职责、完整调用链、挂点、通知、根运动与参数位置。浏览器验证 15 章 / 85 类型 / 149 本地链接，无缺失文件或页面脚本错误；原有学习进度、连招演示、时序页签和移动排版检查通过。
- 编辑器已恢复并重新完成 MCP 握手与工具调用。读取玩家蓝图组件确认等待 3 秒、AM_PlayerSheath 引用、入鞘挂点；读取 Montage 确认 2.7 秒及匹配骨架，保存三项资产成功。证据 Saved/SheathFinalMcpVerified.json。


## 2026-10-01 移动收刀与拔刀衔接（替代上一节的行为描述）

- 资源包中的 `GhostSamurai_APose_Equip01_Root` 与 `Equip02_Root` 都存在且匹配玩家骨架。骨骼采样显示 Equip01 全长约 1.367 秒，0.25 秒手部挂点与入鞘挂点相距约 0.6 cm；项目副本 `AS_PlayerDraw` / `AM_PlayerDraw` 在 0.30 秒设置 `AN_SCLDrawWeapon`。原包资产未改动。
- 移动不再阻止或取消收刀，角色移动期间可播放 `AM_PlayerSheath`，动画本身忽略根位移，CharacterMovement 继续推动胶囊。入鞘后轻/重请求只保存第一笔；拔刀通知把武器切回手，拔刀完成后按原 GAS/Combo 入口出招。受击等动作取消拔刀并丢弃待发攻击。
- Editor Development 构建成功。最终顺序回归 `SoulCombatLabCombat.WeaponSheath`、`SoulCombatLabCombat.ComboRecovery`、`SoulCombatLabDemo.RuntimeFlow` 均为 Result={Success}、退出码 0；日志分别在 `Saved/Logs/Sheath-Final-WeaponSheath.log`、`Sheath-Final-ComboRecovery.log`、`Sheath-Final-RuntimeFlow.log`。
- WeaponSheath 使用可渲染游戏窗口与真实动画 Notify，断言移动中收刀起播、胶囊持续移动、正确换挂点、拔刀期间不扣费、完成后轻/重各自起播并扣费，以及失衡打断后没有延迟攻击。截图 `Saved/Automation/Sheath-Animation.png`、`Draw-BeforeGrip.png`、`Draw-AfterGrip.png` 已检查；它们是抽样帧，不代表逐帧视觉审查。
- 曾以固定等待时间检查最后一次入鞘，实际重击状态会延后下一次收刀起点；现等真实入鞘通知。曾在重击起播后过晚读取体力，自动再生已恢复数值；现从首个攻击帧检查。失败日志是测试时机诊断，不是最终通过证据。
- HTML 动画章已经覆盖移动收刀、拔刀通知、待发轻重输入和用户可编辑位置。浏览器验证 15 章、86 类型、151 个有效本地链接，无页面错误；连招演示、进度保存和移动布局检查通过。
- 项目未做网络客户端武器挂点同步验证；这些行为在当前本地玩家流程中验证。
- 最终重启 UE 编辑器并通过本地 MCP 读取玩家蓝图组件：等待 3 秒、收/拔刀 Montage 均指向项目副本；读取两段 Montage 确认 2.7 / 1.367 秒且骨架相同，保存玩家蓝图和四个动画资源成功。证据 `Saved/DrawSheathFinalMcpVerified.json`。


## 2026-10-02：攻击职责与移动限制重构

> 以下记录是本日上午重构时的历史状态。攻击播放与范围查询后来收回 Combat，当前结构及重新运行的结果见文末“Combat 内聚整理”。

### 本轮改动

- Combat 实现从 1212 行降为 753 行。AttackAnimation 持有 Montage 实例、通知身份、前摇跟转和攻击 Warp；MontageComboState 持有敌人 Section 状态；AreaAttack 持有延迟查询与取消代次。
- ActionMovement 是游戏逻辑中步速、朝向申请和根运动模式的统一管理位置。锁定、格挡、攻击、弹反、处决、武器表现各自撤销自己的申请，移除分散的旧值快照与跨系统延迟恢复。
- 范围查询与动画结束分别处理：等待查询时保留伤害配置；查询完成而动画仍播放时保留取消入口；同步取消会让旧查询代次失效。
- 现有 Blueprint 入口和配置位置保留。收刀、拔刀默认仍关闭；WeaponSheath 回归内部显式开启保留路径，不代表默认已开启。

### 验证结果

Editor Win64 Development 构建通过。以下九项均在独立可渲染游戏进程中运行，具有 Result={Success} 标记与退出码 0：

| 用例 | 本轮关注点 | 日志 |
| --- | --- | --- |
| ActionLifecycle | 步速申请顺序、释放顺序、基础步速更新、朝向/根运动交叠、范围回调同步取消、敌人 Section 连接与末段终止、真实范围伤害及取消后无延迟伤害 | Saved/Logs/Architecture-ActionLifecycle.log |
| RuntimePolish | 格挡、弹反、攻击根位移、目标死亡及剑兵/Boss 实际攻击 | Saved/Logs/Architecture-RuntimePolish.log |
| GhostPlayerCombo | 轻重连招、缓存、费用边界、失败请求 | Saved/Logs/Architecture-GhostPlayerCombo.log |
| LockedLocomotion | 八方向局部速度符号、项目 AnimBP、脚骨变化、格挡中丢锁恢复 | Saved/Logs/Architecture-LockedLocomotion.log |
| WeaponSheath | 显式启用保留的收拔刀路径、真实 Notify 与中断恢复 | Saved/Logs/Architecture-WeaponSheath.log |
| ComboRecovery | 受击后重启连段、旧实例不能清理新招、取消恢复根运动 | Saved/Logs/Architecture-ComboRecovery.log |
| PlayerInput | Controller 输入与轻重请求 | Saved/Logs/Architecture-PlayerInput.log |
| CameraDifficulty | 镜头恢复、难度与处决结算 | Saved/Logs/Architecture-CameraDifficulty.log |
| RuntimeFlow | 实际死亡、重生、区域与胜利循环 | Saved/Logs/Architecture-RuntimeFlow.log |

新增检查记录敌人范围招实际造成 20 点生命伤害与 10 点韧性伤害，取消下一次招式后没有追加生命伤害。锁定前进、右移脚骨局部位移抽样分别为 50.29、54.60 cm；已查看对应游戏截图。自动化和这些抽样帧不能替代长时间人工 PIE 或逐帧穿模审查；本轮没有网络验证。

HTML 更新了职责表、第一刀到缓存第二刀的完整调用链、具名移动申请计算和敌人范围生命周期。Playwright 检查 15 章、91 类型、169 个本地链接均有效，连招演示、缓存超时、进度保存、类型搜索、390px 页面宽度检查通过，无脚本错误。已查看桌面和手机截图。报告 Saved/ArchitectureBrowserReport.txt，截图 output/playwright/Architecture-Structure-Desktop.png 与 Architecture-Combo-Mobile.png。

复现命令：

```powershell
.\Scripts\BuildUnrealTarget.ps1 -EngineRoot 'E:/Epic Games/UE_5.8'
.\Scripts\ValidateCombatAnimation.ps1 -EngineRoot 'E:/Epic Games/UE_5.8' -LogPrefix Architecture
```

本轮修改前快照在 Saved/ArchitectureRefactorBefore，最终检查清单在 Saved/ArchitectureFinalVerification.json。仅依据本轮日志报告结果，不把历史构建与历史测试当成本轮证明。

## 2026-10-02：Combat 内聚整理（当前结构）

### 本次改动

- 移除上轮新增的攻击动画、范围攻击两个 ActorComponent，Character 不再创建它们；没有增加替代组件或执行器 UObject。
- 攻击起播、Montage 委托、通知身份、前摇朝向和攻击 Warp 集中在 Combat 私有函数，播放字段归入内部 `Playback` 状态。
- 范围计时、查询、过滤与取消代次归入内部 `AreaImpact` 状态。伤害仍复用同一 `ApplyAttackDamage`；`FinishAttackIfReady` 统一处理动画与范围查询结束的汇合。
- 保留供锁定、格挡、弹反、处决共用的 ActionMovement，以及普通 C++ 敌人 Section 状态 MontageComboState。Blueprint 配置入口与收拔刀默认关闭设置保持一致。
- Combat 实现按 01—10 编号组织函数区，增加中文职责、阅读路线与取消顺序说明。HTML、README 和 Architecture.md 同步为当前结构，类型目录移除已删除的组件。

### 本轮重新运行的验证

编辑器目标 `SoulCombatLabEditor Win64 Development` 构建成功；日志：`Saved/BuildLogs/SoulCombatLabEditor-Win64-Development.log`。下面九项均在构建后的新游戏进程中运行，验证 Success 标记和进程退出码 0；未复用上午的 Architecture 日志。

| 用例 | 覆盖内容 | 新日志 |
| --- | --- | --- |
| ActionLifecycle | 移动申请叠加与逆序释放、敌人 Section 后继、真实范围伤害回调中同步取消、范围查询完成仍保留动画取消入口、取消后没有延迟伤害 | Saved/Logs/CombatConsolidation-ActionLifecycle.log |
| RuntimePolish | 格挡、弹反、攻击根位移、目标死亡、剑兵与 Boss 实际攻击 | Saved/Logs/CombatConsolidation-RuntimePolish.log |
| GhostPlayerCombo | 轻重连招、输入缓存、费用边界与失败请求 | Saved/Logs/CombatConsolidation-GhostPlayerCombo.log |
| LockedLocomotion | 八方向局部速度符号、实际 AnimBP、前进/右移脚骨变化、格挡中丢锁恢复 | Saved/Logs/CombatConsolidation-LockedLocomotion.log |
| WeaponSheath | 测试中显式启用保留的收拔刀路径，验证 Notify 与中断恢复 | Saved/Logs/CombatConsolidation-WeaponSheath.log |
| ComboRecovery | 受击后重启连段、旧实例不能清理新招、取消恢复根运动 | Saved/Logs/CombatConsolidation-ComboRecovery.log |
| PlayerInput | Controller 输入与轻重请求 | Saved/Logs/CombatConsolidation-PlayerInput.log |
| CameraDifficulty | 镜头恢复、难度与处决结算 | Saved/Logs/CombatConsolidation-CameraDifficulty.log |
| RuntimeFlow | 实际死亡、重生、区域与胜利循环 | Saved/Logs/CombatConsolidation-RuntimeFlow.log |

RuntimePolish 在启动阶段记录引擎 Concert 模块 `FConcertSessionChallengeData::CreationTimeInTicks` 成员初始化错误，发生在引擎初始化及请求的战斗用例执行前；该战斗用例最终报告 Success，进程退出码为 0。本记录不把整份引擎日志描述为无错误日志，也没有修改引擎插件。

HTML 经 Playwright 验证：15 章、89 类型、169 个本地链接均有效；混合连招、缓存超时、学习进度保存、类型搜索与 390px 页面宽度检查通过，无页面脚本错误。已查看桌面与手机截图：`output/playwright/CombatConsolidation-Structure-Desktop.png`、`output/playwright/CombatConsolidation-Combo-Mobile.png`；报告：`Saved/CombatConsolidationBrowserReport.txt`。

**验证边界：**以上为渲染开启的离屏游戏自动化与 HTML 浏览器检查，没有长时间人工 PIE、逐帧穿模审查或网络验证。脚骨变化断言能排除测试场景中纯胶囊平移，但不能保证所有动画衔接的视觉质量。

复现命令：

```powershell
.\Scripts\BuildUnrealTarget.ps1 -EngineRoot 'E:/Epic Games/UE_5.8'
.\Scripts\ValidateCombatAnimation.ps1 -EngineRoot 'E:/Epic Games/UE_5.8' -LogPrefix CombatConsolidation
```

修改前快照：`Saved/CombatConsolidationBefore`；最终清单与源码哈希：`Saved/CombatConsolidationFinalVerification.json`。

## 2026-10-02：格挡、弹反、处决的技能蓝图配置

### 本次改动

- 在既有 ASC 增加 `FindAbilitySpecByBaseClass`，按继承关系查找已授予技能，优先返回活动记录。格挡松开、弹反窗口通知与处决命中通知改用此入口，技能内部保留 Montage 实例 ID 校验。
- 建立 `Combat/Abilities/GA_PlayerBlock`、`GA_PlayerParry`、`GA_PlayerExecution` 配置蓝图，接入 `BP_SCLPlayerSamurai` 的 StartupAbilities；原生父类没有与蓝图子类重复授予。
- 技能 Class Defaults 可修改 Montage 引用，格挡移动倍率、处决距离等参数；持续格挡姿态与八方向防御步伐仍在 `BS_PlayerBlock8Way` 配置。源码补充中文配置职责与通知说明。
- 没有新增 ActorComponent。现有 C++ 默认动画仍作为默认值；实际技能实例使用玩家选择的蓝图类及其默认值。

### 保存配置与运行验证

编辑器构建成功。创建资产后，在独立 UE Python 进程重新读取保存的玩家蓝图与技能默认值，确认实际授予三种派生类，并导出动画引用与参数到 `Saved/AbilityBlueprintConfiguration.json`。对应日志为 `Saved/Logs/AbilityBlueprint-Create.log`、`Saved/Logs/AbilityBlueprint-Verify.log`。

本轮重新运行以下五项，每项使用新游戏进程，Success 标记与进程退出码 0 均通过：

| 用例 | 覆盖内容 | 日志 |
| --- | --- | --- |
| RuntimePolish | 玩家实际授予蓝图子类且没有原生父类重复技能；格挡松开清状态/恢复速度；弹反通知窗口；剑兵/重兵处决真实命中与结束恢复 | Saved/Logs/AbilityBlueprint-RuntimePolish.log |
| LockedLocomotion | 配置后的格挡与锁定移动叠加、丢锁恢复、八方向局部速度与脚骨变化 | Saved/Logs/AbilityBlueprint-LockedLocomotion.log |
| WeaponSheath | 显式启用保留路径后的武器状态与动作中断衔接；默认仍关闭收拔刀 | Saved/Logs/AbilityBlueprint-WeaponSheath.log |
| CameraDifficulty | 使用标签激活配置后的处决，Boss 当前生命 1/5 的真实通知结算与结束恢复 | Saved/Logs/AbilityBlueprint-CameraDifficulty.log |
| RuntimeFlow | 新玩家配置接入后的死亡、重生、区域与胜利循环 | Saved/Logs/AbilityBlueprint-RuntimeFlow.log |

HTML 第 10 章新增完整配置步骤：Class Defaults 改动画/参数，持续格挡样本位置，复制技能配置及 StartupAbilities 选择，以及换动画的 Slot、骨架与 Notify 要求。Playwright 验证 15 章、89 类型、172 个本地链接、练习交互、学习进度保存和手机页面宽度通过，已查看桌面及手机截图。报告：`Saved/AbilityBlueprintBrowserReport.txt`；截图：`output/playwright/AbilityBlueprint-Configuration-Desktop.png`、`AbilityBlueprint-Configuration-Mobile.png`。

**边界：**本轮是保存资产读取、可渲染离屏游戏自动化和 HTML 浏览器检查，没有长时间人工 PIE 或网络验证。没有测试任意第三方替换动画；替换资源必须满足骨架、Slot、弹反窗口/处决命中通知及 Motion Warping 目标名要求。

复现运行检查：

```powershell
.\Scripts\ValidateCombatAnimation.ps1 -EngineRoot 'E:/Epic Games/UE_5.8' -LogPrefix AbilityBlueprint -Only RuntimePolish,LockedLocomotion,WeaponSheath,CameraDifficulty,RuntimeFlow
.\Scripts\RunUnrealHeadless.ps1 -ProjectPath .\SoulCombatLab.uproject -PythonScript .\Scripts\VerifyPlayerAbilityBlueprints.py -EngineRoot 'E:/Epic Games/UE_5.8'
```

修改前快照：`Saved/AbilityBlueprintBefore`；最终检查与源码/资产哈希：`Saved/AbilityBlueprintFinalVerification.json`。

## 2026-10-03：按指定素材替换玩家动画

- 玩家保存的 Moveset 为五个节点：轻击 0→1→2→3（Attack01_1～4），重击节点 4（JumpAttack04，无后继）。费用分别为 12、13、14、18、20。
- GA_PlayerParry 引用 AM_PlayerParryDeflect；源动画为 LSting_DeflectL_CounterExecution_Root，判定窗口为 0.02～0.47 秒。允许格挡转弹反；后段只演出，不自动追加反击伤害。
- GA_PlayerExecution 引用 AM_PlayerExecutionSPAttack；源为 SPAttack01_Root_Montage 的项目副本，命中通知为 1.024 秒，保留 SCL_Execution 校正窗口。
- 格挡起手／收手为 APose2DefenseR_Root／DefenseR2APose_Root。普通和锁定八向均使用 Apose/Movement/Walk，普通步速为 180 cm/s。
- 独立进程读取已保存的技能蓝图、实际玩家引用、Montage 动画段、通知起点/持续时间及 Blend Space 运行时采样：`Saved/RequestedPlayerAnimationsVerified.json`；入口为 `Scripts/VerifyRequestedPlayerAnimations.py`。
- Editor 构建通过。真实渲染游戏回归 GhostPlayerCombo、PlayerInput、ComboRecovery 通过，日志前缀 RequestedAnimations；RuntimePolish、LockedLocomotion 最终通过，日志前缀 RequestedAnimationsVerified。
- RuntimePolish 验证了格挡转弹反、在旧 0.34 秒窗口结束后的有效判定、新处决伤害、完成后的移动和根运动恢复。测试等待改用世界时间，动画位置检查使用 Montage 时间，墙钟只用于总超时。
- LockedLocomotion 验证八方向速度符号及每个方向的脚骨相位变化；松开格挡并等待收手 Montage 结束后，还验证未锁定的普通走路脚骨运动。抽样帧位于 `Saved/Automation/LockedMove-*.png` 与 `FreeWalk.png`。
- HTML 同步为五招表及四轻一重的演示。Playwright 验证 15 章、89 类型、165 个本地链接、缓存/费用/终止重击、进度保存；无脚本错误或 390px 横向溢出。具体记录为 `Saved/RequestedAnimationCourseVerification.txt`。
- 这是保存配置、自动化游戏运行和抽样帧的证据；没有把浏览器按钮当成 UE 测试，也没有宣称已人工连续游玩评估手感。

## 2026-10-03：成功弹反伤害

- GA_PlayerParry → Class Defaults → Ability|Damage → 成功弹反伤害（CounterDamage），默认 20，设为 0 可关闭伤害。
- 受击端确认成功弹反后，通过实际活动的蓝图派生技能结算反击伤害；每次动作最多结算一次，先设置占用标记再调用目标伤害入口，避免重复命中或同步回调重复扣血。
- 伤害走攻击者 TakeDamage → GAS 生命/死亡处理；击杀后跳过追加受击动画。原有失衡和处决机会继续保留。
- Editor 构建通过，渲染游戏 RuntimePolish 回归通过，覆盖实际 20 点伤害、玩家免伤、多次命中只扣一次、新一轮弹反击杀 10 血目标及后续处决/移动恢复。日志：Saved/Logs/ParryCounter-RuntimePolish.log。
- 已保存配置读取：Saved/ParryCounterConfiguration.json。HTML 第 10 章增加配置位置与调用链，移除旧的“弹反不造成伤害”说明。


## 2026-10-03：八向闪避与无敌窗口

- Editor 完整编译成功，使用正常 DLL 启动新的游戏进程；不以 Live Coding 重建结果代替完整构建。
- ConfigurePlayerDodge.py 创建八个原片项目副本、八个 DefaultSlot Montage 和 GA_PlayerDodge，玩家 StartupAbilities 原位替换原生 Dodge。独立进程重新读取配置通过：Saved/DirectionalDodgeAssets.json、Saved/Logs/Dodge-FinalAssets.log。素材包原件保留。
- 新窗口为 0.10～0.60 秒（持续 0.50 秒）；旧 AM_Dodge 保存窗口起点约 0.1933 秒、持续 0.435 秒。起点提前、持续时间增加；并非整段 1～1.67 秒动作都无敌。
- DirectionalDodge 新游戏进程回归 Success、退出码 0：Saved/Logs/DodgeFinal-DirectionalDodge.log。覆盖八向/旋转后局部方向选取、实际方向 Montage 名、每次扣 25 体力、实际胶囊位移约 450 cm、脚骨姿势变化、不人工旋转 Mesh、闪避中锁定不转身、窗口中真实伤害被拒绝、恢复阶段无敌结束、主动取消后无敌与 Dodging 清理/位移停止/根运动模式恢复/保留锁定、取消后真实伤害重新生效。
- PlayerInput、WeaponSheath 回归 Success、退出码 0：Saved/Logs/DodgeVerified-PlayerInput.log 与 DodgeVerified-WeaponSheath.log。当前默认仍关闭收拔刀。
- 首次回归暴露截图卡顿下的测试抽样问题：Montage 位置更新可以早于本帧 Notify 分发，且加载帧可能跳过 0.03～0.09 秒的窄采样区。启动检查直接在技能启动后验证无敌为 false；恢复检查在越过窗口后的下一帧验证通知已结算。资产窗口起止另有独立校验，未为通过测试修改无敌配置。
- 八方向抽样帧位于 Saved/Automation/Dodge-*.png，已查看前斜向、左右、后退的原片翻滚姿势；这属于离屏渲染自动化和静态抽样检查，不宣称已人工连续 PIE 评估手感。
- HTML 第 10 章新增八向方向换算、动画/距离/体力/无敌时间的配置位置与替换 Montage 的通知要求。15 章生成与 186 个链接的静态检查通过；本次未重新做浏览器视觉回归。

复现：

```powershell
.\Scripts\RunUnrealHeadless.ps1 -ProjectPath .\SoulCombatLab.uproject -PythonScript .\Scripts\VerifyPlayerDodge.py
.\Scripts\ValidateCombatAnimation.ps1 -Only DirectionalDodge,PlayerInput,WeaponSheath -LogPrefix DodgeCheck
```

为完整编译重启 UE 前，保存了用户唯一未保存的 AM_GSN_Attack01_3_Root 修改；原磁盘版本备份在 Saved/Backups/DodgeIntegration-20261003/AM_GSN_Attack01_3_Root-before-save.uasset。

编辑器交互启动沿用项目文件系统 DDC（-DDC=(Local)），避开当前失效的全局 Zen 路径；需要正常用户权限写入 ShaderWorkingDir，沙箱内启动会在 ShaderCompilingThread 失败。此环境问题与上述独立游戏回归结果分别记录。


## 2026-10-03：闪避收尾与弹反恢复

- 修复前真实游戏采样：`Saved/Logs/DodgeBaseline-DirectionalDodge.log`。八个方向在松开按键后，闪避末尾仍给基础移动图输入 270～450 cm/s；LocalForwardSpeed / LocalRightSpeed 非零。原八向回归 Success 不能证明收尾姿势正确。
- 动画实例在 Dodging 和清理后的第一帧改读移动意图，普通移动仍读实际速度；没有改变闪避方向、距离、无敌通知或动画资源。
- 弹反成功默认 0.08 秒恢复，空弹窗口结束后默认 0.18 秒恢复。两个参数位于实际授予的 GA_PlayerParry 类默认值 Ability|Recovery。结束或取消清除恢复定时器。
- ParryRecovery 在实际玩家上检查空弹释放限制、成功后移动输入和攻击接续、旧定时器不会取消下一次攻击；实际命中、反击伤害和死亡仍由 RuntimePolish 检查。
- 开场文字改为“刀术试炼”，成功提示读取已确认成功的弹反结果。中文注释与 HTML 课程已同步。
- 本轮为了完整构建，保存了用户待保存的 AM_PlayerHeavyJump04 和 ABP_SCLPlayerCombat；没有重建动画图。HeavyJump04 原磁盘文件备份在 Saved/Backups/BeforeRecovery-20261003-145840/Content 下。
- 完整编辑器构建 Succeeded。ParryRecovery、DirectionalDodge、RuntimePolish 均 Success / exit 0，日志为 Saved/Logs/RecoveryFix-*.log。修复后八向末尾基础输入均为 (0,0)，强制位移仍保留。
- 按住方向键分支的最终补充回归记录见下方。自动化采样不等于人工连续体验；连续 PIE 收尾观感仍需体验确认。

- 最后增量构建 Succeeded；补齐按住方向键后的 DirectionalDodge 再次 Success / exit 0：Saved/Logs/RecoveryFinal-DirectionalDodge.log。同时检查按住时基础姿势方向与真实意图匹配，松开后八向收尾输入为零。
- HTML 静态校验：15 章；新增恢复与收尾内容和源码链接存在。没有进行本轮浏览器视觉检查。

## 2026-10-03：基础跳跃动画

- 原因：Character::Jump 已能通过 CharacterMovement 离地，但项目 AnimGraph 只有地面八方向/防御混合与战斗 Slot，没有起跳、空中和落地分支。
- 三段素材：Katana/Apose/Movement/GhostSamurai_APose_Jump_Start_Root、Jump_Loop_Root、Jump_End_Root。创建 AS_PlayerJumpStart / Loop / Land 项目副本，保留素材原件；项目副本提取并忽略根位移、锁定首帧根位置，由 CharacterMovement 控制跳跃高度。
- 当前 ABP_SCLPlayerCombat 在 DefaultSlot 下接入 PlayerLocomotion 状态机：Ground → JumpStart → JumpLoop → JumpLand → Ground。直接下落跳过 Start；落地期间再次离地优先处理，移动时在落地至少经过 0.08 秒后可混回步伐。保留原三个地面 Blend Space、布尔混合和事件图。
- 运行时动画类只新增实际 bIsFalling、VerticalSpeed、GroundSpeed 输入；没有新增组件或跳跃技能。状态、动画、Play Rate、Crossfade Duration 在 AnimBP 中可直接编辑。编辑器生成 helper 只用于资源配置，游戏不调用。
- 修改前 AnimBP 备份：Saved/Backups/JumpIntegration-20261003/ABP_SCLPlayerCombat-before-jump.uasset。ConfigurePlayerJump.py 已执行保存；VerifyPlayerJump.py 在独立进程重新读取真实玩家动画类、状态、原片副本引用、循环与根锁定设置、Slot/缓存连接，结果通过：Saved/PlayerJumpVerification.json、Saved/Logs/Jump-Verify.log。
- Editor 完整构建和最终增量构建 Succeeded。JumpAnimation 实际渲染游戏回归 Success / exit 0：Saved/Logs/JumpInitial-JumpAnimation.log。原地跳、移动跳、直接下落均经历空中和落地状态；上升跳有 Start，直接下落没有 Start；胶囊跳高约 127.5 cm，脚骨姿势变化约 47.6 cm，根骨组件空间漂移 0 cm，Mesh 整体变换未被改写，落地后恢复 Ground。
- DirectionalDodge Success / exit 0：Saved/Logs/JumpRegression-DirectionalDodge.log；最终 LockedLocomotion Success / exit 0：Saved/Logs/JumpVerified-LockedLocomotion.log。八向锁定脚骨均变化；普通移动连续采样脚骨变化 100.78 cm，最大速度 437.50 cm/s，胶囊移动 131.96 cm。
- 初次普通移动回归仅比较循环动画首尾，失败；增加连续采样后确认脚骨变化 72.32 cm，但终点速度为 0，不能用终点速度单独证明整段是否移动。最终让自由移动从同一空旷起点开始，并同时验证整段最高实际速度、胶囊位移与组件空间脚骨变化；未为通过测试修改玩法速度或动画。
- 旧 BuildPlayerCombatAnimBP.py 在已有 PlayerLocomotion 时保留图，避免重新生成地面节点破坏缓存与跳跃；此保护分支本轮完成源码/语法检查，未另外执行写入脚本。
- HTML 第 10 章已更新调用链、状态表与实际编辑位置。15 章生成、170 个本地链接和三个 Python 脚本语法检查通过：Saved/JumpCourseVerification.txt。本轮未重新做浏览器视觉检查。
- 验证边界：上述是保存资源与真实渲染游戏自动化。编辑器工具发出了 PIE 请求，但没有创建 PIE 世界，因此未完成连续 PIE 视觉验收；没有把请求成功当作实际游玩证据。

复现：

```powershell
.\Scripts\RunUnrealHeadless.ps1 -ProjectPath .\SoulCombatLab.uproject -PythonScript .\Scripts\VerifyPlayerJump.py
.\Scripts\ValidateCombatAnimation.ps1 -Only JumpAnimation,LockedLocomotion,DirectionalDodge -LogPrefix JumpCheck
```

## 2026-10-03：战斗提示与演示体力配置

- 现有 Demo Widget 增加右下角战斗速查：轻/重攻击、按住格挡、弹反时机、失衡处决、八向闪避、锁定/切目标、跳跃与暂停。暂停/开始菜单同步短按轻击、长按重击的实际规则。
- 情境提示读取 ASC 的实际属性/标签/技能实例：格挡中、弹反动作、成功弹反、敌人出招、失衡机会、处决中和玩家失衡。处决机会仍提醒靠近并面向敌人，不承诺路径/距离预检已通过。体力低于或等于上限 20% 时，条形变橙并提示松开格挡、停止耗体动作后恢复。
- 调整实际玩家引用的配置：轻击 [12,13,14,18] → [6,7,8,10]，整套 57 → 31；重击 20 → 12；闪避 25 → 12；玩家 GuardStaminaDamageMultiplier 1.5 → 0.8。弹反/处决原本没有体力成本；保留当前恢复规则和敌人配置。
- 写入入口 Scripts/ConfigureDemoExperience.py；修改前三份资源备份在 Saved/Backups/DemoExperience-20261003-161318。Saved/DemoExperienceConfiguration.json 记录原值；独立 UE 进程重新读取真实招式引用与实际授予的闪避子类通过：Saved/DemoExperienceVerification.json、Saved/Logs/Experience-Assets.log，exit 0。
- Editor 完整构建 Succeeded。真实渲染 RuntimePolish / DirectionalDodge 均 Success、exit 0：Saved/Logs/Experience-*.log。RuntimePolish 在实际 Widget 检查格挡、低体力、弹反成功、处决中与失衡提示；验证 20 点正面攻击消耗 16 点体力且不掉血。DirectionalDodge 验证八向只支付一次 12 点体力与原有无敌/恢复流程。
- 已查看真实游戏截图 Saved/Screenshots/CombatHints-Execution.png（1280×720）：左右提示卡清晰，中央战斗画面保留。本轮没有宣称已人工连续游玩评估难度，也没有验收所有分辨率布局。
- HTML 课程及交互连招模型同步新耗体，轻后重示例为 100−6−12=82；增加参数配置位置与“界面只读状态”的调用链。15 章生成、173 个本地链接和相关 Python 语法检查通过；本轮未重跑浏览器视觉验收。
- 连招耗体边界、缓存不重复扣费和整套四轻击 31 点消耗的检查未报错，但 GhostPlayerCombo 总体仍 Fail：Saved/Logs/ExperienceFinalCombo-GhostPlayerCombo.log。仅剩“自然轻转重的武器检测未使假人掉血”一项；单独重击实际两段各造成 40 点伤害。完整连招回归不能标为通过。
- 排查过程中，命中测试先恢复相同地面起点、清除前一轮混出，再等待实际 Montage 最后一个 WeaponTrace 窗口结束；自然连招分支明确锁定真实假人，单独重击分支仍未锁定。当前自然连招失败在两种朝向条件下都观察过，不能仅归因于检查太早或没有锁定；后续需单独检查轻转重的位移/刀刃轨迹与检测链路。本轮没有改玩法命中范围、伤害、动画或 Motion Warping 参数。
- 最终增量构建 Succeeded；上述连招限制与已经通过的提示、防御/闪避耗体验收分别记录。

复现：

```powershell
.\Scripts\RunUnrealHeadless.ps1 -ProjectPath .\SoulCombatLab.uproject -PythonScript .\Scripts\VerifyDemoExperience.py
.\Scripts\ValidateCombatAnimation.ps1 -Only RuntimePolish,DirectionalDodge -LogPrefix ExperienceCheck
# 当前仍有一项自然轻转重命中失败：
.\Scripts\ValidateCombatAnimation.ps1 -Only GhostPlayerCombo -LogPrefix ExperienceComboCheck
```


## 2026-10-03：竖劈地面刀痕与命中火花方向

- 来源核对：接触火花由 SCLWeapon.cpp 加载 NS_Slash_Hit_L；重击 AM_PlayerHeavyJump04 在约 0.338 / 1.115 秒播放素材包 NS_Slash_Ground / NS_Slash_Fall。原 NE_Decal 使用世界空间朝向，不能保证刀痕沿玩家竖劈方向。
- 新 AN_SCLGroundSlash 是 Montage 的一次性表现通知，未新增组件。项目 Niagara 副本关闭旧贴花、保留粒子；地面查询排除 Pawn，并拒绝没有地面或陡直墙面的结果。刀痕 X 轴朝地面投影，Y 轴沿角色前方在地面上的投影，生成后留在世界位置。
- 轴向依据：UE 5.8 DeferredDecal.usf 的 UV=(局部 Z, 局部 Y)，导出的 T_FireDecal_Hit_002_M 是纵向长条，所以纹理长轴须沿局部 Y。DirectionYawOffset 用于改变当前竖劈平面；不把这一规则当成所有刀招的自动轨迹识别。
- 命中火花位置继续用 ImpactPoint；朝向使用持刀 Mesh 的水平朝向，替代随胶囊接触点变化的 ImpactNormal.Rotation。素材自身随机散射保留，观感仍待确认。
- 完整 Editor 构建及轴向校正后的增量构建均 Succeeded，记录为 Saved/BuildLogs/GroundSlash-InitialBuild.log 与 SoulCombatLabEditor-Win64-Development.log。中文注释与 HTML 第 8 章补充了来源、调用链和可调参数。
- ConfigureGroundSlashEffects.py 已执行保存。备份在 Saved/Backups/GroundSlash；新的两份 Niagara 位于项目 Combat/Effects 目录。脚本重复运行保留已调整的刀痕大小、方向、材质和停留参数。
- 本轮不运行整套战斗回归。资产读取核对见 Saved/GroundSlashAssetVerification.json。人工 PIE 请检查两个相差 90 度方向的竖劈，以及落刀后移动/转身时刀痕是否留在原地。构建与资产配置通过不是视觉验收通过。
