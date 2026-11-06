"""Add weather-driven GPU wind, preserving imported tree materials and grass contact/fade."""
from pathlib import Path
import hashlib
import unreal as u

edit = u.MaterialEditingLibrary
assets = u.EditorAssetLibrary
folder = "/Game/Meadow/Wind"
assets.make_directory(folder)

GRASS_CODE = """
float h = saturate(L.z / 39.0);
float weight = h*h;
float speed=length(W.xy);
float2 direction=W.xy/max(.001,speed);
float2 advected=P.xy*.01-W.xy*WT;
float gust=.7*sin(advected.x*.023+advected.y*.017+WT*.11)+.3*sin(advected.x*.057-advected.y*.033-WT*.17);
float ripple=sin(dot(P.xy,float2(.005,.003))-WT*(.8+speed*.23));
float flutter=sin(.055*P.x+.041*P.y+WT*3.2+V*4.0);
float bend=min(19.0,speed*(1.35+.45*ripple+.9*W.z*gust+.10*flutter));
float3 wind=float3(direction*bend,-.14*bend);
float3 rel=P-A;
float3 away=rel-S*clamp(dot(rel,S),-59.5,59.5);
float touch=1.0-smoothstep(8,44,length(away));
float2 side=away.xy/max(1.0,length(away.xy));
return weight*(wind+touch*float3(16*side,-min(24.0,L.z*.65)));
"""

TREE_CODE = """
float h=saturate((L.z-MinZ)/max(1.0,Height));
float speed=length(W.xy);
float2 dir=W.xy/max(.001,speed);
float2 advected=P.xy*.01-W.xy*WT;
float gust=.7*sin(advected.x*.023+advected.y*.017+WT*.11)+.3*sin(advected.x*.057-advected.y*.033-WT*.17);
float slow=sin(WT*.75+V*6.283+dot(P.xy,float2(.00013,.00017)));
float flutter=sin(WT*2.5+V*17.0+L.z*.018);
float amplitude=speed*Compliance*(.65+.18*slow+.35*W.z*gust);
float2 side=float2(-dir.y,dir.x);
return float3(h*h*(dir*amplitude+side*flutter*speed*Compliance*.06),-h*h*amplitude*.03);
"""

def node(mat, cls):
    return edit.create_material_expression(mat, cls)

def parameter(mat, name, value):
    vector=isinstance(value,tuple)
    cls=u.MaterialExpressionVectorParameter if vector else u.MaterialExpressionScalarParameter
    existing=[e for e in edit.get_material_expressions(mat) if isinstance(e,cls) and str(e.get_editor_property("parameter_name"))==name]
    result=existing[0] if existing else node(mat,cls)
    result.set_editor_property("parameter_name",name)
    result.set_editor_property("default_value",u.LinearColor(*value) if vector else value)
    return result

def custom(mat, code, sources, marker):
    candidates=[e for e in edit.get_material_expressions(mat) if isinstance(e,u.MaterialExpressionCustom) and e.get_editor_property("desc")==marker]
    result=candidates[0] if candidates else node(mat,u.MaterialExpressionCustom)
    result.set_editor_property("desc",marker)
    result.set_editor_property("code",code)
    result.set_editor_property("output_type",u.CustomMaterialOutputType.CMOT_FLOAT3)
    entries=[]
    for name in sources:
        entry=u.CustomInput();entry.set_editor_property("input_name",name);entries.append(entry)
    result.set_editor_property("inputs",entries)
    for name,(source,pin) in sources.items():
        assert edit.connect_material_expressions(source,pin,result,name), name
    return result

def weather_inputs(mat):
    clock=node(mat,u.MaterialExpressionAdd)
    assert edit.connect_material_expressions(node(mat,u.MaterialExpressionTime),"",clock,"A")
    assert edit.connect_material_expressions(parameter(mat,"WeatherTimeOffset",0.),"",clock,"B")
    return {"W":(parameter(mat,"WeatherWind",(-4.4,-.94,.15,1)),"RGB"),
            "WT":(clock,"")}

