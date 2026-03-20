---
abstract: "External integrations and communication protocols across LightwaveOS ecosystem: REST API (47 endpoints), WebSocket real-time control, UDP LED/audio streaming, mDNS discovery, OTA firmware updates, serial debug/proxy. No cloud dependencies. Updated 2026-03-21."
---

# External Integrations

**Analysis Date:** 2026-03-21

## APIs & External Services

**LightwaveOS API (Firmware-provided):**
- **Protocol:** REST (HTTP 1.1) + WebSocket (RFC 6455) + UDP binary
- **Base URL:** `http://192.168.4.1:80` (K1 AP at fixed IP)
- **API Version:** v1 (`/api/v1/*`)
- **Endpoints:** 47 documented REST endpoints
  - Device info: `GET /api/v1/device`, `GET /api/v1/device/stats`
  - Effects: `GET /api/v1/effects`, `POST /api/v1/effects/active` (set active effect)
  - Palettes: `GET /api/v1/palettes`, `GET /api/v1/palettes/{id}`
  - Zones: `GET /api/v1/zones`, `POST /api/v1/zones/{id}/effect` (per-zone control)
  - Parameters: `POST /api/v1/parameters/{name}` (effect parameter updates)
  - Presets: `GET /api/v1/presets`, `POST /api/v1/presets/{id}/activate`
  - Shows/Narratives: `GET /api/v1/shows`, `POST /api/v1/shows/{id}/play`
  - OTA: `POST /api/v1/firmware/upload` (multipart firmware update)
  - Audio: `GET /api/v1/audio/bands`, `GET /api/v1/audio/beat`
  - Telemetry: `GET /api/v1/telemetry` (frame timing, FPS, heap, heap-shed state)
- **Rate limit:** 20 requests/sec (enforced by iOS RESTClient)
- **Error codes:** Standard HTTP (200, 400, 404, 413 for large payloads, 503 for OTA conflicts)
- **Documentation:** `firmware-v3/docs/api/api-v1.md` (2,124 lines)

**WebSocket Control Channel:**
- **Endpoint:** `ws://192.168.4.1/ws`
- **Protocol:** JSON messages (text frames) + binary LED/audio frames
- **Features:**
  - Real-time command subscription (zone changes, effect switches)
  - Status broadcasts (device state, beat events, zone state changes)
  - Audio metrics (frame: BPM, confidence, RMS, bands, chroma)
  - LED stream subscription (binary RGB frames)
- **Message types:** `status`, `beat.event`, `zones.list`, `effects.list`, `palettes.list`, `parameters.changed`, `device.status`, `ledStream.subscribed`, `audio.subscribed`
- **Auto-reconnect:** Exponential backoff (iOS: 3s initial, 30s max, 2.0x multiplier)
- **Implementation:**
  - Firmware: `ESPAsyncWebServer` + manual WebSocket handlers in `WsHandlers.cpp`
  - iOS: `URLSessionWebSocketTask` (native Apple framework, actor-based service)

**Serial Debug Interface:**
- **Endpoint:** USB CDC (`/dev/cu.usbmodem*` on macOS)
- **Baud rate:** 115200
- **Commands:** Single-char (e.g., `c` = cycle effect, `p` = print status, `d` = debug toggle)
- **Capabilities:** Effect cycling, parameter tuning, boot/reset, WiFi diagnostics
- **Note:** Opening port resets board (DTR/RTS toggle on macOS) — feature by design for safety

## Data Storage

**Databases:**
- **None.** No cloud database, no local SQL database. State is ephemeral + NVS only.

**Persistent Storage (On-Device):**
- **ESP32 NVS (Non-Volatile Storage)** — key-value store (littlefs filesystem)
  - Zone configuration (per-zone effect, palette, blend mode)
  - User presets (effect settings snapshots)
  - Device metadata (MAC, SSID, AP password — but AP is now open)
  - `NVSManager` abstraction in firmware (`src/core/persistence/NVSManager.h`)
  - Structured serialization via `struct` blobs, not JSON to NVS

**File Storage:**
- **littlefs** (ESP32 flash filesystem) — show bundles, plugin binaries, effect metadata
- **Local filesystem only** — no S3, no cloud storage
- **Show bundles:** Parseable binary format (Prim8, custom codec in `ShowBundleParser.cpp`)

**Caching:**
- **None.** All data either computed per-frame or loaded at startup
- **Optional:** iOS may cache device info locally (UserDefaults) but not required

## Authentication & Identity

**Auth Provider:**
- **None.** Open network, no passwords, no tokens, no OAuth.
- **K1 AP:** Open WiFi network (SSID: "LightwaveOS-AP", no password)
  - Rationale: Simplified discovery, no WPA handshake failures, development-focused

**Security (Minimal):**
- **No TLS/HTTPS.** HTTP-only (local network only, not internet-exposed)
- **No API tokens or session auth.** Stateless REST, WebSocket connection = implicit auth (on local network)
- **Serial: No auth.** USB device access is physical security only
- **OTA Token:** Temporary single-use token for firmware upload (in-memory, not persisted)
  - `OtaTokenManager` generates token, validates request signature, revokes after upload
  - Prevents replay attacks during OTA window only

