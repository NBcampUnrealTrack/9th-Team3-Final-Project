"""Narrow maintenance style pass; preserves artwork, padding, sizes and gameplay assets."""
import unreal

root = '/Game/Knight/UI/Menu/Maintenance/Hologram/'
for name in ('BS_Holo_Row', 'BS_Holo_Control', 'BS_Holo_MainTab', 'BS_Holo_Action', 'BS_Holo_Slot'):
    bp = unreal.load_asset(root + name)
    assert bp, name
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    # Keep the authored idle brush (including transparent text-button backgrounds).
    # Selection is now a separate persistent edge, input feedback a modest text/icon
    # change plus a neutral navigation outline. Never brighten the entire panel.
    idle = cdo.get_editor_property('normal_base')
    for field in ('normal_hovered', 'selected_base', 'selected_hovered'):
        cdo.set_editor_property(field, idle)
    cdo.set_editor_property('selected_pressed', cdo.get_editor_property('normal_pressed'))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    print('MAINTENANCE_STATE_STYLE', bp.get_path_name())

print('MAINTENANCE_STATE_STYLES_COMPLETE')
