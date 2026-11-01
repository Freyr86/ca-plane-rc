"""Local UDP flight bridge. Unreal supplies contacts with its rendered landscape."""
import argparse
import json
import math
from pathlib import Path
import socket
import time
import jsbsim

ROOT = Path(__file__).resolve().parents[1]
DT = 1 / 120

class Simulator:
    def __init__(self, model_name='topy_4522', dt=DT, breeze=False, scene_contacts=False):
        self.model_name = model_name
        self.dt = dt
        self.breeze = breeze
        self.scene_contacts = scene_contacts
        self.flight_id = 0
        self.reset()

    def reset(self):
        self.fdm = jsbsim.FGFDMExec(None)
        self.fdm.set_debug_level(0)
        self.fdm.set_aircraft_path(str(ROOT/'models'))
        if not self.fdm.load_model(self.model_name):
            raise RuntimeError(f'Impossible de charger {self.model_name}')
        self.fdm.set_dt(self.dt)
        initial = {
            'topy_4522': {'speed_mps': 7.0, 'alpha_deg': 3.5, 'theta_deg': -1.4},
            'rc_demo': {'speed_mps': 12.0, 'alpha_deg': 0.0, 'theta_deg': -3.0},
        }.get(self.model_name, {'speed_mps': 7.0, 'alpha_deg': 3.5, 'theta_deg': -1.4})
        for key,val in {'ic/h-sl-ft':80/.3048, 'ic/vt-fps':initial['speed_mps']/.3048,
                        'ic/lat-gc-deg':0, 'ic/long-gc-deg':0,
                        'ic/psi-true-deg':0, 'ic/alpha-deg':initial['alpha_deg'],
                        'ic/theta-deg':initial['theta_deg']}.items():
            self.fdm[key] = val
        if not self.fdm.run_ic():
            raise RuntimeError('Echec initialisation JSBSim')
        if self.breeze:
            self.fdm["atmosphere/wind-north-fps"] = -2.0 / .3048
        self.north = 0.0
        self.east = 0.0
        self.grounded = False
        self.contact_state = None
        self.flight_id += 1

    def scene_command(self, msg):
        # Older clients use the flat ground and a single unnumbered reset packet.
        if not self.scene_contacts:
            if msg.get('reset'):
                self.reset()
            return
        if msg.get('flight_id') != self.flight_id:
            return
        if msg.get('reset'):
            self.reset()
            return
        contact = msg.get('contact')
        if self.grounded or not isinstance(contact, dict) or contact.get('kind') not in ('ground','tree','water','rock'):
            return
        pose = {key: float(contact[key]) for key in ('north','east','alt','roll','pitch','yaw')}
        if not all(math.isfinite(value) for value in pose.values()):
            return
        self.contact_state = self.state() | pose | dict(contact=contact['kind'],grounded=True,speed=0.,sink=0.)
        self.grounded = True
        print(f"Contact {contact['kind']} confirmé, vol {self.flight_id}",flush=True)

    def step(self, aileron=0, elevator=0, rudder=0):
        if self.grounded:
            return self.state()
        for axis,val in [('aileron',aileron),('elevator',elevator),('rudder',rudder)]:
            if not math.isfinite(val):
                val = 0
            self.fdm[f'fcs/{axis}-cmd-norm'] = max(-1,min(1,val))
        if not self.fdm.run():
            raise RuntimeError('JSBSim stopped')
        self.north += self.fdm['velocities/v-north-fps']*.3048*self.dt
        self.east += self.fdm['velocities/v-east-fps']*.3048*self.dt
        self.grounded = not self.scene_contacts and self.fdm['position/h-sl-ft'] <= 0
        return self.state()

    def state(self):
        if self.contact_state is not None:
            return self.contact_state
        f = self.fdm
        altitude = f['position/h-sl-ft']*.3048
        return dict(north=self.north,east=self.east,alt=altitude if self.scene_contacts else max(0,altitude),
                    roll=f['attitude/phi-rad'],pitch=f['attitude/theta-rad'],yaw=f['attitude/psi-rad'],
                    speed=f['velocities/vt-fps']*.3048,sink=f['velocities/v-down-fps']*.3048,
                    alpha=f['aero/alpha-rad'],beta=f['aero/beta-rad'],model=self.model_name,
                    time=f.get_sim_time(),grounded=self.grounded,flight_id=self.flight_id)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--port',type=int,default=5507)
    parser.add_argument('--model',choices=('topy_4522','rc_demo'),default='topy_4522')
    parser.add_argument("--breeze", action="store_true")
    parser.add_argument("--scene-contacts", action="store_true")
    args=parser.parse_args()
    sim=Simulator(args.model, breeze=args.breeze, scene_contacts=args.scene_contacts)
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1',args.port))
    sock.setblocking(False)
    peer=None
    controls={}
    paused=False
    last_input=time.monotonic()
    deadline=time.monotonic()
    last_status=deadline
    packets_received=0
    telemetry_sent=0
    print(f'JSBSim pret sur 127.0.0.1:{args.port}',flush=True)
    while True:
        pending=False
        try:
            # Bound the receive batch so a busy renderer cannot starve the
            # fixed-step simulation and telemetry stream.
            for _ in range(4):
                data,addr=sock.recvfrom(4096)
                try:
                    msg=json.loads(data)
                    if not isinstance(msg,dict):
                        continue
                    parsed={k:float(msg.get(k,0)) for k in ('aileron','elevator','rudder')}
                    if not all(math.isfinite(v) for v in parsed.values()):
                        continue
                    controls=parsed
                    paused=bool(msg.get("paused",False))
                    peer=addr
                    pending=True
                    packets_received += 1
                    last_input=time.monotonic()
                    sim.scene_command(msg)
                except (KeyError,ValueError,TypeError,OverflowError,UnicodeDecodeError):
                    continue
        except (BlockingIOError, ConnectionResetError):
            pass
        if time.monotonic()-last_input > .5:
            controls={}
        state=sim.step(**controls) if peer and not paused else sim.state()
        # Stream telemetry at the simulation rate once a renderer is known.
        # This keeps the front-end connected even while shader compilation or a
        # short frame stall prevents it from sending a fresh control packet.
        if peer:
            sock.sendto(json.dumps(state,allow_nan=False).encode(),peer)
            telemetry_sent += 1
        now=time.monotonic()
        if now-last_status >= 1.0:
            print(f'UDP rx={packets_received} tx={telemetry_sent} peer={peer}', flush=True)
            last_status=now
            packets_received=0
            telemetry_sent=0
        deadline += DT
        if deadline < now-.25:
            deadline=now
        time.sleep(max(0,deadline-now))

if __name__ == '__main__':
    main()
