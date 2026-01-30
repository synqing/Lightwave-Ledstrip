# Staging and Commit Strategy: OTA + Dual-Zones / Tab5

**Branch:** `pivot/dual-zones-default`  
**Date:** 2026-01-30  
**Scope:** Uncommitted changes at analysis time (35 modified files, 50+ untracked files)

This document provides a line-by-line change analysis, atomic staging strategy, conventional commit messages, context documentation, lessons learned, and future contributor guidance.

---

## 1. Change Analysis

### 1.1 Categorisation

| Category | Files | Description |
|----------|--------|-------------|
| **New features** | OTA (v2): version.h, OtaSessionLock, OtaLedFeedback, OtaBootVerifier, OtaTokenManager, FirmwareHandlers (full impl), WsOtaCommands (thread-safe), WsOtaCodec (token/version/target), V1ApiRoutes (filesystem + ota-token), DeviceHandlers/EffectHandlers (version/limit), firmware/v2/data (OTA UI). Tab5: DeviceRegistry, DeviceSelectorTab, DisplayUI (device selector screen), main (device selector + registry). | OTA infrastructure, REST/WS OTA, LED feedback, per-device token, boot rollback; Tab5 multi-device UI. |
| **Bug fixes** | Tab5: ConnectivityTab (async WiFi scan to avoid WDT), WebSocketClient (sendJSONUnlocked + pendingZonesRefresh), WsMessageRouter (defer zones.get), WiFiManager (mDNS 500ms), Config (I2C 50ms), lvgl_bridge (WDT in flush_cb). | Avoid blocking in LVGL callback; avoid deadlock sending inside WS callback; reduce mDNS/I2C timeouts for WDT. |
| **Refactors** | WsOtaCommands: spinlock-guarded session state, ChunkStateSnapshot, constant-time token check. FirmwareHandlers: spinlock for cross-task reads, separate firmware vs filesystem state. WebSocketClient: extract sendJSONUnlocked for queue path. | Thread-safe OTA state; no mixed concerns in commits. |
| **Documentation** | CLAUDE.md (root: OTA commands, K1/dev board), docs/surfaces/* (firmware-v2, tab5-encoder, artefacts, drift-fixes). | OTA workflow, hardware deployments, surface docs. |
| **Configuration** | firmware/v2/platformio.ini (CONFIG_ASYNC_TCP_RUNNING_CORE=0), firmware/v2/src/main.cpp (default effect 100, OTA init), firmware/v2/src/config/audio_config.h (MicType, SPH0645/INMP441), Tab5 Config.h (I2C timeout 50ms), Tab5 network_config.h (DeviceNVS). | Async TCP core; default effect; mic type; I2C/WDT tuning; device registry NVS. |
| **Audio / diagnostics** | AudioCapture (channel + bit-shift by mic type), AudioActor (DIAG A2/A4), TempoTracker (DIAG A3), audio_config.h (MicType enum). | Compile-time mic selection; optional trace-level diagnostics. |

### 1.2 File Dependencies and Affected Components

- **OTA (v2):**  
  `version.h` → FirmwareHandlers, WsOtaCodec, DeviceHandlers.  
  `OtaSessionLock` → FirmwareHandlers, WsOtaCommands, WiFiManager.  
  `OtaLedFeedback` → FirmwareHandlers, WsOtaCommands.  
  `OtaTokenManager` → FirmwareHandlers, WsOtaCommands (token check), main (init).  
  `OtaBootVerifier` → main (init + markAppValidIfHealthy).  
  FirmwareHandlers ↔ V1ApiRoutes (routes), ApiResponse, network_config, Update.

- **Tab5:**  
  DeviceRegistry ↔ main, DeviceSelectorTab, network_config (DeviceNVS).  
  DeviceSelectorTab ↔ DisplayUI, WebSocketClient, DeviceRegistry.  
  ConnectivityTab ↔ HttpClient (startScan, listNetworks).  
  WsMessageRouter → WebSocketClient (setPendingZonesRefresh).

- **Web UI (v2 data):**  
  app.js (OTA tab, version, token fetch) ↔ firmware REST/WS OTA API and codec.

---

## 2. Staging Strategy

Commits are ordered so that **infrastructure and core behaviour come first**, then **features**, then **fixes and config**, then **docs and chore**. No commit mixes unrelated concerns.

| # | Commit type | Scope | Files (representative) |
|---|-------------|--------|-------------------------|
| 1 | feat | v2 OTA core | version.h, OtaSessionLock.h, OtaLedFeedback.h, OtaBootVerifier.h, OtaTokenManager.h/cpp, main.cpp (OTA init only) |
| 2 | feat | v2 REST OTA | FirmwareHandlers.cpp/h, V1ApiRoutes.cpp (firmware + filesystem + ota-token routes), WiFiManager.cpp (STA retry guard) |
| 3 | feat | v2 WS OTA | WsOtaCommands.cpp/h, WsOtaCodec.cpp/h |
| 4 | feat | v2 device/version API | DeviceHandlers.cpp, EffectHandlers.cpp, V1ApiRoutes (if not in 2) — version + limit 150 |
| 5 | feat | v2 web UI OTA | firmware/v2/data/app.js, index.html, styles.css |
| 6 | chore | v2 build/defaults | platformio.ini, main.cpp (default effect 100; OTA init already in 1) |
| 7 | feat | Tab5 device selector | DeviceRegistry.cpp/h, DeviceSelectorTab.cpp/h, DisplayUI.cpp/h, lvgl_bridge.cpp, main.cpp (registry + device selector wiring), network_config.h (DeviceNVS), Config.h (I2C timeout) |
| 8 | fix | Tab5 connectivity & WS | ConnectivityTab.cpp/h, WebSocketClient.cpp/h, WsMessageRouter.h |
| 9 | fix | Tab5 WiFi/mDNS | WiFiManager.cpp (mDNS 500ms) |
| 10 | feat | Audio mic type & diagnostics | audio_config.h, AudioCapture.cpp, AudioActor.cpp, TempoTracker.cpp |
| 11 | docs | OTA, K1, surfaces | CLAUDE.md (root), docs/surfaces/*, optional: K1_Prototype_Wiring_Schematic.html |
| 12 | chore | CLAUDE.md and tools | Remaining CLAUDE.md under firmware/docs, tools/tab5_serial_to_debuglog.py |

**Exclude from staging:**  
- `firmware/v2/data/.backup-align-*`, `lightwave-simple/.backup-align-*`  
- `lightwave-simple/test-*.plugin.json` (optional fixtures; keep untracked or add in a separate test-data commit if desired)

---

## 3. Commit Message Format

Each commit follows Conventional Commits with **type(scope): subject** (≤50 chars), **body** (implementation, technical notes, architecture), and **footer** (issues, breaking changes, migration).

### Commit 1: feat(v2): OTA core infrastructure (version, lock, LED, boot, token)

```
feat(v2): OTA core infrastructure (version, lock, LED, boot, token)

Add single source of truth for firmware version (version.h) and OTA session
safety: cross-transport lock (OtaSessionLock), LED feedback (OtaLedFeedback),
boot rollback (OtaBootVerifier), per-device token (OtaTokenManager). Init
order in main: OtaBootVerifier early, OtaTokenManager after NVS, markAppValid
after WiFi/WebServer.

Technical considerations:
- OtaSessionLock uses portMUX_TYPE spinlock; critical sections are flag-only.
- OtaLedFeedback writes to FastLED buffers; centre-origin progress fill.
- OtaBootVerifier requires CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE (default S3).
- OtaTokenManager: NVS namespace "ota", key "token"; fallback to compile-time.

Dependencies: version.h used by FirmwareHandlers, WsOtaCodec, DeviceHandlers.
No breaking change to existing API; new modules only.
```

### Commit 2: feat(v2): REST OTA firmware and filesystem with token auth

```
feat(v2): REST OTA firmware and filesystem with token auth

Implement multipart firmware (U_FLASH) and filesystem (U_SPIFFS) upload via
POST /api/v1/firmware/update and POST /api/v1/firmware/filesystem. Legacy
POST /update remains with plain-text response. X-OTA-Token validated with
constant-time comparison; optional X-OTA-Version / X-OTA-Force for version
checks. WiFiManager skips STA retry while REST OTA in progress.

Implementation:
- Upload state guarded by spinlock for cross-task reads (WiFiManager).
- Telemetry: ota.rest.* / ota.fs.* JSON on Serial.
- LED progress/success/failure via OtaLedFeedback; OtaSessionLock acquired
  on first chunk and released on completion/failure.

Routes: V1ApiRoutes registers firmware update, filesystem update, GET/POST
api/v1/device/ota-token. FirmwareHandlers: handleUpload, handleFsUpload,
handleV1Update, handleV1FsUpdate, handleGetOtaToken, handleSetOtaToken.
```

### Commit 3: feat(v2): WebSocket OTA with thread-safe session state

```
feat(v2): WebSocket OTA with thread-safe session state

Spinlock-guard WS OTA session state; add constant-time token check, LED
feedback, and OtaSessionLock acquire/release. WsOtaCodec: token, version,
force, target (firmware/filesystem); encodeOtaStatus gains versionNumber.
isWsOtaInProgress() for WiFiManager STA retry suppression.

Technical considerations:
- ChunkStateSnapshot read under spinlock; flash I/O outside lock.
- handleOtaClientDisconnect / abortOtaSession clear state and call
  OtaLed::showFailure + OtaLock::release.
```

### Commit 4: feat(v2): Device info version and effects list limit 150

```
feat(v2): Device info version and effects list limit 150

DeviceHandlers: expose FIRMWARE_VERSION_STRING and FIRMWARE_VERSION_NUMBER.
EffectHandlers: raise list limit from 100 to 150 to support full effect set.
Uses version.h for single source of truth.
```

### Commit 5: feat(v2): Web UI OTA tab, version display, token retrieval

```
feat(v2): Web UI OTA tab, version display, token retrieval

Add OTA Update tab in v2 data: firmware/filesystem upload, progress, version
check, and retrieval of device OTA token. Align styles and index structure
with new sections. UI calls /api/v1/firmware/version and /api/v1/device/ota-token.
```

### Commit 6: chore(v2): Async TCP core 0 and default effect LGP Holographic

```
chore(v2): Async TCP core 0 and default effect LGP Holographic

Set CONFIG_ASYNC_TCP_RUNNING_CORE=0 to align async_tcp with WiFi task (Core 0).
Set first-boot default effect to 100 (LGP Holographic Auto-Cycle).
```

### Commit 7: feat(Tab5): Device registry and device selector UI

```
feat(Tab5): Device registry and device selector UI

Add DeviceRegistry (NVS-backed saved devices, max 8) and DeviceSelectorTab:
list/discover devices, select target, reconnect WebSocket. Tap LIGHTWAVEOS
title to open Device Selector. Encoder routing for DEVICE_SELECTOR screen.
Reduce I2C timeout to 50ms for WDT margin; add WDT reset and slow-flush warning
in LVGL flush_cb. Config: DeviceNVS namespace/keys in network_config.h.
```

### Commit 8: fix(Tab5): Async WiFi scan and deferred zones refresh

```
fix(Tab5): Async WiFi scan and deferred zones refresh

ConnectivityTab: start async WiFi scan in startScan(); poll results in
checkScanStatus() to avoid blocking in LVGL callback (WDT). WsMessageRouter:
on zones.changed set pendingZonesRefresh; WebSocketClient sends zones.get on
next update() to avoid sending inside WS receive callback (deadlock risk).
Refactor: sendJSONUnlocked for queue path (caller holds mutex).
```

### Commit 9: fix(Tab5): Reduce mDNS query timeout to 500ms

```
fix(Tab5): Reduce mDNS query timeout to 500ms

mdns_query_a timeout 2000 -> 500 ms to avoid long blocks in loop and WDT.
```

### Commit 10: feat(audio): Mic type selection and audio diagnostics

```
feat(audio): Mic type selection and audio diagnostics

audio_config.h: MicType enum (SPH0645, INMP441); MICROPHONE_TYPE compile-time.
AudioCapture: channel and bit-shift by mic type; I2S register path (MSB_SHIFT
for INMP441). Optional [DIAG A1–A4] trace logging in AudioCapture, AudioActor,
TempoTracker (verbosity >= 5). Default remains SPH0645.
```

### Commit 11: docs: OTA workflow, K1 deployment, surfaces

```
docs: OTA workflow, K1 deployment, surfaces

CLAUDE.md: OTA curl examples, filesystem OTA, device ota-token; hardware
deployments table (K1 vs dev board). Add docs/surfaces (firmware-v2, tab5-encoder,
artefacts, drift-fixes). Optional: K1_Prototype_Wiring_Schematic.html.
```

### Commit 12: chore: Agent context and Tab5 serial tool

```
chore: Agent context and Tab5 serial tool

Add CLAUDE.md under firmware/, docs/api, docs/features, firmware/v2/docs and
src subtree. Add tools/tab5_serial_to_debuglog.py for serial-to-JSONL debug
capture.
```

---

## 4. Context Documentation

### 4.1 Original Problems / Requirements

- **OTA:** K1 has no USB; need reliable REST and WebSocket OTA with auth, version checks, and no LED corruption during upload (device must not “run normally” during OTA; LEDs show progress/result only).
- **Tab5:** Multi-device support (select which LightwaveOS device to control); avoid WDT from blocking in LVGL or WS callback; faster mDNS and I2C for loop responsiveness.
- **Audio:** Support both SPH0645 and INMP441 with correct channel and bit-shift; optional diagnostics for tuning.

### 4.2 Solution Design Rationale

- **Single OTA session:** OtaSessionLock ensures only one transport (REST or WS) at a time; WiFiManager checks isRestOtaInProgress() / isWsOtaInProgress() and suppresses STA retry during OTA.
- **LEDs during OTA:** OtaLedFeedback drives centre-origin progress and success/failure flashes; render loop can skip when OtaLedFeedback::isActive() (future improvement: renderer actually checking and pausing).
- **Tab5 zones refresh:** Defer zones.get to next update() so send does not run inside WS receive callback, avoiding deadlock and long blocks.
- **Tab5 WiFi scan:** Async scan + polling in checkScanStatus() keeps LVGL callback short and avoids WDT.

### 4.3 Alternatives Considered

- **OTA:** Rely only on REST or only on WS — rejected; both are needed (REST for curl/scripts, WS for dashboard).  
- **Tab5 zones:** Call requestZonesState() directly from handleZonesChanged — rejected due to send-inside-callback deadlock risk.  
- **Audio:** Runtime mic detection — rejected in favour of compile-time MicType to avoid extra code and failure modes on ESP32.

### 4.4 Technical Challenges

- **Thread safety:** OTA state read by WiFiManager (different task); spinlocks used with short critical sections (no I/O under lock).  
- **Constant-time token comparison:** Used in both REST and WS handlers to reduce timing side-channel risk.  
- **Upload handler contract:** AsyncWebServer calls upload handler per chunk but does not allow sending HTTP response from it; errors are stored in static state and checked in the request handler.

### 4.5 Performance and Security

- **Performance:** OTA progress updates every 10% to limit LED/telemetry overhead. I2C/mDNS timeouts reduced on Tab5 to keep loop under WDT.  
- **Security:** OTA token from NVS (or compile-time fallback); token never logged; constant-time comparison; optional version/force for downgrade control.

### 4.6 Testing and Limitations

- **Testing:** Manual OTA via curl and Web UI; Tab5 device selector and reconnect; WiFi scan and zone refresh on Tab5.  
- **Limitations:** Renderer does not yet skip frames when OtaLedFeedback::isActive(); that is the next step to fully meet “device does not run normally during OTA.”  
- **Known gaps:** No automated tests for OTA or Tab5 UI; coverage is manual and scenario-based.

---

## 5. Lessons Learned

- **OTA:** Do not run normal effect loop during OTA; use a single cross-transport lock and LED-only feedback so the device does not show corrupted colours.  
- **Tab5 WDT:** Avoid long synchronous work in LVGL callbacks (e.g. WiFi.scanNetworks()) and in WS receive path (e.g. sending zones.get from zones.changed). Use async + polling and deferred send.  
- **Tab5 I2C:** 32 transactions per loop; 200ms timeout risked WDT; 50ms kept behaviour and stayed under 5s.  
- **mDNS:** 2s timeout caused noticeable stalls; 500ms was sufficient for local resolution.  
- **WS send path:** processSendQueue already held send mutex; calling sendJSON() would double-lock; introducing sendJSONUnlocked() and using it from the queue path avoids that.  
- **Version:** Centralise version in version.h (string + number) for OTA, device info, and telemetry.

---

## 6. Future Contributor Guidance

### 6.1 Maintenance

- **OTA:** When adding new OTA routes or transports, use OtaSessionLock and OtaLedFeedback; keep token check constant-time.  
- **Version:** Bump FIRMWARE_VERSION_* in version.h for releases; do not hardcode version in handlers.  
- **Tab5:** Keep LVGL and WS callbacks short; defer network or send work to loop/update().

### 6.2 Potential Improvements

- **Renderer during OTA:** In RendererActor (or equivalent), if OtaLedFeedback::isActive(), skip effect rendering and only run OtaLedFeedback updates until OTA finishes.  
- **OTA telemetry:** Persist or forward Serial OTA JSON for CI or fleet monitoring.  
- **Tab5:** Unit tests for DeviceRegistry encode/decode and for WsMessageRouter zones.changed → pendingZonesRefresh.

### 6.3 Gotchas and Pitfalls

- **WiFiManager:** Do not tear down AP during OTA; always check isRestOtaInProgress() and isWsOtaInProgress() (or OtaSessionLock::isOtaInProgress()) before STA retry.  
- **Upload handler:** Cannot send HTTP response from handleUpload/handleFsUpload; set flags and handle errors in the request handler.  
- **Tab5:** Do not call WebSocketClient send or requestZonesState from inside a WS message callback; use setPendingZonesRefresh() and send on next update().

### 6.4 References

- **OTA:** `firmware/v2/src/network/webserver/handlers/FirmwareHandlers.cpp`, `ws/WsOtaCommands.cpp`, `firmware/v2/src/core/system/OtaSessionLock.h`, `OtaLedFeedback.h`, `OtaBootVerifier.h`, `OtaTokenManager.h`.  
- **Version:** `firmware/v2/src/config/version.h`.  
- **Tab5:** `firmware/Tab5.encoder/src/ui/DeviceSelectorTab.cpp`, `network/DeviceRegistry.cpp`, `WsMessageRouter.h` (handleZonesChanged), `ConnectivityTab.cpp` (startScan/checkScanStatus).  
- **API:** `docs/api/API_V1.md`, `CLAUDE.md` (OTA curl examples).

---

*End of staging and commit strategy document.*
