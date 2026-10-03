"""在 UE Python 中运行：复制资源包的快速拔刀动作，生成项目 Montage 和握刀通知。

Equip01 约 1.37 秒；骨骼采样显示 0.25 秒时握刀点与入鞘点相距约 0.6 cm，
0.5 秒时刀已离开鞘。通知放在 0.30 秒；原动画包保持原样。
"""
import unreal

FOLDER = "/Game/SoulCombatLab/Characters/Player/Combat/Animations"
SOURCE = "/Game/GhostSamurai_Bundle/GhostSamurai/Katana/Common/EquipAndUnarm/GhostSamurai_APose_Equip01_Root"
sequence = unreal.load_asset(FOLDER + "/AS_PlayerDraw")
if not sequence:
    sequence = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, FOLDER + "/AS_PlayerDraw")
if not sequence:
    raise RuntimeError("Missing authored draw animation: " + SOURCE)

# 移动时也允许拔刀：动画姿态播放，根位移由表现组件临时忽略。
unreal.AnimationLibrary.set_root_motion_enabled(sequence, True)
unreal.AnimationLibrary.set_root_motion_lock_type(sequence, unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
sequence.set_editor_property("force_root_lock", True)
unreal.AnimationLibrary.remove_all_animation_notify_tracks(sequence)
unreal.EditorAssetLibrary.save_loaded_asset(sequence)

montage = unreal.load_asset(FOLDER + "/AM_PlayerDraw")
if not montage:
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("target_skeleton", sequence.get_editor_property("skeleton"))
    factory.set_editor_property("source_animation", sequence)
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "AM_PlayerDraw", FOLDER, unreal.AnimMontage, factory)
if not montage:
    raise RuntimeError("Could not create player draw montage")
if montage.get_editor_property("skeleton") != sequence.get_editor_property("skeleton"):
    raise RuntimeError("Draw montage skeleton does not match sequence")
if not unreal.SCLAnimationAssetLibrary.configure_combo_sections(montage, [sequence], ["Draw"]):
    raise RuntimeError("Could not configure draw montage segment")
unreal.AnimationLibrary.remove_all_animation_notify_tracks(montage)
unreal.AnimationLibrary.add_animation_notify_track(montage, "SCL_Draw")
unreal.AnimationLibrary.add_animation_notify_event(
    montage, "SCL_Draw", 0.30, unreal.AN_SCLDrawWeapon.static_class())
if not unreal.EditorAssetLibrary.save_loaded_asset(montage):
    raise RuntimeError("Could not save draw montage")
unreal.log("PLAYER_DRAW_CREATED: AM_PlayerDraw, notify=0.30s, sequence=1.3667s")
