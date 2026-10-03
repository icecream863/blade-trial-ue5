"""只读：重新加载玩家配置，核对六项动画选择、通知及可运行的 Blend Space 数据。"""
import unreal
from pathlib import Path
import json

ROOT='/Game/SoulCombatLab/Characters/Player/Combat'
report={}
def load(path):
    obj=unreal.load_asset(path)
    assert obj, path
    return obj
def montage_info(m):
    events=[]
    for e in unreal.AnimationLibrary.get_animation_notify_events(m):
        state=e.get_editor_property('notify_state_class')
        notify=e.get_editor_property('notify')
        events.append({'class':(state or notify).get_class().get_name() if state or notify else str(e.get_editor_property('notify_name')),
                       'start':unreal.AnimationLibrary.get_anim_notify_event_trigger_time(e),
                       'duration':unreal.AnimationLibrary.get_anim_notify_event_duration(e)})
    tracks=m.get_editor_property('slot_anim_tracks')
    sequences=[seg.get_editor_property('anim_reference').get_path_name() for track in tracks for seg in track.get_editor_property('anim_track').get_editor_property('anim_segments')]
    return {'path':m.get_path_name(),'length':m.get_play_length(),'sequences':sequences,'events':events}

player=load('/Game/SoulCombatLab/Blueprints/Player/BP_SCLPlayerSamurai')
cdo=unreal.get_default_object(player.generated_class())
combo=cdo.get_component_by_class(unreal.SCLPlayerComboComponent.static_class())
moveset=combo.get_editor_property('player_moveset')
steps=moveset.get_editor_property('steps')
assert len(steps)==5
assert moveset.get_editor_property('light_opener_index')==0
assert moveset.get_editor_property('heavy_opener_index')==4
report['steps']=[]
for i,s in enumerate(steps):
    info=montage_info(s.get_editor_property('montage'))
    info.update(index=i,name=str(s.get_editor_property('attack_name')),nextLight=s.get_editor_property('next_light_step_index'),nextHeavy=s.get_editor_property('next_heavy_step_index'),cost=s.get_editor_property('stamina_cost'))
    if i<4:
        assert info['name']=='Attack01_'+str(i+1)
        assert info['nextLight']==(i+1 if i<3 else -1)
        assert info['nextHeavy']==4
        assert any('Attack01_'+str(i+1) in x for x in info['sequences'])
    else:
        assert info['name']=='JumpAttack04' and info['nextLight']==info['nextHeavy']==-1
        assert any('AS_PlayerHeavyJump04' in x for x in info['sequences'])
        assert not any(e['class']=='ANS_SCLComboWindow' for e in info['events'])
        assert sum(e['class']=='ANS_SCLWeaponTrace' for e in info['events'])==2
    report['steps'].append(info)

abilities={}
for name,properties in [('GA_PlayerParry',['parry_montage']),('GA_PlayerExecution',['execution_montage']),('GA_PlayerBlock',['block_start_montage','block_end_montage'])]:
    defaults=unreal.get_default_object(load(ROOT+'/Abilities/'+name).generated_class())
    abilities[name]={prop:(montage_info(defaults.get_editor_property(prop)) if defaults.get_editor_property(prop) else None) for prop in properties}
assert abilities['GA_PlayerBlock']['block_start_montage']
assert abilities['GA_PlayerBlock']['block_end_montage'] is None
report['abilities']=abilities
parry=abilities['GA_PlayerParry']['parry_montage']
assert any('AS_PlayerParryDeflect' in x for x in parry['sequences'])
windows=[e for e in parry['events'] if e['class']=='ANS_SCLParryWindow']
assert len(windows)==1 and abs(windows[0]['start']-0.02)<0.001 and abs(windows[0]['duration']-0.45)<0.001
execution=abilities['GA_PlayerExecution']['execution_montage']
assert any('AS_PlayerExecutionSPAttack' in x for x in execution['sequences'])
impacts=[e for e in execution['events'] if e['class']=='AN_SCLExecutionImpact']
assert len(impacts)==1 and abs(impacts[0]['start']-1.024)<0.001
assert any(e['class']=='AnimNotifyState_MotionWarping' for e in execution['events'])
for name,source in [('AS_PlayerParryDeflect','/Apose/Deflect/Root/GhostSamurai_LSting_DeflectL_CounterExecution_Root'),('AS_PlayerExecutionSPAttack','/Apose/Attack/GhostSamurai_APose_SPAttack01_Root'),('AS_PlayerHeavyJump04','/Apose/Attack/GhostSamurai_APose_JumpAttack04_Root'),('AS_PlayerBlockStart','/Apose/Defense/GhostSamurai_APose2DefenseR_Root'),('AS_PlayerBlockEnd','/Apose/Defense/GhostSamurai_DefenseR2APose_Root')]:
    sequence=load(ROOT+'/Animations/'+name)
    original=load('/Game/GhostSamurai_Bundle/GhostSamurai/Katana'+source)
    assert sequence.get_editor_property('skeleton')==original.get_editor_property('skeleton')
    assert abs(sequence.get_play_length()-original.get_play_length())<0.001
    # 项目副本保留来源路径，证明具体选用了哪段源动画，而不是仅凭目标名称。
    report.setdefault('sequenceSources',{})[name]={'source':original.get_path_name(),'length':sequence.get_play_length()}

report['run']={}
for name in ['BS_PlayerArmed8Way','BS_PlayerSheathed8Way']:
    blend=load(ROOT+'/Locomotion/'+name)
    assert unreal.SCLAnimationAssetLibrary.has_blend_space_sampling(blend)
    samples=blend.get_editor_property('sample_data')
    assert len(samples)==9
    movement=[sample.get_editor_property('animation').get_path_name() for sample in samples if sample.get_editor_property('sample_value').x != 0 or sample.get_editor_property('sample_value').y != 0]
    assert len(movement)==8 and len(set(movement))==8 and all('/Apose/Movement/Run/' in x for x in movement)
    report['run'][name]=movement
report['runSpeed']=cdo.get_editor_property('character_movement').get_editor_property('max_walk_speed')
assert report['runSpeed']==437.5
assert cdo.get_component_by_class(unreal.SCLTargetingComponent).get_editor_property('locked_walk_speed') == 437.5
report['animClass']=cdo.get_editor_property('mesh').get_editor_property('anim_class').get_path_name()
assert 'ABP_SCLPlayerCombat' in report['animClass']
report['startupAbilities']=[cls.get_path_name() for cls in cdo.get_editor_property('startup_abilities')]
for name in abilities:
    assert sum(name+'_C' in x for x in report['startupAbilities'])==1
Path(unreal.Paths.project_saved_dir(),'RequestedPlayerAnimationsVerified.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('SCL_REQUESTED_PLAYER_ANIMATIONS_VERIFIED')
