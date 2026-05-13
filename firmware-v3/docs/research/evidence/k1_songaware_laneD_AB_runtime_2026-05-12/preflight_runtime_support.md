# Lane D A/B Runtime Support Preflight

Date: 2026-05-12  
Mode: ENGINEERING_VALIDATION  
Device serial: `/dev/cu.usbmodem1101` at 115200 baud  
Control path: USB serial only. No REST or WebSocket dependency used.  
Firmware/source edits: none during this validation pass.

## Runtime controls used

| Control | Value |
|---|---:|
| effect | `0x1302 K1 Waveform` |
| brightness | 160 |
| speed | 27 |
| intensity | 128 |
| saturation | 128 |
| complexity | 128 |
| variation | 0 |
| palette | `10 Vintage 01` |
| EdgeMixer | `mode=0 mirror`, `spatial=0 uniform`, `temporal=0 static`, `spread=30`, `strength=255` |

Serial JSON setup commands returned success for `setEffect`, `setBrightness`, `setSpeed`, `setIntensity`, `setSaturation`, `setComplexity`, `setVariation`, `setPalette`, and `setEdgeMixer`.

## Preflight checklist

| Check | Result | Evidence |
|---|---|---|
| `songaware status` works over serial | PASS | Returned `songAware:` and `songAware_status:` records with enabled/mode/familyMorphing/constrainedSwitching/status fields. |
| `songaware off` sets disabled/off state | PASS | Command returned `songAware: OFF` and config `enabled=false mode=off familyMorphing=false constrainedSwitching=false`. Subsequent A pre-states and final state reported `effectiveMode=off owner=none suppressed=disabled`. |
| `songaware on` enables director mode | PASS | Command returned `enabled=true mode=on familyMorphing=false constrainedSwitching=false`; subsequent balanced smoke reported `owner=director suppressed=none lastAction=parameter_update`. |
| `songaware mode balanced` works | PASS | Command returned `enabled=true mode=balanced familyMorphing=false constrainedSwitching=false`. |
| `activeEffect` unchanged for 60-second smoke | PASS | Smoke first sample: `activeEffect=0x1302`; smoke last sample: `activeEffect=0x1302`; 60 polls. |
| `automaticEffectSwitches=0` | PASS | Smoke and all A/B runs reported `automaticEffectSwitches=0`. |
| Health counters zero | PASS | Smoke health: `show_skips=0 failures=0 rmt_errors=0 underruns=0`. |
| RendererActor only applies parameter overlays | PASS | `RendererActor.cpp:2037-2061` constructs `SongAwareParams`, calls `SongAwareDirector::apply`, then copies scalar parameters back into render context. There is no effect-id selection in that block. |
| No NVS save route called | PASS | Commands used non-persistent serial setters. The persistent serial route is separate: `SerialJsonGateway.cpp:684-686` handles `saveEdgeMixer` and was not called. |

## Source audit notes

- `firmware-v3/src/core/actors/RendererActor.cpp:2037-2061` applies the song-aware result as scalar render-context fields: brightness, speed, intensity, saturation, complexity, variation, and hue.
- `firmware-v3/src/core/songaware/SongAwareDirector.cpp:94-96` stores the active effect id and resets `automaticEffectSwitches` to zero on each apply.
- `firmware-v3/src/core/songaware/SongAwareDirector.cpp:142-176` computes scalar speed/intensity/complexity deltas from current audio drive and increments `parameterUpdates` only when those parameters change.
- `firmware-v3/src/serial/SerialCLI.cpp:506-545` shows `songaware on`, `songaware off`, and `songaware mode` all force `familyMorphing=false` and `constrainedSwitching=false`.
- `firmware-v3/src/serial/SerialJsonGateway.cpp:309-352`, `452-488`, and `537-588` provide serial runtime setters used in the run.

## Preflight smoke result

| Metric | First smoke sample | Last smoke sample |
|---|---:|---:|
| activeEffect | `0x1302` | `0x1302` |
| parameterUpdates | 323992 | 329564 |
| automaticEffectSwitches | 0 | 0 |
| show_skips | 0 | 0 |
| failures | 0 | 0 |
| rmt_errors | 0 | 0 |
| underruns | 0 | 0 |
| currentSongState | dense | dense |
| confidence | 1.000 | 1.000 |

