"""Create the lightweight colour-print post process used by the Alpine title screen."""
import unreal

folder = "/Game/Materials"
name = "M_MenuAlpinePrint"
assets = unreal.EditorAssetLibrary
edit = unreal.MaterialEditingLibrary
mat = assets.load_asset(folder + "/" + name) if assets.does_asset_exist(folder + "/" + name) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
edit.delete_all_material_expressions(mat)
mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
mat.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
scene = edit.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -350, 0)
scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
depth = edit.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -350, 170)
depth.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_SCENE_DEPTH)
effect = edit.create_material_expression(mat, unreal.MaterialExpressionCustom, 0, 0)
pin = unreal.CustomInput()
pin.set_editor_property("input_name", "Scene")
depth_pin = unreal.CustomInput()
depth_pin.set_editor_property("input_name", "Depth")
effect.set_editor_property("inputs", [pin, depth_pin])
effect.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
effect.set_editor_property("code", """
float3 c = saturate(Scene.rgb);
float l = dot(c,float3(.24,.61,.15));
float tone = floor(12*l+.5)/12;
float sky = smoothstep(.00,.16,c.b-c.r);
float3 ink = float3(.018,.115,.12), pine=float3(.16,.36,.27);
float3 straw=float3(.86,.70,.40), paper=float3(.99,.88,.65);
float3 land = lerp(ink,pine,saturate(tone*3.8));
land=lerp(land,straw,smoothstep(.19,.65,tone));
land=lerp(land,paper,smoothstep(.63,.95,tone));
float3 air=lerp(float3(.016,.18,.22),float3(.34,.70,.69),smoothstep(.10,.64,tone));
air=lerp(air,paper,smoothstep(.61,.92,tone));
return lerp(c,lerp(land,air,sky),.83*smoothstep(1450.,2300.,Depth.r));
""")
assert edit.connect_material_expressions(scene, "Color", effect, "Scene")
assert edit.connect_material_expressions(depth, "Color", effect, "Depth")
assert edit.connect_material_property(effect, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.recompile_material(mat)
assert assets.save_loaded_asset(mat, False)
unreal.log("ALPINE_PRINT_READY")
