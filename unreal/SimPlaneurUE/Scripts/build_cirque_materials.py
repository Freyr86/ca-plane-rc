"""Build the local Creux-du-Van limestone without changing existing materials.

Run with UnrealEditor-Cmd -ExecutePythonScript=<this file> -unattended -nop4.
The sward duplicates the existing meadow graph and changes only its rock branch.
Vertex colors: R woodland, G rock fraction, B shore, A scree fraction.
All new coloration fades away before the 1100 m cirque influence boundary.
"""
import unreal as u

assets = u.EditorAssetLibrary
edit = u.MaterialEditingLibrary
tools = u.AssetToolsHelpers.get_asset_tools()
folder = "/Game/Meadow"
source_path = folder + "/M_AlpineSward"
destination_path = folder + "/M_CirqueSward"
source = assets.load_asset(source_path)
assert isinstance(source, u.Material), source_path


def code_of(expression):
    return str(expression.get_editor_property("code"))


def custom_with(mat, token):
    matches = [expression for expression in edit.get_material_expressions(mat)
               if isinstance(expression, u.MaterialExpressionCustom)
               and token in code_of(expression)]
    assert len(matches) == 1, (mat.get_name(), token, len(matches))
    return matches[0]


def save(mat):
    edit.recompile_material(mat)
    assert assets.save_loaded_asset(mat, False), mat.get_name()
    u.log("CIRQUE_MATERIAL saved " + mat.get_path_name())


if not assets.does_asset_exist(destination_path):
    assert assets.duplicate_asset(source_path, destination_path), destination_path
sward = assets.load_asset(destination_path)
assert isinstance(sward, u.Material), destination_path

# Start from the source code on every run; never accumulate edits on a prior build.
source_albedo = code_of(custom_with(source, "struct TerrainNoise"))
return_token = "return lerp(lerp(wooded,scree,C.g),gravel,C.b);"
assert source_albedo.count(return_token) == 1, "Alpine sward graph changed: review the cirque blend."
limestone_branch = r"""
// CIRQUE_LIMESTONE_BRANCH: geological pigment only; the established sward stays exact.
float2 cirqueQ = q-float2(268000.,160000.);
float geologicalBlend = 1.-smoothstep(88000.,110000.,length(cirqueQ));
// Volume noise, rather than one projected coordinate, avoids the gridded/barcode
// pattern on differently oriented parts of the horseshoe. No periodic joints.
struct GeologyNoise {
    float val(float3 p) {
        float3 i=floor(p), f=frac(p);
        f=f*f*f*(f*(f*6.-15.)+10.);
        float n=dot(i,float3(1.,157.,113.));
        float4 h0=frac(sin(n+float4(0.,1.,157.,158.))*4375.5453);
        float4 h1=frac(sin(n+float4(113.,114.,270.,271.))*4375.5453);
        return lerp(lerp(lerp(h0.x,h0.y,f.x),lerp(h0.z,h0.w,f.x),f.y),
                    lerp(lerp(h1.x,h1.y,f.x),lerp(h1.z,h1.w,f.x),f.y),f.z);
    }
} geology;
float3 stonePoint=float3(cirqueQ,P.z);
float warpA=geology.val(stonePoint/7100.+float3(17.,4.,9.));
float warpB=geology.val(stonePoint/3700.+float3(3.,29.,1.));
float3 warped=stonePoint+float3((warpA-.5)*4600.,(warpB-.5)*3900.,(warpA+warpB-1.)*1450.);
float macro=geology.val(warped/5300.+float3(6.,11.,23.));
float meso=geology.val(warped/1520.+float3(13.,2.,7.));
float bedding=geology.val(float3(warped.xy/17500.,warped.z/780.));
float breaks=geology.val(warped/2160.+float3(39.,16.,2.));
float grain=geology.val(warped/63.);
float nearGrain=1.-smoothstep(7500.,30000.,length(P-E));

// Broad exposed slabs and warmer weathered recesses; the irregular bedding is
// subdued and broken up by erosion patches instead of drawn as ruled lines.
float exposed=saturate((.64*macro+.36*meso-.21)*1.8);
float3 limestone=lerp(float3(.125,.119,.103),float3(.315,.304,.273),exposed);
float layerPigment=lerp(1.,.83+.29*bedding,smoothstep(.22,.72,breaks));
limestone*=layerPigment*(1.+nearGrain*.13*(grain-.5));
float runoff=geology.val(float3(warped.xy/1900.,warped.z/9300.)+float3(15.,3.,21.));
float stain=smoothstep(.50,.78,runoff)*smoothstep(.25,.65,meso);
limestone=lerp(limestone,limestone*float3(.59,.63,.64),.47*stain);
// Metre-scale fracture relief is shared with the normal input and survives the
// 500–1500 m overview distance; unlike sub-metre grain it must not fade at 300 m.
limestone*=lerp(.91,1.09,D.z)*(1.-.12*D.y);

float fragments=.60*noise.val(q/330.+float2(2.7*warpA,3.1*warpB))+.40*noise.val(q/67.+3.);
float3 paleScree=lerp(float3(.148,.140,.118),float3(.290,.279,.247),fragments);
paleScree*=.83+.32*geology.val(stonePoint/3650.+float3(21.,11.,4.));
float3 cirqueStone = lerp(limestone,paleScree,saturate(S));
float3 geologicalRock = lerp(scree,cirqueStone,geologicalBlend);
return lerp(lerp(wooded,geologicalRock,C.g),gravel,C.b);
"""
albedo = custom_with(sward, "struct TerrainNoise")
albedo.set_editor_property("code", source_albedo.replace(return_token, limestone_branch))
vertex = next(expression for expression in edit.get_material_expressions(sward)
              if isinstance(expression, u.MaterialExpressionVertexColor))


