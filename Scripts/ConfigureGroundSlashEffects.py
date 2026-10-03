"""在 UE 编辑器中运行：给项目重击配置竖劈刀痕；不修改素材包。

Niagara 的原 NE_Decal 在世界空间写朝向，无法直接代表角色的竖劈平面。
项目副本关闭该贴花，粒子保留；AN_SCLGroundSlash 在落刀时沿角色前方投影。
再次运行只刷新本脚本拥有的两条 Notify，不改伤害窗口或其他招式。
"""
from pathlib import Path
import shutil
import unreal

PROJECT = Path(unreal.Paths.project_dir())
ANIM = '/Game/SoulCombatLab/Characters/Player/Combat/Animations/AM_PlayerHeavyJump04'
EFFECTS = '/Game/SoulCombatLab/Characters/Player/Combat/Effects'
BUNDLE = '/Game/GhostSamurai_Bundle/GhostSamurai/VFX/Effect/Slash'
LIB = unreal.AnimationLibrary


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def configure():
    require(not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), '请先退出 PIE')
    montage = require(unreal.load_asset(ANIM), '找不到项目重击')
    # 先核对原通知，不能遇到用户改过的未知配置就清空整条时间轴。
    old = []
    for event in LIB.get_animation_notify_events(montage):
        notify = event.get_editor_property('notify')
        if not notify:
            continue
        name = notify.get_class().get_name()
        if name not in ('AnimNotify_PlayNiagaraEffect', 'AN_SCLGroundSlash'):
            continue
        template = notify.get_editor_property('template')
        stem = template.get_name() if template else ''
        if stem not in ('NS_Slash_Ground', 'NS_Slash_Fall', 'NS_SCLSlashGround', 'NS_SCLSlashFall'):
            continue
        props = {p: notify.get_editor_property(p) for p in
                 ('socket_name', 'location_offset', 'rotation_offset', 'scale', 'absolute_scale')}
        if name == 'AN_SCLGroundSlash':
            # 用户在 Montage 上调过大小、方向或停留时间时，重复运行保留这些参数。
            props.update({p: notify.get_editor_property(p) for p in
                          ('ground_material', 'decal_size', 'direction_yaw_offset', 'hold_seconds', 'fade_seconds')})
        old.append((event, notify, stem, props))
    require(len(old) == 2, '重击地面特效数量变化，停止配置以保留用户修改')
    names = {str(event.get_editor_property('notify_name')) for event, _, _, _ in old}
    for event in LIB.get_animation_notify_events(montage):
        if str(event.get_editor_property('notify_name')) in names:
            require(any(event.get_editor_property('notify') == n for _, n, _, _ in old),
                    '其他通知使用相同名称，停止配置')

    backup = PROJECT / 'Saved/Backups/GroundSlash/AM_PlayerHeavyJump04.before-ground-slash.uasset'
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(PROJECT / 'Content/SoulCombatLab/Characters/Player/Combat/Animations/AM_PlayerHeavyJump04.uasset', backup)
    systems = {}
    decal_class = unreal.load_class(None, '/Script/Niagara.NiagaraDecalRendererProperties')
    for source, target in [('NS_Slash_Ground', 'NS_SCLSlashGround'), ('NS_Slash_Fall', 'NS_SCLSlashFall')]:
        path = EFFECTS + '/' + target
        system = unreal.load_asset(path)
        if not system:
            system = require(unreal.EditorAssetLibrary.duplicate_asset(BUNDLE + '/' + source, path), '复制失败：' + source)
        # 只处理当前 NE_Decal 的 Renderer，不修改继承缓存或其他发射器。
        expected = system.get_path_name() + ':NE_Decal.NiagaraDecalRendererProperties_0'
        renderers = [r for r in unreal.ObjectIterator(decal_class) if r.get_path_name() == expected]
        require(len(renderers) == 1, '无法定位地面贴花 Renderer：' + target)
        renderers[0].set_editor_property('is_enabled', False)
        require(not renderers[0].get_editor_property('is_enabled'), '旧贴花没有关闭')
        require(unreal.EditorAssetLibrary.save_loaded_asset(system, only_if_is_dirty=False), '保存特效失败')
        systems[source] = system
    material = require(unreal.load_asset(BUNDLE + '/MI_FX_Slash_Decal_001'), '刀痕材质缺失')

    # 缓存时刻和轨道，再删旧 Notify；WeaponTrace、MotionWarping、TimedNiagara 不受影响。
    replacements = []
    tracks = LIB.get_animation_notify_track_names(montage)
    for event, old_notify, stem, props in old:
        source = 'NS_Slash_Ground' if stem in ('NS_Slash_Ground', 'NS_SCLSlashGround') else 'NS_Slash_Fall'
        # TrackIndex 没有公开给 Python；通过轨道查询保留原位置，不访问私有字段。
        matching_tracks = [track for track in tracks if any(
            item.get_editor_property('notify') == old_notify
            for item in LIB.get_animation_notify_events_for_track(montage, track))]
        require(len(matching_tracks) == 1, '不能确定原通知轨道')
        replacements.append((matching_tracks[0],
                             LIB.get_anim_notify_event_trigger_time(event), systems[source], props))
    for name in names:
        LIB.remove_animation_notify_events_by_name(montage, name)
    for track, time, system, props in replacements:
        notify = require(LIB.add_animation_notify_event(montage, track, time, unreal.AN_SCLGroundSlash.static_class()), '添加竖劈通知失败')
        notify.set_editor_property('ground_material', material)
        for prop, value in props.items():
            notify.set_editor_property(prop, value)
        notify.set_editor_property('template', system)
        notify.set_editor_property('attached', False)
        print('GROUND_SLASH', time, system.get_path_name(), props['location_offset'])
    require(unreal.EditorAssetLibrary.save_loaded_asset(montage, only_if_is_dirty=False), '保存重击失败')
    print('GROUND_SLASH_CONFIGURED: project copies only; visual PIE check remains manual')


if __name__ == '__main__':
    configure()
