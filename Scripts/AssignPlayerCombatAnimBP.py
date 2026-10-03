"""把实际生成的玩家 Blueprint 默认 Mesh 切到项目动画蓝图。"""
import unreal

player = unreal.load_asset("/Game/SoulCombatLab/Blueprints/Player/BP_SCLPlayerSamurai")
animation = unreal.load_asset("/Game/SoulCombatLab/Characters/Player/Combat/ABP_SCLPlayerCombat")
if not player or not animation:
    raise RuntimeError("缺少玩家或项目动画蓝图")

defaults = unreal.get_default_object(player.generated_class())
mesh = defaults.get_editor_property("mesh")
if not mesh:
    raise RuntimeError("玩家蓝图默认对象没有 Mesh")
target = animation.generated_class()
mesh.set_editor_property("anim_class", target)
mesh.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
unreal.EditorAssetLibrary.save_loaded_asset(player, only_if_is_dirty=False)
unreal.log("SCL_PLAYER_ANIM_ASSIGNED " + str(mesh.get_editor_property("anim_class")))
