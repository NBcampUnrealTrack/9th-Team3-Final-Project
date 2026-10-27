"""Run with UE PythonScriptCommandlet after compiling the Enhancement presentation fields.
Imports project-owned raster source art and saves only the Enhancement assets/Danjeol.
"""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/Knight/UI/Menu/Maintenance/Enhancement'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

def texture(filename, name):
    path = DEST + '/' + name
    asset = unreal.load_asset(path)
    if not asset:
        task = unreal.AssetImportTask()
        task.filename = str(ROOT / 'SourceArt/UI/Enhancement' / filename)
        task.destination_path = DEST
        task.destination_name = name
        task.automated = True
        task.save = True
        task.replace_existing = False
        TOOLS.import_asset_tasks([task])
        asset = unreal.load_asset(path)
    assert isinstance(asset, unreal.Texture2D), path
    asset.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    asset.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    asset.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    asset.set_editor_property('srgb', True)
    asset.set_editor_property('never_stream', True)
    # Slate Box corners use the cooked resource size, not Brush.ImageSize.
    # Compact resources keep 12% corners ~31px instead of hundreds of pixels.
    asset.set_editor_property('max_texture_size', 256 if name != 'T_Enhancement_IndustrialBackdrop' else 0)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
    return asset

background = texture('IndustrialBackdrop.png', 'T_Enhancement_IndustrialBackdrop')
panel = texture('MetalPanel.png', 'T_Enhancement_MetalPanel')
button = texture('MetalButton.png', 'T_Enhancement_MetalButton')

def brush(tint):
    value = unreal.SlateBrush()
    value.resource_object = button
    value.draw_as = unreal.SlateBrushDrawType.BOX
    size = value.image_size
    size.import_text('(X=240,Y=80)')
    value.image_size = size
    value.margin = unreal.Margin(.12, .12, .12, .12)
    value.tint_color = unreal.SlateColor(specified_color=unreal.LinearColor(*tint))
    return value

def style(name, source):
    path = DEST + '/' + name
    bp = unreal.load_asset(path)
    if not bp:
        bp = unreal.EditorAssetLibrary.duplicate_asset('/Game/Knight/UI/Style/' + source, path)
    assert bp
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    # Interaction colors are driven by CommonUI states, never latched on click.
    for field, tint in {
        'normal_base': (.45,.48,.52,1), 'normal_hovered': (.7,.75,.8,1),
        'normal_pressed': (.3,.34,.4,1), 'selected_base': (.85,.62,.3,1),
        'selected_hovered': (1,.8,.5,1), 'selected_pressed': (.55,.4,.2,1),
        'disabled': (.22,.24,.27,1)
    }.items():
        cdo.set_editor_property(field, brush(tint))
    cdo.set_editor_property('button_padding', unreal.Margin(36,14,36,14))
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    print('ENHANCEMENT_STYLE_SAVED', path)
    return bp.generated_class()

action_style = style('BS_Enhancement_Action', 'BS_Knight_CraftingControl')
row_style = style('BS_Enhancement_Row', 'BS_Knight_CraftingRow')
bp = unreal.load_asset('/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenance')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
cdo = unreal.get_default_object(bp.generated_class())
for field, value in {
    'enhancement_background_texture': background, 'enhancement_panel_texture': panel,
    'enhancement_button_texture': button, 'enhancement_action_style': action_style,
    'enhancement_row_style': row_style
}.items():
    cdo.set_editor_property(field, value)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)

weapon = unreal.load_asset('/Game/Knight/Character/Knights/Weapons/Danjeol/DA_Weapon_Danjeol')
template = unreal.load_asset('/Game/Knight/Data/Equipment/Janyeol/DA_Equipment_Janyeol_Weapon')
steps = template.get_editor_property('upgrade_steps')
assert len(steps) == 15
# The starter had no upgrade table. Adopt the already-shipped early-weapon rules,
# including the real Janyeol reinforced-bone/essence gates; do not alter the template.
if not weapon.get_editor_property('upgrade_steps'):
    weapon.set_editor_property('upgrade_steps', [step.copy() for step in steps])
if not str(weapon.get_editor_property('display_name')):
    weapon.set_editor_property('display_name', unreal.Text('단절'))
assert unreal.EditorAssetLibrary.save_loaded_asset(weapon)

for path in ['/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenanceEntry', '/Game/Knight/UI/Common/WBP_KnightButton']:
    widget = unreal.load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(widget)
    assert widget.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE, path
    # Validation only: shared widgets have not been edited and must not be resaved.
    print('SHARED_WBP_COMPILE_PASS', path)
print('ENHANCEMENT_IMPORT_COMPLETE')
