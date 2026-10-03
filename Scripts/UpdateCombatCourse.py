"""把本轮战斗动画的学习路径写进现有课程，再交给 BuildLearningCourse.py 渲染。"""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "Docs/CourseLessons.json"
course = json.loads(path.read_text(encoding="utf-8"))
lessons = {lesson["id"]: lesson for lesson in course["lessons"]}

def add(lesson_id, marker, html):
    lesson = lessons[lesson_id]
    content = lesson["html"]
    start, end = f"<!-- {marker}_START -->", f"<!-- {marker}_END -->"
    block = start + html + end
    if start in content:
        a, b = content.index(start), content.index(end) + len(end)
        content = content[:a] + block + content[b:]
    else:
        point = content.index('<label class="lesson-done">')
        content = content[:point] + block + content[point:]
    lesson["html"] = content

defense = lessons["defense"]["html"]
defense = defense.replace("对齐、播放、延迟命中，再恢复移动和根运动设置。", "检查目标与路径、播放项目处决 Montage，在刀刃通知处结算伤害，结束后恢复移动与根运动。")
defense = defense.replace("对齐使用现有移动/位置调整逻辑，这条实现没有使用 Motion Warping。", "处决对位使用有限的 Motion Warping；目标过远或路径被挡时拒绝启动，不再瞬间传送。敌人目前仍使用原有失衡与伤害流程，没有配对受处决动画。")
lessons["defense"]["html"] = defense

# 当前版本先保持持刀；把旧的“自动收刀已启用”课程段落改成当前实际行为。
montage = lessons["montage"]["html"]
old_start = '<h3>停止战斗后的收刀与下一次拔刀</h3>'
locked_start = '<!-- LOCKED_LOCOMOTION_START -->'
if old_start in montage and locked_start in montage:
    current = '''<h3>当前武器表现：保持持刀</h3><p>现在默认暂停自动收刀和攻击前拔刀。角色移动、闲置后刀都留在手上；轻击和重击直接进入原有 GAS → Combat（向 Combo 选招后起播） 流程，不等待拔刀 Montage。动画与通知资源仍保留在项目中，方便以后恢复。</p><p><b>以后想重新启用：</b>打开 <code>BP_SCLPlayerSamurai</code> 的 WeaponPresentationComponent，在 Weapon|Sheath 中勾选“启用收刀与拔刀动作”。该组件会重新加载两段 Montage，并在无战斗动作后计时收刀；重新启用后要复验移动、通知换挂点和攻击输入时机。</p><p class="source">源码：<a href="../Source/SoulCombatLab/Public/Characters/Player/SCLWeaponPresentationComponent.h">启用开关</a> · <a href="../Source/SoulCombatLab/Private/Characters/Player/SCLWeaponPresentationComponent.cpp">收刀与拔刀流程</a></p>'''
    montage = montage[:montage.index(old_start)] + current + montage[montage.index(locked_start):]
    lessons["montage"]["html"] = montage
