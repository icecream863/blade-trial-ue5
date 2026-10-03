"""关闭交互编辑器后用 RunUnrealHeadless.ps1 执行：重编译玩家 AnimBP 的原生字段绑定。

保留现有地面混合、跳跃状态机、Slot 和用户动作引用；不重建动画图。
原生 AnimInstance 增减 UPROPERTY 后，不能仅以旧资源能加载/独立游戏能跑作为 PIE 验收。
"""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

PACKAGE = '/Game/SoulCombatLab/Characters/Player/Combat/ABP_SCLPlayerCombat'
saved = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))
content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()))
source = content / (PACKAGE.removeprefix('/Game/') + '.uasset')
backup = saved / 'Backups' / ('PlayerAnimRecompile-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
backup.mkdir(parents=True, exist_ok=True)
shutil.copy2(source, backup / source.name)

bp = unreal.load_asset(PACKAGE)
assert bp and isinstance(unreal.get_default_object(bp.generated_class()), unreal.SCLPlayerAnimInstance)
before = sorted(str(x) for x in unreal.BlueprintEditorLibrary.list_graph_names(bp))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert before == sorted(str(x) for x in unreal.BlueprintEditorLibrary.list_graph_names(bp))
for graph in unreal.BlueprintEditorLibrary.list_graphs(bp):
    editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
    assert not editor.list_nodes_with_errors(), graph.get_name()
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
report = {'asset': PACKAGE, 'graphs': before, 'backup': str(backup), 'saved': True,
          'note': '仅证明编译保存成功；仍需重新启动 UE 验证 PIE'}
(saved / 'PlayerAnimRecompile.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('SCL_PLAYER_ANIM_RECOMPILED ' + json.dumps(report, ensure_ascii=False))
