"""UE 编辑器 Python：只改项目副本，把三个八方向 Blend Space 接到默认 Montage Slot。"""
import unreal


def build_initial_ground_graph():

    source = "/Game/GhostSamurai_Bundle/GhostSamurai/Blueprints/ABP_GhostSamurai_Katana"
    dest = "/Game/SoulCombatLab/Characters/Player/Combat/ABP_SCLPlayerCombat"
    bp = unreal.load_asset(dest)
    if not bp:
        bp = unreal.EditorAssetLibrary.duplicate_asset(source, dest)
    if not bp:
        raise RuntimeError("无法复制玩家 AnimBP")
    unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.SCLPlayerAnimInstance.static_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    graph = unreal.BlueprintGraphEditor.get_graph_editor_by_name(bp, "AnimGraph")
    nodes = graph.list_all_nodes()
    slot = next(n for n in nodes if n.get_class().get_name() == "AnimGraphNode_Slot")
    old = next((n for n in nodes if n.get_class().get_name() == "AnimGraphNode_SequencePlayer"), None)
    generated = [n for n in nodes if n.get_class().get_name() in
                 ("AnimGraphNode_BlendSpaceEvaluator", "AnimGraphNode_BlendSpacePlayer", "AnimGraphNode_BlendListByBool", "K2Node_VariableGet")]
    if generated:
        graph.remove_nodes(generated)

    def available(token):
        options = graph.list_available_nodes([slot.find_input_pin("Source")])
        matches = [x for x in options if token in x]
        if not matches:
            raise RuntimeError("节点菜单没有 " + token)
        return next((x for x in matches if "混合空间播放器" in x), matches[0])

    def pin(node, name):
        for candidate in node.list_all_pins():
            if str(candidate.get_pin_name()).lower() == name.lower():
                return candidate
        raise RuntimeError(node.get_name() + " 没有引脚 " + name + ": " +
                           str([str(p.get_pin_name()) for p in node.list_all_pins()]))

    players = {}
    for index, name in enumerate(("BS_PlayerArmed8Way", "BS_PlayerSheathed8Way", "BS_PlayerBlock8Way")):
        node = graph.create_node_from_name(available(name), unreal.Vector2D(-800, index * 240), [])
        if not node:
            raise RuntimeError("无法创建 " + name)
        players[name] = node
        unreal.log("SCL_GRAPH_PLAYER " + name + " " + node.get_class().get_name() + " " +
                   str([(str(p.get_pin_name()), str(p.get_pin_direction())) for p in node.list_all_pins()]))

    options = [x for x in graph.list_available_nodes([slot.find_input_pin("Source")])
               if ("Animation|Blends" in x or "动画|混合" in x) and
               ("bool" in x.lower() or "布尔" in x)]
    unreal.log("SCL_GRAPH_BOOL_OPTIONS " + str(options[:30]))
    bool_nodes = []
    for index in range(2):
        node = graph.create_node_from_name("Animation|Blends|按布尔混合姿势", unreal.Vector2D(-420, index * 220), [])
        bool_nodes.append(node)
        unreal.log("SCL_GRAPH_BOOL_NODE " + node.get_name() + " " + node.get_class().get_name() + " " +
                   str([(str(p.get_pin_name()), str(p.get_pin_direction())) for p in node.list_all_pins()]))
    variables = {}
    for name in ("LocalRightSpeed", "LocalForwardSpeed", "bWeaponSheathed", "bBlocking"):
        node = graph.add_get_member_variable_node(name)
        variables[name] = node
        unreal.log("SCL_GRAPH_GET " + name + " " + str(node) + " " +
                   str([(str(p.get_pin_name()), str(p.get_pin_direction())) for p in node.list_all_pins()]) if node else "NONE")
    def connect(output, input_pin):
        if not output.try_create_connection(input_pin):
            raise RuntimeError("连接失败：" + str(output.get_pin_name()) + " -> " + str(input_pin.get_pin_name()))

    for node in players.values():
        connect(pin(variables["LocalRightSpeed"], "LocalRightSpeed"), pin(node, "X"))
        connect(pin(variables["LocalForwardSpeed"], "LocalForwardSpeed"), pin(node, "Y"))

    base, blocking = bool_nodes
    # UE 的布尔姿势节点是 0=True、1=False，并非按 False/True 顺序排列。
    # 收刀为真选 Sheathed；未收刀选 Armed。接反会让普通移动进入错误姿势。
    connect(pin(players["BS_PlayerSheathed8Way"], "Pose"), pin(base, "BlendPose_0"))
    connect(pin(players["BS_PlayerArmed8Way"], "Pose"), pin(base, "BlendPose_1"))
    connect(pin(variables["bWeaponSheathed"], "bWeaponSheathed"), pin(base, "bActiveValue"))
    # 仅 bBlocking=True 才走防御八向；False 必须输出普通 Run，而不是举刀防御。
    connect(pin(players["BS_PlayerBlock8Way"], "Pose"), pin(blocking, "BlendPose_0"))
    connect(pin(base, "Pose"), pin(blocking, "BlendPose_1"))
    connect(pin(variables["bBlocking"], "bBlocking"), pin(blocking, "bActiveValue"))
    pin(slot, "Source").break_pin_links()
    connect(pin(blocking, "Pose"), pin(slot, "Source"))
    if old:
        graph.remove_nodes([old])
    for node in bool_nodes:
        pin(node, "BlendTime_0").set_pin_value("0.12")
        pin(node, "BlendTime_1").set_pin_value("0.12")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    errors = graph.list_nodes_with_errors()
    if errors:
        raise RuntimeError("AnimBP 图编译错误：" + str([node.get_name() for node in errors]))
    default = unreal.get_default_object(bp.generated_class())
    default.set_editor_property("root_motion_mode", unreal.RootMotionMode.IGNORE_ROOT_MOTION)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    unreal.log("SCL_GRAPH_READY " + bp.get_path_name() + " nodes=" + str(len(graph.list_all_nodes())))


# 跳跃接入后地面姿势已经缓存供状态机复用，重建旧图会破坏这些引用。
# 此入口只负责首次创建；后续改移动素材用 ConfigurePlayerLocomotion.py 或直接编辑 Blend Space。
existing = unreal.load_asset('/Game/SoulCombatLab/Characters/Player/Combat/ABP_SCLPlayerCombat')
if existing and 'PlayerLocomotion' in [str(x) for x in unreal.BlueprintEditorLibrary.list_graph_names(existing)]:
    unreal.log('SCL_GRAPH_PRESERVED PlayerLocomotion exists; edit existing locomotion assets directly')
else:
    build_initial_ground_graph()
