"""Small, texture-free precipitation materials; cloud banks use native local fog."""
import unreal as u

assets = u.EditorAssetLibrary
edit = u.MaterialEditingLibrary
folder = "/Game/Meadow/Weather"
assets.make_directory(folder)


def make(name, lightning=False):
    path = folder + "/" + name
    mat = assets.load_asset(path) if assets.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, u.Material, u.MaterialFactoryNew())
    # Existing weather materials may already be referenced by a class default
    # object. Reuse their nodes: deleting rooted expressions asserts in UE 5.8.
    expressions = list(edit.get_material_expressions(mat))
    def node(cls, x, y, label):
        tag = "Weather:" + label
        result = next((e for e in expressions if isinstance(e, cls) and e.get_editor_property("desc") == tag), None)
        if result is None and label in ("Color", "UV", "Brightness", "Glow"):
            result = next((e for e in expressions if isinstance(e, cls) and not str(e.get_editor_property("desc")).startswith("Weather:")), None)
        if result is None:
            result = edit.create_material_expression(mat, cls, x, y)
            expressions.append(result)
        result.set_editor_property("desc", tag)
        return result

    def connect(source, target, pin="", output=""):
        assert edit.connect_material_expressions(source, output, target, pin)

    mat.set_editor_property("blend_mode", u.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    # Preserve the emissive core of the arc through vertex fog attenuation.
    # Rain keeps normal atmospheric attenuation.
    mat.set_editor_property("use_translucency_vertex_fog", not lightning)
    mat.set_editor_property("apply_cloud_fogging", False)
    color = node(u.MaterialExpressionVertexColor, -900, 0, "Color")
    uv = node(u.MaterialExpressionTextureCoordinate, -1600, 300, "UV")
    brightness = node(u.MaterialExpressionScalarParameter, -900, 140, "Brightness")
    brightness.set_editor_property("parameter_name", "Brightness")
    brightness.set_editor_property("default_value", 10000. if lightning else 1500.)
    glow = node(u.MaterialExpressionMultiply, -150, 0, "Glow")
    connect(color, glow, "A")
    connect(brightness, glow, "B")
    assert edit.connect_material_property(glow, "", u.MaterialProperty.MP_EMISSIVE_COLOR)

    # Built-in nodes avoid resetting Custom-expression pins while an existing
    # material is referenced by a default object. Every node has a stable tag,
    # so re-running this script does not grow or delete the material graph.
    def feather(channel, y):
        mask = node(u.MaterialExpressionComponentMask, -1400, y, channel + "Mask")
        for c in ("r", "g", "b", "a"):
            mask.set_editor_property(c, c == channel)
        twice = node(u.MaterialExpressionMultiply, -1200, y, channel + "Twice")
        twice.set_editor_property("const_b", 2.)
        shift = node(u.MaterialExpressionSubtract, -1000, y, channel + "Shift")
        shift.set_editor_property("const_b", 1.)
        absolute = node(u.MaterialExpressionAbs, -800, y, channel + "Abs")
        inverse = node(u.MaterialExpressionOneMinus, -600, y, channel + "Inverse")
        bounded = node(u.MaterialExpressionSaturate, -400, y, channel + "Bounded")
        connect(uv, mask)
        connect(mask, twice, "A")
        connect(twice, shift, "A")
        connect(shift, absolute)
        connect(absolute, inverse)
        connect(inverse, bounded)
        return bounded

    shape = feather("r", 300)
    if not lightning:
        length = feather("g", 550)
        shape_product = node(u.MaterialExpressionMultiply, -200, 450, "Shape")
        connect(shape, shape_product, "A")
        connect(length, shape_product, "B")
        shape = shape_product
    opacity = node(u.MaterialExpressionMultiply, 0, 250, "Opacity")
    connect(shape, opacity, "A")
    connect(color, opacity, "B", "A")
    assert edit.connect_material_property(opacity, "", u.MaterialProperty.MP_OPACITY)
    edit.recompile_material(mat)
    assert assets.save_loaded_asset(mat, False), path
    u.log("WEATHER_MATERIAL_READY " + path)


make("M_RainStreak")
make("M_Lightning", True)
