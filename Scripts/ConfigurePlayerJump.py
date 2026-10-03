"""UE 编辑器 Python：复制指定跳跃动作，给当前项目 AnimBP 补齐跳跃状态机。

保留原有八方向地面图和战斗 Slot。已有 PlayerLocomotion 时不重建，
避免再次运行脚本覆盖用户在状态、播放速度和转换规则上的调整。
"""
import json
from pathlib import Path
import unreal

SOURCE = '/Game/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Movement'
DEST = '/Game/SoulCombatLab/Characters/Player/Combat'
bp = unreal.load_asset(DEST + '/ABP_SCLPlayerCombat')
assert bp, '项目 AnimBP 不存在；先配置地面八方向移动'
sequences = []
report = {'animations': [], 'graphs': [], 'passed': False}
for source_name, project_name in [('Start', 'Start'), ('Loop', 'Loop'), ('End', 'Land')]:
    source = SOURCE + '/GhostSamurai_APose_Jump_' + source_name + '_Root'
    dest = DEST + '/Animations/AS_PlayerJump' + project_name
    animation = unreal.load_asset(dest)
    if not animation:
        animation = unreal.EditorAssetLibrary.duplicate_asset(source, dest)
        assert animation, '复制失败：' + source
        # 胶囊由 CharacterMovement 跳跃；去掉素材自身的根平移，避免视觉二次腾空。
        animation.set_editor_property('enable_root_motion', True)
        animation.set_editor_property('force_root_lock', True)
        animation.set_editor_property('root_motion_root_lock', unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
        assert unreal.EditorAssetLibrary.save_loaded_asset(animation)
    assert animation.get_editor_property('skeleton') == bp.get_editor_property('target_skeleton'), dest
    sequences.append(animation)
    report['animations'].append({'source': source, 'project': dest, 'length': animation.get_play_length(),
                                 'rootMotion': animation.get_editor_property('enable_root_motion'),
                                 'rootLocked': animation.get_editor_property('force_root_lock')})

assert unreal.SCLAnimationAssetLibrary.configure_player_jump_state_machine(bp, *sequences), '跳跃图生成或编译失败，未保存 AnimBP'
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
for graph in unreal.BlueprintEditorLibrary.list_graphs(bp):
    editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
    assert not editor.list_nodes_with_errors(), '图编译失败：' + graph.get_name()
report['graphs'] = [str(x) for x in unreal.BlueprintEditorLibrary.list_graph_names(bp)]
assert {'PlayerLocomotion', 'Ground', 'JumpStart', 'JumpLoop', 'JumpLand'} <= set(report['graphs'])
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
report['passed'] = True
Path(unreal.Paths.project_saved_dir(), 'PlayerJumpAssets.json').write_text(
    json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('SCL_JUMP_ASSETS_READY ' + json.dumps(report))
