# K1 Composer — Zero-Configuration Auto-Connection & Dual-Connectivity Spec

**Version:** 1.1  
**Status:** Authoritative  
**Last updated:** 2026-02-26

---

## 1. Objectives

1. **Zero-configuration auto-connection:** Users never enter IP addresses or configure network settings. The dashboard discovers and connects to a K1 Lightwave device automatically on launch, or shows "device not found" when none is available.
2. **Wired-first:** K1 Lightwave is plugged into the computer for power and for computer-based software; **wired (USB) is the default/primary** means of communication. Wireless (WiFi/mDNS) is fallback.
3. **Persistent connectivity:** Intelligent reconnection with exponential backoff and health checks. No user intervention; no application crashes on disconnect.
4. **Self-healing:** Disconnections trigger automatic restart of discovery and connection protocols without manual reconnect.
5. **Dual-connectivity:** Resolve the conflict between the device's AP-centric operation (for iOS and Tab5.encoder) and the desktop user's need for **simultaneous internet access** while using K1 Composer.

---

## 2. Current Architecture (Firmware)

- **Default build (e.g. `esp32dev_audio_esv11_32khz`):** **AP+STA** mode. Device runs:
  - **Soft-AP:** Clients (iOS app, Tab5 encoder, desktop) connect to the device's WiFi (e.g. SSID `LightwaveOS-*`). AP IP is typically `192.168.4.1`.
  - **STA:** Device can connect to the user's home/router WiFi (via serial `wifi connect SSID PASS` or WiFiManager). When STA is connected, device gets an IP on the user's LAN and advertises **mDNS** as `lightwaveos.local`.
- **AP-only build (e.g. `esp32dev_FH4R2`):** `WIFI_AP_ONLY=1`. Device runs **only** Soft-AP; no STA. Clients must join the device's WiFi. mDNS still runs and resolves to the AP IP (`192.168.4.1`) for clients on that network.
- **mDNS:** Always started by WebServer. Hostname `lightwaveos`; clients resolve `lightwaveos.local` to the device IP (AP IP when AP-only, STA IP when STA connected).

---

## 3. Wired-First Bring-Up (Primary Path)

K1 Lightwave is **plugged into the computer** for power and for computer-based software; users perform that action by default. **Wired connection is the de facto/primary** means of communication for K1 Composer.

### 3.1 Wired path options

| Option | Description | Zero-config |
|--------|-------------|-------------|
| **A. Local proxy (recommended)** | A small host-side process (e.g. `k1-wired-proxy`) enumerates serial ports, probes for K1 (send JSON `device.getStatus`, expect JSON response), and exposes a local HTTP + WebSocket server (e.g. `localhost:8765`) that proxies to the device over USB serial. The dashboard tries `localhost:8765` first; if the proxy is running and the device is plugged in, connection is automatic. | Yes, if proxy is started at login or by a launcher. |
| **B. Web Serial API** | The dashboard uses `navigator.serial.getPorts()` / `requestPort()` to open the device over USB. It speaks the same JSON protocol as the firmware (`processSerialJsonCommand` in `main.cpp`). Requires user gesture for first-time port selection; thereafter `getPorts()` can reconnect without prompt. | Partial (one-time port grant). |

**Architectural references to repurpose:**

- **Firmware:** `firmware/v2/src/main.cpp` — Serial line buffer (lines 1439–1510), JSON gateway (line starting with `{` → `processSerialJsonCommand()`), response format `{"type":"...","data":{...}}`. USB CDC is enabled via `ARDUINO_USB_CDC_ON_BOOT=1` in `platformio.ini`.
- **Init/bring-up order:** `firmware/v2/docs/INIT_SEQUENCE.md` — Serial is available after boot; WebServer starts after actors. Wired proxy can probe serial as soon as the port is enumerated.
- **Tab5.encoder:** Primary network first (WiFi + mDNS), then secondary (device AP). Same priority pattern: **primary (wired) → fallback (wireless)**.
- **iOS:** `DeviceDiscoveryService` — Multi-tier: detect AP → Bonjour → HTTP probe. Repurpose as: **wired (localhost proxy / Web Serial) → wireless (cached, mDNS, 192.168.4.1)**.
- **PRISM serial bridge:** `docs/operations/SESSION_DEBRIEF_2026-02-12_SERIAL_BRIDGE.md` — SerialService, `SERIAL_TYPE_MAP` normalisation (firmware `type` → store event name), flatten `data` to root. Same protocol can be used for K1 Composer over serial or proxy.
- **Trinity bridge:** `tools/trinity-bridge/trinity-bridge.mjs` — Host-side process that connects to device (WebSocket) and forwards stdin. Reverse pattern for wired proxy: **serial in, HTTP/WS out** to localhost.

