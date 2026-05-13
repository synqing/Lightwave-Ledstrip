# Serial Runtime Log

RBDO label: GROUNDED

## Preflight Status

```text
songAware: enabled=false mode=off familyMorphing=false constrainedSwitching=false sensitivity=1.000 intensityScalar=1.000 motionScalar=1.000 confidenceFloor=0.200
songAware_status: effectiveMode=off owner=none suppressed=disabled state=silence confidence=0.000 lastAction=none activeEffect=0x1302 activeEffectName="K1 Waveform" parameterUpdates=0 automaticEffectSwitches=0 lastDecisionAtMs=0
songAware_director: selectedEffect=0xFFFF selectedFamily=none selectedVisualLanguage=none lastSwitchAtMs=0 dwellRemainingMs=0 cooldownRemainingMs=0 lastSwitchReason=none
songAware_health: show_skips=0 failures=0 rmt_errors=0 underruns=0
```

## Fixed Controls

```text
{"type":"setEffect","requestId":"pre_effect","success":true,"data":{"effectId":4866,"name":"K1 Waveform"}}
{"type":"setBrightness","requestId":"pre_brightness","success":true,"data":{"brightness":160}}
{"type":"setSpeed","requestId":"pre_speed","success":true,"data":{"speed":27}}
{"type":"setIntensity","requestId":"pre_intensity2","success":true,"data":{"intensity":128}}
{"type":"setSaturation","requestId":"pre_saturation2","success":true,"data":{"saturation":128}}
{"type":"setComplexity","requestId":"pre_complexity2","success":true,"data":{"complexity":128}}
{"type":"setVariation","requestId":"pre_variation2","success":true,"data":{"variation":0}}
{"type":"setPalette","requestId":"pre_palette2","success":true,"data":{"paletteId":10,"name":"Vintage 01"}}
{"type":"setEdgeMixer","requestId":"pre_edge2","success":true,"data":{"mode":0,"modeName":"mirror","spread":30,"strength":255,"spatial":0,"spatialName":"uniform","temporal":0,"temporalName":"static"}}
```

One initial `setIntensity` line produced `InvalidInput` because several serial JSON commands were sent too tightly together. The command was resent and accepted before the run.

## Director Enable

```text
songAware: enabled=true mode=director familyMorphing=false constrainedSwitching=false sensitivity=1.000 intensityScalar=1.000 motionScalar=1.000 confidenceFloor=0.200
songAware switching: ON (constrained allowlist only)
songAware: enabled=true mode=director familyMorphing=false constrainedSwitching=true sensitivity=1.000 intensityScalar=1.000 motionScalar=1.000 confidenceFloor=0.200
songAware_status: effectiveMode=director owner=none suppressed=low_confidence state=silence confidence=1.000 lastAction=switch_suppressed activeEffect=0x1302 activeEffectName="K1 Waveform" parameterUpdates=0 automaticEffectSwitches=0 lastDecisionAtMs=196865
songAware_health: show_skips=0 failures=0 rmt_errors=0 underruns=0
```

## Automatic Switch Logs

```text
[208446][INFO][Renderer] Effect changed: 0x1302 (K1 Waveform) -> 0x0202 (LGP Modal Resonance)
[208447][INFO][Renderer] SongAware Director switch state=ambient confidence=1.000 prev=0x1302 target=0x0202 family=interference language=calm_modal_resonance reason=ambient_posture

[239706][INFO][Renderer] Effect changed: 0x0202 (LGP Modal Resonance) -> 0x0407 (LGP Photonic Crystal)
[239706][INFO][Renderer] SongAware Director switch state=drop confidence=1.000 prev=0x0202 target=0x0407 family=advanced_optical language=photonic_drop_texture reason=drop_impact
```

## Mid-run Status

```text
songAware_status: effectiveMode=director owner=director suppressed=same_effect state=drop confidence=1.000 lastAction=parameter_update activeEffect=0x0407 activeEffectName="LGP Photonic Crystal" parameterUpdates=5543 automaticEffectSwitches=2 lastDecisionAtMs=252285
songAware_director: selectedEffect=0x0407 selectedFamily=advanced_optical selectedVisualLanguage=photonic_drop_texture lastSwitchAtMs=239706 dwellRemainingMs=0 cooldownRemainingMs=7046 lastSwitchReason=drop_impact
songAware_health: show_skips=0 failures=0 rmt_errors=0 underruns=0
songAware_audio: RMS=0.510 flux=0.116 BPM=122.0 confidence=1.000
```

## End Restore

```text
songAware: OFF
{"type":"setEffect","requestId":"restore_effect","success":true,"data":{"effectId":4866,"name":"K1 Waveform"}}
[275356][INFO][Renderer] Effect changed: 0x0407 (LGP Photonic Crystal) -> 0x1302 (K1 Waveform)
songAware_status: effectiveMode=off owner=none suppressed=disabled state=silence confidence=0.516 lastAction=none activeEffect=0x1302 activeEffectName="K1 Waveform" parameterUpdates=7820 automaticEffectSwitches=2 lastDecisionAtMs=274948
songAware_health: show_skips=0 failures=0 rmt_errors=0 underruns=0
```
