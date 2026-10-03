"""UE Python：首次建立八向闪避及 0.10～0.60 秒无敌通知。

素材包原件不改。重复运行保留已存在的 Montage、通知和技能配置；
参数以后在 GA_PlayerDodge Class Defaults / 各方向 Montage 中调整。
"""
import unreal

ROOT = '/Game/SoulCombatLab/Characters/Player/Combat'
SOURCE = '/Game/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Dodge/'
KEYS = ('F', 'FR', 'R', 'BR', 'B', 'BL', 'L', 'FL')
ENUMS = ('FORWARD', 'FORWARD_RIGHT', 'RIGHT', 'BACKWARD_RIGHT',
         'BACKWARD', 'BACKWARD_LEFT', 'LEFT', 'FORWARD_LEFT')


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


montages = {}
for key, enum in zip(KEYS, ENUMS):
    source = require(unreal.load_asset(SOURCE + 'GhostSamurai_APose_Dodge_' + key + '_Root'), key)
    path = ROOT + '/Animations/AS_PlayerDodge_' + key
    sequence = unreal.load_asset(path)
    if not sequence:
        sequence = require(unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(), path), path)
        unreal.AnimationLibrary.set_root_motion_enabled(sequence, True)
        unreal.AnimationLibrary.set_root_motion_lock_type(sequence, unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
        sequence.set_editor_property('force_root_lock', True)
        require(unreal.EditorAssetLibrary.save_loaded_asset(sequence), '无法保存 ' + path)
    name = 'AM_PlayerDodge_' + key
    path = ROOT + '/Animations/' + name
    montage = unreal.load_asset(path)
    if not montage:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property('target_skeleton', sequence.get_editor_property('skeleton'))
        factory.set_editor_property('source_animation', sequence)
        montage = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, ROOT + '/Animations', unreal.AnimMontage, factory), path)
        require(unreal.SCLAnimationAssetLibrary.configure_combo_sections(montage, [sequence], [name]), name)
        for prop, seconds in (('blend_in', 0.08), ('blend_out', 0.10)):
            blend = montage.get_editor_property(prop)
            blend.set_editor_property('blend_time', seconds)
            montage.set_editor_property(prop, blend)
        unreal.AnimationLibrary.add_animation_notify_track(montage, 'SCL_Dodge')
        unreal.AnimationLibrary.add_animation_notify_state_event(
            montage, 'SCL_Dodge', 0.10, 0.50, unreal.ANS_SCLInvincible.static_class())
        require(unreal.EditorAssetLibrary.save_loaded_asset(montage), '无法保存 ' + path)
    montages[getattr(unreal.SCLDodgeDirection, enum)] = montage
    unreal.log('SCL_DODGE_ASSET ' + montage.get_path_name() + ' source=' + source.get_path_name())

path = ROOT + '/Abilities/GA_PlayerDodge'
blueprint = unreal.load_asset(path)
if not blueprint:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.SCLDodgeAbility.static_class())
    blueprint = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'GA_PlayerDodge', ROOT + '/Abilities', unreal.Blueprint, factory), path)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    defaults = unreal.get_default_object(blueprint.generated_class())
    defaults.set_editor_property('directional_montages', montages)
    require(unreal.EditorAssetLibrary.save_loaded_asset(blueprint), '无法保存技能蓝图')
else:
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    defaults = unreal.get_default_object(blueprint.generated_class())
    if not defaults.get_editor_property('directional_montages'):
        defaults.set_editor_property('directional_montages', montages)
        require(unreal.EditorAssetLibrary.save_loaded_asset(blueprint), '无法保存八向配置')

player = require(unreal.load_asset('/Game/SoulCombatLab/Blueprints/Player/BP_SCLPlayerSamurai'), '缺少玩家蓝图')
defaults = unreal.get_default_object(player.generated_class())
abilities = list(defaults.get_editor_property('startup_abilities'))
# 原位替换原生 Dodge，保留用户后来配置的派生类；防止重复授予两份闪避。
abilities = [blueprint.generated_class() if a == unreal.SCLDodgeAbility.static_class() else a for a in abilities]
matches = [a for a in abilities if a and isinstance(unreal.get_default_object(a), unreal.SCLDodgeAbility)]
require(len(matches) == 1, '玩家应只授予一个闪避技能')
defaults.set_editor_property('startup_abilities', abilities)
require(unreal.EditorAssetLibrary.save_loaded_asset(player), '无法保存玩家技能列表')
unreal.log('SCL_DIRECTIONAL_DODGE_CONFIGURED ' + matches[0].get_path_name())
