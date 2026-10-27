"""Import text-free orange hologram art and wire only Maintenance presentation.

Run with PythonScriptCommandlet. Does not touch progression, balances or saves.
Source originals, prompts and usage are in SourceArt/UI/Hologram/README.md.
"""
from pathlib import Path
import json
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/Knight/UI/Menu/Maintenance/Hologram'
LIB = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def save(bp):
    if isinstance(bp, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE
    assert LIB.save_loaded_asset(bp)


def texture(name, maximum):
    path = DEST + '/T_Holo_' + name
    asset = unreal.load_asset(path)
    if not asset:
        task = unreal.AssetImportTask()
        task.filename = str(ROOT / 'SourceArt/UI/Hologram' / (name + '.png'))
        task.destination_path = DEST
        task.destination_name = 'T_Holo_' + name
        task.automated = True
        task.save = True
        TOOLS.import_asset_tasks([task])
        asset = unreal.load_asset(path)
    assert isinstance(asset, unreal.Texture2D), path
    for field, value in dict(lod_group=unreal.TextureGroup.TEXTUREGROUP_UI,
        compression_settings=unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        srgb=True, never_stream=True, max_texture_size=maximum).items():
        asset.set_editor_property(field, value)
    save(asset)
    print('HOLO_TEXTURE', path, 'max_size', maximum)
    return asset


panel = texture('Panel', 0)
control = texture('Control', 512)
enhancement = texture('Enhancement', 0)
sockets = texture('Sockets', 0)
slot = texture('Slot', 512)


def duplicate(name, source):
    path = DEST + '/' + name
    bp = unreal.load_asset(path) or LIB.duplicate_asset(source, path)
    assert bp, path
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return bp, unreal.get_default_object(bp.generated_class())


def color(rgb):
    # Match the shared C++ palette: author sRGB, store linear Slate colors.
    def linear(v):
        v /= 255.0
        return v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4
    return unreal.LinearColor(*(linear(v) for v in rgb), 1)


def text_style(name, source, rgb):
    bp, cdo = duplicate(name, source)
    cdo.set_editor_property('color', color(rgb))
    save(bp)
    return bp.generated_class()


text_style('TS_Holo_Body', '/Game/Knight/UI/Style/TS_Knight_Body', (244,244,242))


def brush(texture_asset, tint, square=False):
    b = unreal.SlateBrush()
    b.resource_object = texture_asset
    b.draw_as = unreal.SlateBrushDrawType.IMAGE if square else unreal.SlateBrushDrawType.BOX
    if not square:
        b.margin = unreal.Margin(.055,.12,.055,.12)
        # Preserve the high-resolution original; crop transparent export padding in UV space.
        b.import_text('(UVRegion=(Min=(X=0.012,Y=0.14),Max=(X=0.988,Y=0.84),bIsValid=True))')
    b.tint_color = unreal.SlateColor(specified_color=unreal.LinearColor(*tint))
    return b


def style(name, source, padding, height, square=False):
    bp, cdo = duplicate(name, '/Game/Knight/UI/Style/' + source)
    # CommonUI drives these states. No click handler ever latches a tint.
    states = dict(normal_base=(.23,.23,.23,.90), normal_hovered=(.52,.52,.52,1),
        normal_pressed=(.16,.16,.16,1), selected_base=(.90,.90,.90,1),
        selected_hovered=(1,1,1,1), selected_pressed=(.56,.56,.56,1),
        disabled=(.08,.08,.08,.60))
    for field, tint in states.items():
        cdo.set_editor_property(field, brush(slot if square else control, tint, square))
    # Preserve each existing font/size while removing legacy blue text from this local theme.
    for field, rgb in dict(normal_text_style=(244,244,242),
        normal_hovered_text_style=(255,235,215), selected_text_style=(255,187,94),
        selected_hovered_text_style=(255,215,160), disabled_text_style=(116,119,123)).items():
        old = cdo.get_editor_property(field)
        if old:
            source_asset = old.get_path_name().split('.')[0]
            cdo.set_editor_property(field, text_style('TS_' + name + '_' + field, source_asset, rgb))
    cdo.set_editor_property('button_padding', unreal.Margin(*padding))
    cdo.set_editor_property('min_width', 0)
    cdo.set_editor_property('min_height', height)
    save(bp)
    print('HOLO_STYLE', bp.get_path_name())
    return bp.generated_class()


controls = style('BS_Holo_Control', 'BS_Knight_CraftingControl', (8,10,8,10), 48)
tabs = style('BS_Holo_MainTab', 'BS_Knight_CraftingMainTab', (10,12,10,12), 48)
rows = style('BS_Holo_Row', 'BS_Knight_CraftingRow', (14,10,14,10), 72)
actions = style('BS_Holo_Action', 'BS_Knight_CraftingControl', (18,12,18,12), 62)
slots = style('BS_Holo_Slot', 'BS_Knight_CraftingRow', (14,14,14,14), 0, True)

bp = unreal.load_asset('/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenance')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(bp.generated_class())
bindings = dict(crafting_panel_texture=panel, crafting_button_texture=control,
    crafting_control_style=controls, crafting_main_tab_style=tabs, crafting_row_style=rows,
    industrial_side_panel_texture=panel, enhancement_panel_texture=panel,
    enhancement_background_texture=enhancement, enhancement_button_texture=control,
    enhancement_action_style=actions, enhancement_row_style=rows,
    socket_background_texture=sockets, socket_slot_style=slots)
for field, value in bindings.items():
    cdo.set_editor_property(field, value)
save(bp)
for field, value in bindings.items():
    assert cdo.get_editor_property(field) == value
    print('HOLO_BINDING', field, value.get_path_name())

for path in ['/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenance',
    '/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenanceEntry',
    '/Game/Knight/UI/Common/WBP_KnightButton', '/Game/Knight/UI/Menu/WBP_KnightMainMenu']:
    widget = unreal.load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(widget)
    assert widget.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE, path
    print('HOLO_WBP_COMPILE_PASS', path)

print('HOLO_SETUP_COMPLETE')
