"""Import missing hangar aircraft from their FBX and four PBR textures."""

from pathlib import Path
from tempfile import TemporaryDirectory
from shutil import copyfile
import unreal


project_dir = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))

folder = "/Game/Aircraft"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
edit = unreal.MaterialEditingLibrary


def create_project_materials():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    library = unreal.EditorAssetLibrary
    material_library = unreal.MaterialEditingLibrary
    library.make_directory("/Game/Materials")

    terrain_path = "/Game/Materials/M_TerrainVertexColor"
    terrain = library.load_asset(terrain_path)
    if terrain is None:
        terrain = asset_tools.create_asset(
            "M_TerrainVertexColor", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew()
        )
        vertex_color = material_library.create_material_expression(
            terrain, unreal.MaterialExpressionVertexColor, -400, 0
        )
        material_library.connect_material_property(
            vertex_color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
        )
        roughness = material_library.create_material_expression(
            terrain, unreal.MaterialExpressionConstant, -400, 180
        )
        roughness.r = 0.92
        material_library.connect_material_property(
            roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
        )
        material_library.recompile_material(terrain)
        library.save_loaded_asset(terrain)

    solid_path = "/Game/Materials/M_SolidColor"
    solid = library.load_asset(solid_path)
    if solid is None:
        solid = asset_tools.create_asset(
            "M_SolidColor", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew()
        )
        color = material_library.create_material_expression(
            solid, unreal.MaterialExpressionVectorParameter, -400, 0
        )
        color.set_editor_property("parameter_name", "Color")
        color.set_editor_property("default_value", unreal.LinearColor(0.18, 0.38, 0.08, 1.0))
        material_library.connect_material_property(
            color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
        )
        roughness = material_library.create_material_expression(
            solid, unreal.MaterialExpressionConstant, -400, 180
        )
        roughness.r = 0.82
        material_library.connect_material_property(
            roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
        )
        material_library.recompile_material(solid)
        library.save_loaded_asset(solid)
    solid.set_editor_property("used_with_instanced_static_meshes", True)
    material_library.recompile_material(solid)
    library.save_loaded_asset(solid)


def import_asset(path, name, options=None):
    task = unreal.AssetImportTask()
    task.filename = str(path)
    task.destination_path = folder
    task.destination_name = name
    task.automated = task.replace_existing = task.replace_existing_settings = True
    task.options = options
    tools.import_asset_tasks([task])
    objects = task.get_objects()
    if len(objects) != 1:
        raise RuntimeError(f"Import {path.name}: {task.imported_object_paths}")
    return objects[0]


def painted_material(source, prefix):
    name = f"M_{prefix}Peinture"
    path = folder + "/" + name
    mat = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
        name, folder, unreal.Material, unreal.MaterialFactoryNew())
    for old in list(edit.get_material_expressions(mat)):
        edit.delete_material_expression(mat, old)
    channels = (
        ("color", unreal.MaterialProperty.MP_BASE_COLOR, "RGB"),
        ("normal", unreal.MaterialProperty.MP_NORMAL, "RGB"),
        ("roughness", unreal.MaterialProperty.MP_ROUGHNESS, "R"),
        ("metallic", unreal.MaterialProperty.MP_METALLIC, "R"),
    )
    for index, (layer, prop, pin) in enumerate(channels):
        tex = import_asset(source / f"{prefix}_{layer}.png", f"T_{prefix}_" + layer)
        tex.set_editor_property("srgb", layer == "color")
        tex.set_editor_property("max_texture_size", 2048 if layer in ("color", "normal") else 1024)
        sampler = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
        if layer == "normal":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("flip_green_channel", True)
            sampler = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
        elif layer != "color":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            sampler = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
        assert assets.save_loaded_asset(tex, False)
        sample = edit.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, index * 220)
        sample.texture = tex
        sample.sampler_type = sampler
        assert edit.connect_material_property(sample, pin, prop)
    edit.recompile_material(mat)
    assert assets.save_loaded_asset(mat, False)
    return mat


def painted_aircraft(source, prefix, mat):
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_mesh = True
    options.import_as_skeletal = options.import_materials = options.import_textures = False
    data = options.static_mesh_import_data
    data.combine_meshes = True
    data.generate_lightmap_u_vs = data.auto_generate_collision = False
    # Preserve the supplied coordinate system. The aircraft component registers
    # the visual against the flight model, also when shown in the hangar.
    data.import_rotation = unreal.Rotator()
    data.import_uniform_scale = 1.
    fbx = source / f"{prefix}.fbx"
    # The FBX SDK extracts redundant embedded JPEGs next to the FBX during import.
    with TemporaryDirectory(prefix=prefix + "-") as scratch:
        staged = Path(scratch) / fbx.name
        copyfile(fbx, staged)
        mesh = import_asset(staged, prefix + "Couleur", options)
        mesh.get_editor_property("asset_import_data").scripted_add_filename(str(fbx), 0, "")
    mesh_tools = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    build = mesh_tools.get_lod_build_settings(mesh, 0)
    build.build_scale3d = unreal.Vector(1., 1., 1.)
    mesh_tools.set_lod_build_settings(mesh, 0, build)
    lods = unreal.StaticMeshReductionOptions()
    lods.auto_compute_lod_screen_size = False
    lods.reduction_settings = [
        unreal.StaticMeshReductionSettings(percent_triangles=fraction, screen_size=size)
        for fraction, size in ((1., 1.), (.45, .22), (.10, .065))
    ]
    mesh_tools.set_lods(mesh, lods)
    mesh.set_material(0, mat)
    box = mesh.get_bounding_box()
    size = box.max - box.min
    assert min(size.x, size.y, size.z) > 0., box
    assert mesh.get_material(0) == mat and mesh_tools.get_lod_count(mesh) == 3
    assert assets.save_loaded_asset(mesh, False)
    unreal.log(f"AIRCRAFT_READY {mesh.get_path_name()} | cm {size} | {mat.get_path_name()}")


create_project_materials()
for prefix in ("Topy", "K3600"):
    names = [prefix + "Couleur", f"M_{prefix}Peinture"] + [f"T_{prefix}_{layer}" for layer in ("color", "normal", "roughness", "metallic")]
    if not all(assets.does_asset_exist(folder + "/" + name) for name in names):
        source = project_dir / "SourceAssets" / prefix
        painted_aircraft(source, prefix, painted_material(source, prefix))

