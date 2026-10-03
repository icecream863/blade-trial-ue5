"""编辑器 Python：在实际 PIE 世界逐项运行现有回归，每项结束后退出再进入 PIE。

运行前保存编辑器资源，以 -abslog=<项目>/Saved/Logs/RepairPIEFinal.log 启动 UE。
隐藏窗口自动验证另加 -unattended；引擎在所有窗口隐藏时还会强制限帧，设置开关不能覆盖它。
通过模块命名空间 exec 本文件（见 Docs/Testing.md），确保异步回调能读取同一组状态。
结果写 Saved/PIECombatVerification.json。
这是自动化验证，不代替人工连续观察动作观感。
"""
import json
import time
from pathlib import Path
import unreal

TESTS = ['GhostPlayerCombo', 'JumpAnimation', 'RuntimePolish']
saved = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))
log_path = saved / 'Logs' / 'RepairPIEFinal.log'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
state = {'index': 0, 'phase': 'begin', 'time': time.monotonic(), 'offset': 0,
         'results': [], 'passed': False}
handle = None
next_poll = 0.0

def finish(error=None):
    if error:
        state['error'] = error
        level.editor_request_end_play()
    state['passed'] = not error and len(state['results']) == len(TESTS)
    (saved / 'PIECombatVerification.json').write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')
    background_settings.set_editor_property('bThrottleCPUWhenNotForeground', original_background_throttle)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.log('SCL_PIE_VERIFICATION ' + json.dumps(state))

def tick(_delta):
    global next_poll
    try:
        if time.monotonic() < next_poll: return
        next_poll = time.monotonic() + 0.25
        if time.monotonic() - state['time'] > 150:
            finish('PIE phase timed out: ' + state['phase']); return
        world = editor.get_game_world()
        if state['phase'] == 'begin':
            if world: raise RuntimeError('已有游戏世界；请先退出 PIE 再执行')
            level.editor_request_begin_play()
            state['phase'] = 'wait_world'; state['time'] = time.monotonic()
        elif state['phase'] == 'wait_world' and world:
            # 不能把异步请求成功当成启动成功：必须有 UEDPIE 世界和实际玩家。
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            if not pawn: return
            assert 'UEDPIE_' in world.get_path_name(), world.get_path_name()
            state['world'] = world.get_path_name()
            state['pawn'] = pawn.get_class().get_name()
            state['offset'] = len(log_path.read_text(encoding='utf-8', errors='replace')) if log_path.exists() else 0
            unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS 60')
            unreal.SystemLibrary.execute_console_command(world, 'Automation RunTests SoulCombatLabCombat.' + TESTS[state['index']])
            state['phase'] = 'wait_test'; state['time'] = time.monotonic()
        elif state['phase'] == 'wait_test':
            text = log_path.read_text(encoding='utf-8', errors='replace')[state['offset']:]
            name = TESTS[state['index']]
            marker = 'Test Completed. Result={Success} Name={' + name + '}'
            if 'Test Completed. Result={Fail} Name={' + name + '}' in text:
                level.editor_request_end_play(); finish('Failed: ' + name); return
            if marker in text:
                state['results'].append({'test': name, 'result': 'Success', 'world': state['world'], 'pawn': state['pawn']})
                level.editor_request_end_play()
                state['phase'] = 'wait_end'; state['time'] = time.monotonic()
        elif state['phase'] == 'wait_end' and not world:
            state['index'] += 1
            if state['index'] == len(TESTS): finish(); return
            state['phase'] = 'begin'; state['time'] = time.monotonic()
    except Exception as error:
        finish(str(error))

assert '/Game/Maps/L_CombatLab' in editor.get_editor_world().get_path_name()
# 自动化排队前 GIsAutomationTesting 尚为 false；后台 3 FPS 会卡住框架的 >=10 FPS 等待。
# 只临时改运行中的设置对象，不调用 SaveConfig；无论成功或失败都恢复原值。
background_settings = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
original_background_throttle = background_settings.get_editor_property('bThrottleCPUWhenNotForeground')
background_settings.set_editor_property('bThrottleCPUWhenNotForeground', False)
handle = unreal.register_slate_post_tick_callback(tick)
unreal.log('SCL_PIE_VERIFICATION_STARTED')
