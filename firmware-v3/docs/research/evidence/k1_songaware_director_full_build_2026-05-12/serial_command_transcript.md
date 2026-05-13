RBDO label: GROUNDED

Key serial transcript excerpts:

Preflight/off:
- `songaware status`
- `enabled=false mode=off`
- `effectiveMode=off owner=none suppressed=disabled`
- `activeEffect=0x1302 activeEffectName="K1 Waveform"`
- `automaticEffectSwitches=0`

Mode controls:
- `songaware mode parameter`
- `songaware switching on` -> `songAware switching rejected: Director mode is required`
- `songaware mode director`
- `songaware switching on` -> `songAware switching: ON (constrained allowlist only)`

Policy:
- `postEnableGraceMs=4000`
- `minimumDwellMs=8000`
- `switchCooldownMs=20000`
- `maxSwitchesPerWindow=2`

Restore:
- `songaware restore`
- `songAware: RESTORED safe baseline (0x1302, fixed controls, off)`
- final active effect `0x1302 K1 Waveform`.
