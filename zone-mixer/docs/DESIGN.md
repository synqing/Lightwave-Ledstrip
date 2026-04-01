---
abstract: "Zone Mixer hardware design — AtomS3+PaHub controller with 4 Scroll wheels and 2 8Encoder modules in a DJ-mixer paradigm for 3-zone K1 creative control. Contains channel map, control mapping, I2C specs, and implementation phases."
---

# Zone Mixer — Hardware Design Document

## Overview

A dedicated physical controller for the K1 Zone Composer, designed as a creative mixing instrument rather than a generic parameter surface. Uses the DJ mixer paradigm: one scroll wheel per zone (like a channel fader) plus encoder banks for selection and mixing.

## Hardware Stack

```
AtomS3 (ESP32-S3, 240MHz dual-core, 0.85" display)
  └── Grove I2C (GPIO 2 SDA / GPIO 1 SCL, 400kHz)
       └── PaHub v2.1 (PCA9548A, addr 0x70, 6 channels)
            ├── CH0: Unit-Scroll → Zone 1 brightness (cyan)
            ├── CH1: Unit-Scroll → Zone 2 brightness (green)
            ├── CH2: Unit-Scroll → Zone 3 brightness (orange)
            ├── CH3: Unit-Scroll → Master brightness (white)
            ├── CH4: M5Rotate8  → Zone selection bank
            └── CH5: M5Rotate8  → Mix & config
```

## Control Mapping

### 4x Scroll Wheels — Zone Channel Faders

| Channel | Role | Wheel | Button | LED |
|---------|------|-------|--------|-----|
| CH0 | Zone 1 brightness (0-255) | Continuous sweep | Tap: zone 1 enable/disable | Cyan at brightness % |
| CH1 | Zone 2 brightness (0-255) | Continuous sweep | Tap: zone 2 enable/disable | Green at brightness % |
| CH2 | Zone 3 brightness (0-255) | Continuous sweep | Tap: zone 3 enable/disable | Orange at brightness % |
| CH3 | Master brightness (0-255) | Continuous sweep | Tap: toggle audio reactivity | White at brightness % |

No modes. Each wheel ALWAYS controls its assigned brightness. Button ALWAYS toggles its zone.

### 8Encoder #1 (CH4) — Zone Selection Bank

| Encoder | Role | Button | LED |
|---------|------|--------|-----|
| E0 | Zone 1 effect select (scroll ±1) | Tap: reset default | Zone 1 cyan |
| E1 | Zone 2 effect select | Same | Zone 2 green |
| E2 | Zone 3 effect select | Same | Zone 3 orange |
| E3 | Master speed (1-100) | Tap: reset default | Blue |
| E4 | Zone 1 speed/palette (button toggle) | Tap: speed↔palette | Blue/Green |
| E5 | Zone 2 speed/palette | Same | Blue/Green |
| E6 | Zone 3 speed/palette | Same | Blue/Green |
| E7 | Spare / user-assignable | — | Dark |

LED 8 (status): Zone system enabled = green, disabled = dark.

### 8Encoder #2 (CH5) — Mix & Config

| Encoder | Role | Button | LED |
|---------|------|--------|-----|
| E0 | EdgeMixer mode (7 discrete) | Tap: toggle enable | Mode colour |
| E1 | EdgeMixer spread (0-60°) | Tap: reset 0° | Warm amber |
| E2 | EdgeMixer strength (0-255) | Tap: reset 0 | Cool blue |
| E3 | EdgeMixer spatial/temporal | Tap: cycle 4 states | State colour |
| E4 | Zone count (1-3) | Tap: cycle layout preset | Green |
| E5 | Transition type | Tap: trigger transition | Flash |
| E6 | Camera mode | Tap: toggle | Orange/dark |
| E7 | Preset slot (0-7) | Tap: load. Hold: save | White/dark |

LED 8 (status): WebSocket connected = green, connecting = blue, disconnected = red.

## Communication

- **WiFi:** STA mode, connect to K1's AP (`LightwaveOS-AP` at 192.168.4.1, open network)
- **WebSocket:** `ws://192.168.4.1:80/ws`, JSON protocol
- **Commands:** `parameters.set`, `zone.setBrightness`, `zone.setSpeed`, `zone.setEffect`, `zone.setPalette`, `zone.enableZone`, `edge_mixer.set`, `effects.setCurrent`
- **Feedback:** `broadcastStatus` (5s), `broadcastZoneState` (4Hz after change)
- **Rate limiting:** 50ms per-parameter throttle, 1-second echo holdoff (Tab5 pattern)

## I2C Performance

| Metric | Value |
|--------|-------|
| Bus speed | 400kHz Fast Mode |
| Full poll cycle (4 Scroll + 2 8Encoder) | ~3.9ms |
| Maximum poll rate | ~256Hz |
| Target poll rate | 100Hz |
| CPU utilisation at 100Hz | ~20-25% (one core) |
| Inter-transaction delay | 100µs (STM32F030 requirement) |

## Power Budget

| Component | Typical | Worst Case |
|-----------|---------|------------|
| 4x Scroll (4 LEDs dim) | ~60mA | ~240mA |
| 2x 8Encoder (18 LEDs dim) | ~270mA | ~1.08A |
| AtomS3 + WiFi | ~100mA | ~200mA |
| **Total** | **~330mA** | **~1.5A** |

Feasible from USB-C at 50% LED brightness. External 2A supply recommended for sustained full-brightness operation.

## Implementation Phases

1. **Hardware scan** — PaHub detection, device enumeration per channel ✓ (done in main.cpp)
2. **WiFi + WebSocket** — Connect to K1, establish WS, handle reconnection
3. **I2C polling loop** — FreeRTOS task on Core 0, 100Hz poll cycle through PaHub
4. **Parameter mapping** — Encoder/scroll delta → K1 WS command translation
5. **LED feedback** — Zone colours, value indication, connection state
6. **Echo prevention** — 1-second holdoff after local change (Tab5 pattern)
7. **AtomS3 display** — Connection state, active effect name, zone summary on 0.85" screen
8. **Presets** — NVS store/recall of full controller state

## Known Issues / Mitigations

| Issue | Mitigation |
|-------|-----------|
| Scroll `getI2CAddress()` returns 0x01 | Store address (0x40) in variable; never call getI2CAddress() |
| Scroll library immature (8 commits) | Use register-level I2C directly (7 registers) |
| I2C bus lockup (SDA stuck low) | Pre-init bus clear before Wire.begin(); SCL toggling recovery |
| STM32F030 NACK under fast polling | 100µs inter-transaction delay |

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-31 | captain + agent | Created from 26-SSA research synthesis + zone purge |
