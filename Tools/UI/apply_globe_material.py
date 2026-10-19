"""Puts Tools/UI/Shaders/M_UI_Globe.hlsl into the Custom node of /Game/Knight/UI/Menu/M_UI_Globe and wires Region0..7.

Run in the editor (Tools > Execute Python Script) or with the pythonscript commandlet. Safe to run again.
"""
import os

import unreal

MATERIAL = "/Game/Knight/UI/Menu/M_UI_Globe"
HLSL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Shaders", "M_UI_Globe.hlsl")
REGIONS = 8

mel = unreal.MaterialEditingLibrary
material = unreal.load_asset(MATERIAL)
outer = material.get_outermost()
custom = next(c for c in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if c.get_outermost() == outer)
existing = {c.get_editor_property("parameter_name"): c for c in unreal.ObjectIterator(unreal.MaterialExpressionVectorParameter)
            if c.get_outermost() == outer}

with open(HLSL, encoding="utf-8") as f:
    custom.set_editor_property("code", f.read())

inputs = list(custom.get_editor_property("inputs"))
names = [str(i.get_editor_property("input_name")) for i in inputs]
for index in range(REGIONS):
    if "R%d" % index not in names:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", "R%d" % index)
        inputs.append(entry)
custom.set_editor_property("inputs", inputs)

x = custom.get_editor_property("material_expression_editor_x") - 320
y = custom.get_editor_property("material_expression_editor_y") + 260
for index in range(REGIONS):
    name = "Region%d" % index
    param = existing.get(name)
    if not param:
        param = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y + index * 70)
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 0))
        param.set_editor_property("group", "Regions")
    # RGBA, not the default RGB output: the region state rides in alpha.
    mel.connect_material_expressions(param, "RGBA", custom, "R%d" % index)

mel.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material, False)
unreal.log_warning("GLOBE| material updated")
