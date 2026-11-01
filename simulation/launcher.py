from pathlib import Path
import subprocess
import os
import sys

root=Path(__file__).resolve().parents[1]
local_tools=root/'tools'
legacy_tools=Path.home()/'Dropbox'/'SimPlaneurPerso'/'tools'
tools=local_tools if (local_tools/'python'/'python.exe').exists() else legacy_tools
if not (tools/'python'/'python.exe').exists():
    raise RuntimeError('Outils introuvables : installer Python/JSBSim et Godot dans tools')
env=os.environ.copy()
env['APPDATA']=str(tools/'userdata/roaming')
env['LOCALAPPDATA']=str(tools/'userdata/local')
godot=next(p for p in (tools/'godot').glob('*.exe') if 'console' not in p.name)
(root/'work').mkdir(exist_ok=True)
with (root/'work/server.log').open('w') as out, (root/'work/server-errors.log').open('w') as err, (root/'work/game.log').open('w') as game_log:
    server=subprocess.Popen([str(tools/'python/python.exe'),str(root/'simulation/server.py'),'--model','topy_4522'],cwd=root,env=env,stdout=out,stderr=err,creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        import time
        time.sleep(.7)
        if server.poll() is not None:
            raise RuntimeError('JSBSim failed: see work/server-errors.log')
        subprocess.run([str(godot),'--path',str(root/'game'),'--']+sys.argv[1:],cwd=root,env=env,stdout=game_log,stderr=game_log,check=True)
    finally:
        if server.poll() is None:
            server.terminate()
        server.wait()
