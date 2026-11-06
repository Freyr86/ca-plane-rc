"""Prepare the photo-based landscape materials and instanced plants in Unreal."""
from pathlib import Path
import unreal as u

root = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())) / "SourceAssets" / "Meadow"
folder = "/Game/Meadow"
assets = u.EditorAssetLibrary
tools = u.AssetToolsHelpers.get_asset_tools()
edit = u.MaterialEditingLibrary
assets.make_directory(folder)

def import_asset(file, name, cls):
    path = folder + "/" + name
    if cls == u.StaticMesh:
        path = path + "/StaticMeshes/" + name
    if assets.does_asset_exist(path):
        return assets.load_asset(path)
    task = u.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = str(root / file), folder, name
    task.automated = task.save = True
    tools.import_asset_tasks([task])
    for dest in task.imported_object_paths:
        obj = assets.load_asset(dest)
        if isinstance(obj, cls):
            if obj.get_name() != name:
                assert assets.rename_asset(dest, path)
            return obj
    raise RuntimeError(str((file, task.imported_object_paths)))

def material(name):
    path = folder + "/" + name
    mat = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name, folder, u.Material, u.MaterialFactoryNew())
    # UE 5.8's bulk delete mutates the expression list while iterating over it.
    for expression in list(edit.get_material_expressions(mat)):
        edit.delete_material_expression(mat, expression)
    assert edit.get_num_material_expressions(mat) == 0, path
    return mat

def node(cls):
    return edit.create_material_expression(mat, cls)

def connect(src, pin, target, slot):
    assert edit.connect_material_expressions(src, pin, target, slot), (src, pin, slot)

def bind(src, pin, prop):
    assert edit.connect_material_property(src, pin, prop)

def custom(code, result, **sources):
    expr = node(u.MaterialExpressionCustom)
    inputs = []
    for name in sources:
        entry = u.CustomInput()
        entry.set_editor_property("input_name", name)
        inputs.append(entry)
    expr.set_editor_property("inputs", inputs)
    expr.set_editor_property("code", code)
    expr.set_editor_property("output_type", result)
    for name, (src, pin) in sources.items():
        connect(src, pin, expr, name)
    return expr

def sample(tex, uv=None, normal=False):
    s = node(u.MaterialExpressionTextureSample)
    s.texture = tex
    if normal:
        s.sampler_type = u.MaterialSamplerType.SAMPLERTYPE_NORMAL
    if uv:
        connect(uv, "", s, "")
    return s

def finish():
    edit.recompile_material(mat)
    assert assets.save_loaded_asset(mat, False)
    u.log("Meadow: saved " + mat.get_name())

rgb = u.CustomMaterialOutputType.CMOT_FLOAT3
vec2 = u.CustomMaterialOutputType.CMOT_FLOAT2
mp = u.MaterialProperty
mat = material("M_AlpineSward")
p = node(u.MaterialExpressionWorldPosition)
def worlduv(expr):
    return custom(expr,vec2,P=(p,""))
