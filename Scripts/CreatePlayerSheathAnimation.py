"""在 UE Python 中运行：生成玩家收刀资源，原始动画包保持原样。

选择 Unarm_1（2.7 秒），1.5 秒时刀的手部挂点已与入鞘挂点对齐。
真正换挂点由原生 Notify 执行，C++ 不用固定秒数猜测动画进度。
重复运行会重建本脚本管理的通知轨道，不会叠加通知。
"""
import unreal

FOLDER = "/Game/SoulCombatLab/Characters/Player/Combat/Animations"
SOURCE = "/Game/GhostSamurai_Bundle/GhostSamurai/Katana/Common/EquipAndUnarm/GhostSamurai_APose_Unarm_1_Root"
sequence = unreal.load_asset(FOLDER + "/AS_PlayerSheath")
if not sequence:
    sequence = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, FOLDER + "/AS_PlayerSheath")
if not sequence:
    raise RuntimeError("Missing authored sheath animation: " + SOURCE)

# 站立表现提取但丢弃 Root Motion，避免收刀时胶囊或网格漂移。
unreal.AnimationLibrary.set_root_motion_enabled(sequence, True)
unreal.AnimationLibrary.set_root_motion_lock_type(sequence, unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
sequence.set_editor_property("force_root_lock", True)
unreal.AnimationLibrary.remove_all_animation_notify_tracks(sequence)
unreal.EditorAssetLibrary.save_loaded_asset(sequence)

montage = unreal.load_asset(FOLDER + "/AM_PlayerSheath")
if not montage:
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("target_skeleton", sequence.get_editor_property("skeleton"))
    factory.set_editor_property("source_animation", sequence)
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "AM_PlayerSheath", FOLDER, unreal.AnimMontage, factory)
if not montage:
    raise RuntimeError("Could not create player sheath montage")
if montage.get_editor_property("skeleton") != sequence.get_editor_property("skeleton"):
    raise RuntimeError("Sheath montage skeleton does not match sequence")
if not unreal.SCLAnimationAssetLibrary.configure_combo_sections(montage, [sequence], ["Sheath"]):
    raise RuntimeError("Could not configure sheath montage segment")
unreal.AnimationLibrary.remove_all_animation_notify_tracks(montage)
unreal.AnimationLibrary.add_animation_notify_track(montage, "SCL_Sheath")
unreal.AnimationLibrary.add_animation_notify_event(
    montage, "SCL_Sheath", 1.5, unreal.AN_SCLSheathWeapon.static_class())
if not unreal.EditorAssetLibrary.save_loaded_asset(montage):
    raise RuntimeError("Could not save sheath montage")
unreal.log("PLAYER_SHEATH_CREATED: AM_PlayerSheath, notify=1.5s, sequence=2.7s")
