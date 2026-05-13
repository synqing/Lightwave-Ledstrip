RBDO label: GROUNDED

Parameter envelopes are support layer only.

Targets:
- speed
- intensity
- complexity

Parameter mode:
- Native test proves parameter mode changes parameters and keeps `automaticEffectSwitches=0`.

Director runtime:
- Parameter updates occurred alongside visual-language selection.
- During first Director run, `parameterUpdates=1976` while `automaticEffectSwitches=1`.
- Later Director run reached `parameterUpdates=29155` while switching among allowlisted effects.

Restore:
- `parameterUpdates=0` after `songaware restore`.
