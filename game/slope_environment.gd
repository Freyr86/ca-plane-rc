extends Node3D

# Lightweight procedural slope-flying site. Everything is deterministic so the
# same landscape is produced on every launch without external assets.

const TERRAIN_SIZE := 1600.0
const TERRAIN_CELLS := 120
const LAUNCH_Z := 58.0

var rng := RandomNumberGenerator.new()


func _ready() -> void:
	rng.seed = 4522
	build_terrain()
	build_valley_details()
	build_path()
	build_grass()
	build_trees()
	build_rocks()
	build_clouds()
	build_launch_area()
	build_atmosphere()


func terrain_height(x: float, z: float) -> float:
	# Rounded plateau inland, steep face toward the valley (negative Z).
	var slope := smoothstep(-190.0, 85.0, z)
	var height := lerpf(-24.0, 27.0, slope)
	height += 5.5 * exp(-pow((z + 30.0) / 70.0, 2.0))
	height += sin(x * 0.015) * 2.0
	height += sin((x + z) * 0.0085) * 2.6
	height += sin((x * 0.55 - z) * 0.024) * 0.75
	var escarpment_detail := exp(-pow((z + 85.0) / 125.0, 2.0))
	height += sin(x * 0.021 + z * 0.004) * 6.5 * escarpment_detail
	height += sin(x * 0.047 - z * 0.009) * 2.2 * escarpment_detail
	# A broad, almost flat launch patch near the pilot.
	var patch := exp(-pow(x / 75.0, 2.0) - pow((z - LAUNCH_Z) / 52.0, 2.0))
	return lerpf(height, 27.0, patch * 0.72)


func terrain_normal(x: float, z: float) -> Vector3:
	var step := 2.0
	return Vector3(
		terrain_height(x - step, z) - terrain_height(x + step, z),
		2.0 * step,
		terrain_height(x, z - step) - terrain_height(x, z + step)
	).normalized()


func terrain_color(x: float, z: float, normal: Vector3) -> Color:
	var steepness := 1.0 - normal.y
	var variation := sin(x * 0.11 + z * 0.07) * 0.035 + sin(x * 0.031 - z * 0.09) * 0.025
	if steepness > 0.18:
		return Color(0.31 + variation, 0.29 + variation, 0.23 + variation)
	if z < -170.0:
		return Color(0.35 + variation, 0.43 + variation, 0.28 + variation)
	return Color(0.23 + variation, 0.43 + variation, 0.18 + variation)


func build_terrain() -> void:
	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	var colors := PackedColorArray()
	var uvs := PackedVector2Array()
	var indices := PackedInt32Array()
	var points := TERRAIN_CELLS + 1
	var spacing := TERRAIN_SIZE / TERRAIN_CELLS
	vertices.resize(points * points)
	normals.resize(points * points)
	colors.resize(points * points)
	uvs.resize(points * points)
	for z_index in range(points):
		for x_index in range(points):
			var x := -TERRAIN_SIZE * 0.5 + x_index * spacing
			var z := -TERRAIN_SIZE * 0.5 + z_index * spacing
			var index := z_index * points + x_index
			var normal := terrain_normal(x, z)
			vertices[index] = Vector3(x, terrain_height(x, z), z)
			normals[index] = normal
			colors[index] = terrain_color(x, z, normal)
			uvs[index] = Vector2(x_index, z_index) / 10.0
	for z_index in range(TERRAIN_CELLS):
		for x_index in range(TERRAIN_CELLS):
			var a := z_index * points + x_index
			var b := a + 1
			var c := a + points
			var d := c + 1
			# Godot uses clockwise front-face winding.
			indices.append_array(PackedInt32Array([a, b, c, b, d, c]))
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_COLOR] = colors
	arrays[Mesh.ARRAY_TEX_UV] = uvs
	arrays[Mesh.ARRAY_INDEX] = indices
	var terrain_mesh := ArrayMesh.new()
	terrain_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	var instance := MeshInstance3D.new()
	instance.name = "PrairieSlope"
	instance.mesh = terrain_mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = Color("1f4b1d")
	material.roughness = 0.96
	instance.material_override = material
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
	add_child(instance)


