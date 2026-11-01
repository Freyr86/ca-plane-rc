extends Node3D

var udp := PacketPeerUDP.new()
var plane := Node3D.new()
var camera := Camera3D.new()
var hud := Label.new()
var chase := true
var last_packet := 0
var status: Dictionary = {}
var model: Node3D
var surfaces: Dictionary = {}
var inspect_model := false
var radio: CanvasLayer
var keyboard_controls := Vector3.ZERO
var snapshots: Array = []
var send_elapsed := 0.0
var hud_elapsed := 0.0
var packets_received := 0
var packet_hz := 0
var stats_elapsed := 0.0
var site: Node3D



func box(parent: Node3D, dimensions: Vector3, pos: Vector3, color: Color) -> void:
	var mesh := MeshInstance3D.new()
	var shape := BoxMesh.new()
	shape.size = dimensions
	mesh.mesh = shape
	mesh.position = pos
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	mesh.material_override = material
	parent.add_child(mesh)

func _ready() -> void:
	radio = preload("res://radio_input.gd").new()
	add_child(radio)
	site = preload("res://slope_environment.gd").new()
	add_child(site)
	udp.connect_to_host("127.0.0.1", 5507)
	add_child(plane)
	model = preload("res://assets/Topy4522V0.glb").instantiate()
	plane.add_child(model)
	var json_file := FileAccess.open("res://assets/topy_aircraft.json", FileAccess.READ)
	var aircraft: Dictionary = JSON.parse_string(json_file.get_as_text())
	for part in aircraft.parts:
		var node := model.find_child(part.name, true, false) as Node3D
		if node is MeshInstance3D:
			var material := StandardMaterial3D.new()
			material.albedo_color = Color(part.color[0]/255.0, part.color[1]/255.0, part.color[2]/255.0)
			material.roughness = 0.55
			node.material_override = material
		if part.axis != null and node:
			surfaces[part.name] = {"node":node, "rest":node.transform, "axis":Vector3(part.axis[0], part.axis[1], part.axis[2]).normalized()}
	plane.position.y = 80
	add_child(camera)
	camera.far = 2400
	camera.fov = 62
	camera.current = true
	var canvas := CanvasLayer.new()
	add_child(canvas)
	var panel := ColorRect.new()
	panel.color = Color(0.018, .035, .045, .82)
	panel.position = Vector2(18,18)
	panel.size = Vector2(700,224)
	canvas.add_child(panel)
	var accent := ColorRect.new()
	accent.color = Color("e36136")
	accent.position = Vector2(18, 18)
	accent.size = Vector2(5, 224)
	canvas.add_child(accent)
	hud.position = Vector2(32,28)
	hud.add_theme_font_size_override("font_size", 17)
	hud.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.75))
	hud.add_theme_constant_override("shadow_offset_x", 1)
	hud.add_theme_constant_override("shadow_offset_y", 2)
	canvas.add_child(hud)
	if "--radio" in OS.get_cmdline_user_args():
		radio.toggle()
	if "--showcase" in OS.get_cmdline_user_args():
		inspect_model = true
	if "--scenic" in OS.get_cmdline_user_args():
		chase = false
	if "--smoke" in OS.get_cmdline_user_args():
		get_tree().create_timer(3.0).timeout.connect(smoke_capture)

func _unhandled_key_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_J:
			radio.toggle()
		if event.keycode == KEY_V:
			inspect_model = not inspect_model
		if event.keycode == KEY_C:
			chase = not chase
		if event.keycode == KEY_ESCAPE:
			get_tree().quit()
		if event.keycode == KEY_R:
			udp.put_packet(JSON.stringify({"reset":true}).to_utf8_buffer())

