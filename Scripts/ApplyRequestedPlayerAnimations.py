"""UE Python：应用 2026-10-03 的玩家动画选择，只写项目资产。

四段轻击 + 单段重击的连接在 Moveset；防御和处决引用在技能蓝图。
通知决定判定/命中时刻，移动 Blend Space 需要重建采样才能在运行时迈步。
重跑会重新应用这里的配置；手工调参后不要把本脚本当只读检查运行。
"""
import unreal

ROOT = '/Game/SoulCombatLab/Characters/Player/Combat'
ANIM = ROOT + '/Animations'
KATANA = '/Game/GhostSamurai_Bundle/GhostSamurai/Katana'
SKELETON = unreal.load_asset('/Game/GhostSamurai_Bundle/Demo/Characters/Mannequins/Meshes/SK_Mannequin')
LIB = unreal.AnimationLibrary


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def load(path):
    return require(unreal.load_asset(path), '缺少资产：' + path)


def save(obj):
    require(unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False), '保存失败：' + obj.get_path_name())


def duplicate(source, name):
    path = ANIM + '/' + name
    obj = unreal.load_asset(path) or unreal.EditorAssetLibrary.duplicate_asset(source, path)
    require(obj, '复制失败：' + source)
    require(obj.get_editor_property('skeleton') == SKELETON, '骨架不兼容：' + path)
    return obj


def sequence(source, name, root_motion):
    result = duplicate(source, name)
    LIB.set_root_motion_enabled(result, root_motion)
    LIB.set_root_motion_lock_type(result, unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
    result.set_editor_property('force_root_lock', True)
    save(result)
    return result


def make_montage(name, seq, source_montage=None):
    path = ANIM + '/' + name
    if source_montage:
        obj = duplicate(source_montage, name)
    else:
        obj = unreal.load_asset(path)
        if not obj:
            factory = unreal.AnimMontageFactory()
            factory.set_editor_property('target_skeleton', SKELETON)
            factory.set_editor_property('source_animation', seq)
            obj = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ANIM, unreal.AnimMontage, factory)
    require(unreal.SCLAnimationAssetLibrary.configure_combo_sections(obj, [seq], [name]), 'Montage 配置失败：' + name)
    # 本脚本拥有这一条项目通知轨；保留素材包的特效轨，不重复叠加判定通知。
    tracks = [str(x) for x in LIB.get_animation_notify_track_names(obj)]
    if 'SCL_SelectedActions' in tracks:
        LIB.remove_animation_notify_events_by_track(obj, 'SCL_SelectedActions')
    else:
        LIB.add_animation_notify_track(obj, 'SCL_SelectedActions')
    return obj


def state(montage, start, end, cls):
    require(0 <= start < end < montage.get_play_length(), '通知区间越界：' + montage.get_name())
    return require(LIB.add_animation_notify_state_event(montage, 'SCL_SelectedActions', start, end-start, cls.static_class()), '添加通知失败')


def warp(montage, target, start, end):
    notify = state(montage, start, end, unreal.AnimNotifyState_MotionWarping)
    modifier = unreal.new_object(unreal.RootMotionModifier_SkewWarp, outer=notify)
    modifier.set_editor_property('warp_target_name', target)
    modifier.set_editor_property('warp_translation', True)
    modifier.set_editor_property('ignore_z_axis', True)
    modifier.set_editor_property('warp_rotation', True)
    notify.set_editor_property('root_motion_modifier', modifier)


# 防御转换已是指定素材，明确保留项目副本与原地格挡策略。
for name, stem in [('BlockStart','GhostSamurai_APose2DefenseR_Root'), ('BlockEnd','GhostSamurai_DefenseR2APose_Root')]:
    seq = sequence(KATANA+'/Apose/Defense/'+stem, 'AS_Player'+name, False)
    save(make_montage('AM_Player'+name, seq))

# 用户选择的是完整弹反演出；只有前段 0.02～0.47 秒有弹反判定。
# 成功弹反伤害由 GA_PlayerParry.CounterDamage 配置；后段动画不重复结算，也不自动处决。
parry_seq = sequence(KATANA+'/Apose/Deflect/Root/GhostSamurai_LSting_DeflectL_CounterExecution_Root', 'AS_PlayerParryDeflect', False)
parry = make_montage('AM_PlayerParryDeflect', parry_seq)
state(parry, 0.02, 0.47, unreal.ANS_SCLParryWindow)
save(parry)

