"""UE Python：在独立进程重新读取保存的技能蓝图与玩家授予列表，输出配置证据。"""
import json
from pathlib import Path
import unreal

ROOT = "/Game/SoulCombatLab/Characters/Player/Combat/Abilities/"
PLAYER = "/Game/SoulCombatLab/Blueprints/Player/BP_SCLPlayerSamurai"
CONFIGS = (
    ("GA_PlayerBlock", unreal.SCLBlockAbility, ("block_start_montage", "block_end_montage", "blocking_speed_multiplier")),
    ("GA_PlayerParry", unreal.SCLParryAbility, ("parry_montage",)),
    ("GA_PlayerExecution", unreal.SCLExecutionAbility, ("execution_montage", "maximum_execution_distance", "execution_stand_off_distance")),
)
player = unreal.load_asset(PLAYER)
if not player:
    raise RuntimeError("缺少玩家蓝图")
defaults = unreal.get_default_object(player.generated_class())
abilities = list(defaults.get_editor_property("startup_abilities"))
report = {"player": PLAYER, "startupAbilities": [ability.get_path_name() for ability in abilities if ability], "configs": {}}
for name, native, properties in CONFIGS:
    matches = [ability for ability in abilities if ability and
               isinstance(unreal.get_default_object(ability), native)]
    if len(matches) != 1 or matches[0] == native.static_class():
        raise RuntimeError("玩家没有唯一配置派生技能：" + name)
    ability = unreal.get_default_object(matches[0])
    values = {}
    for prop in properties:
        value = ability.get_editor_property(prop)
        if prop.endswith("montage"):
            # 收势是可选过渡；当前玩家清空它，松开后由 AnimBP 混回基础移动。
            if prop == "block_end_montage" and not value:
                values[prop] = None
                continue
            if not value:
                raise RuntimeError("动画引用为空：" + name + "." + prop)
            values[prop] = value.get_path_name()
        else:
            values[prop] = float(value)
    report["configs"][name] = {"grantedClass": matches[0].get_path_name(), "properties": values}
Path(unreal.Paths.project_saved_dir(), "AbilityBlueprintConfiguration.json").write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
unreal.log("SCL_ABILITY_BLUEPRINTS_VERIFIED " + json.dumps(report, ensure_ascii=False))