func build_valley_details() -> void:
	# Patchwork fields and a river make altitude and distance readable from the air.
	var field_colors := [Color("31471f"), Color("514c25"), Color("254325"), Color("57442a")]
	for row in range(4):
		for column in range(7):
			var x := -470.0 + column * 155.0 + (row % 2) * 28.0
			var z := -640.0 + row * 105.0
			add_field_patch(x, z, 128.0, 76.0, field_colors[(row + column) % field_colors.size()])
	var river_vertices := PackedVector3Array()
	var river_indices := PackedInt32Array()
	var segments := 64
	for index in range(segments + 1):
		var z := -720.0 + index * 8.2
		var center_x := -90.0 + sin(index * 0.18) * 72.0
		for side in [-1.0, 1.0]:
			var x: float = center_x + float(side) * 8.0
			river_vertices.append(Vector3(x, terrain_height(x, z) + 0.28, z))
	for index in range(segments):
		var a := index * 2
		river_indices.append_array(PackedInt32Array([a, a + 1, a + 2, a + 1, a + 3, a + 2]))
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = river_vertices
	arrays[Mesh.ARRAY_INDEX] = river_indices
	var river_mesh := ArrayMesh.new()
	river_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	var river := MeshInstance3D.new()
	river.mesh = river_mesh
	var river_material := make_material(Color("4d91ae"), 0.25)
	river_material.metallic = 0.15
	river.material_override = river_material
	add_child(river)


func add_field_patch(center_x: float, center_z: float, width: float, depth: float, color: Color) -> void:
	var cells := 4
	var points := cells + 1
	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	var indices := PackedInt32Array()
	for z_index in range(points):
		for x_index in range(points):
			var x := center_x - width * 0.5 + width * x_index / cells
			var z := center_z - depth * 0.5 + depth * z_index / cells
			vertices.append(Vector3(x, terrain_height(x, z) + 0.12, z))
			normals.append(terrain_normal(x, z))
	for z_index in range(cells):
		for x_index in range(cells):
			var a := z_index * points + x_index
			var b := a + 1
			var c := a + points
			var d := c + 1
			indices.append_array(PackedInt32Array([a, b, c, b, d, c]))
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_INDEX] = indices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	var field := MeshInstance3D.new()
	field.mesh = mesh
	field.material_override = make_material(color, 0.98)
	add_child(field)


func build_path() -> void:
	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	var colors := PackedColorArray()
	var indices := PackedInt32Array()
	var segments := 70
	for index in range(segments + 1):
		var z := 28.0 + index * 5.0
		var center_x := 34.0 + sin(index * 0.13) * 18.0
		var width := 2.2
		for side in [-1.0, 1.0]:
			var x: float = center_x + float(side) * width
			vertices.append(Vector3(x, terrain_height(x, z) + 0.05, z))
			normals.append(terrain_normal(x, z))
			colors.append(Color("8a7954"))
	if segments > 0:
		for index in range(segments):
			var a := index * 2
			indices.append_array(PackedInt32Array([a, a + 1, a + 2, a + 1, a + 3, a + 2]))
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_COLOR] = colors
	arrays[Mesh.ARRAY_INDEX] = indices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	var instance := MeshInstance3D.new()
	instance.name = "Footpath"
	instance.mesh = mesh
	var material := StandardMaterial3D.new()
	material.vertex_color_use_as_albedo = true
	material.roughness = 1.0
	instance.material_override = material
	add_child(instance)


func grass_blade_mesh() -> ArrayMesh:
	var vertices := PackedVector3Array([
		Vector3(-0.08, 0.0, 0.0), Vector3(0.08, 0.0, 0.0),
		Vector3(0.035, 0.55, 0.0), Vector3(0.0, 0.72, 0.0),
	])
	var indices := PackedInt32Array([0, 1, 2, 0, 2, 3])
	var uvs := PackedVector2Array([Vector2(0, 0), Vector2(1, 0), Vector2(0.7, 0.75), Vector2(0.5, 1)])
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_TEX_UV] = uvs
	arrays[Mesh.ARRAY_INDEX] = indices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh


