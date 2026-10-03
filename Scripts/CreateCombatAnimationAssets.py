"""UE 编辑器 Python：创建玩家项目副本、三套八方向移动和防御动作。可重复运行。"""
import unreal

ROOT = "/Game/SoulCombatLab/Characters/Player/Combat"
ANIM = ROOT + "/Animations"
MOVE = ROOT + "/Locomotion"
KATANA = "/Game/GhostSamurai_Bundle/GhostSamurai/Katana"
SKELETON = unreal.load_asset("/Game/GhostSamurai_Bundle/Demo/Characters/Mannequins/Meshes/SK_Mannequin")
assert SKELETON


def asset(path):
    result = unreal.load_asset(path)
    if not result:
        raise RuntimeError("缺少动画：" + path)
    if result.get_editor_property("skeleton") != SKELETON:
        raise RuntimeError("骨架不匹配：" + path)
    return result


def copy_sequence(source, name, root_motion=False):
    dest = ANIM + "/" + name
    sequence = unreal.load_asset(dest)
    if not sequence:
        sequence = unreal.EditorAssetLibrary.duplicate_asset(source, dest)
    if not sequence:
        raise RuntimeError("复制失败：" + source)
    asset(dest)
    unreal.AnimationLibrary.set_root_motion_enabled(sequence, root_motion)
    unreal.AnimationLibrary.set_root_motion_lock_type(sequence, unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
    sequence.set_editor_property("force_root_lock", root_motion)
    unreal.EditorAssetLibrary.save_loaded_asset(sequence)
    return sequence


def montage(name, sequence):
    path = ANIM + "/" + name
    result = unreal.load_asset(path)
    if not result:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("target_skeleton", SKELETON)
        factory.set_editor_property("source_animation", sequence)
        result = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ANIM, unreal.AnimMontage, factory)
    if not result or not unreal.SCLAnimationAssetLibrary.configure_combo_sections(result, [sequence], [name]):
        raise RuntimeError("Montage 创建失败：" + name)
    return result


def blend_space(name, idle, directions):
    path = MOVE + "/" + name
    result = unreal.load_asset(path)
    if not result:
        factory = unreal.BlendSpaceFactoryNew()
        factory.set_editor_property("target_skeleton", SKELETON)
        result = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MOVE, unreal.BlendSpace, factory)
    if not result:
        raise RuntimeError("Blend Space 创建失败：" + name)
    parameters = list(result.get_editor_property("blend_parameters"))
    for index, label in ((0, "Right"), (1, "Forward")):
        parameter = unreal.BlendParameter()
        parameter.set_editor_property("display_name", label)
        parameter.set_editor_property("min", -1.0)
        parameter.set_editor_property("max", 1.0)
        parameter.set_editor_property("grid_num", 2)
        parameters[index] = parameter
    result.set_editor_property("blend_parameters", parameters)
    def sample(path, x, y):
        value = unreal.BlendSample()
        value.set_editor_property("animation", asset(path))
        value.set_editor_property("sample_value", unreal.Vector(x, y, 0))
        return value
    samples = [sample(idle, 0, 0)]
    for key, (x, y) in {"F": (0, 1), "FR": (1, 1), "R": (1, 0), "BR": (1, -1),
                         "B": (0, -1), "BL": (-1, -1), "L": (-1, 0), "FL": (-1, 1)}.items():
        samples.append(sample(directions[key], x, y))
    result.set_editor_property("sample_data", samples)
    # 直接设置 SampleData 不会保证运行时三角剖分已更新；否则玩家胶囊移动，Mesh 仍保持静止姿势。
    if not unreal.SCLAnimationAssetLibrary.rebuild_blend_space_sampling(result):
        raise RuntimeError("Blend Space 运行时采样数据为空：" + path)
    unreal.EditorAssetLibrary.save_loaded_asset(result)
    unreal.log("SCL_CREATED " + path + " samples=" + str(len(samples)))


def paths(folder, names):
    return {key: folder + "/" + stem for key, stem in names.items()}


defense_dir = KATANA + "/Apose/Defense/DefenseR"
defense = paths(defense_dir, {
    "F": "GhostSamurai_DefenseR_Walk_F_Root", "FR": "GhostSamurai_DefenseR_Walk_FR_Root",
    "R": "GhostSamurai_DefenseR_Walk_R_Root", "BR": "GhostSamurai_DefenseR_Walk_BR_Root",
    "B": "GhostSamurai_DefenseR_Walk_B_Root", "BL": "GhostSamurai_DefenseR_Walk_BL_Root",
    "L": "GhostSamurai_DefenseR_Walk_L_Root", "FL": "GhostSamurai_DefenseR_Walk_FL_Root"})

# 先创建普通移动资产，再由统一配置脚本填入当前选择，避免多个脚本互相覆盖。
run_paths = {key: KATANA + '/Apose/Movement/Run/GhostSamurai_APose_Strafe_Run_' + key + ('_Loop' if key == 'F' else '') + '_Root'
             for key in ['F','FR','R','BR','B','BL','L','FL']}
for name in ['BS_PlayerArmed8Way','BS_PlayerSheathed8Way']:
    blend_space(name, KATANA + '/Apose/GhostSamurai_APose_Idle', run_paths)
import runpy
from pathlib import Path
runpy.run_path(str(Path(unreal.Paths.project_dir()) / 'Scripts/ConfigurePlayerLocomotion.py'))
blend_space("BS_PlayerBlock8Way", defense_dir + "/GhostSamurai_DefenseR_Pose", defense)

block_start = copy_sequence(KATANA + "/Apose/Defense/GhostSamurai_APose2DefenseR_Root", "AS_PlayerBlockStart")
block_end = copy_sequence(KATANA + "/Apose/Defense/GhostSamurai_DefenseR2APose_Root", "AS_PlayerBlockEnd")
parry = copy_sequence(defense_dir + "/GhostSamurai_DefenseR_Parry01_Root", "AS_PlayerParry")
execution = copy_sequence(KATANA + "/Apose/Execution/GhostSamurai_Execution01", "AS_PlayerExecution", True)
for name, sequence in (("AM_PlayerBlockStart", block_start), ("AM_PlayerBlockEnd", block_end),
                       ("AM_PlayerParry", parry), ("AM_PlayerExecution", execution)):
    m = montage(name, sequence)
    # 重跑资产脚本时保留已经手工/脚本调好的弹反、处决通知窗口。
    if "SCL_Combat" not in [str(track) for track in unreal.AnimationLibrary.get_animation_notify_track_names(m)]:
        unreal.AnimationLibrary.add_animation_notify_track(m, "SCL_Combat")
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    unreal.log("SCL_CREATED " + m.get_path_name() + " length=" + str(m.get_play_length()))
