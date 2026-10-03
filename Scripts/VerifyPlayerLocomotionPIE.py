"""在已经开始并锁定训练靶的 PIE 中采样八向脚步，不保存游戏世界。

需先开始 Demo、运行 DebugPrepareLockOnSwitch，再用中键锁定训练靶。
脚本异步运行约八秒，结果写 Saved/RunLocomotionPIE.json，结束恢复摆位。
这是运行时验证；八向骨骼在动，不等于脚底接触已经完美无滑动。
"""
import json
import math
import time
from pathlib import Path
import unreal

world = unreal.EditorLevelLibrary.get_game_world()
assert world, '需要正在运行的 PIE'
player = unreal.GameplayStatics.get_player_character(world, 0)
assert player
mesh = player.get_editor_property('mesh')
movement = player.get_editor_property('character_movement')
anim = mesh.get_anim_instance()
targeting = player.get_component_by_class(unreal.SCLTargetingComponent)
locked = targeting.is_locked_on()
assert 'ABP_SCLPlayerCombat' in anim.get_class().get_path_name()
assert abs(movement.get_editor_property('max_walk_speed') - 437.5) < 0.01
assert mesh.get_bone_index('foot_l') >= 0
anchor = player.get_actor_location()
rotation = player.get_actor_rotation()
controller = player.get_controller()
control_rotation = controller.get_control_rotation()
directions = [('F', 0, 1), ('FR', 1, 1), ('R', 1, 0), ('BR', 1, -1),
              ('B', 0, -1), ('BL', -1, -1), ('L', -1, 0), ('FL', -1, 1)]
if not locked:
    directions = [('F', 0, 1)]
report = {'pawn': player.get_path_name(), 'animClass': anim.get_class().get_path_name(),
          'locked': locked, 'speed': movement.get_editor_property('max_walk_speed'), 'directions': [], 'passed': False}
state = {'index': 0, 'start': unreal.GameplayStatics.get_time_seconds(world), 'feet': [],
         'started': time.monotonic(), 'handle': None}


def foot():
    v = mesh.get_socket_transform('foot_l', unreal.RelativeTransformSpace.RTS_COMPONENT).translation
    return [v.x, v.y, v.z]


def finish(error=None):
    # 仅恢复本次测试改变的摆位和速度；不存盘、不销毁用户场景资产。
    unreal.unregister_slate_post_tick_callback(state['handle'])
    movement.stop_movement_immediately()
    player.set_actor_location(anchor, False, True)
    player.set_actor_rotation(rotation, True)
    controller.set_control_rotation(control_rotation)
    if error:
        report['error'] = str(error)
    else:
        report['passed'] = True
    name = 'RunLocomotionPIE.json' if locked else 'RunLocomotionPIEFree.json'
    Path(unreal.Paths.project_saved_dir(), name).write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('SCL_RUN_PIE_RESULT ' + json.dumps(report))


def tick(delta):
    try:
        assert time.monotonic() - state['started'] < 60, 'PIE 采样超时'
        assert targeting.is_locked_on() == locked, '采样中锁定状态改变'
        name, x, y = directions[state['index']]
        # 走真实 CharacterMovement 路径；不直接设置骨骼姿态或伪造动画速度。
        forward, right = player.get_actor_forward_vector(), player.get_actor_right_vector()
        radius = math.hypot(x, y)
        player.add_movement_input(forward * (y / radius) + right * (x / radius), 1.0, False)
        state['feet'].append(foot())
        now = unreal.GameplayStatics.get_time_seconds(world)
        if now - state['start'] < 0.8:
            return
        travel = max(math.dist(a, b) for a in state['feet'] for b in state['feet'])
        f = anim.get_editor_property('local_forward_speed')
        r = anim.get_editor_property('local_right_speed')
        row = {'direction': name, 'footLocalTravelCm': travel, 'forward': f, 'right': r,
               'velocityCmPerSec': player.get_velocity().length()}
        report['directions'].append(row)
        assert travel > 2, name + ' 脚部未迈步'
        assert not y or f * y > 0.1, name + ' 前后方向错误'
        assert not x or r * x > 0.1, name + ' 左右方向错误'
        state['index'] += 1
        if state['index'] == len(directions):
            finish()
            return
        movement.stop_movement_immediately()
        player.set_actor_location(anchor, False, True)
        state['feet'] = []
        state['start'] = now
    except Exception as error:
        finish(error)


state['handle'] = unreal.register_slate_post_tick_callback(tick)
unreal.log('SCL_RUN_PIE_SAMPLING_STARTED')
