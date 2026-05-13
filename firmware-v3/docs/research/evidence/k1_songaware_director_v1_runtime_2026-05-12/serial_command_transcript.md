# Serial Command Transcript

RBDO label: GROUNDED

Port: `/dev/cu.usbmodem1101`  
Baud: `115200`  
Device MAC during upload: `b4:3a:45:a5:87:f8`

## Initial Status

Command:

```text
songaware status
```

Key output:

```text
songAware: enabled=false mode=off familyMorphing=false constrainedSwitching=false
songAware_status: effectiveMode=off owner=none suppressed=disabled state=silence activeEffect=0x1302 activeEffectName="K1 Waveform" parameterUpdates=0 automaticEffectSwitches=0
songAware_health: show_skips=0 failures=0 rmt_errors=0 underruns=0
```

## Fixed Runtime Controls

Serial JSON was used over the serial monitor, not REST or WS.

Commands:

```text
{"type":"setEffect","requestId":"ctrl-effect","effectId":4866}
{"type":"setBrightness","requestId":"ctrl-bright","value":160}
{"type":"setSpeed","requestId":"ctrl-speed","value":27}
{"type":"setIntensity","requestId":"ctrl-int","value":128}
{"type":"setSaturation","requestId":"ctrl-sat2","value":128}
{"type":"setComplexity","requestId":"ctrl-comp2","value":128}
{"type":"setVariation","requestId":"ctrl-var2","value":0}
{"type":"setPalette","requestId":"ctrl-pal2","paletteId":10}
{"type":"setEdgeMixer","requestId":"ctrl-edge2","mode":0}
{"type":"parameters.get","requestId":"get2"}
```

Confirmed:

```text
brightness=160 speed=27 paletteId=10 paletteName="Vintage 01" effectId=4866 intensity=128 saturation=128 complexity=128 variation=0
edge_mixer: mode=mirror
```

## Director Command Gates

Commands:

```text
songaware reset
songaware switching on
songaware mode director
songaware switching on
songaware status
```

Key output:

```text
songAware switching rejected: Director mode is required
songAware: enabled=true mode=director familyMorphing=false constrainedSwitching=false
songAware switching: ON (constrained allowlist only)
songAware: enabled=true mode=director familyMorphing=false constrainedSwitching=true
```

## Runtime Decision

Controlled audio file:

```text
/Users/spectrasynq/Music/Music/Media.localized/Music/Kiro tv/Unknown Album/Tech House drums Loop - 124 BPM  + Bass.mp3
```

Pre-switch status:

```text
songAware_status: effectiveMode=director owner=director suppressed=dwell state=drop confidence=1.000 lastAction=parameter_update activeEffect=0x1302 activeEffectName="K1 Waveform" parameterUpdates=319 automaticEffectSwitches=0
songAware_director: selectedEffect=0x0407 selectedFamily=advanced_optical selectedVisualLanguage=photonic_drop_texture dwellRemainingMs=4689 lastSwitchReason=drop_impact
songAware_audio: RMS=1.000 flux=0.523 BPM=126.0 confidence=1.000
```

Switch log:

```text
[166232][INFO][Renderer] Effect changed: 0x1302 (K1 Waveform) -> 0x0407 (LGP Photonic Crystal)
[166233][INFO][Renderer] SongAware Director switch state=drop confidence=1.000 prev=0x1302 target=0x0407 family=advanced_optical language=photonic_drop_texture reason=drop_impact
```

Post-switch status:

```text
songAware_status: effectiveMode=director owner=director suppressed=same_effect state=drop confidence=1.000 activeEffect=0x0407 activeEffectName="LGP Photonic Crystal" parameterUpdates=2432 automaticEffectSwitches=1
songAware_director: selectedEffect=0x0407 selectedFamily=advanced_optical selectedVisualLanguage=photonic_drop_texture lastSwitchAtMs=166233 cooldownRemainingMs=5559 lastSwitchReason=drop_impact
songAware_health: show_skips=0 failures=0 rmt_errors=0 underruns=0
```

## Manual Ownership Suppression

Commands:

```text
{"type":"setEffect","requestId":"manual-effect","effectId":4866}
songaware status
```

Key output after next Director evaluation:

```text
songAware_status: effectiveMode=director owner=manual suppressed=manual_owner state=drop confidence=1.000 activeEffect=0x1302 activeEffectName="K1 Waveform" automaticEffectSwitches=1
songAware_director: selectedEffect=<allowlisted-target> lastSwitchReason=drop_impact
```

Note: manual suppression was proven during the first same-session smoke before the allowlist correction. The corrected medium-candidate smoke then proved the requested earlier-cycled-effect target `0x0407`.

## Clean Disable And Restore

Commands:

```text
songaware off
{"type":"setEffect","requestId":"restore-effect","effectId":4866}
{"type":"parameters.get","requestId":"restore-get"}
songaware status
vp stack
s
```

Final key output:

```text
songAware: enabled=false mode=off familyMorphing=false constrainedSwitching=false
songAware_status: effectiveMode=off owner=none suppressed=disabled activeEffect=0x1302 activeEffectName="K1 Waveform"
songAware_health: show_skips=0 failures=0 rmt_errors=0 underruns=0
parameters: brightness=160 speed=27 paletteId=10 paletteName="Vintage 01" effectId=4866 intensity=128 saturation=128 complexity=128 variation=0
```