fine = worlduv("return P.xy/185.0;")
wide = worlduv("return float2(P.x*.82+P.y*.57,-P.x*.57+P.y*.82)/1150.0;")
grass = import_asset("LeafyGrass.jpg","LeafyGrass",u.Texture2D)
aerial = import_asset("aerial_grass_rock_color.jpg","RockyMeadow",u.Texture2D)
gn = import_asset("grass_ground_normal.jpg","GrassGroundNormal",u.Texture2D)
gn.set_editor_property("compression_settings",u.TextureCompressionSettings.TC_NORMALMAP)
gn.set_editor_property("srgb",False)
assets.save_loaded_asset(gn,False)
albedo = custom("""
struct TerrainNoise {
    float val(float2 st) {
        float2 i=floor(st), f=frac(st); f=f*f*(3.-2.*f);
        float4 n = float4(0,1,157,158)+dot(i,float2(1,157));
        float4 h=frac(sin(n)*4375.5453);
        return lerp(lerp(h.x,h.y,f.x),lerp(h.z,h.w,f.x),f.y);
    }
} noise;
float2 q=P.xy;
float a=noise.val(q/11300.+16.);
float b=noise.val(q/3700.+7.);
float c=noise.val(q/790.+b);
float heath=smoothstep(.31,.76,.52*a+.29*b+.19*c);
float3 shortGrass=lerp(float3(.048,.100,.038),float3(.14,.175,.052),heath);
float3 leafy=lerp(G,A,.11)*float3(.51,.89,.62);
float farGround=smoothstep(2300.,9700.,length(P-E));
// Preserve metre-scale variations in the sward on approach. The second sample is
// rotated and domain-warped to break up recognisable repeats across a whole field.
float cover=saturate((dot(M,float3(.24,.66,.1))-.052)*4.7);
float bladeClumps=noise.val(q/97.+float2(3.*c,4.*b));
float turfDetail=.68+.52*cover+.22*bladeClumps;
float3 meadow=lerp(leafy,shortGrass*(.81+.38*c)*turfDetail,farGround);
float3 canopy=float3(.020,.046,.031)*(.52+.58*b+.63*c);
float3 wooded=lerp(meadow,canopy,.87*C.r*farGround);
float ribs=noise.val(float2(dot(q,float2(.63,.77))/2500.,(P.z+.16*P.x)/430.));
float fissures=.68+.67*saturate((dot(A,float3(.25,.62,.13))-.035)*5.);
float3 scree=lerp(float3(.061,.070,.078),float3(.26,.250,.225),.34*a+.39*ribs+.27*c)*fissures;
float3 gravel=lerp(float3(.108,.095,.068),float3(.22,.196,.151),b);
return lerp(lerp(wooded,scree,C.g),gravel,C.b);
""",rgb,P=(p,""),E=(node(u.MaterialExpressionCameraPositionWS),""),G=(sample(grass,fine),"RGB"),
                A=(sample(aerial,wide),"RGB"),
                M=(sample(grass,worlduv("return float2(.64*P.x-.77*P.y,.77*P.x+.64*P.y)/2100. + .31*sin(P.yx/1630.);")),"RGB"),
                C=(node(u.MaterialExpressionVertexColor),""))
bind(albedo,"",mp.MP_BASE_COLOR)
ground_normal = custom("return normalize(lerp(N,float3(0,0,1),.97*smoothstep(2000.,9000.,length(P-E))));",rgb,N=(sample(gn,fine,True),"RGB"),P=(p,""),E=(node(u.MaterialExpressionCameraPositionWS),""))
bind(ground_normal,"",mp.MP_NORMAL)
rough = node(u.MaterialExpressionConstant)
rough.r=.91
bind(rough,"",mp.MP_ROUGHNESS)
finish()

# The lake surface receives actual sky and terrain reflections. Its visible
# boundary is the intersection with the sloping bed, including the islands.
mat = material("M_ValleyWater")
mat.set_editor_property("shading_model", next(getattr(u.MaterialShadingModel,k) for k in dir(u.MaterialShadingModel) if "WATER" in k))
def color(values):
    expr = node(u.MaterialExpressionConstant3Vector)
    expr.constant = u.LinearColor(*values,1)
    return expr

def scalar(value):
    expr = node(u.MaterialExpressionConstant)
    expr.r = value
    return expr

water = node(u.MaterialExpressionSingleLayerWaterMaterialOutput)
pins = edit.get_material_expression_input_names(water)
for expr, target in zip((color((.00048,.00115,.00145)),color((.008,.0032,.0020)),scalar(.12),color((1.,1.,1.))), pins,strict=True):
    connect(expr,"",water,str(target))
bind(color((.005,.012,.016)),"",mp.MP_BASE_COLOR)
bind(scalar(.07),"",mp.MP_OPACITY)
bind(scalar(.15),"",mp.MP_ROUGHNESS)
ripples = custom("""
float2 q=P.xy;
float band=.65+.35*sin(dot(q,float2(.00076,.00023))+T*.3);
float d=length(P-E);
float fine = 1.-smoothstep(3500.,48000.,d);
float a = .042*sin(dot(q,float2(.013,.003))+T*1.2)+fine*.054*sin(dot(q,float2(.058,.021))+T*2.0);
float b = .032*cos(dot(q,float2(-.004,.017))+T*1.05)+fine*.035*cos(dot(q,float2(-.023,.049))+T*1.8);
return normalize(float3(a*band,b*band,1));
""",rgb,P=(node(u.MaterialExpressionWorldPosition),""),T=(node(u.MaterialExpressionTime),""),E=(node(u.MaterialExpressionCameraPositionWS),""))
bind(ripples,"",mp.MP_NORMAL)
finish()

