"""Scoped blue variants of the existing common button style; preserves fonts, dimensions and sounds."""
import unreal

eal = unreal.EditorAssetLibrary
folder = "/Game/Knight/UI/Style/"

def variant(source, name):
    path = folder + name
    return unreal.load_asset(path) if eal.does_asset_exist(path) else eal.duplicate_asset(folder + source, path)

def linear(rgb):
    return unreal.LinearColor(*[(v / 255.0 / 12.92 if v / 255.0 <= .04045 else ((v / 255.0 + .055) / 1.055) ** 2.4) for v in rgb], 1)

text = variant("TS_Knight_ButtonSelected", "TS_Knight_OperationMapSelected")
text_default = unreal.get_default_object(text.generated_class())
text_default.set_editor_property("color", linear((119, 197, 255)))
unreal.BlueprintEditorLibrary.compile_blueprint(text)
eal.save_loaded_asset(text, False)

style = variant("BS_Knight_Primary", "BS_Knight_OperationMap")
default = unreal.get_default_object(style.generated_class())
source_default = unreal.get_default_object(unreal.load_asset(folder + "BS_Knight_Primary").generated_class())
for dest, source in (("selected_base", "normal_hovered"), ("selected_hovered", "normal_hovered"), ("selected_pressed", "normal_pressed")):
    brush = source_default.get_editor_property(source)
    outline = brush.get_editor_property("outline_settings")
    outline.set_editor_property("color", unreal.SlateColor(specified_color=linear((119, 197, 255))))
    brush.set_editor_property("outline_settings", outline)
    default.set_editor_property(dest, brush)
for prop in ("selected_text_style", "selected_hovered_text_style"):
    default.set_editor_property(prop, text.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(style)
eal.save_loaded_asset(style, False)
unreal.log("OPMAP| scoped blue style saved; original common style untouched")
