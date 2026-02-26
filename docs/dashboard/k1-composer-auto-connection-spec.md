# K1 Composer — Zero-Configuration Auto-Connection & Dual-Connectivity Spec

**Version:** 1.0  
**Status:** Authoritative  
**Last updated:** 2026-02-26

---

## 1. Objectives

1. **Zero-configuration auto-connection:** Users never enter IP addresses or configure network settings. The dashboard discovers and connects to a K1 Lightwave device automatically on launch, or shows "device not found" when none is available.
2. **Persistent connectivity:** Intelligent reconnection with exponential backoff and health checks. No user intervention; no application crashes on disconnect.
3. **Self-healing:** Disconnections trigger automatic restart of discovery and connection protocols without manual reconnect.
4. **Dual-connectivity:** Resolve the conflict between the device's AP-centric operation (for iOS and Tab5.encoder) and the desktop user's need for **simultaneous internet access** while using K1 Composer.

---

## 2. Current Architecture (Firmware)

- **Default build (e.g. `esp32dev_audio_esv11_32khz`):** **AP+STA** mode. Device runs:
  - **Soft-AP:** Clients (iOS app, Tab5 encoder, desktop) connect to the device's WiFi (e.g. SSID `LightwaveOS-*`). AP IP is typically `192.168.4.1`.
  - **STA:** Device can connect to the user's home/router WiFi (via serial `wifi connect SSID PASS` or WiFiManager). When STA is connected, device gets an IP on the user's LAN and advertises **mDNS** as `lightwaveos.local`.
- **AP-only build (e.g. `esp32dev_FH4R2`):** `WIFI_AP_ONLY=1`. Device runs **only** Soft-AP; no STA. Clients must join the device's WiFi. mDNS still runs and resolves to the AP IP (`192.168.4.1`) for clients on that network.
- **mDNS:** Always started by WebServer. Hostname `lightwaveos`; clients resolve `lightwaveos.local` to the device IP (AP IP when AP-only, STA IP when STA connected).

---

## 3. Discovery Strategy (Zero-Config)

The dashboard determines the device host **without user input** using this order:

| Priority | Method | When it works |
|----------|--------|----------------|
| 1 | **mDNS** | Resolve `lightwaveos.local`. Works when device is on the same L2 network (desktop on device's AP, or desktop and device on same LAN via STA). |
| 2 | **Gateway detection** | When the client is on `192.168.4.x` (device's AP subnet), the gateway is almost always the device: use `192.168.4.1`. Detected via optional network info or by trying 192.168.4.1. |
| 3 | **Fallback** | Try well-known default `192.168.4.1` once (e.g. after mDNS fails in environments where mDNS is blocked). |

**Implementation notes:**

- **mDNS in browser:** Browsers do not expose raw mDNS (multicast DNS) to JavaScript. So "resolve lightwaveos.local" from the dashboard is done by **attempting a fetch/WebSocket to `http://lightwaveos.local`** (or `ws://lightwaveos.local/ws`). The OS resolves `.local` via mDNS (Bonjour on macOS, Avahi on Linux, etc.). If the device is on the same network, the connection succeeds; if not, it fails. So "discovery" = try connect to `lightwaveos.local` first; on failure, try gateway (when we can infer we're on 192.168.4.x) or 192.168.4.1.
- **Inferring "on device AP":** We can try `192.168.4.1` as a second attempt when `lightwaveos.local` fails (no need to read gateway from OS—just try 192.168.4.1). If the user is on the device's AP, 192.168.4.1 is the device. If the user is on home WiFi and device is STA on same LAN, lightwaveos.local usually works.
- **No manual host input:** The host input can remain in the UI for advanced override but is **not required**. On load, the app runs discovery then auto-connects to the first responding host.

---

## 4. Connection State Machine & Reconnection

- **States:** `disconnected` → `discovering` → `connecting` → `connected` → (on drop) → `reconnecting` → `connecting` → `connected`.
- **On load:** Enter `discovering`, run discovery sequence (try lightwaveos.local, then 192.168.4.1). On first host that responds (e.g. HTTP GET `/api/v1/device/status` or WebSocket open), use that host and move to `connecting` then `connected`.
- **On disconnect (WebSocket close/error):** Enter `reconnecting`. Do **not** crash; do **not** require user to click Connect. Start reconnection loop with **exponential backoff** (e.g. 1s, 2s, 4s, 8s, cap at 30s). Each attempt: try same host (cached) first; if several failures, optionally re-run discovery once.
- **Health check:** While connected, optional periodic ping (e.g. GET `/api/v1/device/status` or WebSocket ping/pong) to detect half-open connections. On failure, treat as disconnect and enter reconnecting.
- **Persistence:** Cache last successful host in `localStorage` so next session can try it first (then mDNS, then 192.168.4.1).

---

## 5. Dual-Connectivity: AP-Only vs Internet for Desktop

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

## 6. Protocols & Technical Details

| Concern | Approach |
|---------|----------|
| Discovery | 1) Try `lightwaveos.local` (fetch or WS). 2) Try `192.168.4.1`. 3) Cache last host in localStorage; next session try cache first. |
| Connectivity check | GET `http://<host>/api/v1/device/status` (no auth required for status) or WebSocket open. |
| Reconnection backoff | Exponential: 1s, 2s, 4s, 8s, 16s, 30s (cap). Reset backoff on successful connect. |
| Host persistence | `localStorage.k1_composer_host` = last successful host. Clear on explicit "Disconnect" or after many failures. |
| WS ping/pong | Optional: use WebSocket ping frames or periodic REST status poll to detect half-open; on failure, trigger reconnect. |