mat = material("M_SilverFir")
mat.set_editor_property("blend_mode",u.BlendMode.BLEND_MASKED)
mat.set_editor_property("two_sided",True)
mat.set_editor_property("opacity_mask_clip_value",.19)
mat.set_editor_property("used_with_instanced_static_meshes",True)
mat.set_editor_property("shading_model",next(getattr(u.MaterialShadingModel,k) for k in dir(u.MaterialShadingModel) if "FOLIAGE" in k))
needles = sample(import_asset("FirBough.png","FirBough",u.Texture2D))
needle_color = custom("return lerp(C*.88,C*float3(.84,1.08,.95),R);",rgb,C=(needles,"RGB"),R=(node(u.MaterialExpressionPerInstanceRandom),""))
bind(needle_color,"",mp.MP_BASE_COLOR)
bind(needle_color,"",mp.MP_SUBSURFACE_COLOR)
bind(needles,"A",mp.MP_OPACITY_MASK)
bind(scalar(.87),"",mp.MP_ROUGHNESS)
finish()
fir = import_asset("SilverFir.gltf","SilverFir",u.StaticMesh)
fir.set_material(0,mat)
assets.save_loaded_asset(fir,False)

mat = material("M_ForestCrown")
mat.set_editor_property("used_with_instanced_static_meshes", True)
mat.set_editor_property("two_sided",True)
tint = custom("return lerp(float3(.015,.040,.027),float3(.066,.104,.048),.65*R+.35*U.y)*(.78+.31*U.y);",rgb,R=(node(u.MaterialExpressionPerInstanceRandom),""),U=(node(u.MaterialExpressionTextureCoordinate),""))
bind(tint,"",mp.MP_BASE_COLOR)
bind(scalar(.92),"",mp.MP_ROUGHNESS)
finish()
for tree in ("ForestCrown","AlderCrown"):
    crowns = import_asset(tree+".gltf",tree,u.StaticMesh)
    crowns.set_material(0,mat)
    assets.save_loaded_asset(crowns,False)

