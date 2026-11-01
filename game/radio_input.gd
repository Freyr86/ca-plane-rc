extends CanvasLayer

const AXES := 10
const ROLES := ["aileron", "elevator", "rudder"]
const DIRECTIONS := ["Maintiens le manche de ROULIS a DROITE.", "Maintiens la PROFONDEUR POUSSEE vers l'avant.", "Maintiens le manche de DIRECTION a DROITE."]
const PROFILE_PATH := "user://radio.cfg"
var device := -1
var signature := ""
var mapping: Dictionary = {}
var centers: Array = []
var lows: Array = []
var highs: Array = []
var stage := -1
var active := false
var panel := PanelContainer.new()
var devices := OptionButton.new()
var info := Label.new()
var monitor := Label.new()
var advance := Button.new()
var saved_mapping: Dictionary = {}

func _ready() -> void:
    layer = 5
    var config := ConfigFile.new()
    if config.load(PROFILE_PATH) == OK:
        signature = config.get_value("radio", "signature", "")
        mapping = config.get_value("radio", "mapping", {})
    saved_mapping = mapping.duplicate(true)
    panel.position = Vector2(175, 240)
    panel.custom_minimum_size = Vector2(900, 370)
    var margin := MarginContainer.new()
    for edge in ["margin_left", "margin_right", "margin_top", "margin_bottom"]:
        margin.add_theme_constant_override(edge, 20)
    panel.add_child(margin)
    var layout := VBoxContainer.new()
    layout.add_theme_constant_override("separation", 12)
    margin.add_child(layout)
    var title := Label.new()
    title.text = "RADIO USB — calibration guidee (vol en pause)"
    title.add_theme_font_size_override("font_size", 22)
    layout.add_child(title)
    layout.add_child(devices)
    layout.add_child(info)
    layout.add_child(monitor)
    layout.add_child(advance)
    advance.text = "Commencer la calibration"
    advance.pressed.connect(next_stage)
    devices.item_selected.connect(select_device)
    var close := Button.new()
    close.text = "Fermer / annuler (J)"
    close.pressed.connect(toggle)
    layout.add_child(close)
    add_child(panel)
    panel.hide()
    Input.joy_connection_changed.connect(devices_changed)
    refresh_devices()

func identity(id: int) -> String:
    return Input.get_joy_guid(id) + ":" + Input.get_joy_name(id)

func refresh_devices() -> void:
    devices.clear()
    var ids := Input.get_connected_joypads()
    if not ids.has(device):
        device = -1
    for id in ids:
        devices.add_item(Input.get_joy_name(id), id)
        if identity(id) == signature and device == -1:
            device = id
        if id == device:
            devices.select(devices.item_count-1)
    if device == -1 and not ids.is_empty():
        device = ids[0]
    if device >= 0:
        devices.select(devices.get_item_index(device))
    if ids.is_empty():
        info.text = "Aucune radio detectee. Branche la radio allumee en mode USB Joystick / HID."
    else:
        info.text = "Choisis la radio. Aucune affectation d'axes n'est supposee."
    advance.disabled = device == -1

func devices_changed(_id: int, _connected: bool) -> void:
    if stage >= 0:
        mapping = saved_mapping.duplicate(true)
    stage = -1
    refresh_devices()

func select_device(index: int) -> void:
    if index < 0 or index >= devices.item_count:
        device = -1
        advance.disabled = true
        return
    if stage >= 0:
        mapping = saved_mapping.duplicate(true)
    device = devices.get_item_id(index)
    advance.disabled = device < 0
    stage = -1
    advance.text = "Commencer la calibration"

func toggle() -> void:
    active = not active
    panel.visible = active
    if active:
        stage = -1
        refresh_devices()
        advance.text = "Commencer la calibration"
    elif stage >= 0:
        mapping = saved_mapping.duplicate(true)
        stage = -1

func ready_for_radio() -> bool:
    return device >= 0 and Input.get_connected_joypads().has(device) and identity(device) == signature and mapping.size() == 3

func sample() -> Array:
    var values: Array = []
    for axis in range(AXES):
        values.append(Input.get_joy_axis(device, axis))
    return values

