---
abstract: Forensic survey of current firmware-v3 WiFi source state for K1. Reports infrastructure presence, mode-init code path, feature flags, credential storage, captive-portal vs router-bridge distinction, and gap-to-dual-mode analysis. Bottom line - K1 firmware contains a fully functional STA + AP-mode-switch infrastructure (singleton FreeRTOS state machine, NVS credential store, REST + serial control surfaces) that is HARD-DISABLED at compile time by the WIFI_AP_ONLY flag in the canonical K1 build envs. Removing the flag does NOT yield clean dual-mode - several internal paths still hardcode AP-only behaviour or use AP+STA concurrent mode (the documented esp-idf 4.4.x bug surface).
---

# SSA Forensic-B - Current firmware-v3 source state

Working tree: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip`. All citations are file-relative paths under `firmware-v3/`. Read-only survey - no source files modified.

---

## Task 1 - WiFi-related file inventory

Search: `find firmware-v3/src firmware-v3/include -iname '*wifi*' -o -iname '*ap*' -o -iname '*sta*' -o -iname '*network*'` plus follow-up callers.

Core WiFi/network files (excluding effect/audio false positives):

| File | Lines | Role |
|---|---|---|
| `src/network/WiFiManager.h` | 574 | Singleton state machine + Portable Mode runtime API surface (declares `requestSTAEnable`, `requestAPOnly`, `connectToNetwork`, `connectToSavedNetwork`, `saveNetwork`, `deleteSavedNetwork`, `getSavedNetworks`, `getLastConnectedSSID`) |
| `src/network/WiFiManager.cpp` | 1256 | Full implementation - 7-state FSM (`INIT/SCANNING/CONNECTING/CONNECTED/FAILED/AP_MODE/DISCONNECTED`), event-group sync, async scan, smart-network selector, AP-only short-circuits, AP+STA concurrent-mode switch in `startSoftAP()` and `connectToNetwork()` |
| `src/network/WiFiCredentialsStorage.h` | 194 | NVS-backed `NetworkCredential{ssid,password}` store, MAX_NETWORKS=10, namespace `wifi_creds`, JSON-encoded `net_0..net_9`, last-connected tracking |
| `src/network/WiFiCredentialsStorage.cpp` | 453 | Full NVS implementation |
| `src/network/WiFiCredentialManager.h` | 305 | Higher-level credential lifecycle wrapper (separate from `WiFiCredentialsStorage`) |
| `src/network/WiFiCredentialManager.cpp` | 456 | Implementation |
| `src/network/webserver/handlers/NetworkHandlers.cpp` | 388+ | REST handlers - `handleConnect`, `handleDisconnect`, `handleSavedList/Add/Delete`, `handleScanNetworks`, `handleScanStatus`, `handleEnableSTA`, `handleEnableAPOnly`, `handleListNetworks`, `handleAddNetwork` |
| `src/network/webserver/handlers/NetworkHandlers.h` | (declarations) | Public REST handler surface |
| `src/network/webserver/ws/WsStatusCommands.cpp/.h` | (websocket commands) | WS-side status surface |
| `src/config/network_config.h` | 180 | `NetworkConfig` namespace - `WIFI_SSID_VALUE`, `WIFI_SSID_2_VALUE`, `WIFI_SSID_3_VALUE`, `AP_SSID`, `AP_PASSWORD`, scan/reconnect timing constants |
| `src/config/network_config.h.template` | (template) | Used to seed network_config.h |
| `src/config/runtime_state.h` | (declarations) | Runtime config struct (`network_config.h.template` complement) |
| `src/hal/interface/INetworkDriver.h` | (interface) | HAL interface (currently no STA-specific concrete impl beyond `WiFiManager`) |
| `src/network/ApiKeyManager.{h,cpp}` | - | Auth, not WiFi-mode-related |
| `src/network/ApiResponse.h` | - | Response shape |
| `src/network/webserver/V1ApiRoutes.{h,cpp}` | - | Route registration including network REST surface |
| `src/network/WebServer.cpp` | (sections at lines 448, 482, 759, 783, 796, 798, 843, 1117-1120) | Reads `WiFi.softAPIP()` / `WiFi.localIP()`, AP-restart logic on station-disconnect, mode-aware status formatter |

The `find` returns ~70 effect files containing "ap" or "ar" in their names (false positives - e.g. `LGPHarmonographHaloAREffect.cpp`, `LGPSpirographCrownAREffect.cpp`); these are excluded from the inventory above.

Total WiFi/network-relevant source files: **15** (counting `.cpp` + `.h` pairs for WiFiManager/WiFiCredentialsStorage/WiFiCredentialManager/network_config/NetworkHandlers/WebServer/SerialCLI WiFi block + the route files).

Total LOC across the WiFi-specific subset (excluding WebServer.cpp + SerialCLI.cpp which are mostly non-WiFi):
- WiFiManager.h+cpp = 1830 LOC
- WiFiCredentialsStorage.h+cpp = 647 LOC
- WiFiCredentialManager.h+cpp = 761 LOC
- network_config.h = 180 LOC
- Total dedicated to WiFi-mode infrastructure: **3418 LOC** (significantly more than a notional "AP-only-ever" device would require).

---

## Task 2 - current mode initialisation

Boot sequence resolves through `firmware-v3/src/main.cpp` -> `firmware-v3/src/core/SystemInit.cpp` -> `WiFiManager::begin()`.

**Phase 10 init in `core/SystemInit.cpp:315-342`:**

```
315: // Phase 10: WiFi AP
317: void initWiFiAP() {
318: #if FEATURE_WEB_SERVER
319:     // Start WiFiManager - boots into AP-only mode.
320:     // STA is activated ONLY via serial `wifi connect` command.
321:     LW_LOGI("Initializing WiFiManager (AP-only boot)...");
322:     WIFI_MANAGER.setCredentials(NetworkConfig::WIFI_SSID_VALUE,
                                     NetworkConfig::WIFI_PASSWORD_VALUE);
326:     WIFI_MANAGER.enableSoftAP(NetworkConfig::AP_SSID, NetworkConfig::AP_PASSWORD);
328:     if (!WIFI_MANAGER.begin()) { ... }
333:     LW_LOGI("Use serial 'wifi connect SSID PASS' to enable STA mode");
336:     if (!WIFI_CREDENTIALS.begin()) { LW_LOGW(...); }
```

**Mode set in `network/WiFiManager.cpp:71-131` (begin()):**

- Line 74-76: `#ifdef WIFI_AP_ONLY` emits log warning only - no behavioural branch.
- Line 83: `WiFi.mode(WIFI_MODE_AP);` - **HARDCODED literal**, not gated by `WIFI_AP_ONLY`. Even if the flag is removed, this line still forces pure AP at boot.
- Line 90: `WiFi.softAP(m_apSSID, m_apPassword, m_apChannel)` retried 3x with 500ms delay; on persistent failure -> `ESP.restart()` (line 125).
- Line 129: `m_forceApOnly = true;` runtime lock applied.
- Line 130: `setState(STATE_WIFI_AP_MODE);` - state machine pinned to AP.

**State machine pin in `WiFiManager.cpp:250-294` (handleStateInit()):**

- Line 253-257: `#ifdef WIFI_AP_ONLY` -> `setState(STATE_WIFI_AP_MODE); return;` - **compile-time short-circuit, never enters STA**. This is the second layer of defence after the hardcoded `WIFI_MODE_AP` at line 83.
- Line 261-265: `if (m_forceApOnly)` -> AP_MODE - **runtime short-circuit** (also fired if compile-time gate ever lifted).

**Hardcoded vs configurable:** Mode init is **partly compile-time hardcoded, partly runtime-gated**:
- Line 83 (`WiFi.mode(WIFI_MODE_AP)`) - hardcoded literal, no `#ifdef`.
- Lines 74, 253, 562 - `#ifdef WIFI_AP_ONLY` gates additional STA-suppression paths.
- Line 129 (`m_forceApOnly = true` at boot) - runtime gate, can be cleared by `requestSTAEnable()`.
- `enableSoftAP()` / `setCredentials()` / `setStaticIP()` all configurable through public setters.

**No commented-out blocks for STA in init**; the STA path is in separate handler functions (`handleStateScanning`, `handleStateConnecting`, `connectToAP`) which are simply unreachable when `WIFI_AP_ONLY` is defined or `m_forceApOnly` is true.

**Verdict:** Mode init = **hardcoded AP-at-boot with three layers of STA suppression** (compile-time `#ifdef WIFI_AP_ONLY`, runtime `m_forceApOnly`, hardcoded `WIFI_MODE_AP` literal at `WiFiManager.cpp:83`).

---

## Task 3 - STA infrastructure presence audit

### `esp_wifi_set_mode` / `WIFI_MODE_*` literals

| File:line | Literal | Status |
|---|---|---|
| `WiFiManager.cpp:83` | `WiFi.mode(WIFI_MODE_AP)` | ACTIVE - boot |
| `WiFiManager.cpp:176` | `WiFi.mode(WIFI_OFF)` | ACTIVE - in `stop()` |
| `WiFiManager.cpp:692` | `WiFi.mode(WIFI_MODE_APSTA)` | ACTIVE - inside `startSoftAP()` (called from STATE_WIFI_FAILED path - currently unreachable when `WIFI_AP_ONLY` defined) |
| `WiFiManager.cpp:1232-1233` | `if (WiFi.getMode() == WIFI_MODE_AP) WiFi.mode(WIFI_MODE_APSTA);` | ACTIVE in `connectToNetwork()` - reachable via REST `POST /network/connect`, REST `POST /network/sta/enable`, and serial `wifi connect` |
| `WebServer.cpp:843` | mode-check guard | ACTIVE (read-only) |
| `WebServer.cpp:1119-1120` / `SerialCLI.cpp:1484-1486` | mode-name formatter | ACTIVE (read-only) |
| `network/CLAUDE.md:7` | doc | "K1 boots WIFI_MODE_AP" |

No `esp_wifi_set_mode()` call - all switching is via Arduino `WiFi.mode()` wrapper. No `esp_wifi_set_config(WIFI_IF_STA, ...)` direct call.

### `WiFi.begin` (STA association)

`WiFiManager.cpp:669-671` inside `connectToAP()` (called from `handleStateConnecting`):

```
669: WiFi.begin(m_ssid.c_str(), m_password.c_str(), m_bestChannel, bssid);  // with channel hint
671: WiFi.begin(m_ssid.c_str(), m_password.c_str());                          // fallback
```

ACTIVE code path. Reachable only after `m_forceApOnly` is cleared by `requestSTAEnable()` / `connectToNetwork()` AND `WIFI_AP_ONLY` is **not** defined at compile time. With both canonical envs (`esp32dev_audio_esv11_k1v2_32khz`, `esp32dev_audio_esv11_32khz`) defining `WIFI_AP_ONLY=1`, the path is **unreachable in shipped firmware** but the code is fully wired, fully linked.

### "STA" / "station" usage

Most `softAPgetStationNum()` and `STADISCONNECTED`/`STACONNECTED` events refer to **AP clients connecting to K1's AP** (i.e. K1 hosting a station, not K1 acting as a station). Captain should not confuse these with K1-as-station functionality.

Genuine K1-as-station references:
- `WiFiManager.h:73-79` - state enum (`STATE_WIFI_INIT`/`STATE_WIFI_SCANNING`/`STATE_WIFI_CONNECTING`/`STATE_WIFI_CONNECTED`)
- `WiFiManager.h:512-525` - STA connection params (`m_ssid`, `m_password`, `m_ssid2`, `m_password2`, static-IP fields)
- `WiFiManager.h:548-552` - AP config + `volatile bool m_forceApOnly = true` (boot-default)
- `WiFiManager.cpp:609-627` - `isValidStaSsid()` and `hasAnyStaCandidates()` - candidate-set logic
- `WiFiManager.cpp:638-686` - `connectToAP()` - actual `WiFi.begin()` call
- `WiFiManager.cpp:469-...` - `findBestAvailableNetwork()` smart selector iterating all credential sources
- `WiFiManager.cpp:1207-1250` - `requestSTAEnable`, `requestAPOnly`, `connectToNetwork`, `connectToSavedNetwork`

### WiFi credentials / SSID / password identifiers

`config/network_config.h:38-86`:

```
44: constexpr const char* WIFI_SSID_VALUE = WIFI_SSID;          (default "CONFIGURE_ME")
51: constexpr const char* WIFI_PASSWORD_VALUE = WIFI_PASSWORD;  (default "")
61: constexpr const char* WIFI_SSID_2_VALUE = WIFI_SSID_2;
67: constexpr const char* WIFI_PASSWORD_2_VALUE = WIFI_PASSWORD_2;
77: constexpr const char* WIFI_SSID_3_VALUE = WIFI_SSID_3;
83: constexpr const char* WIFI_PASSWORD_3_VALUE = WIFI_PASSWORD_3;
92: constexpr uint8_t WIFI_ATTEMPTS_PER_NETWORK = 2;
```

Tertiary network `WIFI_SSID_3` is **declared but never read** by any source file under `firmware-v3/src/` outside `network_config.h` itself - vestigial / partially wired.

Primary + secondary are read by `WiFiManager::setCredentials()` (called from `SystemInit.cpp:322`).

### Classification table

| Fragment | Classification |
|---|---|
| `WiFiManager` singleton + 7-state FSM | ACTIVE, fully linked, callable; STA branches blocked at runtime by `m_forceApOnly` and at compile by `#ifdef WIFI_AP_ONLY` |
| `WiFi.begin()` calls in `connectToAP()` | ACTIVE-but-unreachable when both gates closed - DEAD CODE in shipping build, LIVE if either gate lifted |
| `WIFI_MODE_APSTA` switch in `startSoftAP()` line 692 and `connectToNetwork()` line 1233 | ACTIVE-but-unreachable in shipping build; **uses concurrent AP+STA mode** which is the documented IDF 4.4 bug surface Captain references |
| `requestSTAEnable()` / `requestAPOnly()` runtime API | ACTIVE - reachable via REST `/api/v1/network/sta/enable`, REST `/api/v1/network/ap-only`, serial `wifi ap` |
| `connectToNetwork()` / `connectToSavedNetwork()` | ACTIVE - reachable via REST `/api/v1/network/connect` (NetworkHandlers.cpp:107-153) and serial `wifi connect SSID PASS` (SerialCLI.cpp:1547, 1556) |
| NVS credential storage (`wifi_creds` namespace) | ACTIVE - initialised at boot by `WIFI_CREDENTIALS.begin()` (SystemInit.cpp:336) |
| `WIFI_SSID_3` tertiary network slot | VESTIGIAL - declared in network_config.h, never consumed |
| `WIFI_AP_ONLY` `#ifdef` gates | ACTIVE - 3 sites (`WiFiManager.cpp:74, 253, 562`) |
| `wifi_credentials.ini` build-flag injection | ACTIVE infrastructure (file present at 1.9K), reads `WIFI_SSID`/`WIFI_PASSWORD` -> baked into `WIFI_SSID_VALUE` constants -> consumed by `setCredentials()` -> **but consumption gated by `WIFI_AP_ONLY`** |

**Verdict for Task 3:** STA infrastructure is **PRESENT-DISABLED**, not VESTIGIAL or DEAD. Roughly **3000+ LOC of fully-functional STA code** (state machine, smart-selector, NVS, REST surface, serial surface, credential templates) is compiled and linked but blocked from executing by the `WIFI_AP_ONLY` define and the `m_forceApOnly` runtime flag.

---

## Task 4 - feature flags / build envs

Build envs in `firmware-v3/platformio.ini` (top-level):

| Env name | Lines | Inherits | WiFi-related flags |
|---|---|---|---|
| `[wifi_sta]` | 30-31 | (stub) | empty `build_flags =` - placeholder for `wifi_credentials.ini` to extend; doc at lines 23-29 explicitly says "K1 is AP-ONLY in production; the _sta env is dev-only and must not be used on K1 hardware" |
| `[common]` | 33-108 | - | `-D FEATURE_WEB_SERVER=1` (line 103); no `-D WIFI_*` |
| `esp32dev_audio_base` | 112-144 | - | `${wifi_credentials.build_flags}` (line 133) - imports SSID/PW from gitignored ini |
| `esp32dev_audio_esv11` | 146-169 | base | nothing wifi-specific |
| `esp32dev_audio_esv11_k1v2` | 176-205 | esv11 | **`-D WIFI_AP_ONLY=1`** (line 203), comments at lines 201-202 cite STA AUTH_EXPIRE/AUTH_FAIL |
| **`esp32dev_audio_esv11_k1v2_32khz`** (canonical K1) | 219-224 | k1v2 | inherits `WIFI_AP_ONLY=1` from k1v2 parent |
| **`esp32dev_audio_esv11_32khz`** (canonical V1) | 230-238 | esv11 | **`-D WIFI_AP_ONLY=1`** (line 237) |
| `esp32dev_audio_pipelinecore` | 243-252 | base | nothing wifi-specific (no WIFI_AP_ONLY) |
| `esp32dev_audio_pipelinecore_sta` | 269-273 | pipelinecore | `${wifi_sta.build_flags}` - **dev-only STA env**, comment at lines 264-268: "K1 still runs its SoftAP (AP+STA concurrent), but also joins your LAN" |
| `esp32dev_audio_pipelinecore_perf` | 280-286 | pipelinecore | nothing wifi-specific |
| `esp32dev_audio_spine16k` | 291-... | base | nothing wifi-specific |
| `esp32dev_FH4R2` | 400-414 | base | `-D WIFI_AP_ONLY=1` (line 414) |
| `esp32dev_FH4R2_pipelinecore` | 420-... | base | (need to verify - not in window) |

**Platform / framework version:**
- `platform = espressif32@6.9.0` (multiple envs, lines 113, 256, 1072) - this resolves to **Arduino-ESP32 2.0.x family / ESP-IDF 4.4.x**.
- Confirmed in `extra_configs = wifi_credentials.ini` (line 21).

**Build flags that could enable STA at compile time:**
- Defining `WIFI_SSID` / `WIFI_PASSWORD` (via `wifi_credentials.ini`) - sets `WIFI_SSID_VALUE` constant, but is **inert** while `WIFI_AP_ONLY` is defined.
- Removing `-D WIFI_AP_ONLY=1` from a K1 env - lifts compile-time gate but `WiFiManager.cpp:83` still hardcodes `WIFI_MODE_AP` at boot AND `m_forceApOnly = true` is set at line 129.
- Setting `WIFI_AP_ONLY=0` does **not** disable the gates (`#ifdef` checks define-presence, not value). Must `#undef` or remove the line.

`wifi_credentials.ini` exists (1.9K, gitignored) and `wifi_credentials.ini.template` (1.1K) exists. Template defines:
- Required: `-D WIFI_SSID=\"YourNetworkName\"`, `-D WIFI_PASSWORD=\"YourPassword\"`
- Optional: `-D WIFI_SSID_2`, `-D WIFI_PASSWORD_2`, `-D AP_SSID_CUSTOM`, `-D AP_PASSWORD_CUSTOM`

---

## Task 5 - credential storage

**Two parallel credential systems** exist:

### Compile-time (build-flag) credentials
- `WIFI_SSID`, `WIFI_PASSWORD`, `WIFI_SSID_2`, `WIFI_PASSWORD_2`, `WIFI_SSID_3`, `WIFI_PASSWORD_3` injected via `wifi_credentials.ini` -> `network_config.h:44-86`
- Loaded into `WiFiManager` via `setCredentials()` at `SystemInit.cpp:322-325`
- Default value when no flag: `"CONFIGURE_ME"` for SSID, `""` for password (`network_config.h:47, 53`)
- `isValidStaSsid()` (`WiFiManager.cpp:609-615`) explicitly rejects `"CONFIGURE_ME"`, empty, `"PORTABLE_TEST_NONE..."`, and `AP_SSID` self-match - so a default build never accidentally STA-connects.

### NVS runtime credentials
- `WiFiCredentialsStorage` class (`network/WiFiCredentialsStorage.{h,cpp}`)
- Namespace `"wifi_creds"`, MAX_NETWORKS=10, JSON-encoded `net_0..net_9` plus a `count` metadata key plus a last-connected SSID record.
- Init at `SystemInit.cpp:336` via `WIFI_CREDENTIALS.begin()`.
- Headers carry the AP-ONLY warning banner (`WiFiCredentialsStorage.h:5-9`).
- Save path: `WiFiManager::saveNetwork()` -> `WiFiCredentialsStorage::saveNetwork()`.
- Load path: `WiFiManager::getSavedNetworks()`.
- Delete path: `WiFiManager::removeNetwork()`/`deleteSavedNetwork()`.

### REST/Serial entry points (provisioning UI surface)

REST (`network/webserver/handlers/NetworkHandlers.cpp`):
- `POST /api/v1/network/connect` - `handleConnect` lines 107-153 (accepts `ssid`, `password`, `save:bool`) -> `WIFI_MANAGER.saveNetwork()` + `setCredentials()` + `reconnect()`
- `GET /api/v1/network/saved` - `handleSavedList` lines 167-184
- `POST /api/v1/network/saved` - `handleSavedAdd` lines 186-221 (saves without connecting)
- `DELETE /api/v1/network/saved/{ssid}` - `handleSavedDelete` lines 223-275
- `POST /api/v1/network/sta/enable` - `handleEnableSTA` lines 335-370 (with optional `durationSeconds`, `revertToApOnly`)
- `POST /api/v1/network/ap-only` - `handleEnableAPOnly` lines 372-388
- `POST /api/v1/network/disconnect` - `handleDisconnect` line 155
- `GET /api/v1/network/scan` - `handleScan` line 90

Serial (`serial/SerialCLI.cpp:1471-1583`):
- `wifi` / `wifi status` - mode + IP + RSSI + saved-networks listing + compile-time SSIDs
- `wifi connect SSID PASSWORD` -> `connectToNetwork()` (line 1547)
- `wifi connect SSID` -> `connectToSavedNetwork()` (line 1556)
- `wifi connect` -> use last-connected or first saved (lines 1530-1538)
- `wifi ap` -> `requestAPOnly()` (line 1566)
- `wifi scan` -> `scanNetworks()` (line 1571)

**No BLE provisioning, no captive-portal-style provisioning UI (see Task 6).**

**Finding:** Credential infrastructure is **dual-pathed and fully wired**: compile-time defaults via build-flags + runtime NVS storage with REST + serial surfaces. NOT vestigial. Someone built a complete provisioning UX, then disabled the consumer paths via `WIFI_AP_ONLY`. The serial cmd-help banner (`SerialCLI.cpp:1467-1469`) flags STA as "an escape hatch for development/debugging" that is "KNOWN UNRELIABLE".

---

## Task 6 - captive portal vs full-router-mode

### Captive portal infrastructure search

`grep -rn "captive\|CaptivePortal\|dnsserver\|DNSServer" firmware-v3/src` returned **zero matches**.

There is no DNS-server, no captive-portal redirect handler, no `192.168.4.1/generate_204`-style intercept, and no `/connecttest.txt` interception. The `WebServer` registers REST routes and the WS gateway only - there is no portal page-flow.

### mDNS

`network_config.h:127`: `MDNS_HOSTNAME = "lightwaveos"` - so K1 advertises `lightwaveos.local` over mDNS. Logged at `SystemInit.cpp:376-377`: `REST API: http://lightwaveos.local/api/v1/`, `WebSocket: ws://lightwaveos.local/ws`.

mDNS resolution requires the client to be on the same L2 network. In current AP-only mode, the AP IS the L2 - clients (Tab5, iOS) join the K1 AP and resolve `lightwaveos.local` to `192.168.4.1`.

### AP behaviour

- Default password `""` (open network) - `network_config.h:111`. Open network is intentional for portable / festival deployment.
- 4-client cap (`WiFiManager.cpp:698`) - `WiFi.softAP(..., false, 4)`.
- AP serves the full REST + WS surface (`http://192.168.4.1/api/v1/...`, `ws://192.168.4.1/ws`).
- LittleFS partition (board_build.filesystem = littlefs, `platformio.ini:126`) hosts dashboard static assets (per `StaticAssetRoutes.cpp/.h`).

### Distinction matrix

| Mode | Currently supported end-to-end? | Evidence |
|---|---|---|
| AP-only-as-captive-portal (no upstream, K1 IS the network) | **YES** - this is the shipped behaviour. Dashboard served from LittleFS, REST+WS over the AP. | `WiFiManager.cpp:83-131`, `SystemInit.cpp:317-342`, `StaticAssetRoutes.{h,cpp}` |
| AP-only-with-router-bridge (K1 hosts AP, NATs to upstream) | **NO** - no NAT, no bridge code, no upstream interface configured | n/a |
| STA-only (K1 joins existing router, AP off) | **PARTIALLY** - all infrastructure exists, but every code path that reaches `WiFi.begin()` first switches to `WIFI_MODE_APSTA` (AP+STA concurrent), NOT pure STA. There is no `WiFi.mode(WIFI_MODE_STA)` call anywhere in the codebase. | `WiFiManager.cpp:692, 1233` |
| Dual-mode (mode-switchable AP-or-STA, never both) | **NO** - the current STA path activates AP+STA concurrent; the runtime `m_forceApOnly` flag toggles AP-only on/off but the "off" state is AP+STA, not STA-only |

**Critical finding:** Captain's stated requirement is "AP OR STA only, never together" but the **current STA-path implementation uses `WIFI_MODE_APSTA` concurrent mode**, not pure STA. This is the Portable Mode architecture from the Feb 2026 work (memory IDs #25420, #25419, #25418, #25526), which is exactly the AP+STA concurrent configuration Captain wants to avoid (and the documented IDF 4.4 PMK-cache / AUTH_EXPIRE bug surface).

The captive-portal absence is also relevant: if STA mode were enabled, end-users have no UI to enter credentials before connecting (no captive portal, no BLE provisioner). They must POST to `/api/v1/network/connect` over the AP first - which presupposes AP is up, which means a clean cutover from AP to STA cannot be performed in one step.

---

## Task 7 - known-bug context (ESP-IDF AP+STA)

**ESP-IDF / Arduino framework version pin:**
- `platform = espressif32@6.9.0` (`platformio.ini:113, 256, 1072`)
- This resolves to **Arduino-ESP32 2.0.14 / ESP-IDF 4.4.5-7** (the 6.x line of the PIO espressif32 platform packages tracks Arduino-ESP32 2.0.x).
- Project memory confirms IDF 4.4.7 (per `MEMORY.md` reference to `project_idf_version_confirmed.md`: "IDF version confirmed: 4.4.7. RMT4 active, RMT5/I2S dead on IDF 4.x. IDF 5.x blocked by I2C bug + API rewrites.").
- No upgrade in progress on this branch (`feature/heap-stability-day1`).

**What's blocking IDF upgrade per project memory:**
- `MEMORY.md` line: "IDF 5.x blocked by I2C bug + API rewrites" - the move to IDF 5 requires an I2C-driver migration and other API rewrites that haven't landed.
- I did not investigate ESP-IDF release notes (per scope).

**Per-codebase mitigations attempted for AP+STA bugs (visible in source):**
- `WiFiManager.cpp:677-682` - `WiFi.setSleep(false)` + `esp_wifi_set_ps(WIFI_PS_NONE)` (modem-sleep disable to prevent ASSOC_LEAVE).
- `WiFiManager.cpp:540-551` - OtaSessionLock guard to suppress STA retry during OTA (would tear down AP).
- `WiFiManager.cpp:577-585` - "non-destructive STA retry" pattern preserves AP across STA reconnect attempts.
- `WiFiManager.cpp:506-507` - `CONNECTED_DISCONNECT_GRACE_MS=500` to ignore disconnect flaps post-connect.
- `WebServer.cpp:759-798` - immediate AP client disconnect detection + AP re-init to flush WPA2 PMK cache (per memory IDs #25550, #25542 - root cause of Tab5 4WAY_HANDSHAKE_TIMEOUT).

These are workaround layers; per `network/CLAUDE.md:11`: "STA authentication FAILS at the ESP-IDF 802.11 driver level (AUTH_EXPIRE reason 2, AUTH_FAIL reason 202). This has been reproduced across multiple routers and 6+ firmware mitigation attempts - NONE worked."

---

## Task 8 - gap analysis vs dual-mode goal

Goal (Captain): K1 capable of operating in **either AP or STA, never simultaneously**, with mode-switch support for "in the wild" scenarios with no upstream WiFi.

### What's already there

- Full state-machine FSM (7 states, event-driven on FreeRTOS)
- NVS credential storage (10 networks max) with last-connected tracking
- Smart-selector that ranks candidate networks by RSSI across config-primary, config-secondary, NVS-saved sources
- REST surface: `/network/connect`, `/network/saved` (CRUD), `/network/scan`, `/network/sta/enable`, `/network/ap-only`, `/network/disconnect`
- Serial surface: `wifi connect`, `wifi ap`, `wifi scan`, `wifi status`
- Runtime mode-toggle hook (`requestSTAEnable`, `requestAPOnly`, `m_forceApOnly`)
- Static-IP support (`WiFiManager.h:155-157`, used at `WiFiManager.cpp:649`)
- mDNS hostname registration ready
- Multi-network credential pool (primary + secondary + NVS) with auto-fallback
- Auto-reconnect with exponential backoff (`network_config.h:172-173`: 5s -> 60s)
- AP-restart-on-disconnect to flush PMK cache (`WebServer.cpp:783-798`)
- Watchdog-fed STA scan/connect (`WiFiManager.cpp:195-209`)

### What's missing for clean dual-mode

- **No pure STA mode path.** Every STA-enable code path switches to `WIFI_MODE_APSTA` concurrent mode (`WiFiManager.cpp:692, 1233`), not `WIFI_MODE_STA`. No call site sets pure STA. Dual-mode "STA only, AP off" is **not implemented**.
- **No mode-persistence in NVS.** No NVS read of "boot in AP" vs "boot in STA" preference; boot is **hardcoded to AP** (`WiFiManager.cpp:83`, `SystemInit.cpp:321`). `m_forceApOnly = true` is set unconditionally at every boot (`WiFiManager.cpp:129`).
- **No first-boot provisioning flow.** No captive portal, no DNS interceptor, no BLE provisioner. End-users with a brand-new K1 in a network-less environment cannot enter STA credentials without first connecting to the AP and POSTing to `/network/connect` (which itself is fine, but assumes a phone+app or web client is present).
- **No fallback ladder.** No "try STA for N seconds; if fail, fall back to AP" timer logic. The closest analogue is the smart-selector + scan-fail counter (`WiFiManager.cpp:577`: `m_scanAttemptsWithoutKnown < 4`) but its outcome is to go to AP+STA, not pure AP-only.
- **`WIFI_AP_ONLY` define in canonical envs** (`platformio.ini:203, 237`) blocks STA at compile time. Removing it does not yield clean dual-mode because `WiFiManager.cpp:83` still hardcodes `WIFI_MODE_AP` at boot.
- **Hardcoded `m_forceApOnly = true`** at `WiFiManager.h:549` (default) and `WiFiManager.cpp:129` (boot). Must become NVS-backed runtime preference.
- **`startSoftAP()` switches to AP+STA** (line 692) - if dual-mode "AP only never STA simultaneously" is the rule, this must become `WIFI_MODE_AP` only.
- **`connectToNetwork()` switches to AP+STA** (line 1233) - same issue. Must tear down AP before entering STA, not concurrent.
- **No mode-switch UI.** Dashboard (per landing-page memory) has no "join your network" workflow; iOS app has no provisioning sheet (out of scope but worth noting for Captain).
- **Tertiary credential slot vestigial.** `WIFI_SSID_3_VALUE` declared but never consumed.

### What's broken for dual-mode (would need fixing)

- AP+STA concurrent invocation pattern (lines 692, 1233) is the documented bug surface Captain identifies.
- Hardcoded literal `WIFI_MODE_AP` at line 83 (no `#ifdef` gate).
- `WiFiCredentialsStorage.h:5-9` and `WiFiManager.h` header banners claim STA is "KNOWN UNRELIABLE" - if dual-mode becomes supported, these warnings are misleading and would need rewriting.
- `network/CLAUDE.md` (whole file) states "STA mode is HARD architectural constraint - DO NOT add STA logic" - directly contradicts the dual-mode goal.
- Three stacked guards (`WIFI_AP_ONLY` define, `m_forceApOnly` runtime flag, `WIFI_MODE_AP` literal) all pulling toward AP-only - must be unified to a single mode-state preference.

---

## Evidence quality verdicts

| Task | Verdict | Reasoning |
|---|---|---|
| 1 - File inventory | **HIGH** - direct find + size verification, all files line-counted |
| 2 - Mode init path | **HIGH** - traced from `main.cpp` through `SystemInit.cpp:317-342` to `WiFiManager::begin()` lines 44-155, all literals cited file:line |
| 3 - STA infrastructure presence | **HIGH** - 100% of `WIFI_MODE_*`, `WiFi.begin`, `connectTo*` call sites enumerated and classified |
| 4 - Build envs | **HIGH** - read top of `platformio.ini` directly, all canonical envs verified, `WIFI_AP_ONLY` define-sites cited |
| 5 - Credential storage | **HIGH** - `WiFiCredentialsStorage.{h,cpp}`, `WiFiManager.h:367-401`, REST handlers cited, NVS namespace verified |
| 6 - Captive portal vs router bridge | **HIGH** - exhaustive grep for `captive`/`DNSServer`/`dnsserver` returned zero; absence confirmed |
| 7 - IDF version | **MEDIUM** - inferred from `platform = espressif32@6.9.0` plus project memory pointer (file `project_idf_version_confirmed.md` cited but not opened); ESP-IDF release-notes scope explicitly excluded |
| 8 - Gap analysis | **HIGH** - all gaps directly mapped to specific file:line absences or hardcoded literals |

---

## Bottom line

**Current K1 firmware is "STA-infrastructure PRESENT and FULLY WIRED, but compile-time-disabled in canonical envs and hardcoded-to-AP at boot, with the only enabled STA path running in AP+STA concurrent mode (the bug surface Captain wants to avoid)."**

The narrative "AP-only-ever" is real in shipping behaviour but **fictional in source code** - 3000+ LOC of STA functionality is sitting one `#undef WIFI_AP_ONLY` and a few line-edits away from being live. However, simply disabling the gate does not deliver Captain's goal of "AP **OR** STA, never together" - it delivers AP+STA concurrent, which is exactly the failure mode Captain rejects.

To reach clean dual-mode requires: (1) replace `WIFI_MODE_APSTA` literals with mode-conditional `WIFI_MODE_AP` or `WIFI_MODE_STA`, (2) NVS-back the boot-mode preference, (3) remove the `WIFI_AP_ONLY` define and the unconditional `m_forceApOnly = true`, (4) add a first-boot provisioning surface, (5) update three CLAUDE.md docs and two header banners that currently forbid STA work.

---

## What I did NOT verify (scope limits)

- Git history / when each `#ifdef WIFI_AP_ONLY` was introduced, who added the `m_forceApOnly` runtime flag, the AP+STA -> AP-only doctrinal flip (SSA-A scope).
- claude-mem observations beyond what was inlined into the system-reminder banners (SSA-C scope).
- Doctrine drift in `CLAUDE.md` files, `BACKLOG.md`, governance docs (SSA-D scope).
- ESP-IDF release notes, IDF 5.x AP+STA fix status, upstream esp-idf bug reports - explicitly out of scope per Task 7 instructions.
- iOS / Tab5 STA infrastructure (`lightwave-ios-v2/`, `tab5-encoder/`) - scope limited to `firmware-v3/`.
- Dashboard / k1-composer / harness WiFi assumptions - scope limited to `firmware-v3/`.
- Whether `wifi_credentials.ini` actually contains real credentials (file exists at 1.9K but I did not read its contents - secrets discipline).
- WiFiCredentialManager.cpp implementation details (only header + filename surveyed; 456-LOC body not opened).
- Whether `findBestAvailableNetwork()` and the smart-selector actually behave as documented at runtime (read-only static survey, no behavioural verification).
- Network unit tests - I did not search `firmware-v3/test/` or `firmware-v3/tests/` for STA-related test coverage.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:SSA-Forensic-B | Created. Forensic survey of current `firmware-v3/` WiFi source state for K1 dual-mode round-table. |