**Device Discovery:**
- **mDNS/Bonjour** (zero-conf)
  - Service type: `_http._tcp` (standard HTTP)
  - Custom service: `_lightwaveos._tcp` (optional, for branding)
  - iOS announces Bonjour interest in `Info.plist` (`NSBonjourServices` array)
  - Firmware advertises via `ESPAsyncWebServer` (automatic)
  - Implementation: iOS `Network.framework` → `NWBrowser` with Bonjour browser

## Monitoring & Observability

**Error Tracking:**
- **None.** No Sentry, no external error service.
- **Firmware errors:** Logged to serial console + optional `esp_logger` ESP-IDF integration
- **iOS errors:** Standard Apple Xcode console + UserDefaults debug log (optional)

**Logs:**
- **Firmware:** Serial console (115200 baud), structured `LW_LOG_*` macros (tag-based, level: Error/Warn/Info/Debug)
  - Log level: Configurable per environment (`LW_LOG_LEVEL` build flag, default 3 = Info)
- **iOS:** Xcode console, optional UserDefaults-persisted app debug logs
- **Tab5:** Serial console (same UART as firmware)
- **No remote logging.** Logs are local only, captured manually via serial monitor or app console

**Metrics:**
- **Firmware telemetry:** `/api/v1/telemetry` endpoint provides real-time metrics
  - Frame timing (show time, render time, total FPS)
  - Heap usage (free, largest block, heap-shed state)
  - Audio metrics (RMS, beat confidence, tempo)
  - Task stack usage (per-core)
- **Audio pipeline metrics:** 64-bin Goertzel features (octave bands, chroma, beat, tempo)
- **Performance monitor:** `PerformanceMonitor` class tracks per-effect render time, network latency

## CI/CD & Deployment

**Hosting:**
- **K1 device itself.** No external hosting. Device is a self-contained AP + HTTP server.
- **Deployment target:** ESP32-S3 (firmware), iPhone 17+ (iOS), ESP32-P4 (Tab5)

**OTA Firmware Updates:**
- **Mechanism:** Multipart HTTP POST to `/api/v1/firmware/upload`
- **Flow:**
  1. Client calculates SHA256 of new firmware binary
  2. Requests OTA session: `POST /api/v1/firmware/prepare` → gets single-use token + session ID
  3. Uploads binary in chunks: `POST /api/v1/firmware/upload` with token
  4. Triggers flashing: `POST /api/v1/firmware/flash` (replaces running partition)
  5. Device boots into OTA partition
  6. `OtaBootVerifier` validates app is healthy within 30s; if not, auto-rolls back to previous partition
- **Rollback:** Automatic on boot failure (configurable time window, currently 30s)
- **Implementation:** `OtaSessionLock`, `OtaBootVerifier`, `OtaTokenManager`, `OtaLedFeedback` (ESP-IDF app_update API)
- **iOS tool:** Web-based OTA upload form (no native CLI tool yet)

**CI Pipeline:**
- **None.** No GitHub Actions, no Jenkins. Manual build/test on developer machine.
- **Testing:**
  - Firmware: `pio run -e native_test` (unit tests on host)
  - iOS: `xcodebuild test` (XCTest)
  - Python: `pytest` (led_capture, benchmark suite)
- **Linting:** ruff (Python), clang-format (C++, optional)

## Environment Configuration

**Required env vars (Firmware):**
- `WIFI_SSID` — AP SSID to broadcast (default: "LightwaveOS-AP")
- `WIFI_PASSWORD` — AP password (default: empty, open network)
- `K1_LED_STRIP1_DATA` — GPIO pin for strip 1 data line (K1v2: GPIO 6)
- `K1_LED_STRIP2_DATA` — GPIO pin for strip 2 data line (K1v2: GPIO 7)
- `I2S_DOUT_PIN`, `I2S_BCLK_PIN`, `I2S_LRCLK_PIN` — microphone I2S pins
- Passed via `wifi_credentials.ini` (gitignored) or build flags at compile time

**Required env vars (iOS):**
- None at runtime. Device discovery is automatic via mDNS.

**Secrets location:**
- **None stored.** WiFi credentials in `wifi_credentials.ini` (gitignored, template provided)
- **OTA token:** In-memory only (issued per-session, not persisted)

## Webhooks & Callbacks

**Incoming (Firmware receives):**
- **REST requests** — standard HTTP handlers (`AsyncWebServer` built-in)
- **WebSocket messages** — JSON text frames, event subscription/unsubscription

**Outgoing (Firmware sends):**
- **WebSocket broadcasts** — to all connected clients (beat events, zone state changes, device status)
- **No webhook callbacks to external servers.** All communication is pull-based (clients request) or WebSocket push (to local clients only)

## UDP Binary Streaming

