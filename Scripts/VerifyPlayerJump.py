"""独立 UE 进程只读验证：重新读取保存的跳跃状态机、动作引用和真实玩家默认配置。"""
import json
from pathlib import Path
import unreal

ROOT = '/Game/SoulCombatLab/Characters/Player/Combat'
bp = unreal.load_asset(ROOT + '/ABP_SCLPlayerCombat')
assert bp
graphs = {g.get_name(): g for g in unreal.BlueprintEditorLibrary.list_graphs(bp)}
assert {'PlayerLocomotion', 'Ground', 'JumpStart', 'JumpLoop', 'JumpLand'} <= set(graphs)
report = {'states': {}, 'passed': False}
for phase, loop, rate in [('Start', False, 1.8), ('Loop', True, 1.0), ('Land', False, 1.6)]:
    editor = unreal.BlueprintGraphEditor.get_graph_editor(graphs['Jump' + phase])
    players = [n for n in editor.list_all_nodes() if n.get_class().get_name() == 'AnimGraphNode_SequencePlayer']
    assert len(players) == 1
    node = players[0].get_editor_property('node')
    asset = node.get_editor_property('sequence')
    assert asset.get_path_name().startswith(ROOT + '/Animations/AS_PlayerJump' + phase + '.')
    assert node.get_editor_property('loop_animation') == loop
    assert abs(node.get_editor_property('play_rate') - rate) < 0.001
    assert asset.get_editor_property('enable_root_motion') and asset.get_editor_property('force_root_lock')
    assert asset.get_editor_property('root_motion_root_lock') == unreal.RootMotionRootLock.ANIM_FIRST_FRAME
    report['states'][phase] = {'asset': asset.get_path_name(), 'rate': rate, 'loop': loop}

main = unreal.BlueprintGraphEditor.get_graph_editor(graphs['AnimGraph'])
nodes = main.list_all_nodes()
assert sum(n.get_class().get_name() == 'AnimGraphNode_BlendSpacePlayer' for n in nodes) == 3
slot = next(n for n in nodes if n.get_class().get_name() == 'AnimGraphNode_Slot')
links = slot.find_input_pin('Source').list_connected_pins()
assert len(links) == 1 and links[0].get_owning_node().get_class().get_name() == 'AnimGraphNode_StateMachine'
cache = next(n for n in nodes if n.get_class().get_name() == 'AnimGraphNode_SaveCachedPose')
assert cache.get_editor_property('cache_name') == 'PlayerGroundPose'
assert len(cache.find_input_pin('Pose').list_connected_pins()) == 1

player = unreal.load_asset('/Game/SoulCombatLab/Blueprints/Player/BP_SCLPlayerSamurai')
defaults = unreal.get_default_object(player.generated_class())
assert defaults.get_editor_property('mesh').get_editor_property('anim_class') == bp.generated_class()
report['pawn'] = player.get_path_name()
report['animClass'] = bp.generated_class().get_path_name()
report['graphs'] = list(graphs)
report['passed'] = True
Path(unreal.Paths.project_saved_dir(), 'PlayerJumpVerification.json').write_text(
    json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('SCL_JUMP_VERIFIED ' + json.dumps(report))
