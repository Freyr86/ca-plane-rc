"""Add an opaque geometry fade to living grass without rebuilding other assets."""
import unreal as u


def apply_grass_fade(mat):
    edit = u.MaterialEditingLibrary
    marker = "Grass distance fade"
    expressions = list(edit.get_material_expressions(mat))
    if any(expr.get_editor_property("desc") == marker for expr in expressions):
        return
    wind = edit.get_material_property_input_node(mat, u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    assert wind is not None, "Living grass must have its wind displacement"
    local = next(expr for expr in expressions if isinstance(expr, u.MaterialExpressionPreSkinnedPosition))
    # Use the individual tuft's rotation and scale, including sloping ground.
    extent = edit.create_material_expression(mat, u.MaterialExpressionTransform)
    extent.set_editor_property("transform_source_type", u.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_INSTANCE)
    extent.set_editor_property("transform_type", u.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    assert edit.connect_material_expressions(local, "", extent, "")
    fade = edit.create_material_expression(mat, u.MaterialExpressionPerInstanceFadeAmount)
    blend = edit.create_material_expression(mat, u.MaterialExpressionCustom)
    blend.set_editor_property("desc", marker)
    blend.set_editor_property("output_type", u.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in ("Wind", "Extent", "Fade"):
        entry = u.CustomInput()
        entry.set_editor_property("input_name", name)
        inputs.append(entry)
    blend.set_editor_property("inputs", inputs)
    # Shrink each opaque tuft into its root before culling; avoid transparency
    # and its extra overdraw across the wider field of grass.
    blend.set_editor_property("code", "float f = smoothstep(0.0, 1.0, saturate(Fade)); return Wind*f + Extent*(f-1.0);")
    for source, name in ((wind, "Wind"), (extent, "Extent"), (fade, "Fade")):
        assert edit.connect_material_expressions(source, "", blend, name)
    assert edit.connect_material_property(blend, "", u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    edit.recompile_material(mat)
    assert u.EditorAssetLibrary.save_loaded_asset(mat, False)
    u.log("Meadow: smooth grass distance fade saved")


if __name__ == "__main__":
    apply_grass_fade(u.EditorAssetLibrary.load_asset("/Game/Meadow/M_LivingGrass"))
