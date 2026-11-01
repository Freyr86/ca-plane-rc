"""Generate a deliberately unvalidated RC demonstrator, in SI units."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def generate():
    def prop(name):
        return f'<property>{name}</property>'
    def value(n):
        return f'<value>{n}</value>'
    def product(*args):
        return '<product>' + ''.join(args) + '</product>'
    def term(coefficient, *names):
        return product(value(coefficient), *(prop(n) for n in names))
    def function(name, expr, moment=False, roll=False):
        factors = [prop('aero/qbar-psf'), prop('metrics/Sw-sqft')]
        if moment:
            factors.append(prop('metrics/bw-ft' if roll else 'metrics/cbarw-ft'))
        return f'<function name="aero/{name}">' + product(*factors, expr) + '</function>'
    lift = '<table><independentVar lookup="row">aero/alpha-rad</independentVar><tableData>-1.57 0\n-0.4 -0.6\n-0.2 -0.7\n-0.1 -0.2\n0 0.3\n0.1 0.8\n0.2 1.1\n0.3 0.8\n0.6 0.5\n1.57 0</tableData></table>'
    axes = {
        'LIFT': function('lift', lift),
        'DRAG': function('drag', '<sum>' + value(.025) + term(.4, 'aero/alpha-rad', 'aero/alpha-rad') + '</sum>'),
        'SIDE': function('side', term(-.5, 'aero/beta-rad')),
        'ROLL': function('roll', '<sum>' + term(-.08,'aero/beta-rad') + term(-.5,'aero/bi2vel','velocities/p-aero-rad_sec') + term(.12,'fcs/aileron-cmd-norm') + '</sum>', True, True),
        'PITCH': function('pitch', '<sum>' + value(.015) + term(-.6,'aero/alpha-rad') + term(-12,'aero/ci2vel','velocities/q-aero-rad_sec') + term(-.3,'fcs/elevator-cmd-norm') + '</sum>', True),
        'YAW': function('yaw', '<sum>' + term(.12,'aero/beta-rad') + term(-.2,'aero/bi2vel','velocities/r-aero-rad_sec') + term(-.08,'fcs/rudder-cmd-norm') + '</sum>', True, True),
    }
    xml = '''<?xml version="1.0"?>
<fdm_config name="RC Demo - UNVALIDATED" version="2.0">
 <fileheader><author>SimPlaneurPerso</author><description>Synthetic RC demonstrator. Not a measured aircraft. No validated stall or ground handling.</description></fileheader>
 <metrics>
  <wingarea unit="M2">0.5</wingarea><wingspan unit="M">2.5</wingspan><chord unit="M">0.2</chord>
  <location name="AERORP" unit="M"><x>0</x><y>0</y><z>0</z></location>
  <location name="EYEPOINT" unit="M"><x>0</x><y>0</y><z>0</z></location>
  <location name="VRP" unit="M"><x>0</x><y>0</y><z>0</z></location>
 </metrics>
 <mass_balance>
  <ixx unit="KG*M2">0.15</ixx><iyy unit="KG*M2">0.09</iyy><izz unit="KG*M2">0.23</izz>
  <emptywt unit="KG">1.5</emptywt>
  <location name="CG" unit="M"><x>0</x><y>0</y><z>0</z></location>
 </mass_balance>
 <ground_reactions/>
 <aerodynamics>'''
    xml += ''.join(f'<axis name="{axis}">{expr}</axis>' for axis,expr in axes.items())
    xml += '</aerodynamics></fdm_config>'
    dest = ROOT/'models/rc_demo/rc_demo.xml'
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(xml, encoding='utf-8')
    return dest

if __name__ == '__main__':
    print(generate())
