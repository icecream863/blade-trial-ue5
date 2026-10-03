"""只读盘点战斗动画：骨架、长度、根运动和动画蓝图引用。"""
import unreal

paths = [
    "/Game/GhostSamurai_Bundle/GhostSamurai/Blueprints/ABP_GhostSamurai_Katana",
    "/Game/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Defense/GhostSamurai_APose2DefenseR_Root",
    "/Game/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Defense/DefenseR/GhostSamurai_DefenseR_Loop_Root",
    "/Game/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Defense/DefenseR/GhostSamurai_DefenseR_Parry01_Root",
    "/Game/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Execution/GhostSamurai_Execution01",
    "/Game/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Movement/Walk/GhostSamurai_APose_Strafe_Walk_B_Root",
    "/Game/GhostSamurai_Bundle/GhostSamurai/Katana/Common/GhostSamurai_Common_StrafeWalkB_Root",
    "/Game/SoulCombatLab/Characters/Player/Combat/Animations/AM_PlayerDraw",
]
for path in paths:
    asset = unreal.load_asset(path)
    if asset is None:
        unreal.log_warning("SCL_INSPECT MISSING " + path)
        continue
    values = ["SCL_INSPECT", path, "class=" + asset.get_class().get_name()]
    for prop in ("skeleton", "target_skeleton", "preview_skeletal_mesh", "enable_root_motion", "force_root_lock"):
        try:
            values.append(prop + "=" + str(asset.get_editor_property(prop)))
        except Exception:
            pass
    try:
        values.append("length=" + str(asset.get_play_length()))
    except Exception:
        pass
    unreal.log(" ".join(values))