def extra_input(expression, name, output, source=None):
    inputs = list(expression.get_editor_property("inputs"))
    if not any(str(entry.get_editor_property("input_name")) == name for entry in inputs):
        entry = u.CustomInput()
        entry.set_editor_property("input_name", name)
        inputs.append(entry)
        expression.set_editor_property("inputs", inputs)
    assert edit.connect_material_expressions(source or vertex, output, expression, name)


# The default vertex-color output is RGB in UE; alpha must be wired separately.
extra_input(albedo, "S", "A")

# One shared field drives both pigment and actual lighting normals. It is 3D,
# so neither the vertical cliffs nor the sloping talus stretches a 2D texture.
detail_matches = [expression for expression in edit.get_material_expressions(sward)
                  if isinstance(expression, u.MaterialExpressionCustom)
                  and str(expression.get_editor_property("desc")) == "Cirque fracture field"]
detail = detail_matches[0] if detail_matches else edit.create_material_expression(sward, u.MaterialExpressionCustom)
detail.set_editor_property("desc", "Cirque fracture field")
detail.set_editor_property("output_type", u.CustomMaterialOutputType.CMOT_FLOAT3)
world = next(expression for expression in edit.get_material_expressions(sward)
             if isinstance(expression, u.MaterialExpressionWorldPosition))
eye = next(expression for expression in edit.get_material_expressions(sward)
           if isinstance(expression, u.MaterialExpressionCameraPositionWS))
extra_input(detail, "P", "", world)
extra_input(detail, "E", "", eye)
detail.set_editor_property("code", r"""
struct FractureNoise {
    float val(float3 p) {
        float3 i=floor(p), f=frac(p); f=f*f*(3.-2.*f);
        float n=dot(i,float3(1.,157.,113.));
        float4 h0=frac(sin(n+float4(0.,1.,157.,158.))*4375.5453);
        float4 h1=frac(sin(n+float4(113.,114.,270.,271.))*4375.5453);
        return lerp(lerp(lerp(h0.x,h0.y,f.x),lerp(h0.z,h0.w,f.x),f.y),
                    lerp(lerp(h1.x,h1.y,f.x),lerp(h1.z,h1.w,f.x),f.y),f.z);
    }
} rock;
float distanceToEye=length(P-E);
float middle=1.-smoothstep(150000.,270000.,distanceToEye);
float fine=1.-smoothstep(6000.,22000.,distanceToEye);
float3 local=P-float3(268000.,160000.,0.);
float warp=rock.val(local/1800.+float3(1.,29.,14.));
float3 q=local+float3(137.,-113.,91.)*(warp-.5);
float footprint=max(length(ddx(q)),length(ddy(q)));
float slabResolved=1.-smoothstep(100.,330.,footprint);
float chipResolved=1.-smoothstep(35.,130.,footprint);
float slab=rock.val(q/365.);
float chips=rock.val(q/168.+float3(21.,9.,3.));
float breaks=rock.val(q/740.+float3(7.,26.,1.));
float edge=abs(slab-.5);
float aa=max(.008,fwidth(slab)*.75);
float fracture=(1.-smoothstep(.025+aa,.095+aa,edge))*smoothstep(.27,.72,breaks);
float facet=smoothstep(.15,.85,.66*slab+.34*chips);
float grains=rock.val(q/19.);
float height=26.*(slab-.5)*slabResolved+7.*(chips-.5)*chipResolved
    -10.*fracture*slabResolved+fine*3.*(grains-.5);
return float3(height*middle,fracture*middle*slabResolved,lerp(.5,facet,middle*slabResolved));
""")
extra_input(albedo, "D", "", detail)