static func normalize_axis(raw: float, settings: Dictionary) -> float:
    var difference := raw-float(settings.center)
    var span := float(settings.high)-float(settings.center) if difference >= 0 else float(settings.center)-float(settings.low)
    if span < 0.05:
        return 0.0
    var value := clampf(difference/span, -1.0, 1.0)
    var deadzone := 0.025
    value = signf(value)*maxf(0.0,(absf(value)-deadzone)/(1.0-deadzone))
    value *= float(settings.sign)
    # Light expo around centre; no time-based filtering for radio sticks.
    return 0.8*value + 0.2*value*value*value

func controls() -> Vector3:
    if not ready_for_radio() or active:
        return Vector3.ZERO
    var result := Vector3.ZERO
    for i in range(3):
        var settings: Dictionary = mapping[ROLES[i]]
        result[i] = normalize_axis(Input.get_joy_axis(device, int(settings.axis)), settings)
    return result

func next_stage() -> void:
    if device < 0:
        return
    if stage == -1:
        saved_mapping = mapping.duplicate(true)
        stage = 0
        info.text = "Centre les manches de roulis, profondeur et direction, trims au neutre.\nLe manche des gaz n'est pas utilise. Clique quand les manches sont immobiles."
        advance.text = "Memoriser les neutres"
    elif stage == 0:
        centers = sample()
        lows = centers.duplicate()
        highs = centers.duplicate()
        stage = 1
        info.text = "Deplace les trois commandes jusqu'aux deux butees, plusieurs fois.\nPuis recentre les manches et clique pour continuer."
        advance.text = "Butees parcourues : continuer"
    elif stage == 1:
        mapping = {}
        stage = 2
        info.text = DIRECTIONS[0] + "\nLes autres manches doivent rester au neutre."
        advance.text = "Affecter cette commande"
    elif stage >= 2 and stage <= 4:
        var values := sample()
        var candidate := -1
        var best := 0.0
        var second := 0.0
        for axis in range(AXES):
            var amplitude := absf(float(values[axis])-float(centers[axis]))
            if amplitude > best:
                second = best
                best = amplitude
                candidate = axis
            elif amplitude > second:
                second = amplitude
        if candidate < 0 or best < .2 or second > best*.65:
            info.text = DIRECTIONS[stage-2] + "\nUn seul axe doit bouger nettement. Reessaie."
            return
        for settings in mapping.values():
            if int(settings.axis) == candidate:
                info.text = "Cet axe est deja affecte. Bouge uniquement la commande demandee.\n" + DIRECTIONS[stage-2]
                return
        if float(highs[candidate])-float(centers[candidate]) < .1 or float(centers[candidate])-float(lows[candidate]) < .1:
            stage = -1
            mapping = saved_mapping.duplicate(true)
            info.text = "Les deux butees de cet axe n'ont pas ete mesurees. Recommence la calibration."
            advance.text = "Recommencer"
            return
        mapping[ROLES[stage-2]] = {"axis":candidate,"center":centers[candidate],"low":lows[candidate],"high":highs[candidate],"sign":signf(float(values[candidate])-float(centers[candidate]))}
        stage += 1
        if stage <= 4:
            info.text = DIRECTIONS[stage-2] + "\nLes autres manches doivent rester au neutre."
        else:
            signature = identity(device)
            var config := ConfigFile.new()
            config.set_value("radio", "signature", signature)
            config.set_value("radio", "mapping", mapping)
            var result := config.save(PROFILE_PATH)
            saved_mapping = mapping.duplicate(true)
            info.text = "Calibration terminee. Verifie le sens des gouvernes avant de voler.\nProfil sauvegarde." if result == OK else "Calibration active, mais sauvegarde impossible."
            stage = -1
            advance.text = "Recommencer"

func label() -> String:
    if ready_for_radio():
        return "Radio : " + Input.get_joy_name(device)
    if device >= 0:
        return "USB detecte — J pour calibrer"
    return "Clavier progressif — J : radio USB"

func _process(_delta: float) -> void:
    if not active or device < 0:
        return
    var values := sample()
    if stage == 1:
        for axis in range(AXES):
            lows[axis] = minf(lows[axis],values[axis])
            highs[axis] = maxf(highs[axis],values[axis])
    var lines := "Axes USB : "
    for axis in range(AXES):
        lines += "%d:%+.2f  " % [axis,values[axis]]
        if axis == 4:
            lines += "\n"
    monitor.text = lines
