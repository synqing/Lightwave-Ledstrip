# Build & Deploy

> **When to read this:** Before building firmware, deploying OTA updates, or modifying feature flags and build environments.

## Primary Build

```bash
cd firmware/v2
pio run -e esp32dev_audio              # Build
pio run -e esp32dev_audio -t upload    # Build + flash via USB
pio device monitor -b 115200           # Serial monitor
pio run -e esp32dev_audio -t clean     # Clean
```

## Build Environments

| Environment | Purpose |
|-------------|---------|
| `esp32dev_audio` | Primary: WiFi + audio + web server |
| `esp32dev_audio_benchmark` | Audio pipeline benchmarking |
| `esp32dev_audio_trace` | Perfetto timeline tracing |
| `esp32dev_SSB` | Alternate GPIO mapping |
| `native` | Native unit tests (`pio test -e native`) |

## Tab5 Encoder (separate project, ESP32-P4)

```bash
# MUST run from repo root with clean PATH:
PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin" pio run -e tab5 -d firmware/Tab5.encoder
PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin" pio run -e tab5 -t upload -d firmware/Tab5.encoder
pio device monitor -d firmware/Tab5.encoder -b 115200
```

Never `cd` into the Tab5 directory. Never add hardcoded toolchain paths.

## OTA Firmware Update

```bash
# Get device OTA token
curl http://lightwaveos.local/api/v1/device/ota-token

# Push firmware
curl -X POST http://lightwaveos.local/api/v1/firmware/update \
     -H "X-OTA-Token: <device-token>" \
     -F "firmware=@.pio/build/esp32dev_audio/firmware.bin"
```

Also available via web UI at `http://lightwaveos.local` (OTA Update tab).

## Hardware Deployments

| Device | USB Access | Update Method |
|--------|------------|---------------|
| **K1 Prototype** (sealed LGP) | Requires disassembly | OTA only |
| **Dev Board** (bench) | USB-C available | USB or OTA |

## Desktop Dashboard

The dashboard is a separate vanilla JS app, not embedded in the firmware.

```bash
cd lightwave-controller
python3 lightwave_controller.py   # Opens http://localhost:8888
```

Edit files in `lightwave-controller/static/` (index.html, app.js, styles.css). The ESP32 at `http://lightwaveos.local` serves only a minimal inline launcher page.

## Hardware

- ESP32-S3-DevKitC-1 @ 240MHz, 8MB flash
- Dual WS2812 strips: GPIO 4 (Strip 1), GPIO 5 (Strip 2)
- 160 LEDs per strip = 320 total
- Optional: M5ROTATE8 encoder via I2C (`FEATURE_ROTATE8_ENCODER`)

## Feature Flags

Defined in `firmware/v2/src/config/features.h` and `firmware/v2/platformio.ini`. See [CONSTRAINTS.md](../../CONSTRAINTS.md) for the full flag table and resource budgets.
