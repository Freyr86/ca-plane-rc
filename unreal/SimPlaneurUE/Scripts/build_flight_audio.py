"""Build missing flight loops, or rebuild only the loop names passed as arguments."""
import array
import math
import random
import sys
import tempfile
import wave
from pathlib import Path

import unreal


RATE = 32000
FADE = RATE // 2


def render(name, seconds, path):
    rng = random.Random(275)
    slow = mid = bright = 0.0
    phase = 0.0
    flutter = hiss = 0.0
    samples = []
    for i in range(RATE * seconds + FADE):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        slow += .021 * (noise - slow)
        mid += .16 * (noise - mid)
        bright += .48 * (noise - bright)
        if name == "SiteWind":
            gust = .78 + .14 * math.sin(1.07*t) + .08 * math.sin(2.41*t)
            value = gust * (2.1*slow + .26*mid)
        elif name == "WingRush":
            # Broad, smooth air noise with irregular swells instead of a pitched partial.
            flutter += .00045 * (noise - flutter)
            hiss += .32 * ((bright-mid) - hiss)
            value = (.78*hiss + .22*(mid-slow)) * (1 + 8*flutter)
        elif name == "VarioTone":
            value = math.sin(math.tau * 640 * t)
        elif name == "ServoGear":
            # A small geared actuator heard through the skin of the fuselage.
            phase += math.tau * (215 + 6*math.sin(21*t) + 5*mid) / RATE
            teeth = max(0, math.sin(phase*2))**2 - .25
            value = (.23*math.sin(phase) + .12*teeth + .09*(mid-slow))*(.87+.13*math.sin(phase*.125))
        else:
            phase += math.tau * (186 + 1.4*math.sin(4*t) + 1.1*slow) / RATE
            value = (.44*math.sin(phase) + .16*math.sin(2*phase+.1) + .12*math.sin(3*phase)
                     + .08*math.sin(6*phase) + .045*math.sin(12*phase) + .24*(mid-slow))
        samples.append(value)
    end = len(samples) - FADE
    for i in range(FADE):
        mix = .5 - .5*math.cos(math.pi*i/FADE)
        samples[i] = samples[end+i]*(1-mix) + samples[i]*mix
    del samples[end:]
    scale = 27500/max(map(abs, samples))
    with wave.open(str(path), "wb") as out:
        out.setparams((1, 2, RATE, 0, "NONE", ""))
        out.writeframes(array.array("h", (round(x*scale) for x in samples)).tobytes())


def build(selected):
    library = unreal.EditorAssetLibrary
    with tempfile.TemporaryDirectory() as temp:
        for name, seconds in (("SiteWind", 12), ("WingRush", 6), ("ElectricProp", 4), ("ServoGear", 2), ("VarioTone", 2)):
            asset = "/Game/FlightAudio/" + name
            if (selected and name not in selected) or (not selected and library.does_asset_exist(asset)):
                continue
            path = Path(temp, name + ".wav")
            render(name, seconds, path)
            task = unreal.AssetImportTask()
            task.filename, task.destination_path = str(path), "/Game/FlightAudio"
            task.automated, task.replace_existing = True, True
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            sound = library.load_asset(asset)
            if not sound:
                raise RuntimeError("Failed to import " + asset)
            sound.set_editor_property("looping", True)
            if not library.save_loaded_asset(sound, only_if_is_dirty=False):
                raise RuntimeError("Failed to save " + asset)


build(sys.argv[1:])
