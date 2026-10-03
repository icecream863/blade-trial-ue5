"""UE 编辑器写入脚本：降低当前玩家的攻击、闪避和格挡耗体。

只改实际玩家引用的配置资产，不改动画、连招连接或敌人参数。
手工调好参数后不要当检查器重跑；只读检查用 VerifyDemoExperience.py。
"""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
player = unreal.load_asset('/Game/SoulCombatLab/Blueprints/Player/BP_SCLPlayerSamurai')
assert player
defaults = unreal.get_default_object(player.generated_class())
moveset = defaults.get_editor_property('player_combo_component').get_editor_property('player_moveset')
assert moveset and len(moveset.get_editor_property('steps')) == 5
dodge_classes = [c for c in defaults.get_editor_property('startup_abilities')
                 if c and isinstance(unreal.get_default_object(c), unreal.SCLDodgeAbility)]
assert len(dodge_classes) == 1
dodge = unreal.get_default_object(dodge_classes[0])
dodge_bp = unreal.load_asset(dodge_classes[0].get_path_name().split('.')[0])
assert dodge_bp

# 保存修改前磁盘副本，之后只保存本脚本拥有的三个资源。
backup = ROOT / 'Saved/Backups' / ('DemoExperience-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
for asset in [moveset, dodge_bp, player]:
    relative = Path('Content' + asset.get_path_name().split('.')[0][len('/Game'):] + '.uasset')
    dest = backup / relative
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / relative, dest)

steps = list(moveset.get_editor_property('steps'))
old_costs = [s.get_editor_property('stamina_cost') for s in steps]
for step, name, cost in zip(steps, ['Attack01_1', 'Attack01_2', 'Attack01_3', 'Attack01_4', 'JumpAttack04'], [6, 7, 8, 10, 12]):
    assert str(step.get_editor_property('attack_name')) == name, '招式表已变化，请先核对再调参'
    step.set_editor_property('stamina_cost', float(cost))
moveset.set_editor_property('steps', steps)
old_dodge = dodge.get_editor_property('stamina_cost')
old_guard = defaults.get_editor_property('guard_stamina_damage_multiplier')
dodge.set_editor_property('stamina_cost', 12.0)
defaults.set_editor_property('guard_stamina_damage_multiplier', 0.8)
for bp in [dodge_bp, player]:
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
for asset in [moveset, dodge_bp, player]:
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset), asset.get_path_name()
report = {'oldAttackCosts': old_costs, 'attackCosts': [6, 7, 8, 10, 12],
          'oldDodgeCost': old_dodge, 'dodgeCost': 12, 'oldGuardMultiplier': old_guard,
          'guardMultiplier': 0.8, 'backup': str(backup)}
(ROOT / 'Saved/DemoExperienceConfiguration.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('SCL_DEMO_EXPERIENCE_CONFIGURED ' + json.dumps(report))