### 3.2 Wired proxy protocol (Option A)

- **Probe:** Open serial port at 115200 baud, send line `{"type":"device.getStatus"}\n`, wait for a line of JSON containing `"type"` (e.g. `{"type":"device.getStatus","data":{...}}`). If received within timeout (e.g. 2s), treat port as K1.
- **Local server:** HTTP GET `/api/v1/device/status` → proxy to serial `device.getStatus`, parse response, return JSON. WebSocket `/ws` → bidirectional: WS messages (JSON or binary) translated to/from serial JSON lines (and binary frames if needed). REST routes that the dashboard uses can be implemented by serial JSON equivalents where they exist in `processSerialJsonCommand`.
- **Port:** Default `8765` (configurable). Dashboard discovery tries `http://127.0.0.1:8765` (or `http://localhost:8765`) first.

### 3.3 Discovery order (wired-first)

1. **Wired — localhost proxy:** Try `localhost:8765` (or `127.0.0.1:8765`). If GET `/api/v1/device/status` succeeds, use `localhost:8765` as host.
2. **Wireless — cached:** Use `localStorage.k1_composer_host` if set and not localhost (already tried).
3. **Wireless — mDNS:** Try `lightwaveos.local`.
4. **Wireless — gateway:** Try `192.168.4.1`.

### 3.4 Firmware contract (K1-Lightwave wired protocol)

**Confirmed:** K1-Lightwave firmware is designed to respond over the wired (USB serial) protocol used by the proxy and Web Serial.

- **Serial transport:** `platformio.ini` sets `ARDUINO_USB_CDC_ON_BOOT=1` and `CONFIG_TINYUSB_CDC_ENABLED=1`, so when K1 is plugged in via USB, the same serial port is **USB CDC** (not a separate UART). The proxy and Web Serial talk to this port.
- **Input path:** In `main.cpp` `loop()`, serial input is read into `serialCmdBuffer`; on newline or carriage return the line is processed. Any line whose first character is `{` is passed to `processSerialJsonCommand()` (see comment “Serial JSON command gateway (for PRISM Studio Web Serial)”).
- **Response format:** Handlers in `processSerialJsonCommand()` use `serialJsonResponse(type, requestId, dataJson)` or `serialJsonError(requestId, error)`; output is a single JSON line terminated by `\n`. The proxy and dashboard expect `type`, `requestId`, and either `data` (success) or `error` (failure).
- **Commands implemented over serial (in `main.cpp`):**
  - **`device.getStatus`** — Implemented. Returns device info (freeHeap, uptimeMs, wifi, fps, effectCount). Used for probe and for GET `/api/v1/device/status`.
  - Many other commands (effects, zones, parameters, setEffect, etc.) are implemented; see the full `if (strcmp(type, ...))` chain in `processSerialJsonCommand()`.
- **Commands not implemented over serial (WebSocket only):**
  - **`control.status`**, **`control.acquire`**, **`control.release`**, **`control.heartbeat`** — Handled only in `WsControlCommands.cpp` (WebSocket). In `main.cpp`, these types are listed in `isSerialJsonReadOnlyCommand()` (so they are allowed without a lease) but there is **no** `strcmp(type, "control.status")` handler in `processSerialJsonCommand()`. Sending `control.status` over serial therefore results in `serialJsonError(requestId, "unknown command type")`.
- **Implication for wired proxy:** GET `/api/v1/control/status` is implemented by the proxy as “send `control.status` over serial”. Current firmware responds with `"unknown command type"`. The proxy treats that as “control lease status not available over serial” and returns a stub response (e.g. `active: false`, `remainingMs: 0`) so the dashboard continues to work; when firmware adds a serial handler for `control.status`, the proxy will return real data without change.

**References:** `firmware/v2/src/main.cpp` (serial buffer, `processSerialJsonCommand`, `serialJsonResponse`/`serialJsonError`), `firmware/v2/platformio.ini` (USB CDC), `firmware/v2/src/network/webserver/ws/WsControlCommands.cpp` (control.* over WebSocket only).

---

## 4. Discovery Strategy (Zero-Config) — Full Order

The dashboard determines the device **without user input** using this order:

| Priority | Method | When it works |
|----------|--------|----------------|
| 1 | **Wired — localhost proxy** | K1 plugged in via USB; `k1-wired-proxy` (or equivalent) running; GET `http://localhost:8765/api/v1/device/status` succeeds. |
| 2 | **Wireless — cached** | Last successful host in localStorage (skip if already tried as wired). |
| 3 | **Wireless — mDNS** | Resolve `lightwaveos.local` (attempt fetch/WS). Device on same L2 (AP or STA). |
| 4 | **Wireless — gateway** | Try `192.168.4.1` (device AP or mDNS fallback). |

