"""Builds /Game/Knight/UI/Menu/M_UI_StageMap around the Custom node in Tools/UI/Shaders/M_UI_StageMap.hlsl.

Run in the editor (Tools > Execute Python Script) or with the pythonscript commandlet. When the material already exists
only the HLSL is replaced, so it is safe to run again after editing the shader.
"""
import os

import unreal

FOLDER = "/Game/Knight/UI/Menu"
NAME = "M_UI_StageMap"
LAND = "/Game/Knight/UI/Menu/Textures/T_UI_GlobeLand"
HLSL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Shaders", "M_UI_StageMap.hlsl")
REGIONS = 8

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
with open(HLSL, encoding="utf-8") as f:
    code = f.read()

path = FOLDER + "/" + NAME
if eal.does_asset_exist(path):
    material = unreal.load_asset(path)
    outer = material.get_outermost()
    custom = next(c for c in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if c.get_outermost() == outer)
    custom.set_editor_property("code", code)
else:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    custom = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -400, 0)
    custom.set_editor_property("code", code)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    names = ["UV", "ViewRect", "MapPixels", "Tex", "Blink"] + ["R%d" % i for i in range(REGIONS)]
    inputs = []
    for name in names:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name)
        inputs.append(entry)
    custom.set_editor_property("inputs", inputs)

    def vector(name, y, group=""):
        param = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -800, y)
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 0))
        if group:
            param.set_editor_property("group", group)
        return param

    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -800, -300)
    mel.connect_material_expressions(uv, "", custom, "UV")
    # RGBA, not the default RGB output: every channel carries data.
    view = vector("ViewRect", -200)
    view.set_editor_property("default_value", unreal.LinearColor(0.2, 0.2, 0.6, 0.6))
    mel.connect_material_expressions(view, "RGBA", custom, "ViewRect")
    pixels = vector("MapPixels", -100)
    pixels.set_editor_property("default_value", unreal.LinearColor(2048, 1024, 0, 0))
    mel.connect_material_expressions(pixels, "RGBA", custom, "MapPixels")
    tex = mel.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -800, 0)
    tex.set_editor_property("parameter_name", "LandTex")
    tex.set_editor_property("texture", unreal.load_asset(LAND))
    tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    mel.connect_material_expressions(tex, "", custom, "Tex")
    blink = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -800, 120)
    blink.set_editor_property("parameter_name", "Blink")
    mel.connect_material_expressions(blink, "", custom, "Blink")
    for index in range(REGIONS):
        region = vector("Region%d" % index, 220 + index * 70, "Regions")
        mel.connect_material_expressions(region, "RGBA", custom, "R%d" % index)

    rgb = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -150, -40)
    for channel, on in (("r", True), ("g", True), ("b", True), ("a", False)):
        rgb.set_editor_property(channel, on)
    mel.connect_material_expressions(custom, "", rgb, "")
    alpha = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -150, 60)
    for channel, on in (("r", False), ("g", False), ("b", False), ("a", True)):
        alpha.set_editor_property(channel, on)
    mel.connect_material_expressions(custom, "", alpha, "")
    mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)

mel.recompile_material(material)
eal.save_loaded_asset(material, False)
unreal.log_warning("STAGEMAP| material saved")
