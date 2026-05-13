# Parameter Activity A vs B

## Summary

Condition A showed no autonomous parameter activity. Condition B showed autonomous parameter update activity on all three tracks while holding the fixed effect id.

| Track | A parameter delta | A changes/min | B parameter delta | B changes/min | B lastAction evidence |
|---|---:|---:|---:|---:|---|
| ambient / low-energy intro-heavy | 0 | 0.0 | 25,560 | 6,262.2 | `parameter_update` observed during run and pre-state |
| build/drop electronic | 0 | 0.0 | 26,741 | 6,197.1 | `parameter_update` observed during run and end-state |
| dense / high-energy | 0 | 0.0 | 30,210 | 6,317.8 | `parameter_update` observed during run and end-state |

## Parameter output visibility

`vp stack` reported the persisted base controls as fixed in both A and B:

| Field | A/B persisted value |
|---|---:|
| brightness | 160 |
| speed | 27 |
| intensity | 128 |
| saturation | 128 |
| complexity | 128 |
| variation | 0 |

The current serial status does not expose the per-frame post-overlay speed/intensity/complexity/brightness values produced inside `SongAwareDirector::apply`. Evidence for autonomous activity is therefore the `parameterUpdates` counter, `lastAction=parameter_update`, `owner=director`, and `currentSongState` transitions, not a direct serial dump of post-overlay output values.

## Runtime behaviour observed

| Condition | Enabled/mode | Family morphing | Constrained switching | Owner/action behaviour |
|---|---|---|---|---|
| A | `enabled=false mode=off` | false | false | `owner=none`, `lastAction=none`, `parameterUpdates` unchanged |
| B | `enabled=true mode=balanced` | false | false | `owner=director` during confident audio; `lastAction=parameter_update`; `parameterUpdates` increased |

## Source support

- `SongAwareDirector.cpp:142-176` computes speed/intensity/complexity deltas and increments `parameterUpdates` when changed.
- `RendererActor.cpp:2037-2061` applies the resulting scalar overlay to the render context only.

