# LightwaveOS

**Audio-reactive LED controller for Light Guide Plates**

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-green.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Effects](https://img.shields.io/badge/Effects-100+-orange.svg)](firmware-v3/src/effects/)

<!-- TODO: Replace with hero photo/GIF of the Light Guide Plate in action -->
<!-- <img src="assets/hero.gif" alt="LightwaveOS Light Guide Plate" width="100%"> -->

An ESP32-S3 firmware that drives 320 WS2812 LEDs across two strips edge-injected into an acrylic Light Guide Plate. The system listens to music through an I2S microphone, extracts beat, tempo, and harmonic features in real time, and maps them to visual effects that radiate from the centre of the plate outward. Controlled via REST API, WebSocket, iOS app, or physical encoders.

---

## At a glance

| | |
|---|---|
| **100 effects** across 11 families | **75 colour palettes** |
| **120 FPS** rendering target | **47 REST** + **21 WebSocket** API endpoints |
| **320 LEDs** (2 x 160, centre-origin) | **4 independent zones** with transitions |

---

## Features

### Audio-reactive pipeline
Real-time I2S audio capture at 32 kHz, Goertzel frequency analysis, beat detection with tempo tracking, chroma/harmonic extraction, and musical style classification (electronic, organic, acoustic). Audio features map directly to visual parameters via a ControlBus architecture.

### Centre-origin effects
Every effect originates from LEDs 79/80 at the physical centre of the Light Guide Plate and propagates outward (or converges inward). This constraint produces unique interference and depth patterns impossible with conventional linear LED strips.

### Multi-zone composer
Up to 4 independent zones, each running its own effect and palette. 12 transition types with 15 easing curves for smooth cross-fades between effects.

### Multi-device ecosystem
- **iOS app** -- SwiftUI dashboard with Canvas-based LED preview and beat-sync UI
- **Tab5 encoder** -- M5Stack hardware controller with 16 rotary encoders and 5" LVGL display
- **Web dashboard** -- Browser-based effect composer with live parameter tuning
- **REST + WebSocket + UDP** -- Full programmatic control from any language

---

## Quick start

### Prerequisites

- [PlatformIO CLI](https://platformio.org/install/cli) (or VS Code extension)
- ESP32-S3 development board
- USB-C cable

### Build and flash

```bash
git clone https://github.com/synqing/Lightwave-Ledstrip.git
cd Lightwave-Ledstrip/firmware-v3
pio run -e esp32dev_audio              # build
pio run -e esp32dev_audio -t upload    # flash
pio device monitor -b 115200           # serial monitor
```

---

## Project layout

```
LightwaveOS/
  firmware-v3/       ESP32-S3 firmware (PlatformIO, C++17)
  lightwave-ios-v2/  iOS companion app (SwiftUI, iOS 17+)
  tab5-encoder/      M5Stack Tab5 hardware controller (LVGL 9.3)
  k1-composer/       Web-based effect composer dashboard
  instructions/      Governance policies and changelog fragments
```

---

<details>
<summary><strong>Architecture</strong></summary>

### Dual-core actor model

- **Core 0** -- Network stack (WiFi, HTTP, WebSocket), audio capture and DSP
- **Core 1** -- LED rendering at 120 FPS, effect computation, zone composition

Cross-core communication uses lock-free snapshot buffers. The system follows a CQRS (Command Query Responsibility Segregation) pattern: commands flow in through the API, state changes propagate through an actor message bus, and the renderer queries the latest state each frame.

### IEffect plugin system

Effects implement a simple interface:

```cpp
class IEffect {
public:
    virtual void render(const RenderContext& ctx, CRGB* leds) = 0;
    virtual const char* name() const = 0;
};
```

Each effect receives a `RenderContext` with timing, audio features, palette, and zone information. Effects are registered in `PatternRegistry` and selectable at runtime via API or hardware controls.

### Zone composer

The zone composer divides the 320-LED strip into up to 4 independent zones, each with its own effect, palette, and parameters. A transition engine handles cross-fades between effects using 12 transition types (fade, wipe, dissolve, etc.) and 15 easing curves.

</details>

<details>
<summary><strong>Audio pipeline</strong></summary>

```
SPH0645 mic (I2S, 32 kHz)
  -> DMA capture (256-sample hops)
  -> Goertzel frequency analysis (64 bins)
  -> Beat detection + tempo tracking
  -> Chroma/harmonic extraction
  -> Style classification (electronic/organic/acoustic)
  -> ControlBus frame
  -> Effect render() receives audio features
```

The audio pipeline runs on Core 0 and produces a `ControlBusFrame` every hop (~8 ms). Effects read audio features through the `RenderContext`:

- `rms` -- overall loudness
- `flux` -- spectral change (onset detection)
- `bands[]` -- frequency band energies
- `chromaAngle` -- dominant pitch as hue angle
- `bpm` -- current tempo estimate
- `beatPhase` -- position within the current beat (0.0-1.0)

</details>

<details>
<summary><strong>API</strong></summary>

### REST API (47 endpoints)

Base URL: `http://<device-ip>/api/v1/`

| Category | Endpoints | Examples |
|----------|-----------|---------|
| Device | 2 | `/device/info`, `/device/restart` |
| Effects | 5 | `/effects/current`, `/effects/list`, `/effects/{id}` |
| Parameters | 2 | `/parameters`, `/parameters/{name}` |
| Transitions | 4 | `/transitions/trigger`, `/transitions/config` |
| Zones | 10 | `/zones`, `/zones/{id}`, `/zones/{id}/effect` |
| Batch | 1 | `/batch` (multiple commands in one request) |
| Debug | 1 | `/debug/metrics` |

### WebSocket (21 commands)

Connect to `ws://<device-ip>/ws` for real-time bidirectional control.

### Binary streaming

- **LED frames**: 966 bytes/frame over WebSocket or UDP (port 41234)
- **Audio metrics**: 464 bytes/frame for visualisation clients

See [firmware-v3/docs/api/api-v1.md](firmware-v3/docs/api/api-v1.md) for the complete API reference.

</details>

<details>
<summary><strong>Ecosystem</strong></summary>

### k1-composer (web dashboard)

Browser-based effect composer with live code visualisation. Discovers 170 effects and 1,162 exposed parameters. Supports binary frame streaming (960 bytes/frame), lease-based control protocol, and runtime parameter patching.

### tab5-encoder (hardware controller)

M5Stack Tab5 (ESP32-P4) with a 5" 800x480 LVGL display and dual M5ROTATE8 encoder units (16 rotary encoders). Features an 8-bank preset system and WebSocket synchronisation with the main firmware.

### lightwave-ios-v2 (iOS app)

SwiftUI companion app for iOS 17+ with Canvas-based 320-LED rendering, beat-synchronised UI animations, and REST + WebSocket + UDP connectivity. Provides a music visualisation tool with a centre-origin LED preview.

</details>

<details>
<summary><strong>Hardware</strong></summary>

### Bill of materials

| Component | Description |
|-----------|-------------|
| ESP32-S3-DevKitC-1 | Main controller (240 MHz, 8 MB flash, 2 MB PSRAM) |
| WS2812 LED strip | 2 x 160 LEDs (320 total) |
| SPH0645 | I2S MEMS microphone (32 kHz sampling) |
| Acrylic plate | Light Guide Plate (~329 mm between LED edges) |
| 5V PSU | Adequate for 320 LEDs at target brightness |

### LED layout

```
Strip A (160 LEDs)     Light Guide Plate     Strip B (160 LEDs)
[0 ---- 79|80 ---- 159]  <== acrylic ==>  [0 ---- 79|80 ---- 159]
            ^                                         ^
        centre origin                            centre origin
```

All effects radiate from LEDs 79/80 outward to the edges, creating symmetric interference patterns within the acrylic waveguide.

</details>

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for repository structure rules, changelog fragment process, and DCO sign-off requirements.

## License

Licensed under the [Apache License 2.0](LICENSE). See [NOTICE](NOTICE) for additional information and [TRADEMARK.md](TRADEMARK.md) for branding guidelines.

## Acknowledgements

- [FastLED](https://github.com/FastLED/FastLED) -- LED control library
- [ArduinoJson](https://arduinojson.org/) -- JSON serialisation
- [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) -- Async HTTP and WebSocket server
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32) -- ESP-IDF Arduino framework