func build_grass() -> void:
	var count := 6500
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.use_colors = true
	multimesh.instance_count = count
	multimesh.mesh = grass_blade_mesh()
	for index in range(count):
		var x := rng.randf_range(-330.0, 330.0)
		var z := rng.randf_range(-35.0, 390.0)
		var scale := rng.randf_range(0.65, 1.45)
		var basis := Basis(Vector3.UP, rng.randf_range(0.0, TAU)).scaled(Vector3(scale, scale, scale))
		multimesh.set_instance_transform(index, Transform3D(basis, Vector3(x, terrain_height(x, z) + 0.03, z)))
		var tint := rng.randf_range(0.85, 1.15)
		multimesh.set_instance_color(index, Color(0.24 * tint, 0.48 * tint, 0.16 * tint, 1.0))
	var grass := MultiMeshInstance3D.new()
	grass.name = "MeadowGrass"
	grass.multimesh = multimesh
	grass.visibility_range_end = 230.0
	grass.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
	var material := StandardMaterial3D.new()
	material.albedo_color = Color("4e922d")
	material.shading_mode = BaseMaterial3D.SHADING_MODE_PER_VERTEX
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	material.roughness = 1.0
	grass.material_override = material
	add_child(grass)


func build_trees() -> void:
	var count := 105
	var trunk_mesh := CylinderMesh.new()
	trunk_mesh.top_radius = 0.16
	trunk_mesh.bottom_radius = 0.28
	trunk_mesh.height = 3.3
	trunk_mesh.radial_segments = 7
	var crown_mesh := CylinderMesh.new()
	crown_mesh.top_radius = 0.05
	crown_mesh.bottom_radius = 2.15
	crown_mesh.height = 5.6
	crown_mesh.radial_segments = 9
	var trunks := MultiMesh.new()
	var crowns := MultiMesh.new()
	for multimesh in [trunks, crowns]:
		multimesh.transform_format = MultiMesh.TRANSFORM_3D
		multimesh.instance_count = count
	trunks.mesh = trunk_mesh
	crowns.mesh = crown_mesh
	for index in range(count):
		var side := -1.0 if index % 2 == 0 else 1.0
		var x := side * rng.randf_range(95.0, 440.0)
		var z := rng.randf_range(80.0, 520.0)
		var y := terrain_height(x, z)
		var scale := rng.randf_range(0.72, 1.35)
		trunks.set_instance_transform(index, Transform3D(Basis.IDENTITY.scaled(Vector3(scale, scale, scale)), Vector3(x, y + 1.65 * scale, z)))
		crowns.set_instance_transform(index, Transform3D(Basis.IDENTITY.scaled(Vector3(scale, scale, scale)), Vector3(x, y + 4.6 * scale, z)))
	var trunk_instance := MultiMeshInstance3D.new()
	trunk_instance.multimesh = trunks
	trunk_instance.material_override = make_material(Color("6a4930"), 1.0)
	add_child(trunk_instance)
	var crown_instance := MultiMeshInstance3D.new()
	crown_instance.multimesh = crowns
	crown_instance.material_override = make_material(Color("214f2b"), 0.95)
	add_child(crown_instance)


func build_rocks() -> void:
	var count := 145
	var mesh := SphereMesh.new()
	mesh.radius = 1.0
	mesh.height = 2.0
	mesh.radial_segments = 8
	mesh.rings = 5
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.instance_count = count
	multimesh.mesh = mesh
	for index in range(count):
		var x := rng.randf_range(-500.0, 500.0)
		var z := rng.randf_range(-230.0, 30.0)
		var scale := Vector3(rng.randf_range(0.5, 2.2), rng.randf_range(0.35, 1.2), rng.randf_range(0.6, 2.4))
		var basis := Basis(Vector3.UP, rng.randf_range(0.0, TAU)).scaled(scale)
		multimesh.set_instance_transform(index, Transform3D(basis, Vector3(x, terrain_height(x, z) + scale.y * 0.45, z)))
	var rocks := MultiMeshInstance3D.new()
	rocks.multimesh = multimesh
	rocks.material_override = make_material(Color("45443f"), 0.9)
	add_child(rocks)


