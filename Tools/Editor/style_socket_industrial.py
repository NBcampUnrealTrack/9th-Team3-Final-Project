"""Import presentation-only art and wire the existing Maintenance WBP.
Run with UE PythonScriptCommandlet after compiling knightEditor. No gameplay data edits.
"""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/Knight/UI/Menu/Maintenance/Sockets'


def texture(name, filename, maximum):
    path = DEST + '/' + name
    asset = unreal.load_asset(path)
    if not asset:
        task = unreal.AssetImportTask()
        task.filename = str(ROOT / 'SourceArt/UI/Sockets' / filename)
        task.destination_path = DEST
        task.destination_name = name
        task.automated = True
        task.save = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset = unreal.load_asset(path)
    assert isinstance(asset, unreal.Texture2D), path
    for field, value in {
        'lod_group': unreal.TextureGroup.TEXTUREGROUP_UI,
        'compression_settings': unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        'mip_gen_settings': unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        'srgb': True, 'never_stream': True, 'max_texture_size': maximum,
    }.items():
        asset.set_editor_property(field, value)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
    print('SOCKET_TEXTURE_SAVED', path, 'max=', maximum)
    return asset


background = texture('T_Socket_IndustrialCarrier', 'IndustrialCarrier.png', 0)
bay = texture('T_Socket_Bay', 'SocketBay.png', 512)
# Reuse the existing native-resolution tall surface; never alter Crafting's asset/settings.
side = unreal.load_asset('/Game/Knight/UI/Menu/Maintenance/Crafting/T_Crafting_IntegratedSide')
assert isinstance(side, unreal.Texture2D)
style_path = DEST + '/BS_Socket_IndustrialSlot'
style = unreal.load_asset(style_path) or unreal.EditorAssetLibrary.duplicate_asset(
    '/Game/Knight/UI/Style/BS_Knight_CraftingRow', style_path)
unreal.BlueprintEditorLibrary.compile_blueprint(style)
cdo = unreal.get_default_object(style.generated_class())
for field, tint in {
    'normal_base': (.38,.40,.43,1), 'normal_hovered': (.67,.72,.80,1),
    'normal_pressed': (.25,.28,.33,1), 'selected_base': (.92,.66,.30,1),
    'selected_hovered': (1,.83,.48,1), 'selected_pressed': (.60,.42,.18,1),
    'disabled': (.18,.20,.24,1),
}.items():
    brush = unreal.SlateBrush()
    brush.resource_object = bay
    # The controls are always square, so the entire raster retains its proportions.
    brush.draw_as = unreal.SlateBrushDrawType.IMAGE
    brush.tint_color = unreal.SlateColor(specified_color=unreal.LinearColor(*tint))
    cdo.set_editor_property(field, brush)
cdo.set_editor_property('button_padding', unreal.Margin(14,14,14,14))
cdo.set_editor_property('min_width', 0)
cdo.set_editor_property('min_height', 0)
unreal.BlueprintEditorLibrary.compile_blueprint(style)
assert style.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
assert unreal.EditorAssetLibrary.save_loaded_asset(style)

bp = unreal.load_asset('/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenance')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(bp.generated_class())
for field, value in {
    'industrial_side_panel_texture': side,
    'socket_background_texture': background,
    'socket_slot_style': style.generated_class(),
}.items():
    cdo.set_editor_property(field, value)
    assert cdo.get_editor_property(field) == value
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)

for path in [
    '/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenance',
    '/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenanceEntry',
    '/Game/Knight/UI/Menu/WBP_KnightMainMenu',
    '/Game/Knight/UI/Common/WBP_KnightButton',
]:
    widget = unreal.load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(widget)
    assert widget.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE, path
    print('INDUSTRIAL_WBP_COMPILE_PASS', path)
print('SOCKET_INDUSTRIAL_SETUP_COMPLETE')
