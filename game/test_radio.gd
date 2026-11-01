extends SceneTree
const Radio = preload("res://radio_input.gd")
func _initialize() -> void:
    var settings := {"center":0.1,"low":-0.7,"high":0.9,"sign":1.0}
    assert(is_zero_approx(Radio.normalize_axis(0.1,settings)))
    assert(is_zero_approx(Radio.normalize_axis(0.11,settings)))
    assert(is_equal_approx(Radio.normalize_axis(0.9,settings),1.0))
    assert(is_equal_approx(Radio.normalize_axis(-0.7,settings),-1.0))
    assert(is_equal_approx(Radio.normalize_axis(1.5,settings),1.0))
    settings.sign = -1.0
    assert(is_equal_approx(Radio.normalize_axis(0.9,settings),-1.0))
    settings.high = settings.center
    assert(is_zero_approx(Radio.normalize_axis(0.5,settings)))
    print("RADIO TESTS: neutral, deadzone, asymmetric ranges, inversion, clamp, invalid range OK")
    call_deferred("test_controller_selection")

func test_controller_selection() -> void:
    var radio = Radio.new()
    root.add_child(radio)
    # Regression: selecting a device after a disabled refresh must enable Start.
    radio.devices.clear()
    radio.devices.add_item("RM TX16s", 7)
    radio.device = -1
    radio.advance.disabled = true
    radio.select_device(0)
    assert(radio.device == 7)
    assert(not radio.advance.disabled, "Start remains disabled after controller selection")
    radio.advance.pressed.emit()
    assert(radio.stage == 0, "Start must enter neutral calibration")
    radio.select_device(-1)
    assert(radio.advance.disabled and radio.device == -1)
    radio.free()
    print("RADIO UI TEST: controller selection enables Start and begins calibration OK")
    quit()
