"""UE 编辑器 Python：只更新玩家八向跑步与速度，不改攻击或防御技能。

普通移动和锁定移动共用项目 Blend Space；素材原件保持不变。
可重复执行，但会覆盖这两套 Blend Space 和玩家蓝图的移动速度配置。
"""
import math
import unreal

ROOT = '/Game/SoulCombatLab/Characters/Player/Combat'
RUN = '/Game/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Movement/Run'
# 八个循环动作均约 0.8 秒走 350 cm。角色胶囊由 CharacterMovement 推动，
# AnimBP 忽略基础移动的根位移，所以移动速度应与动画步幅接近。
RUN_SPEED = 437.5
DIRECTIONS = {'F': (0, 1), 'FR': (1, 1), 'R': (1, 0), 'BR': (1, -1),
              'B': (0, -1), 'BL': (-1, -1), 'L': (-1, 0), 'FL': (-1, 1)}


def load(path):
    result = unreal.load_asset(path)
    assert result, '缺少资产：' + path
    return result


def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


idle = load('/Game/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/GhostSamurai_APose_Idle')
for name in ['BS_PlayerArmed8Way', 'BS_PlayerSheathed8Way']:
    blend = load(ROOT + '/Locomotion/' + name)
    samples = []
    center = unreal.BlendSample()
    center.set_editor_property('animation', idle)
    center.set_editor_property('sample_value', unreal.Vector(0, 0, 0))
    samples.append(center)
    for direction, (x, y) in DIRECTIONS.items():
        # F 使用 Loop；Start、End 和 Sprint 不是这里的持续八向跑步样本。
        stem = 'GhostSamurai_APose_Strafe_Run_' + direction + ('_Loop' if direction == 'F' else '') + '_Root'
        sequence = load(RUN + '/' + stem)
        assert sequence.get_editor_property('skeleton') == blend.get_editor_property('skeleton')
        sample = unreal.BlendSample()
        sample.set_editor_property('animation', sequence)
        # AnimInstance 输入是速度 / 最大速度。满速斜向为 ±0.707，而非 ±1；
        # 样本也归一化，避免斜向满速时仍混入 Idle，导致脚步软、滑。
        radius = math.hypot(x, y)
        sample.set_editor_property('sample_value', unreal.Vector(x / radius, y / radius, 0))
        samples.append(sample)
    blend.set_editor_property('interpolate_using_grid', False)
    blend.set_editor_property('sample_data', samples)
    # 写入样本之后必须重建运行时插值；只改资源表不能保证游戏里骨骼会动。
    assert unreal.SCLAnimationAssetLibrary.rebuild_blend_space_sampling(blend), name
    save(blend)

player = load('/Game/SoulCombatLab/Blueprints/Player/BP_SCLPlayerSamurai')
defaults = unreal.get_default_object(player.generated_class())
defaults.get_editor_property('character_movement').set_editor_property('max_walk_speed', RUN_SPEED)
defaults.get_component_by_class(unreal.SCLTargetingComponent).set_editor_property('locked_walk_speed', RUN_SPEED)
unreal.BlueprintEditorLibrary.compile_blueprint(player)
save(player)

# 更新当前编辑器内已经加载的项目 AnimBP，保留用户的现有图连接。
anim_bp = load(ROOT + '/ABP_SCLPlayerCombat')
unreal.BlueprintEditorLibrary.compile_blueprint(anim_bp)
save(anim_bp)
unreal.log('SCL_RUN_LOCOMOTION_CONFIGURED eight directions; free/locked speed=' + str(RUN_SPEED))