bp = unreal.load_asset(paths[0])
if bp:
    for prop in ("function_graphs", "ubergraph_pages", "implemented_interfaces", "simple_construction_script"):
        try:
            value = bp.get_editor_property(prop)
            unreal.log("SCL_ABP " + prop + "=" + ",".join(x.get_name() for x in value))
        except Exception as exc:
            unreal.log_warning("SCL_ABP " + prop + " error=" + str(exc))
    for name in ("BlueprintEditorLibrary", "BlueprintGraphEditor", "AnimationBlueprintLibrary", "KismetEditorUtilities", "AnimBlueprint", "AnimGraphNode_BlendSpacePlayer", "AnimGraphNode_SequencePlayer", "BlendSpaceFactoryNew", "AnimBlueprintFactory"):
        cls = getattr(unreal, name, None)
        unreal.log("SCL_API " + name + "=" + str([item for item in dir(cls) if any(word in item.lower() for word in ("graph", "node", "blend", "skeleton", "compile", "anim"))])[:1800])
    unreal.log("SCL_BP_PROPERTIES " + str([item for item in dir(bp) if any(word in item.lower() for word in ("graph", "node", "generate", "editor", "anim"))])[:3000])
    for method in ("get_graph_editor", "get_graph_editor_by_name", "list_all_nodes", "list_available_nodes", "create_node_from_name", "remove_nodes"):
        unreal.log("SCL_DOC " + method + " " + str(getattr(unreal.BlueprintGraphEditor, method).__doc__)[:1500])
    unreal.log("SCL_PIN_API " + str([item for item in dir(unreal.BlueprintGraphPin) if not item.startswith("_")])[:3000])
    for name in ("BlendSpace", "BlendSpaceLibrary", "AnimationLibrary", "BlueprintGraphEditor"):
        cls = getattr(unreal, name, None)
        unreal.log("SCL_CLASS " + name + " " + str([item for item in dir(cls) if any(w in item.lower() for w in ("sample", "axis", "blend", "connect", "variable", "reparent"))])[:3000])
    for method in ("add_get_member_variable_node",):
        unreal.log("SCL_DOC " + method + " " + str(getattr(unreal.BlueprintGraphEditor, method).__doc__)[:1000])
    blend = unreal.load_asset("/Game/GhostSamurai_Bundle/Demo/Characters/Mannequins/Animations/Manny/BS_MM_WalkRun")
    if blend:
        for prop in ("blend_parameters", "sample_data"):
            try:
                unreal.log("SCL_BLEND_PROP " + prop + " " + str(blend.get_editor_property(prop))[:1500])
            except Exception as exc:
                unreal.log_warning("SCL_BLEND_PROP_ERROR " + prop + " " + str(exc))
    unreal.log("SCL_BLEND_NAMES " + str([n for n in dir(unreal) if "blend" in n.lower() and ("editor" in n.lower() or "library" in n.lower())])[:2500])
    try:
        unreal.log("SCL_GRAPHS " + str([(graph.get_name(), graph.get_class().get_name()) for graph in bp.get_animation_graphs()]))
        unreal.log("SCL_GRAPHS_ALL " + str(unreal.BlueprintEditorLibrary.list_graph_names(bp)))
        ge = unreal.BlueprintGraphEditor.get_graph_editor_by_name(bp, "AnimGraph")
        unreal.log("SCL_GE_NODES " + str([(node.get_name(), node.get_class().get_name()) for node in ge.list_all_nodes()])[:5000])
        unreal.log("SCL_GE_AVAILABLE " + str([name for name in ge.list_available_nodes([]) if any(term in name.lower() for term in ("blendspaceplayer", "blendspace", "blend list by bool", "blendlistbybool", "sequence player", "calculate direction"))][-100:])[:10000])
        slot = next(node for node in ge.list_all_nodes() if node.get_class().get_name() == "AnimGraphNode_Slot")
        unreal.log("SCL_GE_POSE_AVAILABLE " + str([name for name in ge.list_available_nodes([slot.find_input_pin("Source")]) if any(term in name.lower() for term in ("blend", "sequence", "space"))][-100:])[:10000])
        unreal.log("SCL_GE_ASSET_AVAILABLE " + str([name for name in ge.list_available_nodes([slot.find_input_pin("Source")]) if any(term in name.lower() for term in ("bs_mm", "bs_idle", "blend space player", "blendspace player", "blendspaceplayer", "ghostsamurai_apose"))][:70])[:10000])
        for node in ge.list_all_nodes():
            unreal.log("SCL_GE_PINS " + node.get_name() + " " + str([(p.get_pin_name(), p.get_pin_direction(), p.get_pin_value(), [q.get_owning_node().get_name() for q in p.list_connected_pins()]) for p in node.list_all_pins()])[:3500])
        for graph in unreal.BlueprintEditorLibrary.list_graphs(bp):
            try:
                nodes = graph.get_editor_property("nodes")
                unreal.log("SCL_GRAPH_NODES " + graph.get_name() + " " + str([(n.get_name(), n.get_class().get_name()) for n in nodes])[:8000])
            except Exception as exc:
                unreal.log_warning("SCL_GRAPH_NODES_ERROR " + graph.get_name() + " " + str(exc))
    except Exception as exc:
        unreal.log_warning("SCL_GRAPHS_ERROR " + str(exc))
    for class_name in ("AnimGraphNode_SequencePlayer", "AnimGraphNode_BlendSpacePlayer", "AnimGraphNode_StateMachine", "AnimGraphNode_Slot", "AnimGraphNode_BlendListByBool"):
        cls = getattr(unreal, class_name, None)
        if cls:
            try:
                nodes = bp.get_nodes_of_class(cls)
                unreal.log("SCL_NODES " + class_name + " " + str([n.get_name() for n in nodes])[:5000])
                for node in nodes[:12]:
                    try:
                        struct = node.get_editor_property("node")
                        unreal.log("SCL_NODE_DATA " + node.get_name() + " " + str(struct)[:600])
                        unreal.log("SCL_NODE_DIR " + node.get_name() + " " + str([x for x in dir(node) if any(w in x.lower() for w in ("pin", "asset", "sequence", "node"))])[:1500])
                        unreal.log("SCL_NODE_STRUCT_DIR " + node.get_name() + " " + str([x for x in dir(struct) if any(w in x.lower() for w in ("sequence", "asset", "loop", "blend"))])[:1500])
                        unreal.log("SCL_NODE_ASSET " + node.get_name() + " " + str([(p, str(struct.get_editor_property(p))[:250]) for p in ("sequence", "blend_space", "slot_name") if hasattr(struct, p)]))
                    except Exception as exc:
                        unreal.log_warning("SCL_NODE_DATA_ERROR " + str(exc))
            except Exception as exc:
                unreal.log_warning("SCL_NODES_ERROR " + class_name + " " + str(exc))