func _process(delta: float) -> void:
	var a := float(Input.is_physical_key_pressed(KEY_RIGHT))-float(Input.is_physical_key_pressed(KEY_LEFT))
	var e := float(Input.is_physical_key_pressed(KEY_UP))-float(Input.is_physical_key_pressed(KEY_DOWN))
	var r := float(Input.is_physical_key_pressed(KEY_D))-float(Input.is_physical_key_pressed(KEY_A))
	var target := Vector3(a, e, r)
	keyboard_controls = keyboard_controls.move_toward(target, delta * 3.0)
	var input_controls: Vector3 = radio.controls() if radio.ready_for_radio() else keyboard_controls
	if radio.active:
		input_controls = Vector3.ZERO
	a = input_controls.x
	e = input_controls.y
	r = input_controls.z
	var model_rudder := -r
	for surface_name in surfaces:
		var surface: Dictionary = surfaces[surface_name]
		var angle := 0.0
		if surface_name.begins_with("vtail"):
			var mixed := clampf(e + model_rudder * (1.0 if surface_name.ends_with("left") else -1.0), -1.0, 1.0)
			angle = mixed * deg_to_rad(15)
		surface.node.transform = surface.rest
		surface.node.basis = Basis(surface.axis, angle) * surface.rest.basis
	send_elapsed += delta
	if send_elapsed >= 1.0/60.0:
		send_elapsed = fmod(send_elapsed, 1.0/60.0)
		udp.put_packet(JSON.stringify({"aileron":0.0,"elevator":e,"rudder":model_rudder,"paused":radio.active}).to_utf8_buffer())
	while udp.get_available_packet_count() > 0:
		var packet = JSON.parse_string(udp.get_packet().get_string_from_utf8())
		if packet is Dictionary:
			if not status.is_empty() and float(packet.time) < float(status.time):
				snapshots.clear()
			status = packet
			last_packet = Time.get_ticks_msec()
			packets_received += 1
			snapshots.append(packet)
			if snapshots.size() > 12:
				snapshots.pop_front()
	if not snapshots.is_empty():
		# Render 25 ms behind the latest simulation clock, interpolating poses.
		var display_time := float(status.time) + (Time.get_ticks_msec()-last_packet)/1000.0 - 0.025
		var previous: Dictionary = snapshots[0]
		var following: Dictionary = snapshots[-1]
		for snapshot in snapshots:
			if float(snapshot.time) <= display_time:
				previous = snapshot
			if float(snapshot.time) >= display_time:
				following = snapshot
				break
		var interval := float(following.time)-float(previous.time)
		var weight := clampf((display_time-float(previous.time))/interval,0.0,1.0) if interval > 0.00001 else 1.0
		plane.transform = pose(previous).interpolate_with(pose(following),weight)
	if inspect_model:
		camera.position = plane.position + plane.basis * Vector3(.8, .55, -.9)
	elif chase:
		# A high three-quarter chase view keeps both the model and the slope visible.
		camera.position = plane.position + plane.basis*Vector3(1.8, 3.4, 7.2)
	else:
		camera.position = Vector3(12,site.terrain_height(12,58)+1.72,58)
	if camera.position.distance_to(plane.position) > .1:
		camera.look_at(plane.position, Vector3.UP)
	stats_elapsed += delta
	if stats_elapsed >= 1.0:
		packet_hz = roundi(packets_received/stats_elapsed)
		packets_received = 0
		stats_elapsed = 0
	hud_elapsed += delta
	if hud_elapsed < 0.1:
		return
	hud_elapsed = 0
	var connected := not status.is_empty() and Time.get_ticks_msec()-last_packet < 1500
	var telemetry := "En attente du moteur JSBSim..."
	if connected:
		telemetry = "Altitude %.1f m   Vitesse %.1f m/s   Chute %.2f m/s" % [status.alt,status.speed,status.sink]
		if status.grounded:
			telemetry = "Sol atteint — appuyer sur R pour relancer"
	hud.text = "VOL DE PENTE  •  GRAUPNER TOPY 4522 V0\nEnvergure 1,19 m  |  Masse 0,320 kg  |  Physique provisoire\n" + telemetry + "\n↑/↓ profondeur  •  A/D direction  •  R relancer\nC caméra sol/poursuite  •  V inspection  •  Échap quitter\n" + radio.label() + "  |  J : calibration\n%d FPS  •  Télémétrie %d Hz" % [Engine.get_frames_per_second(),packet_hz]

func pose(snapshot: Dictionary) -> Transform3D:
	var orientation := Basis(Vector3.UP,-float(snapshot.yaw))*Basis(Vector3.RIGHT,float(snapshot.pitch))*Basis(Vector3.BACK,-float(snapshot.roll))
	return Transform3D(orientation,Vector3(snapshot.east,snapshot.alt,-snapshot.north))

func smoke_capture() -> void:
	if not radio.active:
		radio.device = -1
		var key := InputEventKey.new()
		key.physical_keycode = KEY_D
		key.pressed = true
		Input.parse_input_event(key)
		await get_tree().process_frame
		await get_tree().process_frame
		assert(not surfaces["vtail_right"].node.basis.is_equal_approx(surfaces["vtail_right"].rest.basis), "V-tail animation failed")
		key.pressed = false
		Input.parse_input_event(key)
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(ProjectSettings.globalize_path("res://../work/topy-v0-preview.png"))
	assert(surfaces.size() == 2, "Missing V-tail control surfaces")
	print("SMOKE: surfaces=", surfaces.size(), " FPS=",Engine.get_frames_per_second()," packets_hz=",packet_hz)
	print("SMOKE: connected=", not status.is_empty(), " altitude=", status.get("alt", -1))
	get_tree().quit(0 if not status.is_empty() else 1)
