"""Move project-owned GhostSamurai combat copies and create player config assets.

Run with Unreal Editor's Python plugin enabled. The source GhostSamurai bundle is
left in its vendor directory; only the ten project-authored Montage copies move.
"""

import unreal


OLD_MONTAGE_FOLDER = "/Game/SoulCombatLab/Animations/GhostSamurai/Native"
NEW_MONTAGE_FOLDER = "/Game/SoulCombatLab/Characters/Player/Combat/Animations"
MOVES_FOLDER = "/Game/SoulCombatLab/Characters/Player/Combat/Data"
PLAYER_FOLDER = "/Game/SoulCombatLab/Characters/Player"
PLAYER_BLUEPRINT_FOLDER = "/Game/SoulCombatLab/Blueprints/Player"
GAME_MODE_BLUEPRINT_FOLDER = "/Game/SoulCombatLab/Blueprints/GameModes"
MOVES_ASSET_PATH = MOVES_FOLDER + "/DA_GhostSamurai_PlayerMoveset"
OLD_PLAYER_BP_PATH = PLAYER_FOLDER + "/BP_SCLPlayerSamurai"
OLD_GAME_MODE_BP_PATH = PLAYER_FOLDER + "/BP_SCLPlayerGameMode"
PLAYER_BP_PATH = PLAYER_BLUEPRINT_FOLDER + "/BP_SCLPlayerSamurai"
GAME_MODE_BP_PATH = GAME_MODE_BLUEPRINT_FOLDER + "/BP_SCLPlayerGameMode"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        require(unreal.EditorAssetLibrary.make_directory(path), "Could not create " + path)


def save(asset):
    require(unreal.EditorAssetLibrary.save_loaded_asset(asset), "Could not save " + asset.get_path_name())


ensure_directory(NEW_MONTAGE_FOLDER)
ensure_directory(MOVES_FOLDER)
ensure_directory(PLAYER_FOLDER)
ensure_directory(PLAYER_BLUEPRINT_FOLDER)
ensure_directory(GAME_MODE_BLUEPRINT_FOLDER)

montage_names = [
    "AM_GSN_Attack01_1_ALL_Root",
    "AM_GSN_Attack01_2_Root",
    "AM_GSN_Attack01_3_Root",
    "AM_GSN_Attack01_4_Root",
    "AM_GSN_Attack02_1_ALL_Root",
    "AM_GSN_Attack02_2_Root",
    "AM_GSN_Attack02_3_Root",
    "AM_GSN_Attack02_4_Root",
    "AM_GSN_Attack02_5_Root",
    "AM_GSN_Attack02_6_Root",
]

for name in montage_names:
    old_path = OLD_MONTAGE_FOLDER + "/" + name
    new_path = NEW_MONTAGE_FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(new_path):
        continue
    require(unreal.EditorAssetLibrary.does_asset_exist(old_path), "Missing Montage: " + old_path)
    require(unreal.EditorAssetLibrary.rename_asset(old_path, new_path), "Could not move " + old_path)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
for old_path, new_path in [
    (OLD_PLAYER_BP_PATH, PLAYER_BP_PATH),
    (OLD_GAME_MODE_BP_PATH, GAME_MODE_BP_PATH),
]:
    if not unreal.EditorAssetLibrary.does_asset_exist(new_path) and unreal.EditorAssetLibrary.does_asset_exist(old_path):
        require(unreal.EditorAssetLibrary.rename_asset(old_path, new_path), "Could not move " + old_path)

moveset = unreal.load_asset(MOVES_ASSET_PATH)
if not moveset:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.SCLPlayerMovesetData.static_class())
    moveset = asset_tools.create_asset(
        "DA_GhostSamurai_PlayerMoveset", MOVES_FOLDER, unreal.SCLPlayerMovesetData, factory
    )
require(moveset is not None, "Could not create player moveset DataAsset")

