# Director Runtime Smoke

RBDO label: GROUNDED

## Hardware And Build

- Device port: `/dev/cu.usbmodem1101`
- Device MAC: `b4:3a:45:a5:87:f8`
- Firmware env: `esp32dev_audio_esv11_k1v2_32khz`
- Upload result: success

## Runtime Setup

Initial fixed controls:

- effect: `0x1302 K1 Waveform`
- brightness: `160`
- speed: `27`
- intensity: `128`
- saturation: `128`
- complexity: `128`
- variation: `0`
- palette: `10 Vintage 01`
- EdgeMixer: `mirror`

## Audio Stimulus

Used local file:

```text
/Users/spectrasynq/Music/Music/Media.localized/Music/Kiro tv/Unknown Album/Tech House drums Loop - 124 BPM  + Bass.mp3
```

`afinfo` duration: approximately `499.032000` seconds.

## Results

Director entered `drop` state and selected:

- target effect: `0x0407 LGP Photonic Crystal`
- selected family: `advanced_optical`
- selected visual language: `photonic_drop_texture`
- reason: `drop_impact`
- confidence: `1.000`

Observed corrected medium-candidate automatic switch:

```text
0x1302 K1 Waveform -> 0x0407 LGP Photonic Crystal
automaticEffectSwitches=1
```

This stayed within the 2 switches/minute limit:

- First corrected medium-candidate switch: `166233 ms`
- Count within this corrected smoke: `1`

## Manual Suppression

Manual `setEffect` suppression was proven during the prior same-session smoke before the allowlist correction. On the next Director evaluation, status reported:

```text
owner=manual suppressed=manual_owner
```

## Clean Disable

`songaware off` reported:

```text
enabled=false mode=off constrainedSwitching=false suppressed=disabled
```

Runtime effect and controls were restored to:

```text
effect=0x1302 brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0 palette=10
```

## Smoke Result

PASS: Director V1 produced real visual-language/effect decisions, not parameter-only modulation.
