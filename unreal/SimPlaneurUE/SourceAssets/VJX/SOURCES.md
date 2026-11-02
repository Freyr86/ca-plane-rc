# VJX F3F — 2022

Design by Jochen Guenzel and Mario Perner, https://github.com/jxjo/VJX at a67bb119134d5136f435b9c24e8a8791631088b9.
The GLB and its painted atlas are simulator adaptations under CC BY-SA 4.0 (see LICENSE.md). The wing uses the left VJX half of the FLZ comparison, mirrored. Pod and V-tail sections are reconstructed. Paint is redrawn from the three WC2022 photos, not an original texture. Unphotographed details are approximate.

To regenerate the GLB, run simulation/build_vjx_visual.py with the upstream checkout path (Python, NumPy, Pillow).

The flight preset is experimental. It uses the published wing dimensions, the recommended initial CG of 98 mm from the root leading edge (VJX Settings, April 2023), and an assumed flying mass of 2.73 kg. Drag and moment derivatives are adapted from the simulator's K3600 baseline. Inertias, aileron authority and overall response require flight validation.

The additional down-elevator pitching moment is a simulator tuning estimate. It builds progressively with forward stick. The regression case starts already inverted in still air at stock CG and control gains, with neutral flaps. It checks sustained descending flight, not the entry roll or a measured inverted polar.