def apply_grass_wind(mat):
    # Replace the original wind expression in place so the distance fade's
    # connection and all surface shading survive unchanged.
    expressions=edit.get_material_expressions(mat)
    offset=next(e for e in expressions if isinstance(e,u.MaterialExpressionCustom)
        and (e.get_editor_property("desc")=="Weather grass" or "float3 away" in e.get_editor_property("code")))
    offset.set_editor_property("desc","Weather grass")
    inputs=list(offset.get_editor_property("inputs"))
    names=[str(i.get_editor_property("input_name")) for i in inputs]
    for name in ("W","WT"):
        if name not in names:
            entry=u.CustomInput();entry.set_editor_property("input_name",name);inputs.append(entry)
    offset.set_editor_property("inputs",inputs)
    for name,(source,pin) in weather_inputs(mat).items():
        assert edit.connect_material_expressions(source,pin,offset,name)
    offset.set_editor_property("code",GRASS_CODE)
    edit.recompile_material(mat)
    assert assets.save_loaded_asset(mat,False)
    u.log("VEGETATION_WIND grass contact and distance fade preserved")

def tree_wind(mat):
    if mat.get_editor_property("use_material_attributes"):
        raise RuntimeError("Unsupported tree material attributes: "+mat.get_path_name())
    sources=weather_inputs(mat)
    sources.update({"L":(node(mat,u.MaterialExpressionPreSkinnedPosition),""),
        "P":(node(mat,u.MaterialExpressionWorldPosition),""),
        "V":(node(mat,u.MaterialExpressionPerInstanceRandom),""),
        "MinZ":(parameter(mat,"VegetationMinZ",0.),""),
        "Height":(parameter(mat,"VegetationHeight",600.),""),
        "Compliance":(parameter(mat,"VegetationCompliance",2.3),"")})
    offset=custom(mat,TREE_CODE,sources,"Weather tree")
    assert edit.connect_material_property(offset,"",u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mat.set_editor_property("used_with_instanced_static_meshes",True)
    edit.recompile_material(mat)
    assert assets.save_loaded_asset(mat,False)

cache={}
def duplicate_chain(source):
    path=source.get_path_name()
    if path in cache:return cache[path]
    suffix=hashlib.sha1(path.encode()).hexdigest()[:8]
    destination=folder+"/Wind_"+source.get_name()+"_"+suffix
    clone=assets.load_asset(destination) if assets.does_asset_exist(destination) else assets.duplicate_asset(path,destination)
    assert clone, path
    cache[path]=clone
    if isinstance(source,u.MaterialInstanceConstant):
        parent=duplicate_chain(source.get_editor_property("parent"))
        edit.set_material_instance_parent(clone,parent)
        edit.update_material_instance(clone)
        assert assets.save_loaded_asset(clone,False)
    else:
        assert isinstance(clone,u.Material), path
        tree_wind(clone)
    return clone

def main():
    apply_grass_wind(assets.load_asset("/Game/Meadow/M_LivingGrass"))
    paths=["/Game/ArchVis/SampleScene/Tree/HillTree_02","/Game/Meadow/SilverFir/StaticMeshes/SilverFir"]+[
        "/Game/Meadow/LakePines/LakePines/StaticMeshes/pine_sapling_small_"+s for s in ("a","b","c")]
    for path in paths:
        mesh=assets.load_asset(path)
        assert mesh,path
        for slot in range(len(mesh.get_editor_property("static_materials"))):
            source=mesh.get_material(slot)
            if not source:continue
            parent=duplicate_chain(source)
            name="MI_Wind_"+mesh.get_name()+"_"+str(slot)
            destination=folder+"/"+name
            instance=assets.load_asset(destination) if assets.does_asset_exist(destination) else u.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
            edit.set_material_instance_parent(instance,parent)
            edit.update_material_instance(instance)
            assert assets.save_loaded_asset(instance,False)
            u.log("VEGETATION_WIND saved "+destination)
    for name in ("M_WildGrass","M_Flowers"):
        mat=assets.load_asset("/Game/Meadow/"+name)
        sources=weather_inputs(mat)
        sources.update({"L":(node(mat,u.MaterialExpressionPreSkinnedPosition),""),"P":(node(mat,u.MaterialExpressionWorldPosition),""),"V":(node(mat,u.MaterialExpressionPerInstanceRandom),""),
            "MinZ":(parameter(mat,"VegetationMinZ",0.),""),"Height":(parameter(mat,"VegetationHeight",40.),""),"Compliance":(parameter(mat,"VegetationCompliance",1.1),"")})
        offset=custom(mat,TREE_CODE,sources,"Weather small plants")
        assert edit.connect_material_property(offset,"",u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        edit.recompile_material(mat);assert assets.save_loaded_asset(mat,False)
    u.log("VEGETATION_WIND complete")

if __name__=="__main__":main()
