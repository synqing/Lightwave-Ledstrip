# tab5-encoder Codebase Map

## Overview

| Field | Value |
|-------|-------|
| Total Files | 142 |
| Total LOC | 119,517 |
| Platform | ESP32-P4 (pioarduino) |
| Framework | Arduino + ESP-IDF |
| Board | esp32-p4-evboard |

## Languages

| Language | Files | LOC | % |
|----------|------:|----:|--:|
| C | 35 | 91,794 | 76.8% |
| C/C++ Header | 51 | 14,224 | 11.9% |
| C++ | 29 | 13,499 | 11.3% |

> 77% C is almost entirely LVGL font bitmaps in `ui/fonts/`

## Dependencies (platformio.ini lib_deps)

| Package | Version |
|---------|---------|
| lvgl/lvgl | ^9.3.0 |
| bblanchon/ArduinoJson | ^7.0.0 |
| links2004/WebSockets | ^2.4.0 |
| me-no-dev/ESPAsyncWebServer | ^3.0.0 |
| robtillaart/M5ROTATE8 | ^0.4.1 |
| M5Stack/M5Unified | git |
| M5Stack/M5GFX | git |
| tobozo/LGFX_PPA | git |

## Directory Structure (LOC)

```
src/                      119,517
|-- main.cpp                2,521  (entrypoint: setup() + loop())
|-- config/                 1,379  (Config.h, PaletteLedData, network_config)
|-- hal/                      189  (EspHal, SimHal platform abstraction)
|-- hardware/                 407  (OrientationManager)
|-- input/                  3,381  (DualEncoderService, TouchHandler, I2CRecovery, ClickDetector)
|-- network/                3,437  (WebSocketClient, WsMessageRouter, HttpClient, WiFiManager)
|-- parameters/               657  (ParameterHandler, ParameterMap)
|-- presets/                  466  (PresetManager)
|-- storage/                  880  (NvsStorage, PresetStorage, PresetData)
|-- ui/                   106,152  <-- 89% of total LOC
|   |-- fonts/            ~94,000  <-- LVGL bitmap font .c files (40 files)
|   |-- widgets/            1,099  (PresetSlot, PresetBank, Gauge, UIHeader, ActionRow)
|   |-- DisplayUI.cpp       1,838
|   |-- ConnectivityTab     1,502  (network management UI)
|   |-- ZoneComposerUI      1,685  (zone layout editor)
|   `-- ControlSurfaceUI      572
|-- utils/                     14
`-- zones/                     34
```

## Most-Imported Modules

| Module | Imports |
|--------|--------:|
| config/Config.h | 17 |
| ui/Theme.h | 12 |
| network/WebSocketClient.h | 11 |
| parameters/ParameterMap.h | 8 |
| hal/EspHal.h | 8 |
| input/ButtonHandler.h | 6 |

## Key Patterns

- **UI**: LVGL 9.3 widgets, 10 FPS render loop, theme-driven styling
- **Input**: Dual M5ROTATE8 units (16 encoders), I2C bus with recovery, touch + button
- **Network**: WebSocket bidirectional comms to LightwaveOS, HTTP REST for connectivity UI
- **State**: 9 FSMs (click detection, WiFi, WS, LED feedback, presets, zones, discovery)
- **Storage**: NVS for presets and network config, PresetData serialization
- **Zones**: Up to 4 LED zones, per-zone effect/speed/palette control

## No Test Infrastructure or CI/CD Detected