**LED Frame Stream:**
- **Protocol:** Custom binary format (9 bytes header + 960 bytes RGB data + 32 bytes metrics trailer = 1,001 bytes per frame)
- **Port:** 41234 (UDP)
- **Direction:** Firmware → iOS (one-way broadcast, no ACK)
- **Payload structure:**
  ```
  [1 byte frame type] [1 byte frame counter] [1 byte reserved]
  [960 bytes: 320 LEDs × 3 (RGB)]
  [32 bytes: metrics trailer — beat, RMS, bands, confidence]
  ```
- **Frequency:** 15-30 FPS configurable (via `capture stream` serial command)
- **Implementation:** Async UDP listener in firmware (low-priority Core 0 task)
- **iOS:** `UDPStreamReceiver` actor listens on `255.255.255.255:41234` (broadcast)
- **Use case:** Real-time LED visualization with minimal latency (no WebSocket framing overhead)

**Audio Metrics Stream:**
- **Embedded in LED frames** (trailing 32 bytes) OR sent separately via WebSocket JSON
- **Metrics:** BPM, beat confidence, RMS energy, octave bands [0-7], chroma [0-11], onset trigger
- **Frequency:** Frame-sync (same as LED stream)

## Protocol Contracts

**WebSocket Message Format (JSON):**
```json
{
  "type": "beat.event|zones.list|parameters.changed|...",
  "data": { ... },
  "timestamp": 1679123456
}
```

**REST Response Format:**
```json
{
  "success": true|false,
  "data": { ... },
  "error": "optional error message",
  "version": "1.0"
}
```

**Audio Metrics Contract:**
```json
{
  "rms": 0.12,
  "beat": true,
  "beatConfidence": 0.95,
  "tempo": 128.5,
  "bands": [0.1, 0.2, 0.15, 0.08, 0.05, 0.03, 0.02, 0.01],
  "chroma": [0.1, 0.15, ..., 0.05]  // 12 pitch classes
}
```

**Effect Parameter Update:**
```json
{
  "parameter": "brightness",
  "value": 255,
  "zone": 0
}
```

## Cross-Device Communication

**K1 ↔ iOS:**
- **REST:** Stateless, pull-based (iOS asks for device state, effect list, etc.)
- **WebSocket:** Stateful, bidirectional (iOS sends commands, K1 broadcasts updates)
- **UDP:** One-way broadcast (K1 sends LED frames + metrics)

**K1 ↔ Tab5:**
- **WebSocket:** Same protocol as iOS (Tab5 is a client like iOS)
- **Implementation:** Tab5 connects to same `/ws` endpoint as iOS

**K1 ↔ Serial Console:**
- **Serial Protocol:** Single-character commands + newline, echo responses
- **Use case:** Developer debugging, local testing (no network needed)

**k1-composer Proxy ↔ K1:**
- **Serial connection** — USB CDC via `serialport` Node.js library
- **Proxy ↔ Browser:** Local WebSocket at `ws://localhost:8080` (or configurable port)
- **Translation:** Serial messages ↔ WebSocket frames
- **Use case:** Debug tool for developers (firmware inspection, live effect tuning)

## Device Discovery Protocol

**mDNS/Bonjour Flow:**
1. K1 boots, starts WiFi AP
2. `ESPAsyncWebServer` auto-advertises via mDNS as `lightwaveos._http._tcp.local` (or custom `_lightwaveos._tcp`)
3. iOS app initializes `DeviceDiscoveryService`
4. `NWBrowser` scans local network for `_http._tcp` and `_lightwaveos._tcp` services
5. For each discovered service, resolve hostname to IP (e.g., `lightwaveos.local` → `192.168.4.1`)
6. User taps device in UI → app connects via REST/WebSocket

**Fallback:** If mDNS fails, manual IP entry (user enters `192.168.4.1` directly)

## Rate Limiting & Throttling

**REST API:**
- **Firmware:** No explicit rate limiting (async handlers)
- **iOS client:** 20 req/s limit enforced in `RESTClient` (raises `APIClientError.rateLimited`)
- **Per-endpoint:** Some endpoints (OTA, firmware operations) lock globally via `OtaSessionLock`

**WebSocket:**
- **No rate limiting.** Messages queued per-client (queue size: `WS_MAX_QUEUED_MESSAGES=32`)
- **Overflow handling:** If client queue full, new frames dropped silently (client recovers on next status broadcast)

**Serial Commands:**
- **No rate limiting.** Processed immediately, echo'd back

## Synchronization & Timing

**Audio-to-Visual Sync:**
- **Audio processed every I2S buffer** (typically 10-20ms at 32 kHz 512-sample buffers)
- **Features computed:** Goertzel 64-bin analysis → octave bands, chroma, beat detection
- **Features available to effects via `ControlBus`** (read-only snapshot, updated every audio frame)
- **Rendering:** Synchronized to 120 FPS (8.33ms per frame) via `vTaskDelay(0)` in RendererActor
- **Beat events:** Published via WebSocket to clients (low-latency, < 50ms typical)

**Network-to-Device Sync:**
- **REST commands:** Applied immediately in command handler (next render frame picks up state)
- **WebSocket commands:** Same as REST (async, applied next frame)
- **LED visual feedback:** Rendered 120 FPS, streamed via UDP/WebSocket to iOS

---

*Integration audit: 2026-03-21*