for plant,file,name in [("grass_medium_01","GrassAtlas.png","M_WildGrass"),("dandelion_01","FlowerAtlas.png","M_Flowers")]:
    tex = import_asset(file,Path(file).stem,u.Texture2D)
    mat = material(name)
    mat.set_editor_property("blend_mode",u.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided",True)
    mat.set_editor_property("used_with_instanced_static_meshes",True)
    mat.set_editor_property("shading_model",next(getattr(u.MaterialShadingModel,k) for k in dir(u.MaterialShadingModel) if "FOLIAGE" in k))
    atlas = sample(tex)
    bind(atlas,"RGB",mp.MP_BASE_COLOR)
    bind(atlas,"RGB",mp.MP_SUBSURFACE_COLOR)
    bind(atlas,"A",mp.MP_OPACITY_MASK)
    sway = custom("float h=saturate((P.z-C.z+12.0)/35.0); return float3(-1,.18,0)*h*h*(1.8+.95*sin(P.x*.014+P.y*.009+T*2.1));",rgb,P=(node(u.MaterialExpressionWorldPosition),""),C=(node(u.MaterialExpressionObjectPositionWS),""),T=(node(u.MaterialExpressionTime),""))
    bind(sway,"",mp.MP_WORLD_POSITION_OFFSET)
    finish()
    for meshname in (["GrassTall","GrassTuft","GrassShort"] if plant=="grass_medium_01" else ["Dandelion"]):
        mesh = import_asset(plant+"/"+meshname+".gltf",meshname,u.StaticMesh)
        mesh.set_material(0,mat)
        assets.save_loaded_asset(mesh,False)
mat = material("M_LivingGrass")
mat.set_editor_property("two_sided", True)
mat.set_editor_property("used_with_instanced_static_meshes", True)
mat.set_editor_property("shading_model", next(getattr(u.MaterialShadingModel,k) for k in dir(u.MaterialShadingModel) if "FOLIAGE" in k))
local = node(u.MaterialExpressionPreSkinnedPosition)
world = node(u.MaterialExpressionWorldPosition)
uv = node(u.MaterialExpressionTextureCoordinate)
leaf_uv = node(u.MaterialExpressionTextureCoordinate)
leaf_uv.set_editor_property("coordinate_index", 1)
variety = node(u.MaterialExpressionPerInstanceRandom)
pigment = custom("""
float height = saturate(U.y);
float3 young = lerp(float3(.018,.058,.019), float3(.12,.25,.082), height);
float3 mature = lerp(float3(.076,.085,.023), float3(.34,.31,.098), height);
float3 leaf = lerp(young, mature, saturate(1.2*U.x + .65*(V-.55)));
float across = B.x;
float rib = 1.-smoothstep(.025,.13,abs(across-.5));
float veins = cos(across*44.) * .045;
leaf *= .81 + .27*across + .24*rib + veins;
float dry = smoothstep(.77,1.,height)*smoothstep(.44,.69,U.x)*(.2 + .55*V);
leaf = lerp(leaf,float3(.41,.31,.12),dry);
return lerp(leaf, float3(.32,.22,.076), smoothstep(.77,.92,U.x));
""",rgb,U=(uv,""),V=(variety,""),B=(leaf_uv,""))
bind(pigment,"",mp.MP_BASE_COLOR)
bind(pigment,"",mp.MP_SUBSURFACE_COLOR)
fill = custom("return C*.16;",rgb,C=(pigment,""))
bind(fill,"",mp.MP_EMISSIVE_COLOR)
rough = node(u.MaterialExpressionConstant)
rough.r = .85
bind(rough,"",mp.MP_ROUGHNESS)
def vector_parameter(name, value):
    param = node(u.MaterialExpressionVectorParameter)
    param.set_editor_property("parameter_name", name)
    param.set_editor_property("default_value", u.LinearColor(*value))
    return param

aircraft = vector_parameter("AircraftCentre", (0,0,-100000,1))
span = vector_parameter("WingDirection", (0,1,0,1))
offset = custom("""
float h = saturate(L.z / 39.0);
float weight = h*h;
float wave = sin(dot(P.xy,float2(.005,.003)) + 1.75*T);
float flutter = sin(.055*P.x + .041*P.y + T*3.2 + V*4.0);
float3 wind = float3(-1,.16,0)*(2.8 + 1.9*wave + .55*flutter);
float3 rel = P - A;
float3 away = rel - S * clamp(dot(rel,S), -59.5, 59.5);
float touch = 1.0 - smoothstep(8, 44, length(away));
float2 side = away.xy / max(1.0,length(away.xy));
return weight * (wind + touch*float3(16*side, -min(24.0,L.z*.65)));
""",rgb,L=(local,""),P=(world,""),T=(node(u.MaterialExpressionTime),""),V=(variety,""),A=(aircraft,"RGB"),S=(span,"RGB"))
bind(offset,"",mp.MP_WORLD_POSITION_OFFSET)
import runpy
runpy.run_path(str(Path(__file__).with_name("grass_distance_fade.py")), run_name="grass_fade")["apply_grass_fade"](mat)
runpy.run_path(str(Path(__file__).with_name("build_vegetation_wind.py")), run_name="vegetation_wind")["apply_grass_wind"](mat)
finish()
for name in ("BentGrass","FloweringFescue","CreepingBent"):
    path = f"{folder}/{name}/StaticMeshes/{name}"
    if "-RefreshGrassMeshes" in u.SystemLibrary.get_command_line() and assets.does_asset_exist(path):
        assert assets.delete_asset(path)
    mesh = import_asset(name+".gltf",name,u.StaticMesh)
    mesh.set_material(0,mat)
    assert assets.save_loaded_asset(mesh,False)
    u.log(f"Meadow: {name} {mesh.get_bounding_box()}")
u.log("Meadow: all assets ready")
