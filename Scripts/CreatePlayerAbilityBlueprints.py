"""UE Python：建立动作的配置蓝图，并替换玩家默认技能列表中的原生类。

C++ 负责判定与执行，技能蓝图 Class Defaults 负责动画引用和可调参数。
重复运行保留已有技能蓝图的设置，以及玩家自行选择的其他派生技能。
"""
import unreal

ROOT = "/Game/SoulCombatLab/Characters/Player/Combat/Abilities"
PLAYER = "/Game/SoulCombatLab/Blueprints/Player/BP_SCLPlayerSamurai"
CONFIGS = (
    ("GA_PlayerBlock", unreal.SCLBlockAbility),
    ("GA_PlayerParry", unreal.SCLParryAbility),
    ("GA_PlayerExecution", unreal.SCLExecutionAbility),
    ("GA_PlayerDodge", unreal.SCLDodgeAbility),
)


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


replacements = {}
for name, native in CONFIGS:
    blueprint = unreal.load_asset(ROOT + "/" + name)
    if not blueprint:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", native.static_class())
        blueprint = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, ROOT, unreal.Blueprint, factory), "无法创建技能配置蓝图：" + name)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(isinstance(unreal.get_default_object(blueprint.generated_class()), native),
            "技能配置蓝图父类不匹配：" + name)
    # 默认继承现有参数，已有配置不被脚本重新赋值；用户在 Class Defaults 自由修改。
    require(unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False),
            "无法保存技能蓝图：" + name)
    replacements[native.static_class()] = blueprint.generated_class()

player = require(unreal.load_asset(PLAYER), "缺少玩家配置蓝图")
unreal.BlueprintEditorLibrary.compile_blueprint(player)
defaults = unreal.get_default_object(player.generated_class())
configured = list(defaults.get_editor_property("startup_abilities"))
# 原位替换，不追加父类+子类两份同类动作；其他轻重攻击、躲闪和用户技能保留顺序。
configured = [replacements.get(ability, ability) for ability in configured]
for name, native in CONFIGS:
    matches = [ability for ability in configured if ability and
               isinstance(unreal.get_default_object(ability), native)]
    if len(matches) > 1:
        raise RuntimeError("同一动作配置了多个技能，请保留一个：" + name)
    if not matches:
        configured.append(replacements[native.static_class()])
defaults.set_editor_property("startup_abilities", configured)
require(unreal.EditorAssetLibrary.save_loaded_asset(player, only_if_is_dirty=False),
        "无法保存玩家默认技能列表")
unreal.log("SCL_ABILITY_BLUEPRINTS_CONFIGURED " + str([ability.get_path_name() for ability in configured]))
