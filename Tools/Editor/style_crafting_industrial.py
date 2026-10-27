"""Presentation-only setup. Reuse existing imported metal textures; never modify item data.
Run via PythonScriptCommandlet after the Development Editor build.
"""
import unreal
from pathlib import Path

DEST = '/Game/Knight/UI/Menu/Maintenance/Crafting'
def import_surface(name, source, maximum):
    asset = unreal.load_asset(DEST + '/' + name)
    if not asset:
        task = unreal.AssetImportTask()
        task.filename = str(Path(unreal.Paths.project_dir()).resolve() / 'SourceArt/UI/Crafting' / source)
        task.destination_path = DEST
        task.destination_name = name
        task.automated = True
        task.save = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset = unreal.load_asset(DEST + '/' + name)
    assert isinstance(asset, unreal.Texture2D)
    asset.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    asset.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    asset.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    asset.set_editor_property('srgb', True)
    asset.set_editor_property('never_stream', True)
    asset.set_editor_property('max_texture_size', maximum)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
    print('CRAFTING_TEXTURE_SAVED', asset.get_path_name(), 'max=', maximum)
    return asset

# Full-height artwork retains its native detail and aspect ratio. It is not nine-sliced.
panel = import_surface('T_Crafting_IntegratedSide', 'IntegratedSideMetal.png', 0)
button = import_surface('T_Crafting_RestrainedButton', 'RestrainedMetalButton.png', 1024)

def brush(tint):
    value = unreal.SlateBrush()
    value.resource_object = button
    value.draw_as = unreal.SlateBrushDrawType.BOX
    value.margin = unreal.Margin(.008, .025, .008, .025)
    value.tint_color = unreal.SlateColor(specified_color=unreal.LinearColor(*tint))
    return value

def make_style(name, source, padding, main_tab=False, row=False):
    path = DEST + '/' + name
    bp = unreal.load_asset(path) or unreal.EditorAssetLibrary.duplicate_asset(
        '/Game/Knight/UI/Style/' + source, path)
    assert bp, path
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    states = {
        'normal_base': (.45,.48,.52,1), 'normal_hovered': (.72,.79,.86,1),
        'normal_pressed': (.28,.33,.40,1), 'selected_base': (.45,.68,.95,1),
        'selected_hovered': (.65,.85,1,1), 'selected_pressed': (.30,.48,.70,1),
        'disabled': (.22,.24,.27,1)
    }
    if main_tab:
        states.update(selected_base=(.85,.62,.30,1), selected_hovered=(1,.80,.50,1),
                      selected_pressed=(.55,.40,.20,1))
    for field, tint in states.items():
        cdo.set_editor_property(field, brush(tint))
    cdo.set_editor_property('button_padding', unreal.Margin(*padding))
    cdo.set_editor_property('min_width', 0)
    cdo.set_editor_property('min_height', 72 if row else 48)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
    print('CRAFTING_STYLE_SAVED', path)
    return bp.generated_class()

controls = make_style('BS_Crafting_MetalControl', 'BS_Knight_CraftingControl', (8,10,8,10))
tabs = make_style('BS_Crafting_MetalMainTab', 'BS_Knight_CraftingMainTab', (10,12,10,12), main_tab=True)
rows = make_style('BS_Crafting_MetalRow', 'BS_Knight_CraftingRow', (14,10,14,10), row=True)
bp = unreal.load_asset('/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenance')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(bp.generated_class())
for field, value in {
    'crafting_panel_texture': panel, 'crafting_button_texture': button,
    'crafting_control_style': controls, 'crafting_main_tab_style': tabs, 'crafting_row_style': rows
}.items():
    cdo.set_editor_property(field, value)
    assert cdo.get_editor_property(field) == value
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
for path in ['/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenance',
             '/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenanceEntry',
             '/Game/Knight/UI/Common/WBP_KnightButton',
             '/Game/Knight/UI/Menu/WBP_KnightMainMenu']:
    widget = unreal.load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(widget)
    assert widget.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE, path
    print('CRAFTING_WBP_COMPILE_PASS', path)
print('CRAFTING_INDUSTRIAL_SETUP_COMPLETE')
