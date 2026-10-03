"""UE 编辑器 Python：把判定窗口与有限位移校正标在项目 Montage 时间轴。"""
import unreal

ROOT = "/Game/SoulCombatLab/Characters/Player/Combat/Animations/"


def events(montage):
    return unreal.AnimationLibrary.get_animation_notify_events(montage)


def add_state_once(montage, marker, start, duration, state_class):
    tracks = [str(name) for name in unreal.AnimationLibrary.get_animation_notify_track_names(montage)]
    if "SCL_Combat" not in tracks:
        unreal.AnimationLibrary.add_animation_notify_track(montage, "SCL_Combat")
    def find():
        for event in events(montage):
            for prop in ("notify_state_class", "notify_state", "notify_name"):
                try:
                    value = event.get_editor_property(prop)
                    if marker in str(value):
                        return event, value
                except Exception:
                    pass
        return None
    existing = find()
    if not existing:
        result = unreal.AnimationLibrary.add_animation_notify_state_event(
            montage, "SCL_Combat", start, duration, state_class)
        unreal.log("SCL_NOTIFY_ADD " + montage.get_name() + " " + marker + " result=" + str(result))
    else:
        return existing
    found = find()
    if found:
        return found
    raise RuntimeError("找不到通知状态 " + montage.get_name() + ": " + marker)


def warp(montage, target, start, end):
    _, state = add_state_once(montage, "MotionWarping", start, end - start,
                              unreal.AnimNotifyState_MotionWarping.static_class())
    modifier = unreal.new_object(unreal.RootMotionModifier_SkewWarp, outer=state)
    modifier.set_editor_property("warp_target_name", target)
    modifier.set_editor_property("warp_translation", True)
    modifier.set_editor_property("ignore_z_axis", True)
    modifier.set_editor_property("warp_rotation", True)
    state.set_editor_property("root_motion_modifier", modifier)
    unreal.EditorAssetLibrary.save_loaded_asset(montage)
    unreal.log("SCL_WARP " + montage.get_path_name() + " target=" + target +
               " window=" + str((start, end)))


ability_root = "/Game/SoulCombatLab/Characters/Player/Combat/Abilities/"
# 从当前配置蓝图读取；用户替换 Montage 后，不能继续往旧资产写通知。
parry = unreal.get_default_object(unreal.load_asset(ability_root + "GA_PlayerParry").generated_class()).get_editor_property("parry_montage")
execution = unreal.get_default_object(unreal.load_asset(ability_root + "GA_PlayerExecution").generated_class()).get_editor_property("execution_montage")
if not parry or not execution:
    raise RuntimeError("先运行 CreateCombatAnimationAssets.py")
add_state_once(parry, "ANS_SCLParryWindow", 0.06, 0.28, unreal.ANS_SCLParryWindow.static_class())
unreal.EditorAssetLibrary.save_loaded_asset(parry, only_if_is_dirty=False)
def has_impact():
    for event in events(execution):
        try:
            if "AN_SCLExecutionImpact" in str(event.get_editor_property("notify")):
                return True
        except Exception:
            pass
    return False
if not has_impact():
    unreal.AnimationLibrary.add_animation_notify_event(
        execution, "SCL_Combat", 1.16, unreal.AN_SCLExecutionImpact.static_class())
warp(execution, "SCL_Execution", 0.08, 1.10)

moveset = unreal.load_asset("/Game/SoulCombatLab/Characters/Player/Combat/Data/DA_GhostSamurai_PlayerMoveset")
if not moveset:
    raise RuntimeError("缺少玩家连招表")
seen = set()
for step in moveset.get_editor_property("steps"):
    montage = step.get_editor_property("montage")
    if not montage or montage.get_path_name() in seen:
        continue
    seen.add(montage.get_path_name())
    # 仅项目资产允许写入；素材包原件绝不写通知。
    if not montage.get_path_name().startswith("/Game/SoulCombatLab/"):
        raise RuntimeError("Moveset 指向不可修改的素材包 Montage：" + montage.get_path_name())
    length = montage.get_play_length()
    warp(montage, "SCL_Attack", 0.04, min(length * 0.38, 0.55))
unreal.log("SCL_COMBAT_NOTIFIES_CONFIGURED moves=" + str(len(seen)))
