# Architecture

> **When to read this:** Before making structural changes, adding new subsystems, or when you need to understand how components connect.

## Quick Facts

- ESP32-S3 @ 240MHz, 8MB flash, 320KB SRAM
- 320 WS2812 LEDs (2 x 160 strips), GPIO 4 and GPIO 5
- Centre point: LED 79/80 (all effects originate here)
- Target: 120 FPS

For timing budgets and memory limits, see [CONSTRAINTS.md](../../CONSTRAINTS.md).

## System Overview

```
Desktop Dashboard (lightwave-controller/static/)
  |  REST + WebSocket over WiFi
  v
WebServer (firmware/v2/src/network/)
  |  REST: /api/v1/*   WS: real-time updates
  v
Zone Composer (firmware/v2/src/effects/zones/)
  |  Up to 4 independent zones, buffer proxy isolation
  v
Effect System (firmware/v2/src/effects/)
  |  100+ IEffect plugins, TransitionEngine (12 types)
  v
LED Buffers: leds[320] -> strip1[160] + strip2[160]
  |
  v
FastLED + RMT Driver (WS2812 @ 800kHz)
```

The desktop dashboard is a vanilla JS app served from `lightwave-controller/` on the developer's computer. The ESP32 serves only a minimal inline launcher page (no LittleFS).

## Key Source Files

| File | Purpose |
|------|---------|
| `firmware/v2/src/main.cpp` | Entry point, actor system bootstrap |
| `firmware/v2/src/effects/CoreEffects.cpp` | Effect registration (101+ effects) |
| `firmware/v2/src/effects/ieffect/*.cpp` | Individual IEffect implementations |
| `firmware/v2/src/core/actors/RendererActor.h` | Render pipeline, effect management |
| `firmware/v2/src/effects/zones/ZoneComposer.h` | Multi-zone orchestration |
| `firmware/v2/src/effects/transitions/TransitionEngine.h` | 12 transition types |
| `firmware/v2/src/network/WebServer.cpp` | REST + WebSocket handlers |
| `firmware/v2/src/network/WiFiManager.cpp` | WiFi state machine with AP fallback |
| `firmware/v2/src/config/features.h` | Compile-time feature flags |
| `firmware/v2/src/config/hardware_config.h` | Pin definitions, LED counts |
| `firmware/v2/src/config/network_config.h` | WiFi credentials, ports |
| `lightwave-controller/static/app.js` | Desktop dashboard JavaScript |
| `lightwave-controller/lightwave_controller.py` | Desktop dashboard server |

## Design Philosophy

- **Centre origin**: LGP creates interference patterns -- edge-originating effects look wrong on this hardware.
- **Dual-strip symmetry**: Strip 1 and Strip 2 should complement each other.
- **Optional HMI**: M5ROTATE8 encoder support via I2C (`FEATURE_ROTATE8_ENCODER`), but primary control is web/serial.
- **Performance**: Minimise heap allocations in render loops.

## Deeper References

- [ARCHITECTURE.md](../../ARCHITECTURE.md) -- Extracted system facts, LED buffer layout, build environments
- [CONSTRAINTS.md](../../CONSTRAINTS.md) -- Hard timing/memory/power limits
