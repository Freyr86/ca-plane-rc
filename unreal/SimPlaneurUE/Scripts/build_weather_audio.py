"""Original synthesized weather recordings; no third-party audio dependencies."""
import array
import math
import random
import wave
from pathlib import Path
import unreal

RATE = 32000
DEST = Path(unreal.Paths.project_dir()) / 'SourceAssets' / 'WeatherAudio'
DEST.mkdir(parents=True, exist_ok=True)

def render(name, seconds, loop):
    rng = random.Random(5438)
    samples = []
    low = mid = soft = gust = tap = 0.
    cross = RATE // 2 if loop else 0
    for i in range(RATE*seconds+cross):
        t = i/RATE
        noise = rng.uniform(-1, 1)
        low += .012*(noise-low)
        mid += .15*(noise-mid)
        soft += .42*(noise-soft)
        gust += .0004*(noise-gust)
        if name == 'RainShower':
            tap *= .73
            if rng.random() < .002: tap += rng.uniform(-.28, .28)
            value = (.55*(soft-mid)+.45*mid+tap)*(.8+.12*math.sin(t*.71)+gust*3)
        elif name == 'FoliageRustle':
            value = (mid-low)*(.5+.22*math.sin(t*1.11)+.12*math.sin(t*2.73)+gust*4)
        else:
            attack = 1-math.exp(-t*18)
            tail = math.exp(-t*.53)*(1-min(1,t/seconds))
            rolls = 1+.35*math.sin(t*4.4)+.2*math.sin(t*9.1)
            value = (4.4*low+.35*mid)*attack*tail*rolls
        samples.append(value)
    if loop:
        end = len(samples)-cross
        for i in range(cross):
            mix = .5-.5*math.cos(math.pi*i/cross)
            samples[i] = samples[end+i]*(1-mix)+samples[i]*mix
        samples = samples[:end]
    scale = 23000/max(map(abs,samples))
    path = DEST/(name+'.wav')
    with wave.open(str(path),'wb') as out:
        out.setparams((1,2,RATE,0,'NONE',''))
        out.writeframes(array.array('h',(round(v*scale) for v in samples)).tobytes())
    return path

for name,seconds,loop in [('RainShower',12,True),('FoliageRustle',10,True),('ThunderRoll',7,False)]:
    task = unreal.AssetImportTask()
    task.filename = str(render(name,seconds,loop))
    task.destination_path = '/Game/FlightAudio'
    task.automated = task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound = unreal.EditorAssetLibrary.load_asset('/Game/FlightAudio/'+name)
    if not sound: raise RuntimeError(name)
    sound.set_editor_property('looping',loop)
    unreal.EditorAssetLibrary.save_loaded_asset(sound,False)
    unreal.log('WEATHER_AUDIO ready '+name)
