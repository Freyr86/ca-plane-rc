"""Bake distance LODs from the valley's existing 3D conifers in Unreal Editor."""
import unreal as u

library = u.EditorAssetLibrary
editor = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
oak = library.load_asset("/Game/ArchVis/SampleScene/Tree/HillTree_02")
if oak:
    u.log(f"TREE_OAK native triangles {[oak.get_num_triangles(i) for i in range(editor.get_lod_count(oak))]}")

paths = ["/Game/Meadow/SilverFir/StaticMeshes/SilverFir"] + [
    "/Game/Meadow/LakePines/LakePines/StaticMeshes/pine_sapling_small_" + suffix
    for suffix in ("a", "b", "c")
]
for path in paths:
    mesh = library.load_asset(path)
    if not mesh:
        raise RuntimeError("Missing valley tree: " + path)
    original = mesh.get_num_triangles(0)
    settings = u.StaticMeshReductionOptions()
    settings.auto_compute_lod_screen_size = False
    settings.reduction_settings = [
        u.StaticMeshReductionSettings(percent_triangles=fraction, screen_size=screen)
        for fraction, screen in [(1., 1.), (.55, .095), (.25, .043), (.11, .018)]
    ]
    editor.set_lods(mesh, settings)
    counts = [mesh.get_num_triangles(i) for i in range(editor.get_lod_count(mesh))]
    if len(counts) != 4 or counts[0] != original or not all(a>b>0 for a,b in zip(counts,counts[1:])):
        raise RuntimeError(f"Invalid LODs for {path}: {counts}")
    assert library.save_loaded_asset(mesh, False)
    u.log(f"TREE_LODS {path} triangles {counts} thresholds {list(editor.get_lod_screen_sizes(mesh))}")