func build_clouds() -> void:
	var count := 54
	var mesh := SphereMesh.new()
	mesh.radius = 1.0
	mesh.height = 2.0
	mesh.radial_segments = 10
	mesh.rings = 6
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.instance_count = count
	multimesh.mesh = mesh
	for index in range(count):
		var cluster := index / 6
		var center := Vector3(-650.0 + cluster * 165.0, 245.0 + sin(cluster) * 18.0, -520.0 + (cluster % 3) * 210.0)
		var offset := Vector3(rng.randf_range(-28.0, 28.0), rng.randf_range(-5.0, 5.0), rng.randf_range(-12.0, 12.0))
		var scale := Vector3(rng.randf_range(12.0, 25.0), rng.randf_range(5.0, 10.0), rng.randf_range(9.0, 18.0))
		multimesh.set_instance_transform(index, Transform3D(Basis.IDENTITY.scaled(scale), center + offset))
	var clouds := MultiMeshInstance3D.new()
	clouds.multimesh = multimesh
	var material := make_material(Color(1.0, 0.98, 0.94, 0.72), 0.92)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.shading_mode = BaseMaterial3D.SHADING_MODE_PER_VERTEX
	clouds.material_override = material
	add_child(clouds)


func build_launch_area() -> void:
	var y := terrain_height(0.0, LAUNCH_Z)
	# Windsock pole.
	var pole := MeshInstance3D.new()
	var pole_mesh := CylinderMesh.new()
	pole_mesh.top_radius = 0.035
	pole_mesh.bottom_radius = 0.045
	pole_mesh.height = 4.4
	pole_mesh.radial_segments = 8
	pole.mesh = pole_mesh
	pole.position = Vector3(-7.0, y + 2.2, LAUNCH_Z)
	pole.material_override = make_material(Color("ded8c9"), 0.6)
	add_child(pole)
	var sock_mesh := ArrayMesh.new()
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([
		Vector3(-7.0, y + 4.25, LAUNCH_Z), Vector3(-7.0, y + 3.65, LAUNCH_Z),
		Vector3(-10.0, y + 3.85, LAUNCH_Z + 0.08), Vector3(-10.0, y + 4.05, LAUNCH_Z + 0.08),
	])
	arrays[Mesh.ARRAY_INDEX] = PackedInt32Array([0, 1, 2, 0, 2, 3])
	sock_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	var sock := MeshInstance3D.new()
	sock.mesh = sock_mesh
	var sock_material := make_material(Color("e34f32"), 0.8)
	sock_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	sock.material_override = sock_material
	add_child(sock)
	# A small timber bench gives the launch point a human scale.
	add_box(Vector3(2.6, 0.16, 0.48), Vector3(6.0, y + 0.75, LAUNCH_Z + 5.0), Color("765035"))
	add_box(Vector3(0.16, 0.75, 0.16), Vector3(5.1, y + 0.35, LAUNCH_Z + 5.0), Color("54402d"))
	add_box(Vector3(0.16, 0.75, 0.16), Vector3(6.9, y + 0.35, LAUNCH_Z + 5.0), Color("54402d"))


func build_atmosphere() -> void:
	var sun := DirectionalLight3D.new()
	sun.name = "EveningSun"
	sun.rotation_degrees = Vector3(-42.0, -32.0, 0.0)
	sun.light_color = Color("fff0cf")
	sun.light_energy = 0.82
	sun.shadow_enabled = true
	sun.directional_shadow_max_distance = 420.0
	add_child(sun)
	var sky_material := ProceduralSkyMaterial.new()
	sky_material.sky_top_color = Color("4d86b8")
	sky_material.sky_horizon_color = Color("b7d4df")
	sky_material.ground_bottom_color = Color("273526")
	sky_material.ground_horizon_color = Color("8ca38a")
	sky_material.sun_angle_max = 18.0
	sky_material.sun_curve = 0.09
	var sky := Sky.new()
	sky.sky_material = sky_material
	var environment := Environment.new()
	environment.background_mode = Environment.BG_SKY
	environment.sky = sky
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	environment.ambient_light_energy = 0.38
	environment.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	environment.fog_enabled = true
	environment.fog_light_color = Color("c5d5d3")
	environment.fog_light_energy = 0.55
	environment.fog_density = 0.00022
	environment.fog_sky_affect = 0.16
	var world := WorldEnvironment.new()
	world.environment = environment
	add_child(world)


func make_material(color: Color, roughness: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = roughness
	return material


func add_box(size: Vector3, position_: Vector3, color: Color) -> void:
	var instance := MeshInstance3D.new()
	var mesh := BoxMesh.new()
	mesh.size = size
	instance.mesh = mesh
	instance.position = position_
	instance.material_override = make_material(color, 0.9)
	add_child(instance)
