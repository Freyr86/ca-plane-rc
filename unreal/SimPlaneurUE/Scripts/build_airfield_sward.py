"""Build the mown strip on the local terrain patch, retaining the meadow outside it."""
import unreal as u

assets = u.EditorAssetLibrary
edit = u.MaterialEditingLibrary
path = "/Game/Meadow/M_AirfieldSward"
mat = (assets.load_asset(path) if assets.does_asset_exist(path)
       else assets.duplicate_asset("/Game/Meadow/M_AlpineSward", path))
assert mat
prop = u.MaterialProperty.MP_BASE_COLOR
base = edit.get_material_property_input_node(mat, prop)
if base.get_editor_property("desc") == "Mown airfield":
    blend = base
else:
    uv = edit.create_material_expression(mat, u.MaterialExpressionTextureCoordinate)
    blend = edit.create_material_expression(mat, u.MaterialExpressionCustom)
    blend.set_editor_property("desc", "Mown airfield")
    blend.set_editor_property("output_type", u.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in ("Meadow", "Field"):
        entry = u.CustomInput()
        entry.set_editor_property("input_name", name)
        inputs.append(entry)
    blend.set_editor_property("inputs", inputs)
    assert edit.connect_material_expressions(base, "", blend, "Meadow")
    assert edit.connect_material_expressions(uv, "", blend, "Field")
    assert edit.connect_material_property(blend, "", prop)

# Field runs from -1 to 1 at each edge, supplied by the terrain mesh. Signed edge
# distances here are in fractions of each dimension, including the chalk corners.
blend.set_editor_property("code", """
float2 edge = 1.0 - abs(Field);
float mask = smoothstep(-.003,.003,edge.x) * smoothstep(-.018,.018,edge.y);
float stripes = smoothstep(-.25,.25,cos(Field.y * 21.99115));
float detail = clamp(dot(Meadow,float3(.25,.65,.1))*3.3+.75,.72,1.28);
float3 cut = lerp(float3(.071,.153,.043),float3(.083,.175,.055),stripes)*detail;
cut = lerp(cut, Meadow, .28);
float corner = max((1.-smoothstep(.002,.005,edge.x))*(1.-smoothstep(.19,.22,edge.y)),
                   (1.-smoothstep(.015,.04,edge.y))*(1.-smoothstep(.033,.036,edge.x)));
return lerp(Meadow,lerp(cut,float3(.65,.68,.50)*detail,corner*.77),mask);
""")
edit.recompile_material(mat)
assert assets.save_loaded_asset(mat, False)
u.log("AIRFIELD_SWARD_SAVED")