**Implementation notes:**

- **mDNS in browser:** Browsers do not expose raw mDNS. "Resolve lightwaveos.local" = attempt fetch/WebSocket to `http://lightwaveos.local`; OS resolves `.local` via mDNS.
- **No manual host input:** Host input remains for advanced override. On load, the app runs discovery (wired first, then wireless) and auto-connects to the first responding host.

---

## 5. Connection State Machine & Reconnection

- **States:** `disconnected` → `discovering` → `connecting` → `connected` → (on drop) → `reconnecting` → `connecting` → `connected`.
- **On load:** Enter `discovering`, run discovery sequence (try lightwaveos.local, then 192.168.4.1). On first host that responds (e.g. HTTP GET `/api/v1/device/status` or WebSocket open), use that host and move to `connecting` then `connected`.
- **On disconnect (WebSocket close/error):** Enter `reconnecting`. Do **not** crash; do **not** require user to click Connect. Start reconnection loop with **exponential backoff** (e.g. 1s, 2s, 4s, 8s, cap at 30s). Each attempt: try same host (cached) first; if several failures, optionally re-run discovery once.
- **Health check:** While connected, optional periodic ping (e.g. GET `/api/v1/device/status` or WebSocket ping/pong) to detect half-open connections. On failure, treat as disconnect and enter reconnecting.
- **Persistence:** Cache last successful host in `localStorage` so next session can try it first (then mDNS, then 192.168.4.1).

---

## 6. Dual-Connectivity: AP-Only vs Internet for Desktop

**Conflict:** K1 currently supports (a) **AP-only** builds for compatibility with iOS and Tab5 (device is the WiFi AP; clients join it), and (b) **AP+STA** builds where the device also joins the user's WiFi. When a **desktop** user connects to the device's **AP**, the desktop typically has only one WiFi interface and thus **loses internet** while connected to K1.

**Options:**

### Option A: AP+STA (recommended for “desktop + internet”)

- **Behaviour:** Device runs **AP+STA**. STA connects to user's home WiFi (credentials via serial or WiFiManager). AP remains up for iOS/Tab5. mDNS advertises `lightwaveos.local` on the STA interface.
- **Desktop:** Stays on **home WiFi** (has internet). Resolves `lightwaveos.local` to device's STA IP. Connects to device over LAN. No loss of internet.
- **iOS / Tab5:** Connect to device's **AP** as today. No change.
- **Pros:** No extra hardware; single firmware image; desktop has internet; zero-config via mDNS.  
- **Cons:** Device must have STA credentials configured; some deployments may prefer not to join user's network.

### Option B: AP-only + desktop dual interface

- **Behaviour:** Device is AP-only (`WIFI_AP_ONLY=1`). Desktop has **two** network interfaces (e.g. Ethernet for internet, WiFi for K1).
- **Desktop:** Connects to K1's AP on WiFi; uses Ethernet (or USB tethering) for internet. Discovery: try `lightwaveos.local` (on K1's AP, mDNS gives 192.168.4.1) or try 192.168.4.1.
- **Pros:** Device never joins user's network; simple device behaviour.  
- **Cons:** Requires second interface on desktop; user must join K1's AP on that interface.

### Option C: Bridge / proxy

- **Behaviour:** A separate device or app (e.g. Raspberry Pi, or a phone app) joins K1's AP and bridges to the user's LAN. Desktop on LAN talks to the bridge; bridge proxies to K1.
- **Pros:** Desktop on LAN has internet; K1 can stay AP-only.  
- **Cons:** Extra hardware/software to deploy and maintain.

### Option D: AP-only, single-interface desktop (no internet while using K1)

- **Behaviour:** Desktop has one WiFi interface. User joins K1's AP. Dashboard discovers device at 192.168.4.1 (gateway). Desktop has **no** internet while connected to K1.
- **Pros:** Works with current AP-only builds; no firmware change.  
- **Cons:** No simultaneous internet for desktop.

**Recommendation:** Use **Option A (AP+STA)** for builds targeting desktop + internet. Keep **Option D** and **Option B** as fallbacks. Use **AP-only** build only where required (e.g. FH4R2 hardware); document that desktop users can either use a second interface (Option B) or accept no internet (Option D). Document Option A in the implementation path so default firmware supports STA and mDNS on LAN.

---

## 7. Protocols & Technical Details

