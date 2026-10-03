"""Create project Montage copies from the bundle's two authored katana families.

Run inside the UE editor Python environment. The original montages and their
Niagara notifies stay untouched; these copies add SoulCombatLab damage and
combo windows on the bundle's own skeleton.
"""

import unreal


SOURCE = "/Game/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Attack/GhostSamurai_APose_"
DESTINATION = "/Game/SoulCombatLab/Characters/Player/Combat/Animations"
TRACE_STATE = unreal.ANS_SCLWeaponTrace.static_class()
COMBO_STATE = unreal.ANS_SCLComboWindow.static_class()

if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
    unreal.EditorAssetLibrary.make_directory(DESTINATION)


def source_name(family, step):
    return "Attack{:02d}_{}_{}Root".format(family, step, "ALL_" if step == 1 else "")


def slash_events(montage):
    events = []
    for event in unreal.AnimationLibrary.get_animation_notify_events(montage):
        notify = event.get_editor_property("notify")
        if not notify or notify.get_class().get_name() != "AnimNotify_PlayNiagaraEffect":
            continue
        system = notify.get_editor_property("template")
        if not system:
            continue
        events.append((unreal.AnimationLibrary.get_anim_notify_event_trigger_time(event), system.get_name()))
    return sorted(events)


for family, count in ((1, 4), (2, 6)):
    for step in range(1, count + 1):
        stem = source_name(family, step)
        source = SOURCE + stem + "_Montage"
        target = DESTINATION + "/AM_GSN_" + stem
        montage = unreal.load_asset(target)
        if not montage:
            montage = unreal.EditorAssetLibrary.duplicate_asset(source, target)
        if not montage:
            raise RuntimeError("Could not duplicate " + source)

        event_names = [str(e.get_editor_property("notify_name")) for e in unreal.AnimationLibrary.get_animation_notify_events(montage)]
        existing_trace_count = event_names.count("SCL Weapon Trace")
        existing_combo_count = event_names.count("SCL Combo Window")
        if existing_trace_count or existing_combo_count:
            if existing_trace_count < 1 or existing_combo_count != (step < count):
                raise RuntimeError("Incomplete existing notify setup: " + target)
            unreal.log("NATIVE_COMBO_ALREADY_PRESENT {}".format(target))
            continue

        events = slash_events(montage)
        damage_times = [t for t, system in events if system not in ("NS_Slash_Trail_01", "NS_Slash_Hit_L")]
        if not damage_times:
            raise RuntimeError("No slash effects to place trace windows: " + stem)
        unreal.AnimationLibrary.add_animation_notify_track(montage, "SCL_Trace")
        if step < count:
            unreal.AnimationLibrary.add_animation_notify_track(montage, "SCL_Combo")

        for index, hit in enumerate(damage_times):
            start = max(0.0, hit - 0.08)
            end = min(montage.get_play_length() - 0.05, hit + 0.23)
            if index + 1 < len(damage_times):
                next_start = max(0.0, damage_times[index + 1] - 0.08)
                end = min(end, next_start - 0.01)
            if end > start:
                unreal.AnimationLibrary.add_animation_notify_state_event(montage, "SCL_Trace", start, end - start, TRACE_STATE)

        if step < count:
            last_effect_time = max(t for t, _ in events)
            combo_start = min(last_effect_time + 0.16, montage.get_play_length() - 0.45)
            combo_end = min(montage.get_play_length() - 0.08, combo_start + 0.55)
            unreal.AnimationLibrary.add_animation_notify_state_event(
                montage, "SCL_Combo", combo_start, combo_end - combo_start, COMBO_STATE
            )

        if not unreal.EditorAssetLibrary.save_loaded_asset(montage):
            raise RuntimeError("Could not save " + target)
        unreal.log("NATIVE_COMBO_CREATED {} family={} step={} effects={} traces={}".format(
            target, family, step, events, len(damage_times)
        ))
