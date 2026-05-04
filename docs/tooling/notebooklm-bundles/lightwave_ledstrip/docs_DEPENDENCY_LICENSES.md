# LightwaveOS Dependency License Audit

**Date:** 2026-02-27
**Project license:** Apache License 2.0
**Auditor:** Automated audit via web research

---

## 1. firmware-v3 (ESP32-S3 LED Controller)

### Platform & Framework

| Dependency | Version | Licence | Compatible? | Notes |
|---|---|---|---|---|
| [espressif32](https://github.com/platformio/platform-espressif32) (PlatformIO platform) | 6.9.0 | Apache 2.0 | YES | ESP-IDF core is Apache 2.0 |
| [arduino-esp32](https://github.com/espressif/arduino-esp32) (Arduino framework) | bundled with platform | **LGPL-2.1** | CONDITIONAL | See [LGPL Analysis](#lgpl-analysis) below |

### Library Dependencies

| Dependency | Version | Licence | Compatible? | Notes |
|---|---|---|---|---|
| [FastLED](https://github.com/FastLED/FastLED) | 3.10.0 | MIT | YES | No issues |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | 7.0.4 | MIT | YES | No issues |
| [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) (esp32async) | 3.9.3 | **LGPL-3.0** | CONDITIONAL | See [LGPL Analysis](#lgpl-analysis) below |
| [AsyncTCP](https://github.com/ESP32Async/AsyncTCP) (esp32async) | 3.4.9 | **LGPL-3.0** | CONDITIONAL | See [LGPL Analysis](#lgpl-analysis) below |
| [Unity](https://github.com/ThrowTheSwitch/Unity) (test only) | ^2.5.2 | MIT | YES | Test-only; not shipped in firmware |
| [MabuTrace](https://github.com/mabuware/MabuTrace) (trace build only) | latest | **GPL-3.0** | **RED FLAG** | See [GPL Analysis](#gpl-3-0-red-flag-mabutrace) below |

### Transitive / Bundled (ESP-IDF components used at runtime)

| Component | Licence | Notes |
|---|---|---|
| esp_websocket_client | Apache 2.0 | ESP-IDF native component |
| FreeRTOS | MIT | Bundled with ESP-IDF |
| LittleFS | MIT (BSD-like) | Filesystem |
| lwIP | BSD | TCP/IP stack |
| mbedTLS | Apache 2.0 | TLS library |
| newlib | BSD / GPL (unused parts) | C standard library |

---

## 2. tab5-encoder (ESP32-P4 Rotary Encoder Controller)

### Platform & Framework

| Dependency | Version | Licence | Compatible? | Notes |
|---|---|---|---|---|
| [pioarduino/platform-espressif32](https://github.com/pioarduino/platform-espressif32) | 54.03.21 | Apache 2.0 | YES | Community fork of official platform |
| Arduino framework (ESP32-P4) | bundled | **LGPL-2.1** | CONDITIONAL | Same as above |

### Library Dependencies

| Dependency | Version | Licence | Compatible? | Notes |
|---|---|---|---|---|
| [M5Unified](https://github.com/m5stack/M5Unified) | latest (git) | MIT | YES | No issues |
| [M5GFX](https://github.com/m5stack/M5GFX) | latest (git) | MIT | YES | No issues |
| [M5ROTATE8](https://github.com/RobTillaart/M5ROTATE8) | ^0.4.1 | MIT | YES | No issues |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | ^7.0.0 | MIT | YES | No issues |
| [WebSockets](https://github.com/Links2004/arduinoWebSockets) (links2004) | ^2.4.0 | **LGPL-2.1** | CONDITIONAL | See [LGPL Analysis](#lgpl-analysis) below |
| [LGFX_PPA](https://github.com/tobozo/LGFX_PPA) | latest (git) | MIT | YES | No issues |
| [LVGL](https://github.com/lvgl/lvgl) | ^9.3.0 | MIT | YES | No issues |
| [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) (me-no-dev) | ^3.0.0 | **LGPL-3.0** | CONDITIONAL | See [LGPL Analysis](#lgpl-analysis) below |

---

## 3. k1-composer (Web UI + Node.js Proxy)

### Node.js Dependencies (proxy/)

| Dependency | Version | Licence | Compatible? | Notes |
|---|---|---|---|---|
| [serialport](https://github.com/serialport/node-serialport) | ^12.0.0 | MIT | YES | No issues |
| [@serialport/parser-readline](https://github.com/serialport/node-serialport) | ^12.0.0 | MIT | YES | No issues |
| [ws](https://github.com/websockets/ws) | ^8.18.0 | MIT | YES | No issues |

### CDN Dependencies (browser)

| Dependency | Loaded From | Licence | Compatible? | Notes |
|---|---|---|---|---|
| [Tailwind CSS](https://github.com/tailwindlabs/tailwindcss) | cdn.tailwindcss.com | MIT | YES | No issues |
| [Iconify Icon](https://github.com/iconify/iconify) | code.iconify.design | MIT | YES | Individual icon sets have own licences |
| [React 18](https://github.com/facebook/react) | unpkg.com (diagnostic page only) | MIT | YES | No issues |
| [ReactDOM 18](https://github.com/facebook/react) | unpkg.com (diagnostic page only) | MIT | YES | No issues |
| [Babel Standalone](https://github.com/babel/babel) | unpkg.com (diagnostic page only) | MIT | YES | No issues |

---

## 4. lightwave-ios-v2 (iOS Companion App)

### Swift Package Manager Dependencies

| Dependency | Licence | Compatible? | Notes |
|---|---|---|---|
| [rive-ios](https://github.com/rive-app/rive-ios) | MIT | YES | Animation runtime |

### Transitive SPM Dependencies (build artifacts)

| Dependency | Licence | Compatible? | Notes |
|---|---|---|---|
| [swift-snapshot-testing](https://github.com/pointfreeco/swift-snapshot-testing) | MIT | YES | Test-only |
| [swift-custom-dump](https://github.com/pointfreeco/swift-custom-dump) | MIT | YES | Test-only |
| [xctest-dynamic-overlay](https://github.com/pointfreeco/swift-issue-reporting) | MIT | YES | Test-only |
| [swift-syntax](https://github.com/swiftlang/swift-syntax) | Apache 2.0 + Runtime Library Exception | YES | Macro support |

---

## 5. Development / CI Tools (not shipped)

| Tool | Licence | Notes |
|---|---|---|
| [detect-secrets](https://github.com/Yelp/detect-secrets) (pre-commit) | Apache 2.0 | Dev-only |
| [black](https://github.com/psf/black) (pre-commit) | MIT | Dev-only |
| [yamllint](https://github.com/adrienverge/yamllint) (pre-commit) | GPL-3.0 | Dev-only; not shipped |
| [pre-commit-hooks](https://github.com/pre-commit/pre-commit-hooks) (pre-commit) | MIT | Dev-only |

---

## LGPL Analysis

### The Issue

Several dependencies use LGPL licences:

| Library | LGPL Version | Used In |
|---|---|---|
| arduino-esp32 (Arduino framework) | LGPL-2.1 | firmware-v3, tab5-encoder |
| ESPAsyncWebServer (esp32async) | LGPL-3.0 | firmware-v3 |
| AsyncTCP (esp32async) | LGPL-3.0 | firmware-v3 |
| ESPAsyncWebServer (me-no-dev) | LGPL-3.0 | tab5-encoder |
| arduinoWebSockets (links2004) | LGPL-2.1 | tab5-encoder |

### Compatibility with Apache 2.0

**LGPL-3.0 + Apache 2.0:** Compatible. Apache 2.0 code can be combined with LGPL-3.0 code because GPLv3 (and by extension LGPLv3) accepts Apache 2.0 licensed works. The resulting combined work must comply with LGPL-3.0 requirements for the LGPL-covered portions.

**LGPL-2.1 + Apache 2.0:** Technically **incompatible** in the strict FSF interpretation due to Apache 2.0's patent termination clauses conflicting with LGPL-2.1. However, this applies to relicensing Apache code under LGPL-2.1, not to the typical embedded firmware scenario where the LGPL library is dynamically linked or used as a shared library.

### Practical Impact for Embedded Firmware

In embedded firmware (ESP32), all libraries are **statically linked** into a single binary. The LGPL requires that users can re-link the LGPL portions:

1. **Arduino framework (LGPL-2.1):** This is the standard situation for every Arduino-based project. The LGPL-2.1 requires that users have the ability to modify and re-link the library. Since the framework source is publicly available and users build from source via PlatformIO, this requirement is inherently satisfied.

2. **ESPAsyncWebServer / AsyncTCP (LGPL-3.0):** Same as above -- source is publicly available, and the build system (PlatformIO) allows users to substitute modified versions.

3. **arduinoWebSockets (LGPL-2.1):** Same analysis as Arduino framework.

### Required Actions for LGPL Compliance

- Include the LGPL licence text in your distribution (NOTICE file or documentation)
- Provide (or link to) the source code of LGPL-covered libraries
- Ensure users can re-link the firmware with modified versions of LGPL libraries (satisfied by the PlatformIO build system and public source)
- Do NOT claim the LGPL-covered code is under Apache 2.0 -- only YOUR code is Apache 2.0

---

## GPL-3.0 Red Flag: MabuTrace

### The Issue

**MabuTrace** is licensed under **GPL-3.0**. GPL-3.0 is a copyleft licence that requires any combined work to also be distributed under GPL-3.0. This is **incompatible** with distributing the combined binary under Apache 2.0.

### Mitigation

MabuTrace is **only** included in the `esp32dev_audio_trace` build environment, which is a specialised **developer instrumentation build** -- not a production build. The default `esp32dev_audio` environment and all other production environments do **not** include MabuTrace.

### Required Actions

1. **NEVER include MabuTrace in production/release builds.** The current configuration already isolates it to `esp32dev_audio_trace` only. This is correct.
2. **Do NOT distribute firmware binaries built with `esp32dev_audio_trace`.** If trace-enabled binaries are distributed, the entire binary must comply with GPL-3.0.
3. **Document this restriction** for contributors: trace builds are for internal debugging only.
4. Consider replacing MabuTrace with an Apache 2.0 or MIT-licensed alternative if trace builds ever need distribution.

---

## Required Attributions (for NOTICE file)

Under Apache 2.0 and the various dependency licences, you should maintain a NOTICE file acknowledging all third-party software. Below are the required attributions:

### MIT-Licensed Dependencies
```
FastLED - Copyright (c) Daniel Garcia, Mark Kriegsman
  Licensed under the MIT License

ArduinoJson - Copyright (c) Benoit Blanchon
  Licensed under the MIT License

Unity Test Framework - Copyright (c) ThrowTheSwitch.org
  Licensed under the MIT License

M5Unified - Copyright (c) M5Stack
  Licensed under the MIT License

M5GFX - Copyright (c) M5Stack / lovyan03
  Licensed under the MIT License

M5ROTATE8 - Copyright (c) Rob Tillaart
  Licensed under the MIT License

LGFX_PPA - Copyright (c) tobozo
  Licensed under the MIT License

LVGL - Copyright (c) LVGL Kft
  Licensed under the MIT License

rive-ios - Copyright (c) 2020 Rive
  Licensed under the MIT License

serialport - Copyright (c) serialport contributors
  Licensed under the MIT License

ws - Copyright (c) Einar Otto Stangvik
  Licensed under the MIT License

Tailwind CSS - Copyright (c) Tailwind Labs, Inc.
  Licensed under the MIT License

React - Copyright (c) Meta Platforms, Inc. and affiliates
  Licensed under the MIT License

Babel - Copyright (c) Sebastian McKenzie and other contributors
  Licensed under the MIT License

Iconify - Copyright (c) Vjacheslav Trushkin
  Licensed under the MIT License
```

### LGPL-Licensed Dependencies
```
Arduino core for ESP32 - Copyright (c) Espressif Systems
  Licensed under the GNU Lesser General Public License v2.1

ESPAsyncWebServer - Copyright (c) me-no-dev / ESP32Async community
  Licensed under the GNU Lesser General Public License v3.0

AsyncTCP - Copyright (c) me-no-dev / ESP32Async community
  Licensed under the GNU Lesser General Public License v3.0

arduinoWebSockets - Copyright (c) Markus Sattler (Links2004)
  Licensed under the GNU Lesser General Public License v2.1
```

### Apache 2.0 Licensed Dependencies
```
ESP-IDF - Copyright (c) Espressif Systems
  Licensed under the Apache License, Version 2.0

swift-syntax - Copyright (c) Apple Inc.
  Licensed under Apache License v2.0 with Runtime Library Exception

detect-secrets - Copyright (c) Yelp, Inc.
  Licensed under the Apache License, Version 2.0
```

---

## Summary Verdict

### Are we clear for Apache 2.0?

**YES, with caveats.**

The LightwaveOS project can be licensed under Apache 2.0 for all **original code**. The situation breaks down as follows:

| Category | Status | Action Required |
|---|---|---|
| MIT dependencies | CLEAR | Include attributions in NOTICE file |
| Apache 2.0 dependencies | CLEAR | Include attributions in NOTICE file |
| LGPL-3.0 dependencies (ESPAsyncWebServer, AsyncTCP) | CLEAR | Include LGPL licence text; ensure source availability for LGPL portions |
| LGPL-2.1 dependencies (Arduino framework, arduinoWebSockets) | CLEAR (practical) | Same as above; standard for all Arduino ESP32 projects |
| GPL-3.0 dependency (MabuTrace) | **CLEAR only if not distributed** | Already isolated to `esp32dev_audio_trace` env; never ship this build |
| iOS dependencies | CLEAR | All MIT or Apache 2.0 |
| Node.js / web dependencies | CLEAR | All MIT |
| Dev/CI tools | CLEAR | Not shipped; no licence concerns |

### Action Items

1. **Create a NOTICE file** at the repository root with the attributions listed above
2. **Include full licence texts** for LGPL-2.1 and LGPL-3.0 in the repository (e.g., `LICENSES/` directory)
3. **Add a warning comment** in `platformio.ini` next to the `mabuware/mabutrace` dependency noting it is GPL-3.0 and must not be included in distributed builds
4. **Ensure README or distribution notes** state that LGPL-covered libraries remain under their original licences and link to their source repositories
5. **Consider long-term**: evaluate MIT/Apache-licensed alternatives for ESPAsyncWebServer and AsyncTCP if strict Apache 2.0 purity is desired (though the current LGPL usage is standard practice for ESP32 projects)
