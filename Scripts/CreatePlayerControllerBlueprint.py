"""在 UE Python commandlet 中补齐 Controller 配置蓝图，并接入玩家 GameMode。"""
import unreal
import json
from pathlib import Path

CONTROLLER_PATH = "/Game/SoulCombatLab/Blueprints/Player/BP_SCLPlayerController"
GAME_MODE_PATH = "/Game/SoulCombatLab/Blueprints/GameModes/BP_SCLPlayerGameMode"


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


# 保留已存在的蓝图；创建的是原生 Controller 的配置子类，输入逻辑继承 C++。
controller = unreal.load_asset(CONTROLLER_PATH)
if controller is None:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.SCLDemoPlayerController.static_class())
    controller = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "BP_SCLPlayerController", "/Game/SoulCombatLab/Blueprints/Player", unreal.Blueprint, factory
    ), "Could not create Controller blueprint")
unreal.BlueprintEditorLibrary.compile_blueprint(controller)
require(isinstance(unreal.get_default_object(controller.generated_class()), unreal.SCLDemoPlayerController),
        "Controller blueprint does not inherit the native player Controller")
# 把拆分前玩家蓝图的实际参数迁移到 Controller，保留用户已调好的数值。
legacy = Path(unreal.Paths.project_saved_dir(), "LegacyInputThreshold.json")
migration_marker = Path(unreal.Paths.project_saved_dir(), "ControllerInputThresholdMigrated.json")
if legacy.exists() and not migration_marker.exists():
    threshold = json.loads(legacy.read_text())["threshold"]
    unreal.get_default_object(controller.generated_class()).set_editor_property("heavy_attack_hold_threshold", threshold)

# 输入引用迁移前由 MCP 从实际玩家蓝图 CDO 导出，包含用户覆盖值，而不是只填 C++ 默认。
input_snapshot = Path(unreal.Paths.project_saved_dir(), "LegacyPlayerInputAssets.json")
input_marker = Path(unreal.Paths.project_saved_dir(), "ControllerInputAssetsMigrated.json")
input_properties = {
    "DefaultMappingContext": "default_mapping_context",
    "MouseLookMappingContext": "mouse_look_mapping_context",
    "JumpAction": "jump_action",
    "MoveAction": "move_action",
    "LookAction": "look_action",
    "MouseLookAction": "mouse_look_action",
    "DodgeAction": "dodge_action",
}
if input_snapshot.exists() and not input_marker.exists():
    snapshot = json.loads(input_snapshot.read_text(encoding="utf-8"))
    controller_cdo = unreal.get_default_object(controller.generated_class())
    for old_name, new_name in input_properties.items():
        value = snapshot[old_name]
        asset_path = value.get("refPath", "") if isinstance(value, dict) else ""
        asset = require(unreal.load_asset(asset_path), "Missing input asset: " + asset_path) if asset_path else None
        controller_cdo.set_editor_property(new_name, asset)
        actual = controller_cdo.get_editor_property(new_name)
        require((actual.get_path_name() if actual else "") == asset_path,
                "Input reference was not migrated: " + new_name)
require(unreal.EditorAssetLibrary.save_loaded_asset(controller), "Could not save Controller blueprint")

# GameMode 决定实际生成哪种 Controller；只创建资产而不接入不会改变游戏。
game_mode = require(unreal.load_asset(GAME_MODE_PATH), "Player GameMode blueprint is missing")
game_mode_cdo = unreal.get_default_object(game_mode.generated_class())
game_mode_cdo.set_editor_property("player_controller_class", controller.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(game_mode)
require(unreal.EditorAssetLibrary.save_loaded_asset(game_mode), "Could not save GameMode blueprint")
require(unreal.get_default_object(game_mode.generated_class()).get_editor_property("player_controller_class")
        == controller.generated_class(), "GameMode does not use Controller blueprint")
player = require(unreal.load_asset('/Game/SoulCombatLab/Blueprints/Player/BP_SCLPlayerSamurai'), 'Player blueprint is missing')
unreal.BlueprintEditorLibrary.compile_blueprint(player)
player_cdo = unreal.get_default_object(player.generated_class())
require(player_cdo.get_component_by_class(unreal.SCLPlayerDeveloperComponent.static_class()),
        'Player blueprint has no developer component')
require(unreal.EditorAssetLibrary.save_loaded_asset(player), 'Could not save Player blueprint')
if legacy.exists() and not migration_marker.exists():
    migration_marker.write_text(legacy.read_text())
if input_snapshot.exists() and not input_marker.exists():
    input_marker.write_text(input_snapshot.read_text(encoding="utf-8"), encoding="utf-8")
unreal.log("PLAYER_CONTROLLER_CONFIGURED " + controller.generated_class().get_path_name())
