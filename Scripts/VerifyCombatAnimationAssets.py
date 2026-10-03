"""重新加载项目动画，检查通知、八方向采样和玩家动画图。只读。"""
import unreal

root = "/Game/SoulCombatLab/Characters/Player/Combat/"
configured_montages = []
for ability_name, prop in (("GA_PlayerParry", "parry_montage"), ("GA_PlayerExecution", "execution_montage")):
    ability_bp = unreal.load_asset(root + "Abilities/" + ability_name)
    configured_montages.append(unreal.get_default_object(ability_bp.generated_class()).get_editor_property(prop))
for montage in configured_montages:
    name = montage.get_name() if montage else "未配置 Montage"
    if not montage:
        raise RuntimeError("缺少 " + name)
    entries = []
    for event in unreal.AnimationLibrary.get_animation_notify_events(montage):
        fields = []
        for prop in ("notify_name", "notify", "notify_state_class", "notify_state", "trigger_time_offset", "duration", "link_value"):
            try:
                fields.append(prop + "=" + str(event.get_editor_property(prop)))
            except Exception:
                pass
        entries.append(";".join(fields))
    unreal.log("SCL_VERIFY_NOTIFIES " + name + " count=" + str(len(entries)) + " " + " | ".join(entries))
    # 玩家新选的 Montage 保留素材特效，不能把通知总数固定为 1/2。
    # 精确的判定/命中通知和选择校验见 VerifyRequestedPlayerAnimations.py。
    if not entries:
        raise RuntimeError(name + " 缺少动作通知")

for name in ("BS_PlayerArmed8Way", "BS_PlayerSheathed8Way", "BS_PlayerBlock8Way"):
    blend = unreal.load_asset(root + "Locomotion/" + name)
    if not blend:
        raise RuntimeError("缺少 " + name)
    samples = blend.get_editor_property("sample_data")
    if len(samples) != 9:
        raise RuntimeError(name + " 需要九方向采样")
    if not unreal.SCLAnimationAssetLibrary.has_blend_space_sampling(blend):
        raise RuntimeError(name + " 只有采样列表，没有运行时插值数据；角色会原地滑行")
    detail = []
    for sample in samples:
        animation = sample.get_editor_property("animation")
        position = sample.get_editor_property("sample_value")
        if not animation:
            raise RuntimeError(name + " 存在空采样")
        detail.append(animation.get_name() + "@" + str(position) +
                      " len=" + str(animation.get_play_length()) +
                      " additive=" + str(animation.get_editor_property("additive_anim_type")))
    unreal.log("SCL_VERIFY_BLEND " + name + " count=" + str(len(samples)) + " " + " | ".join(detail))

bp = unreal.load_asset(root + "ABP_SCLPlayerCombat")
if not bp:
    raise RuntimeError("缺少项目 AnimBP")
graph = unreal.BlueprintGraphEditor.get_graph_editor_by_name(bp, "AnimGraph")
unreal.log("SCL_VERIFY_GRAPH nodes=" + str([(node.get_name(), node.get_class().get_name()) for node in graph.list_all_nodes()]))
for node in graph.list_all_nodes():
    unreal.log("SCL_VERIFY_PINS " + node.get_name() + " " + str([(pin.get_pin_name(), [other.get_owning_node().get_name() for other in pin.list_connected_pins()]) for pin in node.list_all_pins()])[:1400])
    if node.get_class().get_name() == "AnimGraphNode_BlendSpacePlayer":
        runtime = node.get_editor_property("node")
        unreal.log("SCL_VERIFY_PLAYER_ASSET " + node.get_name() + " " +
                   str(runtime.get_editor_property("blend_space")))

# UE BlendListByBool: 0 为 True、1 为 False。验证最终分支，防止普通移动误用防御。
nodes = graph.list_all_nodes()
def graph_pin(node, name):
    return next(p for p in node.list_all_pins() if str(p.get_pin_name()) == name)
booleans = {str(graph_pin(n, 'bActiveValue').list_connected_pins()[0].get_pin_name()): n
            for n in nodes if n.get_class().get_name() == 'AnimGraphNode_BlendListByBool'}
def input_source(node, index):
    pins = graph_pin(node, 'BlendPose_' + str(index)).list_connected_pins()
    assert len(pins) == 1
    return pins[0].get_owning_node()
def blend_name(node):
    return node.get_editor_property('node').get_editor_property('blend_space').get_name()
base, guard = booleans['bWeaponSheathed'], booleans['bBlocking']
assert blend_name(input_source(base, 0)) == 'BS_PlayerSheathed8Way'
assert blend_name(input_source(base, 1)) == 'BS_PlayerArmed8Way'
assert blend_name(input_source(guard, 0)) == 'BS_PlayerBlock8Way'
assert input_source(guard, 1) == base
unreal.log('SCL_VERIFY_BRANCHES normal=Run blocking=DefenseR True=0 False=1')
