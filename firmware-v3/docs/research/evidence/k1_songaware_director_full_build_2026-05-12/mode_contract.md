RBDO label: GROUNDED

Modes:
- off: disabled, switching off, suppressed=disabled, no Director action.
- parameter: fixed effect, parameter overlays only, automaticEffectSwitches remains zero.
- director: classifier + visual-language/effect target + parameter envelopes + gated switching.

Serial precheck:
- `songaware mode parameter` accepted.
- `songaware switching on` while parameter mode returned: `songAware switching rejected: Director mode is required`.
- `songaware mode director` accepted.
- `songaware switching on` accepted only in Director mode and set constrainedSwitching=true.

Runtime restore:
- `songaware restore` returned SongAware to off and restored fixed baseline controls.