---

## 7. Implementation Roadmap

### Phase 1 — Dashboard: Zero-config + reconnection (this spec)

1. **Discovery module:** `resolveDeviceHost()`: try `lightwaveos.local`, then `192.168.4.1`; return first host that responds to GET `/api/v1/device/status` (or WS open). Use timeout per attempt (e.g. 3s).
2. **Auto-connect on load:** After `init()`, if not already connected, run discovery and connect to first successful host. Optionally try `localStorage` host first.
3. **Reconnection loop:** On WebSocket close/error, enter reconnecting state; exponential backoff; attempt reconnect to cached host (then re-run discovery if repeated failures). No user click required.
4. **UI:** Hide or de-emphasise host input (e.g. "Device: auto (lightwaveos.local)" with expandable override). Show "Connecting…", "Connected", "Reconnecting…", "No device found".
5. **Persistence:** Save last successful host to localStorage; prefer it on next load.

### Phase 2 — Firmware (optional)

1. Ensure mDNS is stable in both AP and STA modes (already the case).
2. Document STA setup (serial `wifi connect`, or WiFiManager) for "desktop + internet" deployments.
3. For AP-only builds, document that desktop users either use a second interface or accept no internet; discovery still works via 192.168.4.1 when on device AP.

### Phase 3 — Dual-connectivity documentation

1. Add a short "Network modes" section to user/docs: AP+STA (recommended for desktop), AP-only (iOS/Tab5, or single-interface desktop without internet).
2. Build matrix: which env is AP-only (FH4R2), which is AP+STA (default).

---

## 8. Fallback & Edge Cases

- **mDNS blocked:** e.g. corporate networks. Fallback to 192.168.4.1 when client is on device AP; or user can use manual host override.
- **Multiple devices:** First responding host wins. Future: optional list from mDNS TXT or discovery API.
- **Offline first load:** Discovery fails → show "No device found. Connect to K1's WiFi or ensure device is on this network." No crash.
- **User disconnects on purpose:** "Disconnect" button clears cache and stops reconnection (or stops for this session). Next load runs discovery again.

---

## 9. Acceptance Criteria

- [x] On first load, dashboard attempts discovery (cached host, then lightwaveos.local, then 192.168.4.1) and connects without user entering an IP.
- [x] If no device responds, dashboard shows "No device found" and does not crash.
- [x] On WebSocket disconnect, dashboard enters reconnecting state and retries with exponential backoff without user action.
- [x] Last successful host is cached (localStorage) and tried first on next session.
- [x] Manual host override remains available (Device Host input; Connect/Disconnect button).
- [x] Document dual-connectivity options (AP+STA vs AP-only) and recommendation for desktop + internet (Section 5).

## 10. Implementation Summary (2026-02-26)

- **Discovery:** `probeHost(host)` uses `fetch` GET `/api/v1/device/status` with 3s timeout. `resolveDeviceHost()` tries cached host (localStorage), then `lightwaveos.local`, then `192.168.4.1`.
- **Auto-connect on load:** `init()` calls `autoConnect()` after `bindEvents()`. `autoConnect()` sets badge "Connecting…", runs discovery, then `connect(host)` or shows "No device found".
- **Reconnection:** On WebSocket `onclose`, if `!state.userRequestedDisconnect`, `scheduleReconnect()` runs with exponential backoff (1s, 2s, 4s, 8s, 16s, 30s cap). `tryReconnect()` reuses last host and calls `connect(host)`.
- **Disconnect:** "Disconnect" button (when connected) sets `userRequestedDisconnect`, stops reconnect timer, clears localStorage cache, and cleans up WS.
- **UI:** Connect button toggles to "Disconnect" when connected; shows "Reconnecting…" and is disabled during reconnection. Device Host input default empty; placeholder "Auto (lightwaveos.local)".
