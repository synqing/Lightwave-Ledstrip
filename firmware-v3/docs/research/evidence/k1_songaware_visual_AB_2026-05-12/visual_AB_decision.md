# Visual A/B Decision

RBDO label: GROUNDED

Decision: `FAIL_PARAMETER_MODE_VISIBLE_GAIN`

## Basis

Serial/runtime behaviour remained healthy:

- Condition B produced autonomous parameter activity: `parameter_update_delta=25,604`.
- Effect ID remained fixed at `0x1302 K1 Waveform`.
- `automaticEffectSwitches=0`.
- `show_skips=0`, `failures=0`, `rmt_errors=0`, `underruns=0`.
- Final `songaware off` was confirmed disabled/off over serial.

Captain's live visual assessment rejected the result:

```text
That was fucking shit. not once did it ever switch effects
```

Because automatic effect-ID switching was explicitly prohibited in this test, the absence of effect switching is not a runtime violation. It is the product conclusion: fixed-effect parameter-only mode does not create the expected "K1 understands the song" visible behaviour.

## Decision classification

- Not `PASS_PARAMETER_MODE_VISUAL_REVIEW`: Captain rejected the visible result and no score threshold was met.
- Not `FAIL_PARAMETER_MODE_VISUAL_CHAOS`: no specific jarring/discontinuous moments were scored; the failure was lack of visible/product gain.
- Not `FAIL_HEALTH_REGRESSION`: hard health counters stayed zero.
- Not `FAIL_EFFECT_ID_STABILITY`: effect ID remained stable by design.
- Not `BLOCKED_NO_VISUAL_REVIEW`: Captain did perform live visual review and gave a decisive negative verdict.

## Stop point

Stopped after the visual A/B decision. No further tracks, tuning, implementation, family morphing, constrained switching, NVS saves, REST/WS controls, or production default changes were performed.

