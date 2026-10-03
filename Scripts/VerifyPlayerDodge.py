"""UE Python：重新读取保存的玩家技能与八向闪避，输出资产证据。

此检查证明引用和通知正确；实际播放、位移及中断由 DirectionalDodge 游戏回归验证。
"""
import json
from pathlib import Path
import unreal

player = unreal.load_asset('/Game/SoulCombatLab/Blueprints/Player/BP_SCLPlayerSamurai')
abilities = unreal.get_default_object(player.generated_class()).get_editor_property('startup_abilities')
matches = [a for a in abilities if a and isinstance(unreal.get_default_object(a), unreal.SCLDodgeAbility)]
assert len(matches) == 1 and matches[0] != unreal.SCLDodgeAbility.static_class(), '必须授予唯一的配置闪避子类'
defaults = unreal.get_default_object(matches[0])
mapping = defaults.get_editor_property('directional_montages')
assert len(mapping) == 8, '八向配置缺项'
legacy = unreal.load_asset('/Game/SoulCombatLab/Animations/AM_Dodge')
legacy_windows = [{'start': unreal.AnimationLibrary.get_anim_notify_event_trigger_time(e),
                   'duration': unreal.AnimationLibrary.get_anim_notify_event_duration(e)}
                  for e in unreal.AnimationLibrary.get_animation_notify_events(legacy)
                  if isinstance(e.get_editor_property('notify_state_class'), unreal.ANS_SCLInvincible)]
report = {'legacyWindows': legacy_windows, 'grantedClass': matches[0].get_path_name(), 'staminaCost': defaults.get_editor_property('stamina_cost'),
          'dodgeDistance': defaults.get_editor_property('dodge_distance'), 'directions': {}}
for direction, montage in mapping.items():
    assert montage, '方向引用为空'
    tracks = montage.get_editor_property('slot_anim_tracks')
    segments = tracks[0].get_editor_property('anim_track').get_editor_property('anim_segments')
    assert len(segments) == 1
    sequence = segments[0].get_editor_property('anim_reference')
    assert sequence and sequence.get_editor_property('enable_root_motion')
    windows = []
    for event in unreal.AnimationLibrary.get_animation_notify_events(montage):
        state = event.get_editor_property('notify_state_class')
        if isinstance(state, unreal.ANS_SCLInvincible):
            # 起点由 linkable element 返回，不能拿 TriggerTimeOffset 当通知开始时间。
            windows.append({'start': unreal.AnimationLibrary.get_anim_notify_event_trigger_time(event),
                            'duration': unreal.AnimationLibrary.get_anim_notify_event_duration(event)})
    assert len(windows) == 1 and abs(windows[0]['duration'] - 0.5) < 0.005
    assert abs(windows[0]['start'] - 0.10) < 0.005
    report['directions'][str(direction)] = {'montage': montage.get_path_name(),
        'sequence': sequence.get_path_name(), 'length': montage.get_play_length(), 'invincibility': windows}
Path(unreal.Paths.project_saved_dir(), 'DirectionalDodgeAssets.json').write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
unreal.log('SCL_DIRECTIONAL_DODGE_ASSETS_VERIFIED ' + json.dumps(report, ensure_ascii=False))
