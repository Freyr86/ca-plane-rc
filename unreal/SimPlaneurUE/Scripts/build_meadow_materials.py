"""Build the GPU-animated scenery materials used on the slope."""
import unreal as u

lib = u.MaterialEditingLibrary
assets = u.EditorAssetLibrary

def material(name):
    path = "/Game/Materials/" + name
    mat = assets.load_asset(path) if assets.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(name, "/Game/Materials", u.Material, u.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    return mat

def node(cls):
    return lib.create_material_expression(mat, cls)

def bind(src, pin, prop):
    assert lib.connect_material_property(src, pin, prop)

def wave(code):
    expr = node(u.MaterialExpressionCustom)
    expr.set_editor_property("code", code)
    inputs = []
    sources = [("P", node(u.MaterialExpressionWorldPosition)), ("T", node(u.MaterialExpressionTime)), ("Col", node(u.MaterialExpressionVertexColor))]
    for name, source in sources:
        entry = u.CustomInput()
        entry.set_editor_property("input_name", name)
        inputs.append(entry)
    expr.set_editor_property("inputs", inputs)
    for name, source in sources:
        assert lib.connect_material_expressions(source, "", expr, name)
    expr.set_editor_property("output_type", u.CustomMaterialOutputType.CMOT_FLOAT3)
    return expr

def finish():
    lib.recompile_material(mat)
    assets.save_loaded_asset(mat)

mat = material("M_MeadowBlades")
bind(node(u.MaterialExpressionVertexColor), "", u.MaterialProperty.MP_BASE_COLOR)
bind(wave("return float3(-1,0.18,0)*max(0,Col.r-0.055)*12*(0.65*sin(P.x*0.012+T*2.4)+0.35*sin(P.y*0.02+T*1.6));"), "", u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
finish()
mat = material("M_ValleyLake")
mat.set_editor_property("used_with_instanced_static_meshes", True)
bind(wave("float a=sin(P.x*.035+P.y*.009+T*2.1); float b=cos(P.y*.047-P.x*.012+T*1.4); return normalize(float3(a*.12,b*.065,1));"), "", u.MaterialProperty.MP_NORMAL)
base = node(u.MaterialExpressionConstant3Vector)
base.constant = u.LinearColor(.017,.095,.13,1)
bind(base,"",u.MaterialProperty.MP_BASE_COLOR)
rough = node(u.MaterialExpressionConstant)
rough.r = .19
bind(rough,"",u.MaterialProperty.MP_ROUGHNESS)
finish()