| Concern | Approach |
|---------|----------|
| Discovery | 1) Try `lightwaveos.local` (fetch or WS). 2) Try `192.168.4.1`. 3) Cache last host in localStorage; next session try cache first. |
| Connectivity check | GET `http://<host>/api/v1/device/status` (no auth required for status) or WebSocket open. |
| Reconnection backoff | Exponential: 1s, 2s, 4s, 8s, 16s, 30s (cap). Reset backoff on successful connect. |
| Host persistence | `localStorage.k1_composer_host` = last successful host. Clear on explicit "Disconnect" or after many failures. |
| WS ping/pong | Optional: use WebSocket ping frames or periodic REST status poll to detect half-open; on failure, trigger reconnect. |

---

## 8. Implementation Roadmap

### Phase 1 — Dashboard: Zero-config + wired-first + reconnection (this spec)

1. **Discovery module (wired-first):** `resolveDeviceHost()`: try **localhost:8765** (wired proxy) first, then cached host (localStorage), then `lightwaveos.local`, then `192.168.4.1`; return first host that responds to GET `/api/v1/device/status`. Use timeout per attempt (e.g. 3s).
2. **Auto-connect on load:** After `init()`, if not already connected, run discovery and connect to first successful host.
3. **Reconnection loop:** On WebSocket close/error, enter reconnecting state; exponential backoff; attempt reconnect to cached host (then re-run discovery if repeated failures). No user click required.
4. **UI:** Hide or de-emphasise host input (e.g. "Device: auto (lightwaveos.local)" with expandable override). Show "Connecting…", "Connected", "Reconnecting…", "No device found".
5. **Persistence:** Save last successful host to localStorage; prefer it on next load.
6. **Wired proxy (optional but recommended):** Run `tools/k1-wired-proxy` when K1 is plugged in via USB; dashboard discovers `localhost:8765` first. See `tools/k1-wired-proxy/README.md`.

### Phase 2 — Firmware (optional)

1. Ensure mDNS is stable in both AP and STA modes (already the case).
2. Document STA setup (serial `wifi connect`, or WiFiManager) for "desktop + internet" deployments.
3. For AP-only builds, document that desktop users either use a second interface or accept no internet; discovery still works via 192.168.4.1 when on device AP.

### Phase 3 — Dual-connectivity documentation

1. Add a short "Network modes" section to user/docs: AP+STA (recommended for desktop), AP-only (iOS/Tab5, or single-interface desktop without internet).
2. Build matrix: which env is AP-only (FH4R2), which is AP+STA (default).

---

## 9. Fallback & Edge Cases

- **mDNS blocked:** e.g. corporate networks. Fallback to 192.168.4.1 when client is on device AP; or user can use manual host override.
- **Multiple devices:** First responding host wins. Future: optional list from mDNS TXT or discovery API.
- **Offline first load:** Discovery fails → show "No device found. Connect to K1's WiFi or ensure device is on this network." No crash.
- **User disconnects on purpose:** "Disconnect" button clears cache and stops reconnection (or stops for this session). Next load runs discovery again.

---

## 10. Acceptance Criteria

- [x] On first load, dashboard attempts discovery (cached host, then lightwaveos.local, then 192.168.4.1) and connects without user entering an IP.
- [x] If no device responds, dashboard shows "No device found" and does not crash.
- [x] On WebSocket disconnect, dashboard enters reconnecting state and retries with exponential backoff without user action.
- [x] Last successful host is cached (localStorage) and tried first on next session.
- [x] Manual host override remains available (Device Host input; Connect/Disconnect button).
- [x] Document dual-connectivity options (AP+STA vs AP-only) and recommendation for desktop + internet (Section 5).

## 11. Implementation Summary (2026-02-26)

- **Discovery (wired-first):** `resolveDeviceHost()` tries (1) wired proxy `localhost:8765`, (2) cached host (localStorage), (3) `lightwaveos.local`, (4) `192.168.4.1`. `probeHost(host)` uses `fetch` GET `/api/v1/device/status` with 3s timeout.
- **Wired proxy:** `tools/k1-wired-proxy` probes USB serial for K1 (device.getStatus), then exposes HTTP + WebSocket on port 8765 so the dashboard can connect to localhost without WiFi.
- **Auto-connect on load:** `init()` calls `autoConnect()` after `bindEvents()`. `autoConnect()` sets badge "Connecting…", runs discovery, then `connect(host)` or shows "No device found".
- **Reconnection:** On WebSocket `onclose`, if `!state.userRequestedDisconnect`, `scheduleReconnect()` runs with exponential backoff (1s, 2s, 4s, 8s, 16s, 30s cap). `tryReconnect()` reuses last host and calls `connect(host)`.
- **Disconnect:** "Disconnect" button (when connected) sets `userRequestedDisconnect`, stops reconnect timer, clears localStorage cache, and cleans up WS.
- **UI:** Connect button toggles to "Disconnect" when connected; shows "Reconnecting…" and is disabled during reconnection. Device Host input default empty; placeholder "Auto (lightwaveos.local)".