values = [
    # Name, input, stamina, damage, poise, light-next, heavy-next
    ("Attack01_1", 1, 20.0, 1.45, 1.50, 4, 1),
    ("Attack01_2", 1, 22.0, 1.50, 1.55, 4, 2),
    ("Attack01_3", 1, 25.0, 1.60, 1.65, 4, 3),
    ("Attack01_4", 1, 28.0, 1.80, 1.90, -1, -1),
    ("Attack02_1", 0, 12.0, 1.00, 1.00, 5, 0),
    ("Attack02_2", 0, 13.0, 1.05, 1.05, 6, 0),
    ("Attack02_3", 0, 14.0, 1.10, 1.10, 7, 0),
    ("Attack02_4", 0, 15.0, 1.15, 1.15, 8, 0),
    ("Attack02_5", 0, 16.0, 1.20, 1.20, 9, 0),
    ("Attack02_6", 0, 18.0, 1.30, 1.35, -1, -1),
]
steps = []
for index, row in enumerate(values):
    step = unreal.SCLPlayerAttackStep()
    step.set_editor_property("attack_name", row[0])
    step.set_editor_property(
        "input_type", unreal.SCLPlayerAttackInput.HEAVY if row[1] else unreal.SCLPlayerAttackInput.LIGHT
    )
    step.set_editor_property("montage", unreal.load_asset(NEW_MONTAGE_FOLDER + "/" + montage_names[index]))
    step.set_editor_property("stamina_cost", row[2])
    step.set_editor_property("damage_multiplier", row[3])
    step.set_editor_property("poise_damage_multiplier", row[4])
    step.set_editor_property("next_light_step_index", row[5])
    step.set_editor_property("next_heavy_step_index", row[6])
    steps.append(step)
require(all(step.get_editor_property("montage") for step in steps), "A configured Montage could not be loaded")
moveset.set_editor_property("light_opener_index", 4)
moveset.set_editor_property("heavy_opener_index", 0)
moveset.set_editor_property("steps", steps)
save(moveset)

player_bp = unreal.load_asset(PLAYER_BP_PATH)
if not player_bp:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.SCLPlayerCharacter.static_class())
    player_bp = asset_tools.create_asset("BP_SCLPlayerSamurai", PLAYER_BLUEPRINT_FOLDER, unreal.Blueprint, factory)
require(player_bp is not None, "Could not create BP_SCLPlayerSamurai")
unreal.BlueprintEditorLibrary.compile_blueprint(player_bp)
player_cdo = unreal.get_default_object(player_bp.generated_class())
player_cdo.set_editor_property(
    "player_skeletal_mesh",
    unreal.load_asset("/Game/GhostSamurai_Bundle/Demo/Characters/Mannequins/Meshes/SKM_Manny"),
)
player_cdo.set_editor_property(
    "player_anim_class",
    unreal.load_class(None, "/Game/GhostSamurai_Bundle/Demo/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C"),
)
player_cdo.set_editor_property(
    "scabbard_asset",
    unreal.load_asset("/Game/GhostSamurai_Bundle/GhostSamurai/Weapon/Mesh/Katana/SM_Scabbard01"),
)
player_combo = player_cdo.get_player_combo_component()
require(player_combo is not None, "BP_SCLPlayerSamurai has no PlayerComboComponent")
player_combo.set_editor_property("player_moveset", moveset)
save(player_bp)

game_mode_bp = unreal.load_asset(GAME_MODE_BP_PATH)
if not game_mode_bp:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.SCLGameMode.static_class())
    game_mode_bp = asset_tools.create_asset("BP_SCLPlayerGameMode", GAME_MODE_BLUEPRINT_FOLDER, unreal.Blueprint, factory)
require(game_mode_bp is not None, "Could not create BP_SCLPlayerGameMode")
unreal.BlueprintEditorLibrary.compile_blueprint(game_mode_bp)
game_mode_cdo = unreal.get_default_object(game_mode_bp.generated_class())
game_mode_cdo.set_editor_property("player_character_class", player_bp.generated_class())
save(game_mode_bp)

unreal.log("PLAYER_COMBAT_ASSETS_ORGANIZED " + MOVES_ASSET_PATH)