add("defense", "COMBAT_ANIMATION", """
<h3>防御和处决的实际调用链</h3>
<div class="flow"><span>右键 / Q / E</span><i>→</i><span>Controller 转发请求</span><i>→</i><span>GAS 技能验证条件</span><i>→</i><span>项目 Montage 与 Notify</span><i>→</i><span>状态或伤害结算</span></div>
<p><b>格挡：</b>按住右键后，Block Ability 授予 State.Blocking，播放起手 Montage；AnimBP 根据此标签改用 BS_PlayerBlock8Way，松开时播放结束 Montage。移动速度倍率在 <a href="../Source/SoulCombatLab/Public/AbilitySystem/Abilities/SCLBlockAbility.h">SCLBlockAbility.h</a> 的 BlockingSpeedMultiplier。体力不足时技能不启动，武器状态也不被这次无效请求改变。</p>
<p><b>弹反：</b>Q 启动 AM_PlayerParry；<code>ANS_SCLParryWindow</code> 的时间区间才授予 State.Parrying。要调成功时机，打开该 Montage 移动通知的起止边界；修改技能中的旧计时常量不会改变现在的判定。动作期间 State.ParryAction 阻止普通移动和新攻击，处决仍可接管有效目标。</p>
<p><b>处决：</b>E 先找可处决目标并检查距离、朝向和路径，再播放 AM_PlayerExecution。<code>AN_SCLExecutionImpact</code> 到达时才计算普通敌人或 Boss 的伤害。Montage 的 Motion Warping 窗口叫 SCL_Execution，Ability 只在合法位置设置这个目标点；无目标、墙后目标都不会瞬移。</p>
<p class="source">源码：<a href="../Source/SoulCombatLab/Private/AbilitySystem/Abilities/SCLBlockAbility.cpp">格挡技能</a> · <a href="../Source/SoulCombatLab/Private/AbilitySystem/Abilities/SCLParryAbility.cpp">弹反技能</a> · <a href="../Source/SoulCombatLab/Private/AbilitySystem/Abilities/SCLExecutionAbility.cpp">处决技能</a> · <a href="../Scripts/ConfigureCombatNotifies.py">通知时间轴</a></p>
""")
add("montage", "LOCKED_LOCOMOTION", """
<h3>锁定时为什么不能只让角色转身</h3>
<p>锁定组件让角色朝向敌人；按 S 时世界速度指向后方。动画蓝图若只按“有速度就播向前走”，脚会像向前迈步却向后滑。<code>USCLPlayerAnimInstance</code> 把世界速度投影到角色前、右两个方向，输出 -1～1 的 LocalForwardSpeed / LocalRightSpeed；八方向 Blend Space 用这两个值选前后左右及斜向步伐。</p>
<p>项目 AnimBP <code>ABP_SCLPlayerCombat</code> 有收刀、持刀、格挡三套 Blend Space；当前收刀流程关闭，普通移动选持刀步伐，格挡选防御步伐。最后进入 DefaultSlot，让攻击和弹反 Montage 覆盖基础移动。素材包原 AnimBP 不修改。</p>
<p><b>这次滑步的根因：</b>脚本把九个动画写进 <code>SampleData</code> 后，Blend Space 的运行时插值数据仍为空；因此“资源里看得到样本、动画图也连上了”并不代表骨骼真的会动。<a href="../Scripts/CreateCombatAnimationAssets.py">资产生成脚本</a>现在调用 <code>RebuildBlendSpaceSampling</code> 重建并检查插值数据。移动回归还测量 <code>foot_l</code> 相对 Mesh 的位置变化：前进和右移都必须大于 2 cm，避免只测到胶囊平移就误报通过。</p>
<p><b>自己修改：</b>在 <code>/Game/SoulCombatLab/Characters/Player/Combat/Locomotion/</code> 调八方向样本；在项目 AnimBP 调切换与混合；在 <a href="../Source/SoulCombatLab/Private/Animation/SCLPlayerAnimInstance.cpp">SCLPlayerAnimInstance.cpp</a> 看方向怎样计算。<code>BP_SCLPlayerSamurai</code> 的 Mesh → Anim Class 必须指向项目 AnimBP，可用 <a href="../Scripts/AssignPlayerCombatAnimBP.py">资源配置脚本</a> 核对。锁定移动速度在 TargetingComponent 的 <code>LockedWalkSpeed</code> 调；默认约 180 cm/s 是按素材一步周期约 190 cm / 1.13 s 选的，调快后要重新看脚底是否滑。攻击贴近上限在 CombatComponent，处决停刀距离在 ExecutionAbility；二者分别控制，不要把锁定相机距离当作攻击范围。</p>
<p><b>验证边界：</b><a href="../Scripts/ValidateCombatAnimation.ps1">可渲染回归脚本</a>检查八方向局部速度、锁定状态与抽样帧，也检查防御、处决和武器流程；脚底是否踩稳仍要在 PIE 连续观察前、后、左、右与斜向移动，单帧截图无法证明。</p>
""")

path.write_text(json.dumps(course, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