# Suppress the leafy normal on bare limestone while retaining the original
# near/far sward normals on grass and everywhere beyond the local transition.
normal_matches = [expression for expression in edit.get_material_expressions(sward)
                  if isinstance(expression, u.MaterialExpressionCustom)
                  and ("normalize(lerp(N," in code_of(expression)
                       or "CIRQUE_GROUND_NORMAL" in code_of(expression))]
assert len(normal_matches) == 1, "Sward normal graph changed."
normal = normal_matches[0]
extra_input(normal, "C", "")
extra_input(normal, "S", "A")
extra_input(normal, "D", "", detail)
normal.set_editor_property("code", r"""
// CIRQUE_GROUND_NORMAL
float distanceToEye=length(P-E);
float3 turf=normalize(lerp(N,float3(0,0,1),.97*smoothstep(2000.,9000.,distanceToEye)));
float blend=C.g*(1.-smoothstep(88000.,110000.,length(P.xy-float2(268000.,160000.))));
float detail=(1.-smoothstep(7000.,26000.,distanceToEye))*(.12+.12*S);
// Surface-gradient bump mapping in world space. The shared field is a height
// in centimetres, so the perturbation has the same scale on every cliff face.
float3 geometric=normalize(Parameters.TangentToWorld[2]);
float3 dx=ddx(P), dy=ddy(P);
float3 rx=cross(dy,geometric), ry=cross(geometric,dx);
float determinant=dot(dx,rx);
float3 gradient=(ddx(D.x)*rx+ddy(D.x)*ry)*sign(determinant)/max(abs(determinant),.001);
gradient*=min(1.,1.15/max(length(gradient),.001));
float3 stoneWorld=normalize(geometric-gradient);
float3 stone=normalize(mul(stoneWorld,transpose(Parameters.TangentToWorld))+float3(N.xy*detail,0.));
return normalize(lerp(turf,stone,saturate(blend)));
""")
save(sward)

# Independent material for instanced talus blocks. No world-aligned horizontal
# stripes on fallen stones: their fracture surfaces carry mottled limestone.
name = "M_CirqueLimestone"
path = folder + "/" + name
rock = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
    name, folder, u.Material, u.MaterialFactoryNew())
assert isinstance(rock, u.Material), path
for expression in list(edit.get_material_expressions(rock)):
    edit.delete_material_expression(rock, expression)
rock.set_editor_property("used_with_instanced_static_meshes", True)


def node(cls):
    return edit.create_material_expression(rock, cls)


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
    for name, source in sources.items():
        assert edit.connect_material_expressions(source, "", expr, name)
    return expr


color = custom(r"""
struct StoneNoise {
    float val(float3 q) {
        float3 i=floor(q), f=frac(q); f=f*f*(3.-2.*f);
        float n=dot(i,float3(1.,157.,113.));
        float4 a=frac(sin(n+float4(0.,1.,157.,158.))*4375.5453);
        float4 b=frac(sin(n+float4(113.,114.,270.,271.))*4375.5453);
        return lerp(lerp(lerp(a.x,a.y,f.x),lerp(a.z,a.w,f.x),f.y),
                    lerp(lerp(b.x,b.y,f.x),lerp(b.z,b.w,f.x),f.y),f.z);
    }
} noise;
float broad=noise.val(P/390.+7.);
float fine=noise.val(P/29.);
float3 base=lerp(float3(.151,.143,.123),float3(.315,.302,.269),.72*broad+.28*R);
float detail=1.-smoothstep(4500.,18000.,length(P-E));
base*=1.+detail*.15*(fine-.5);
return base;
""", u.CustomMaterialOutputType.CMOT_FLOAT3,
    P=node(u.MaterialExpressionWorldPosition),
    E=node(u.MaterialExpressionCameraPositionWS),
    R=node(u.MaterialExpressionPerInstanceRandom))
assert edit.connect_material_property(color, "", u.MaterialProperty.MP_BASE_COLOR)
rough = node(u.MaterialExpressionConstant)
rough.r = .94
assert edit.connect_material_property(rough, "", u.MaterialProperty.MP_ROUGHNESS)
specular = node(u.MaterialExpressionConstant)
specular.r = .23
assert edit.connect_material_property(specular, "", u.MaterialProperty.MP_SPECULAR)
save(rock)
u.log("CIRQUE_MATERIAL complete")