# 沿用原 SPAttack 的特效时间：约 1.024 秒斩击时只结算一次处决。
execution_seq = sequence(KATANA+'/Apose/Attack/GhostSamurai_APose_SPAttack01_Root', 'AS_PlayerExecutionSPAttack', True)
execution = make_montage('AM_PlayerExecutionSPAttack', execution_seq, KATANA+'/Apose/Attack/GhostSamurai_APose_SPAttack01_Root_Montage')
LIB.add_animation_notify_event(execution, 'SCL_SelectedActions', 1.024, unreal.AN_SCLExecutionImpact.static_class())
warp(execution, 'SCL_Execution', 0.08, 0.98)
save(execution)

# JumpAttack04 保留原特效；两个挥刀区间均用现有武器轨迹链路，重击没有接招窗口。
heavy_seq = sequence(KATANA+'/Apose/Attack/GhostSamurai_APose_JumpAttack04_Root', 'AS_PlayerHeavyJump04', True)
heavy = make_montage('AM_PlayerHeavyJump04', heavy_seq, KATANA+'/Apose/Attack/GhostSamurai_APose_JumpAttack04_Root_Montage')
state(heavy, 0.258, 0.558, unreal.ANS_SCLWeaponTrace)
state(heavy, 1.035, 1.345, unreal.ANS_SCLWeaponTrace)
warp(heavy, 'SCL_Attack', 0.04, 0.25)
save(heavy)

# 竖劈刀痕按项目 Notify 对齐地面；重复生成重击时保留玩家调整过的表现参数。
import sys
from pathlib import Path
scripts_dir = str(Path(unreal.Paths.project_dir()) / 'Scripts')
if scripts_dir not in sys.path:
    sys.path.insert(0, scripts_dir)
import ConfigureGroundSlashEffects
ConfigureGroundSlashEffects.configure()

moveset = load(ROOT+'/Data/DA_GhostSamurai_PlayerMoveset')
old = list(moveset.get_editor_property('steps'))
light_steps = [s for s in old if str(s.get_editor_property('attack_name')) in ['Attack01_1','Attack01_2','Attack01_3','Attack01_4']]
require(len(light_steps)==4, '缺少 Attack01 四段，先运行 CreateNativeGhostCombos.py')
steps=[]
for index, existing in enumerate(light_steps):
    s = unreal.SCLPlayerAttackStep()
    s.set_editor_property('attack_name', 'Attack01_'+str(index+1))
    s.set_editor_property('input_type', unreal.SCLPlayerAttackInput.LIGHT)
    s.set_editor_property('montage', existing.get_editor_property('montage'))
    s.set_editor_property('stamina_cost', [6,7,8,10][index])
    s.set_editor_property('damage_multiplier', [1.0,1.05,1.1,1.25][index])
    s.set_editor_property('poise_damage_multiplier', [1.0,1.0,1.1,1.3][index])
    s.set_editor_property('next_light_step_index', index+1 if index<3 else -1)
    s.set_editor_property('next_heavy_step_index', 4)
    steps.append(s)
s = unreal.SCLPlayerAttackStep()
for key, value in {'attack_name':'JumpAttack04','input_type':unreal.SCLPlayerAttackInput.HEAVY,'montage':heavy,'stamina_cost':12.0,'damage_multiplier':2.0,'poise_damage_multiplier':2.5,'next_light_step_index':-1,'next_heavy_step_index':-1}.items():
    s.set_editor_property(key,value)
steps.append(s)
moveset.set_editor_property('steps', steps)
moveset.set_editor_property('light_opener_index', 0)
moveset.set_editor_property('heavy_opener_index', 4)
save(moveset)

# Class Defaults 仍是日后自由替换引用的位置；不把新选择写进输入/Character 逻辑。
for name, fields in [('GA_PlayerParry', {'parry_montage':parry}), ('GA_PlayerExecution', {'execution_montage':execution}), ('GA_PlayerBlock', {'block_start_montage':load(ANIM+'/AM_PlayerBlockStart'),'block_end_montage':None})]:
    bp=load(ROOT+'/Abilities/'+name)
    defaults=unreal.get_default_object(bp.generated_class())
    for key,value in fields.items():
        defaults.set_editor_property(key,value)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(bp)

# 移动配置独立维护；重跑全套动画时也沿用当前 Run 八向，不回退到 Walk。
import runpy
from pathlib import Path
runpy.run_path(str(Path(unreal.Paths.project_dir()) / 'Scripts/ConfigurePlayerLocomotion.py'))
unreal.log('SCL_REQUESTED_PLAYER_ANIMATIONS_APPLIED light=0,1,2,3 heavy=4 parry=0.02..0.47 execution=1.024 run=437.5')
