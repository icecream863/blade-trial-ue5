"""独立 UE 进程只读检查：实际玩家引用的招式表、授予的闪避技能与防御耗体。"""
import json
from pathlib import Path
import unreal

bp = unreal.load_asset('/Game/SoulCombatLab/Blueprints/Player/BP_SCLPlayerSamurai')
player = unreal.get_default_object(bp.generated_class())
moveset = player.get_editor_property('player_combo_component').get_editor_property('player_moveset')
costs = [s.get_editor_property('stamina_cost') for s in moveset.get_editor_property('steps')]
assert costs == [6, 7, 8, 10, 12], costs
classes = [c for c in player.get_editor_property('startup_abilities')
           if c and isinstance(unreal.get_default_object(c), unreal.SCLDodgeAbility)]
assert len(classes) == 1
dodge = unreal.get_default_object(classes[0]).get_editor_property('stamina_cost')
guard = player.get_editor_property('guard_stamina_damage_multiplier')
assert dodge == 12 and abs(guard - 0.8) < 0.001
report = {'moveset': moveset.get_path_name(), 'attackCosts': costs, 'lightChainCost': sum(costs[:4]),
          'dodgeClass': classes[0].get_path_name(), 'dodgeCost': dodge, 'guardMultiplier': guard, 'passed': True}
Path(unreal.Paths.project_saved_dir(), 'DemoExperienceVerification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('SCL_DEMO_EXPERIENCE_VERIFIED ' + json.dumps(report))
