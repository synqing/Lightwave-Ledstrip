---
abstract: "Forensic audit handover from 2026-04-18 session. 22 SSAs dispatched in parallel across hazard classes A–Z+DD to sweep tab5-encoder (ESP32-P4 + LVGL 9.3 + WS client to K1). Found ~30 distinct P0, ~60 P1, ~80 P2 issues. Most severe: AsyncTCP priority inversion preempts loopTask during OTA (same bug as firmware-v3, never ported), ConnectivityTab loop() never pumps in LVGL build (deferred load and scan status-polling both dead), blocking HTTP inside LVGL event callbacks (5s UI freezes per tap), OTA has no rollback/SHA256/session-timeout (brick + security + wedge), discovery task unsubscribed to WDT during 75s subnet sweep, phantom handleActionButton from legacy touch ghost zone, WiFi STA retries single SSID forever with no give-up, USB-CDC serial floods loopTask at 7 KB/s on encoder use, \"showNetworkConfigScreen\" creates an unreachable stuck modal (no save/cancel event cb). Also found significant areas of strong design — NVS PSRAM-primary architecture is correctly hardened, no ISRs exist so Guru-class bugs are architecturally impossible, FSM transitions are deadlock-free. Details every finding with file:line, proposed fix, and SSA provenance for resumption via SendMessage."
type: firmware-audit-handover
version: 1.0
project: tab5-encoder
build: tab5 (pioarduino espressif32@54.03.21, Arduino framework, ESP32-P4)
hardware: M5Stack Tab5 — ESP32-P4 (RISC-V dual-core) + ESP32-C6 (WiFi radio)
session_date: 2026-04-18
sister_audit: firmware-v3/docs/forensic-audit-2026-04-17.md
---

# Forensic Audit Handover — tab5-encoder (M5Stack Tab5)

**Audience:** The next agent or engineer picking up this work.
**Purpose:** Full context transfer. You should be able to continue without re-running any of the investigation in this document.

---

## Part 1 — Session context

### 1.1 What triggered this audit

Sister audit `firmware-v3/docs/forensic-audit-2026-04-17.md` swept the K1 LED controller firmware with 22 SSAs and produced a Track A/B/C remediation tree. Captain requested the same depth of sweep against tab5-encoder — the M5Stack Tab5 companion controller that pairs with K1. Today's session dispatched 22 SSAs in parallel and consolidated their returns into this document.

### 1.2 What tab5-encoder is

| Dimension | Value |
|---|---|
| Hardware | M5Stack Tab5 (ESP32-P4 RISC-V dual-core HP + ESP32-C6 WiFi co-processor over SDIO, 32 MB PSRAM, 16 MB flash, 1280×720 MIPI DSI panel with GT911 touch, battery + USB charging) |
| Framework | Arduino + ESP-IDF (pioarduino `espressif32@54.03.21`) |
| UI | LVGL 9.3 (software render, no HELIUM/NEON/DMA2D), 10 FPS target |
| Network role | WiFi STA (connects to K1's AP at 192.168.4.1) — opposite of K1's AP-only role |
| Comms to K1 | WebSocket CLIENT (`links2004/WebSockets@^2.4.0`) + HTTP REST client for connectivity/discovery |
| Input | Dual M5ROTATE8 (16 encoders) over I2C + GT911 touch + M5 buttons |
| Persistence | NVS (64 KB partition, recently enlarged from 20 KB) + PSRAM-primary preset cache |
| OTA | ESPAsyncWebServer on tab5 itself serves `/api/v1/firmware/update` + `/update` (legacy) |
| Source size | 142 files / 119,517 LOC — of which **89%** (~94,000 LOC) is LVGL font bitmap `.c` files; real code is ~25K LOC across ~100 files |
| FSMs | 9 documented (ClickDetector, WiFiConnectionStatus, WebSocketStatus, LED Feedback, SpeedPaletteMode, Zone Mode, PresetSlotState, ConnectivityState, DiscoveryState) |

### 1.3 Prior audits on tab5-encoder (respected — this audit surfaces NEW findings only)

- `tab5-encoder/docs/TAB5_MEMORY_AUDIT_2026-04-03.md` — memory audit. Closed three leaks (`g_otaServer`, `_sendMutex`, `lgfx::PPASrm`). Today's class-H audit re-verified and found NEW leaks not covered there (see P0-H01, P1-H02, P1-H03).
- `tab5-encoder/docs/UI_AUDIT_REPORT.md` — UI audit. Today's class V, W, X findings are additive, not overlapping.

### 1.4 CLAUDE.md layers (relevant constraints)

- Root `CLAUDE.md` + `tab5-encoder/CLAUDE.md` + `src/CLAUDE.md` + `src/ui/CLAUDE.md` + `src/input/CLAUDE.md` — all claude-mem-auto-populated, no hard-constraint declarations of their own. Inherited rules: British English in comments/docs/logs, K1 AP-ONLY (tab5 is STA — do not invert), LVGL widget tree MUST follow `docs/reference/lvgl-component-reference.md` (12 anti-patterns), no heap alloc in LVGL render tick.

---

## Part 2 — Proactive audit methodology

22 SSAs were dispatched in parallel, each with tight scope (< 30K tokens), clangd-first tool routing, QMD for docs, Context7 for library APIs, and a strict return contract (file:line citations only — no file dumps). Read-only audit — no code modifications.

### 2.1 SSA catalogue

Each SSA is resumable via `Agent` (matching `subagent_type`) + `SendMessage({to: '<agentId>', prompt: '…'})`.

| # | Short-name | Agent type | Agent ID (resume) | Scope |
|---|---|---|---|---|
| A | Blocking primitives | embedded-system-engineer | ab1ee8ade1015f7a2 | Unbounded blocking on hot paths |
| B | Latching state flags | deep-technical-analyst | a1f26cf050c2e7853 | Flags that can set but never clear |
| C | ISR / IRAM safety | Embedded Firmware Engineer | a172f5b4ce85ba01b | Functions reachable from ISR touching flash |
| D | Priority inversion | deep-technical-analyst | a302028ebd518b970 | Task scheduling + starvation |
| E | Watchdog coverage | Embedded Firmware Engineer | a3d54580a1122830e | TWDT/IWDT subscriptions + feeders |
| F | Queue saturation | embedded-system-engineer | a46757eab3eb011bb | Silent-drop paths from ignored return codes |
| G | Integer overflow / wrap | c-pro | a40535bd2d035863f | uint8/uint16 counters + millis arithmetic |
| H | Resource leaks | cpp-pro | af164a60bc75d1e8b | new/delete, malloc/free, FreeRTOS handles, LVGL objects |
| I | Dead code / unused paths | deep-technical-analyst | a788d9abaad9a9de4 | Unreferenced symbols, dead `#ifdef` branches |
| J | Cross-core data races | embedded-system-engineer | a0b0271585d0b4218 | Volatile misuse across cores |
| L | NVS + persistence | Embedded Firmware Engineer | a6d4217c5049d852f | NVS, preset, wifi-cred, OTA token |
| M | WiFi STA state machine | network-api-engineer | a5a8b885337eb026e | STA FSM, reconnect, antenna switching |
| Q | Stack + memory budget | cpp-pro | a680418e74a981d20 | Per-task stack headroom |
| R | Log / serial bandwidth | c-pro | ab1a8db7e002c9876 | Cumulative serial load under workload |
| S | Reentrancy + callback-under-lock | cpp-pro | a82f9215be139f644 | Callback invoked while mutex held |
| T | Config drift / sdkconfig / lv_conf | Embedded Firmware Engineer | a2bb3c069f3507825 | Build flags, partition table, LVGL config |
| V | LVGL widget tree hazards | agent-lvgl-uiux | a4ede5402de6b1671 | 12 anti-patterns + widget lifecycle |
| W | Touch input pipeline | agent-lvgl-uiux | a2acb4b0c88796a85 | Touch → LVGL indev dispatch, gestures |
| X | Screen FSM / navigation | deep-technical-analyst | af4e8dfdc66883687 | DisplayUI screen switching, modal overlays, back-stack |
| Y | K1 protocol client | network-api-engineer | a721bb2df9356dc11 | WS + REST contract compliance to K1 |
| Z | Boot + OTA + power | Embedded Firmware Engineer | a06fd5f161e5ebdac | setup() order, OTA, partition rollback, M5.Power, display lifecycle |
| DD | Discovery + DeviceRegistry | embedded-system-engineer | afbf7bebb6b7fa147 | mDNS task, multi-device support |

To resume any: use `Agent` with matching `subagent_type`, then `SendMessage` to continue.

### 2.2 Build / flash commands

```bash
cd tab5-encoder
pio run -e tab5                         # production build
pio run -e tab5_debug                   # debug build (-Og, -g3, -ggdb)
pio run -e tab5 -t upload --upload-port /dev/cu.usbmodem1101
pio device monitor -b 115200
```

### 2.3 Reference files (cite this audit from source)

| Topic | File |
|---|---|
| Codebase map | `tab5-encoder/docs/reference/codebase-map.md` |
| FSM reference | `tab5-encoder/docs/reference/fsm-reference.md` |
| LVGL anti-patterns (MANDATORY for UI work) | `tab5-encoder/docs/reference/lvgl-component-reference.md` |
| K1 WS contract | `docs/protocol/k1-ws-contract.yaml` |
| K1 REST contract | `docs/protocol/k1-rest-contract.yaml` |

---

## Part 3 — Findings (P0)

These are correctness, safety, or security hazards. Ship-blocking. **Fix first.** Severity ranked within priority bucket.

### P0-01 — AsyncTCP priority 10 preempts loopTask during OTA (mirrors firmware-v3 P1-01, never ported)

- **SSA:** D (priority inversion)
- **File:line:** `tab5-encoder/platformio.ini` — no `CONFIG_ASYNC_TCP_*` overrides. AsyncTCP defaults: prio 10, no core affinity.
- **Severity:** During OTA, `async_tcp` task at prio 10 preempts the prio-1 loopTask (which feeds the only subscribed task watchdog). Flash erase/write inside `Update.write()` blocks the async task for 10+ seconds. loopTask stops feeding WDT → 5s panic → OTA aborts mid-write → device boots the half-written slot (see P0-02 for why this bricks).
- **Details:** firmware-v3 already fixed this with `-D CONFIG_ASYNC_TCP_PRIORITY=5 -D CONFIG_ASYNC_TCP_RUNNING_CORE=0 -D CONFIG_ASYNC_TCP_STACK_SIZE=4096` in its common build_flags. Tab5's `platformio.ini` has no equivalent. AsyncTCP README (`.pio/libdeps/tab5/AsyncTCP/README.md:46-48`) documents the recommended override.
- **Fix:** Add three lines to `[common]` build_flags in `tab5-encoder/platformio.ini` matching firmware-v3.
- **Risk if not fixed:** WDT panic during every large OTA + degraded WiFi latency under concurrent WS + HTTP traffic.

### P0-02 — OTA has no partition rollback validation (brick risk)

- **SSA:** Z (boot/OTA)
- **File:line:** `tab5-encoder/src/network/OtaHandler.cpp` (zero calls to `esp_ota_mark_app_valid_cancel_rollback`, `esp_ota_get_state_partition`, or rollback API anywhere in tree)
- **Severity:** Dual-OTA partition layout (6.25 MB × 2, see `partitions_tab5.csv`) implies A/B rollback, but no code marks slots valid/invalid. A bad image that boots long enough to clear `setup()` but crashes later cannot be rolled back by the bootloader. Device is permanently wedged on the bad image until USB re-flash.
- **Fix:** At end of `setup()` after all critical init succeeds, call `esp_ota_mark_app_valid_cancel_rollback()`. ~15 LOC.
- **Risk if not fixed:** Field brick on any bad OTA.

### P0-03 — OTA has no SHA256/MD5 verification

- **SSA:** Z (boot/OTA)
- **File:line:** `tab5-encoder/src/network/OtaHandler.cpp:173` — `Update.begin(s_updateTotal, U_FLASH)` called with no preceding `Update.setMD5()`
- **Severity:** Corrupted OTA (bit flip, truncated TCP, malicious replay) reaches `Update.end(true)` which sets the new boot partition based on write-count parity only. Combined with P0-02, a corrupted image boots with zero integrity check.
- **Fix:** Require `X-Checksum: <sha256>` header in `/api/v1/firmware/update`; call `Update.setMD5(hash)` before `Update.begin()`.
- **Risk if not fixed:** Silent corruption → brick. Security bypass: K1 AP is open, so the OTA auth token (plaintext `#define OTA_UPDATE_TOKEN "LW-OTA-2024-SecureUpdate"` in `network_config.h:136`) is recoverable from any firmware dump.

### P0-04 — OTA session wedges permanently on client disconnect

- **SSA:** Z (boot/OTA), B (latching-flag) — cross-confirmed
- **File:line:** `tab5-encoder/src/network/OtaHandler.cpp:156` (`s_updateStarted = true` set) — no clear path on client disconnect
- **Severity:** If WS OTA client drops TCP mid-upload (half-open socket, no FIN received), `s_updateStarted` stays true forever. `Update` library holds the app partition open. Next upload attempt fails (`Update.begin()` returns false on active session). Cold reboot required.
- **Fix:** Add `static uint32_t s_updateStartedAtMs` set in `handleUpload(index==0)`; in `loop()`, if `s_updateStarted && (millis() - s_updateStartedAtMs) > 30000 && !final` call `Update.abort()` and reset state. Mirrors firmware-v3 P0-02 fix.
- **Risk if not fixed:** OTA lost on first disconnect until device reboots.

### P0-05 — `ConnectivityTab::loop()` is never invoked in the LVGL build (deferred load dead)

- **SSA:** X (screen FSM) — HIGHEST-VALUE single finding
- **File:line:** `tab5-encoder/src/ui/DisplayUI.cpp:1375-1468` (LVGL loop path) — iterates feedback timers and calls ONLY `_controlSurface->loop()` at line 1466. Does NOT call `_connectivityTab->loop()` or `_zoneComposer->loop()`. Those calls exist at lines 2268, 2277, 2345, 2358 but are inside the `#else` (M5GFX, non-LVGL) branch starting at line 1977 — dead on hardware.
- **Severity:** ConnectivityTab requires `loop()` for (a) `_needsInitialLoad` deferred loading — so **saved networks never load**, (b) `checkScanStatus()` polling — so **WiFi SCAN results never surface**, (c) `updateConnectionStatus()` every 2s — status label frozen at boot value, (d) discovery state polling — saved networks dead. The entire `DEFERRED LOADING` architecture put in place specifically to survive the WDT panic (claude-mem #23486) never fires.
- **Fix:** At `DisplayUI.cpp:1466` add:
  ```cpp
  if (_currentScreen == UIScreen::CONNECTIVITY && _connectivityTab) {
      _connectivityTab->loop();
  }
  if (_currentScreen == UIScreen::ZONE_COMPOSER && _zoneComposer) {
      _zoneComposer->loop();
  }
  ```
- **Risk if not fixed:** ConnectivityTab is half-broken in production — user opens it, sees empty lists, can tap SCAN (event-driven works), but results never appear.

### P0-06 — Blocking HTTP inside LVGL event callbacks (UI freeze up to HTTP_TIMEOUT_MS per tap)

- **SSA:** A (blocking), S (reentrancy) — cross-confirmed
- **Sites:**
  - `tab5-encoder/src/ui/ConnectivityTab.cpp:1187-1197` — `connectButtonCb` → `connectToSelectedNetwork()` → `HttpClient::post()` — blocking polling loop up to `HTTP_TIMEOUT_MS = 5000`.
  - `:1200-1229` — `addNewNetwork()` → `HttpClient::post()`
  - `:1232-1259` — `deleteSelectedNetwork()` → `HttpClient::del()`
  - `:1262-1277` — `disconnectFromNetwork()` → `HttpClient::post()`
- **Severity:** The LV_EVENT_CLICKED callback runs inside `lv_timer_handler()` on loopTask. During the blocking HTTP wait, LVGL's render loop is frozen — touch unresponsive, animations paused, WS heartbeat missed. `esp_task_wdt_reset()` inside the HTTP loop prevents watchdog reboot but UI is solid rectangle.
- **Fix:** Convert HTTP to async task pattern (mirror existing `lw_discovery` worker task; post request via queue, poll state from LVGL tick). `startScan()` already does this correctly (`WiFi.scanNetworks(true)`, line 1033) — apply same pattern.
- **Risk if not fixed:** Every network action taps freezes the UI for up to 5 seconds. Under an unreachable K1 this is every tap.

### P0-07 — I2C recovery `powerCycleGrove()` blocks loopTask ~1.45s every 5s while encoders missing

- **SSA:** A (blocking)
- **File:line:** `tab5-encoder/src/input/I2CRecovery.cpp:120` (`delay(800)`) + `:134` (`delay(500)`) + 50 ms + 100 ms settles. Called from `main.cpp:2915` in the loopTask encoder-reprobe branch.
- **Severity:** When encoders are missing and reprobe count ≥ 5, LVGL + WebSocket + WiFi pipeline is frozen ~1.45 s every 5 s. WDT fed but touch unresponsive, WS heartbeat missed, user reports "device crashed during testing".
- **Fix:** Convert recovery escalation to a state machine that consumes one delay slice per `loop()` pass (gate by `millis()` elapsed), OR move recovery to its own pinned task that signals main loop on completion.
- **Risk if not fixed:** 1.5s stutter every 5s during Grove cable jiggle or encoder hot-plug.

### P0-08 — `_discoveryCancelRequested` genuinely unprotected cross-core flag (read-after-free risk)

- **SSA:** J (cross-core races)
- **File:line:** `tab5-encoder/src/network/HttpClient.h:214` (`volatile bool _discoveryCancelRequested`)
- **Severity:** On RISC-V LX7, `volatile` is compiler-only — no hardware release/acquire. Writer is loopTask in `~HttpClient()` at line 28 (NOT under mutex) and `startDiscovery()` at line 102 (under mutex). Readers are discovery task at lines 144, 194, 254, 304 — none take the mutex. During dtor + `vTaskDelete` sequence, a deferred-visibility scenario can cause the task to miss the cancel, continue the 254-IP subnet scan, and dereference a destroyed `HttpClient` via subsequent `_discoveryMutex` access. Mutex-read-after-free.
- **Fix:** Convert to `std::atomic<bool>` with `memory_order_release` on writer and `memory_order_acquire` on the five readers. Hold the mutex during cancel-and-join in the destructor. Matches firmware-v3 Track B atomic migration pattern.
- **Risk if not fixed:** Rare but deterministic UAF crash during `ConnectivityTab` teardown mid-discovery.

### P0-09 — `lw_discovery` task unsubscribed to TWDT; 12 `esp_task_wdt_reset()` calls are silent no-ops

- **SSA:** E (watchdog)
- **File:line:** `tab5-encoder/src/network/HttpClient.cpp:104` — `xTaskCreate("lw_discovery", 6144, priority 1)` with no `esp_task_wdt_add(NULL)` inside the task body. The 12 `esp_task_wdt_reset()` calls at lines 298/370/375/401/411/435/481/491/515/558/568/592 return `ESP_ERR_NOT_FOUND` silently for unsubscribed tasks.
- **Severity:** Discovery task can block on `_client.connect()` (line 371) with implementation-defined WiFiClient timeout, loop 254 hosts with 500 ms each (~127 s of silent blocking), or block on `xSemaphoreTake(_discoveryMutex, portMAX_DELAY)` — none of this ever trips the watchdog. Sister bug to firmware-v3 P0-07 (AudioActor/WiFiManager unsubscribed).
- **Fix:** First line of `discoveryTask()`: `esp_task_wdt_add(NULL);`. Before `vTaskDelete(nullptr)`: `esp_task_wdt_delete(NULL);`. Alternatively remove the 12 feeds entirely since loopTask isn't blocked during discovery — but then a hung discovery task stays invisible forever.
- **Risk if not fixed:** A wedged discovery task is silent — no panic, no reboot, just a permanent "RESOLVING" state.

### P0-10 — Phantom `handleActionButton` from legacy TouchHandler ghost zone

- **SSA:** W (touch input)
- **File:line:** `tab5-encoder/src/input/TouchHandler.cpp:204-209` combined with `main.cpp:2317-2319` (`g_touchHandler.onActionButton(...)` registration) and `main.cpp:2522` (`g_ui->setActionButtonCallback(handleActionButton)`)
- **Severity:** `handleActionButton(idx)` fires via TWO independent paths for the same physical touch on the GLOBAL screen: (1) LVGL path (`_mode_buttons[i]` click event) — authoritative; (2) Legacy TouchHandler path — raw y-band test at `ACTION_ROW_Y_START=404, ACTION_ROW_Y_END=523` and `idx = x / 320` giving only 0..3 (live UI has 6 mode buttons). The TouchHandler constants derive from the pre-LVGL M5GFX layout. At y=404..523 in the LVGL tree the user sees a different row (preset or FX), but a tap there ALSO fires a phantom `handleActionButton(0..3)` that dispatches gamma/colour/EdgeMixer/spatial toggles. Wrong functions fire silently on any tap in the ghost zone.
- **Fix:** Remove `g_touchHandler.onActionButton(...)` registration entirely (LVGL now owns action buttons). Or disable `ACTION_ROW` hit-test in `TouchHandler::hitTestZone()`. Also: STATUS_BAR and PARAMETER_GRID zones (`GRID_Y_START = SCREEN_H+1 = 721`, unreachable) are similar dead layout.
- **Risk if not fixed:** Every tap in the middle band of GLOBAL changes render behaviour unexpectedly.

### P0-11 — `showNetworkConfigScreen` creates unreachable stuck modal

- **SSA:** V (LVGL), X (screen FSM) — cross-confirmed
- **File:line:** `tab5-encoder/src/ui/DisplayUI.cpp:2538-2645`
- **Severity:** `showNetworkConfigScreen()` creates `_network_config_screen` via `lv_obj_create(nullptr)` (ref anti-pattern #2). Save/Cancel buttons at lines 2585-2602 have NO `lv_obj_add_event_cb` attached — inline comment at 2611-2613: *"Button callbacks (simplified - would need proper event handling)"*. Modal has no dismissal path. Further: `_currentScreen` is not updated (line 2615 calls `lv_scr_load` directly bypassing `setScreen()`), so `getCurrentScreen()` reports the previous screen — touch-gate at `main.cpp:2292` sends touches to the wrong screen. Separately, `DisplayUI::~DisplayUI()` deletes only the four persistent screens, so `_network_config_screen` leaks on shutdown if the modal was open.
- **Currently dormant:** `showNetworkConfigScreen()` has no call sites anywhere — unreachable. Becomes real the moment a user-facing button invokes it.
- **Fix:** Either wire Save/Cancel event_cbs + guarantee `hideNetworkConfigScreen()` on all exit paths + update `_currentScreen`, OR delete the unused function outright.
- **Risk if not fixed:** The first caller brickwalls the user at this modal. Also a ship-your-interns-a-footgun.

### P0-12 — `lv_switch_create` called while `LV_USE_SWITCH=0`

- **SSA:** V (LVGL), T (config drift) — cross-confirmed
- **File:line:** `tab5-encoder/src/ui/lv_conf.h:81` (`#define LV_USE_SWITCH 0`) vs `tab5-encoder/src/ui/DisplayUI.cpp:2569` (`lv_switch_create(_network_config_screen)`)
- **Severity:** LVGL 9.3 excludes widget source when `LV_USE_*` is 0. Either the build link fails when this path is compiled, or at runtime `lv_switch_create` is a stub returning NULL. Dead code today because `showNetworkConfigScreen` has no callers — but becomes a real bug the moment it's wired.
- **Fix:** Set `LV_USE_SWITCH 1` in `lv_conf.h`, OR remove the `lv_switch_create` call from the (to-be-deleted) `showNetworkConfigScreen`.
- **Risk if not fixed:** Same trap as P0-11 — unseen silently broken until invoked.

### P0-13 — Rate-limit-dropped zone commands silently discarded (no queue fallback)

- **SSA:** F (queue saturation)
- **File:line:** `tab5-encoder/src/network/WebSocketClient.cpp:703-724` (`sendZoneEffect`), with sibling patterns at 474-489 (`sendEffectParameterChange`) and 815-849 (`sendZonesSetLayout`)
- **Severity:** When `canSend(paramIndex)` returns false (rate-limiter bucket collision), `sendZoneEffect` returns without calling `queueParameterChange`. Comment at 715-716 explicitly says "zone effects use direct send only". A user tapping ZONE-MODE in DisplayUI (DisplayUI.cpp:622-624) to assign the current effect to all 3 zones in rapid succession has the 2nd and 3rd `sendZoneEffect` calls silently dropped — they collide on `ParamIndex::ZONE1_EFFECT + (zoneId*2)` rate-limit windows. UI proceeds, `_zonesEnabled` toggles true, serial logs success, but K1 renders only zone 0.
- **Fix:** Add a dedicated FIFO queue for discrete (non-idempotent) commands: preset save/load/delete, zone setEffect, setZonesLayout, zone enable. Or block-send with a 10 ms timeout and surface the failure to UI.
- **Risk if not fixed:** Visible correctness bugs on rapid multi-zone operations.

### P0-14 — Queued zone commands send wrong field name (contract mismatch)

- **SSA:** Y (K1 protocol)
- **File:line:** `tab5-encoder/src/network/WebSocketClient.cpp:1026-1038` (`processSendQueue` else branch)
- **Severity:** When a `zone.setBrightness` / `zone.setBlend` command is queued (not direct-sent), `processSendQueue` falls into the else branch setting `doc["value"] = …`. But K1's decoder strictly enforces `ALLOWED_KEYS = {"zoneId", "brightness", "requestId"}` (`firmware-v3/.../WsZonesCodec.cpp:143`). K1 silently rejects any `"value"` key. Direct-path send is correct; ONLY the queued retry is broken.
- **Fix:** Update the else branch to emit the correct contract field per command type (brightness → `"brightness"`, blend → `"blendMode"`). Or refuse to queue these commands and surface failure.
- **Risk if not fixed:** Under rapid encoder motion, queued zone brightness/blend updates drop.

### P0-15 — `parameters.set` preset save/load/delete void-returning; UI reports misleading success

- **SSA:** F (queue saturation)
- **File:line:** `tab5-encoder/src/ui/ControlSurfaceUI.cpp:355, 363, 371` (preset delete/load/save) — all followed by Serial log claiming success; underlying `sendEffectPresetSave/Load/Delete` are void-returning (`WebSocketClient.cpp:526-532`). If `takeSendLock` fails (mutex busy, 10 ms timeout), or `_sendDegraded`, or `!isConnected()`, the function silently returns.
- **Severity:** UI marks preset bank "saved" (`_presetSlots[]` state updates locally), serial logs success, K1 never receives the command. On next `requestEffectPresetsList` refresh, the saved preset is absent — user has already moved on.
- **Fix:** Convert the three methods to return bool; surface failure to UI (e.g. red border on slot); don't mutate local state until ack received OR treat as optimistic-with-rollback.
- **Risk if not fixed:** Silent data loss on preset operations.

### P0-16 — `_sendDegraded` latch hides all subsequent sends with no UI signal

- **SSA:** F (queue saturation), B (latching flags) — cross-confirmed
- **File:line:** `tab5-encoder/src/network/WebSocketClient.cpp:371-376, 422-427, 989-994`
- **Severity:** After 3 consecutive send failures, `_sendDegraded = true`. While degraded, `processSendQueue` clears the queue (991-993) AND new `sendJSON` calls keep failing silently (each failure increments `_consecutiveSendFailures` keeping latch set). Only path out is a successful send — if WS is actually broken, no such path. UI has ZERO indication: no getter, no callback, all 25 UI call sites continue to call `sendXxx()` confidently.
- **Fix:** Add `isDegraded()` getter; wire to SidebarWidget or status bar for a "degraded WS" indicator. Or add a time-based clear (N seconds healthy → clear). Or a heartbeat-healthy probe.
- **Risk if not fixed:** Silent UX collapse during network trouble, no user signal.

### P0-17 — Boot banner + encoder-hot-path logs flood USB-CDC @ ~7 KB/s → blocks loopTask

- **SSA:** R (log bandwidth)
- **File:line:** `tab5-encoder/src/parameters/ParameterHandler.cpp:84, 101` (unconditional `[Param]` log on every encoder delta) + `tab5-encoder/src/storage/NvsStorage.cpp:253` (`[NVS] Saved` on every debounced NVS write)
- **Severity:** No `Serial.setTxBufferSize` is called — TX ring size is the pioarduino default (possibly as small as 256 bytes on USB-CDC). Workload: 16 encoders, fast spin at ~100 delta/s per encoder, multi-encoder use → 800 callbacks/s → 24-36 KB/s of `[Param]` output. When the USB-CDC ring fills and no terminal is draining, `Serial.printf` blocks in the CDC ISR context, injecting multi-millisecond delays into the encoder polling loop. Also: boot banner totals ~3 KB, close to the 4 KB threshold on some ring sizes.
- **Fix:** Rate-limit `[Param]` to 10 Hz max, or gate behind a `ENABLE_VERBOSE_PARAM_LOG` flag. Add `Serial.setTxBufferSize(4096)` + `Serial.setTxTimeoutMs(20)` before `Serial.begin()`.
- **Risk if not fixed:** Encoder latency spikes during performance use, dropped touch events from starved loopTask.

### P0-18 — No crash-loop detector; any repeating panic reboots forever

- **SSA:** Z (boot)
- **File:line:** `tab5-encoder/src/main.cpp:1437-1446` — `esp_task_wdt_init` with `trigger_panic = true`; zero uses of `esp_reset_reason()`, zero `RTC_NOINIT_ATTR` variables, zero boot-counter logic.
- **Severity:** A crash during setup (e.g. I2C hang exceeding 5s WDT, LVGLBridge init crash in a future firmware) triggers panic → reset → same crash → reset → forever. No safe-mode fallback. Combined with P0-02 (no rollback), a bad OTA can crash-loop the device with no possibility of automatic recovery.
- **Fix:** Add `RTC_NOINIT_ATTR uint8_t s_bootFailCounter` incremented at top of `setup()`, reset to 0 once `loop()` has run for 30 seconds. If counter > 3, skip WiFi/WS init and show "RECOVERY MODE — connect via USB" on LoadingScreen.
- **Risk if not fixed:** First hard crash bricks the field device.

### P0-19 — `_feedback_until_ms` deadline idiom wrap-unsafe at ~49.7 days

- **SSA:** G (integer overflow)
- **File:line:** `tab5-encoder/src/ui/DisplayUI.cpp:1718, 1725, 1732` (set) and `:1378` (check), plus `src/input/EncoderService.h:311` (`_ledFlash[].start_time`)
- **Severity:** Pattern `_feedback_until_ms[slot] = millis() + 600; if (now >= _feedback_until_ms[slot])`. When `millis()` is within 600 ms of `0xFFFFFFFF` (~49.7 days), the addition wraps to near zero, and `now >= deadline` instantly fires — the feedback flash is skipped. Sister bug to firmware-v3 P0-04.
- **Fix:** Use `(now - start) >= duration` wrap-safe elapsed idiom. Same pattern applies everywhere the codebase uses deadline-addition.
- **Risk if not fixed:** Visual feedback silently vanishes after 49.7 days uptime.

### P0-20 — `HttpClient` destructor busy-wait wrap-unsafe (premature `vTaskDelete`)

- **SSA:** G (integer overflow)
- **File:line:** `tab5-encoder/src/network/HttpClient.cpp:30-31`
- **Severity:** `uint32_t timeoutAt = millis() + 1000; while (... && millis() < timeoutAt)`. If `millis()` is within 1000 ms of wraparound, `timeoutAt` wraps to small, loop exits immediately (0 ms timeout). Destructor then force-`vTaskDelete` on a still-running task → FreeRTOS UB if the task held `_discoveryMutex`.
- **Fix:** `uint32_t started = millis(); while (...&& (millis() - started) < 1000)`.
- **Risk if not fixed:** Heap corruption on `ConnectivityTab` teardown after long uptime.

---

## Part 4 — Findings (P1)

Systemic degradation under load or over time. Not ship-blocking today but increase risk with every deployed unit. Compressed format — file:line + one-line details + fix direction.

### Network / protocol / state

- **P1-01 — WS reconnect uses polling, no `WiFi.onEvent`.** `WiFiManager.cpp` has zero `WiFi.onEvent()` registrations. Disconnection detection depends entirely on polling `WiFi.status()`, no reason codes captured. **Fix:** register `WiFi.onEvent` for `ARDUINO_EVENT_WIFI_STA_*` and surface reason codes to serial.
- **P1-02 — Multi-SSID fallback is dead code.** `platformio.ini:67-76` + `wifi_credentials.ini:24-33` define 4 SSIDs for a nonexistent `[tab5_sta]` env. Only `WIFI_SSID` used; `WIFI_SSID2` referenced once in `HttpClient.cpp:152` as a Lightwave-network heuristic; 3 and 4 zero source refs. **Additionally: real home-network passwords (`parrs45432vw`, `3232AA90E0F24`, `smallKitten57@`) ship in plaintext `.rodata` in every OTA binary** (per class L P2-L4 — worse than XOR obfuscation). **Fix:** Delete the dead fallback; move any intended multi-SSID support to NVS-backed storage.
- **P1-03 — No WiFi retry give-up; retry loop infinite on bad SSID.** `WiFiManager::handleDisconnected()` retries forever with 5s→30s backoff. `WIFI_RETRY_TIMEOUT_MS = 120s` and `WIFI_ATTEMPTS_PER_NETWORK = 2` are declared but never referenced. `enterErrorState()` is DEFINED but NEVER CALLED — the ERROR state is dead code. **Fix:** Wire `enterErrorState()` after N failed attempts; surface to UI.
- **P1-04 — mDNS cached IP never invalidated.** `HttpClient::runDiscovery` caches `_discoveryResult` on first success, `resolveHostname` short-circuits to cached IP forever. If K1 reboots with a new DHCP lease, tab5 uses stale IP. **Fix:** Add `invalidateDiscovery()` called on WS reconnect-fail escalation.
- **P1-05 — No app-level WS ping/pong / missed-pong liveness.** `WebSocketClient::handleEvent` explicitly ignores `WStype_PING/PONG` (lines 310-313). No `_ws.enableHeartbeat()` call. Dead TCP sessions persist until socket error surfaces (>30s). K1 contract documents `getStatus._id` ping mechanism tab5 doesn't use. **Fix:** call `_ws.enableHeartbeat(15000, 3000, 2)` or adopt `_id + _time` RTT.
- **P1-06 — Two parallel discovery strategies with no shared state.** `HttpClient::runDiscovery()` and `main.cpp:2676-2794` multi-tier fallback both resolve IPs, never cross-populate. HTTP REST and WS can end up targeting different K1s on the same LAN. **Fix:** Unify into a single resolver consumed by both.
- **P1-07 — `shouldUseManualIP()` and `isMDNSTimeoutExceeded()` hardcoded `false`.** `WiFiManager.h:103, 109` stubs. Main-loop P2 "manual IP from NVS" and P4 "timeout fallback to MDNS_FALLBACK_IP_PRIMARY" branches dead. `MDNS_FALLBACK_IP_PRIMARY=192.168.1.101` unreachable. User on foreign WiFi where mDNS fails is wedged forever with no UI override.
- **P1-08 — No device-dropped detection except WiFi loss.** `g_wsConfigured` not cleared on WS drop (explicit "DO NOT reset" comment at `main.cpp:2797-2800`). WS library hammers dead IP forever with backoff up to 30s. No failover to a different K1 at a different IP.
- **P1-09 — `zone.setBrightness`/`setBlend` queued fallback contract mismatch** — covered as P0-14 above.
- **P1-10 — REST DELETE `/api/v1/network/networks/{ssid}` not in contract.** `HttpClient.cpp:714` calls a path not in `k1-rest-contract.yaml:585-587` (which lists only GET/POST). **Fix:** update contract or remove call.
- **P1-11 — `zone.status` handler is dead code.** K1 never emits it; `WsMessageRouter::handleZoneStatus` is only invoked from its own `handleZonesList` fallback. **Fix:** remove route at `WsMessageRouter.h:111`.
- **P1-12 — `tempo.h`-style ping-pong for multi-tier fallback.** mDNS-only-on-first-connect gated through DiscoveryState::SUCCESS — no spam, good. But `DiscoveryState::FAILED` is not auto-reset; downstream callers of `resolveHostname()` see misleading "in progress" messaging while stuck in FAILED. **Fix:** clear FAILED after ~30s or on WS reconnect.

### UI / LVGL / touch / screens

- **P1-13 — Sidebar tab state desynchronises with screen on Back.** `DisplayUI.cpp:359-395` sidebar callback sets `_currentTab = tab` then for ZONES calls `setScreen(ZONE_COMPOSER)` and early-returns. On Back, `_currentScreen` becomes GLOBAL but `_currentTab` stays `ZONES` — neon colour, panel visibility, encoder routing all desynchronised until next sidebar tap.
- **P1-14 — `setScreen` no transition-during-transition guard.** `DisplayUI.cpp:1632-1654` non-reentrant-safe: rapid double-tap to CONTROL_SURFACE + back + CONTROL_SURFACE re-issues `onScreenEnter` → duplicate WS requests, out-of-order callbacks on stale state. No `onScreenExit` hook so scans started in ConnectivityTab continue after Back.
- **P1-15 — `ConnectivityTab` styles never `lv_style_reset`-ed in destructor.** `ConnectivityTab.h:182-184` + `ConnectivityTab.cpp:60-68`. Three `lv_style_t` with multiple property sets; LVGL 9 uses heap prop arrays for styles with enough properties. Dormant at shutdown, active leak if ConnectivityTab is ever re-instantiated.
- **P1-16 — `LVGLBridge::gDrawBuf` 160 KB PSRAM never freed; no `deinit()`.** `lvgl_bridge.cpp:51`. Any OTA path calling `cleanup()` then re-init LVGL double-allocates. `gDisplay` / `gTouchIndev` handles also leak.
- **P1-17 — `HttpClient` force-`vTaskDelete` may leave `_discoveryMutex` locked, then `vSemaphoreDelete` on locked mutex.** `HttpClient.cpp:35 + 27-43`. Low likelihood race but UB per FreeRTOS. **Fix:** lengthen join wait to 3s OR acquire cancel-through-mutex pattern.
- **P1-18 — `refreshNetworkLists()` creates unbounded LVGL widgets per scan refresh.** `ConnectivityTab.cpp:944, 984, 930-1018`. Called from `loop()` on discovery/scan completion → `lv_obj_clean` + rebuild → 300 widget allocations per refresh. Mitigated by WDT reset every 3 items. Fragmentation risk. **Fix:** pre-create all items, show/hide/update.
- **P1-19 — Modal-overlay touch not blocked.** PresetSlotWidget SAVING/DELETING are visual-only 500 ms flashes; no `LV_OBJ_FLAG_CLICKABLE` toggle, no `lv_indev_reset`. User taps during 500ms window → LVGL dispatches click → preset manager may re-enter save.
- **P1-20 — `LV_DISP_DEF_REFR_PERIOD` is LVGL 8 macro name; override silently dropped.** `lv_conf.h:15`. Correct LVGL 9 macro is `LV_DEF_REFR_PERIOD`. Intended 60 Hz refresh is actually default 30 Hz. **Fix:** rename.
- **P1-21 — `LoadingScreen` is runtime-create / runtime-destroy (violates anti-pattern #2).** Intentional exception; self-documenting but undocumented in reference. Correctly tears timer before screen. **Fix:** either document the exception or migrate to persistent-hidden pattern.
- **P1-22 — Touch debounce is event-timer not press-suppression.** `TouchHandler.cpp:77`. `DEBOUNCE_MS = 100` applied asymmetrically; rapid dual-taps may only fire `handleActionButton` once (amplifies P0-10 mismatch).

### Boot / OTA / power / persistence

- **P1-23 — OTA progress never surfaces to LVGL or M5ROTATE8 LEDs.** `OtaHandler::getProgress()` defined but no callers. No progress bar, no LED sweep.
- **P1-24 — OTA token compile-time plaintext.** `OTA_UPDATE_TOKEN` is a `#define` in `network_config.h:136`. Recoverable from any firmware dump. Should be NVS-stored, user-settable, per-device unique.
- **P1-25 — Watchdog only subscribes `loopTask`; AsyncTCP and M5Unified internals unsubscribed.** `main.cpp:1443`. If AsyncTCP hangs, WDT doesn't catch it — device appears alive (loop feeds WDT) but OTA and WS are dead.
- **P1-26 — No backlight / power management.** `M5.Display.setBrightness()` never called, runs at full brightness forever. Zero `esp_sleep_*`, zero `M5.Power.setLed`, no idle dim, no motion-idle detection. Battery drains in ~2-3h.
- **P1-27 — `NvsStorage::flushAll()` exists but never called on shutdown/OTA.** `NvsStorage.cpp:214`. Encoder changes within 2s of `ESP.restart()` (OTA path) are silently discarded by the debounce.
- **P1-28 — PresetStorage V1→V2 migration doesn't persist migrated struct.** `PresetStorage.cpp:85-103` — `memcpy` into PSRAM but no `nvsBackupSlot(i)` call. Every cold boot re-runs migration.
- **P1-29 — PresetStorage `s_nvsHealthy` latches FALSE on any NVS error, never re-evaluated until reboot.** `PresetStorage.cpp:247, 255`. Single `nvs_commit` failure permanently disables preset-NVS persistence, user saves presets in-session (PSRAM) but silent loss on reboot.
- **P1-30 — ConnectivityTab `fallbackAttempted` one-shot lifetime latch.** `ConnectivityTab.cpp:696`. Function-local static, never reset. If initial discovery + fallback both fail, tab is wedged empty — user must reboot.
- **P1-31 — `ConnectivityTab::_state == CONNECTING` has no self-clear.** `ConnectivityTab.cpp:1188` set; no code transitions CONNECTING→IDLE on success. Cosmetic today; state reset only on next SCANNING.
- **P1-32 — No first-boot provisioning.** `main.cpp:2265` uses compile-time `WIFI_SSID` macro, not NVS. Every device ships pre-paired to one SSID.
- **P1-33 — No WiFi TX power / power-save configured.** No `WiFi.setTxPower()`, no `WiFi.setSleep()` / `esp_wifi_set_ps()`. Default `WIFI_PS_MIN_MODEM` adds ~100ms DTIM latency per packet — painful for encoder hot path.

### Memory / stack / performance

- **P1-34 — `loopTask` runs ALL UI + WS + touch + WiFi in one thread.** Stack 8 KB (Arduino default, no override). LVGL dispatch chain adds unknown depth. M5GFX SPI flush chain adds another. Worst-case stack depth never measured on P4.
- **P1-35 — `lw_discovery` (6 KB stack) tight for 254-host subnet sweep.** Estimated 40-57% peak usage. TCP socket frame depth + `String` growth indirection. Thin margin.
- **P1-36 — `configCHECK_FOR_STACK_OVERFLOW` not explicitly set; no `vApplicationStackOverflowHook`.** If platform default is 0 or 1 (canary only), overflow silently corrupts adjacent heap.
- **P1-37 — 200 KB LVGL heap sits in internal SRAM, not PSRAM.** `SPIRAM_MALLOC_ALWAYSINTERNAL=4096` keeps small allocations (LVGL widgets) internal. ~49% of internal SRAM consumed by LVGL alone.
- **P1-38 — No `Serial.setTxBufferSize`; no `Serial.setTxTimeoutMs`.** USB-CDC TX ring at platform default (potentially 256 B). Boot banner ~3 KB close to limit.

---

## Part 5 — Findings (P2) — summary buckets

Full details in SSA returns (Part 7). Listed here with short descriptions for triage.

### Dead code / cleanup (from SSA-I)

- **`.bak` files committed:** `src/ui/ConnectivityTab.cpp.bak` (1,363 LOC), `ZoneComposerUI.h.bak` (572), `ZoneComposerUI.h.bak2` (572) — 2,507 LOC in scratch.
- **Entire simulator subtree dead** (`src/hal/SimHal.*`, `src/input/SimEventQueue.h`, `simulator/*`) — ~1,568 LOC. `SIMULATOR_BUILD` never defined anywhere; 100+ `#if !defined(SIMULATOR_BUILD)` guards obscure every UI file.
- **`src/input/EncoderService.h` (382 LOC):** superseded by `DualEncoderService.h` per refactor memory #10774. Zero include sites.
- **`src/ui/widgets/PresetBankWidget.h` (356 LOC):** unused sibling widget. Zero instantiations.
- **Dead WebSocketClient API:** `sendGenericParameter`, `requestEdgeMixerGet`, `sendColourCorrectionMode` (header: *"K1 has no handlers for these"*).
- **Dead NvsStorage API (5 methods):** `loadParameter`, `flushAll`, `saveAllParameters`, `eraseAll`, `hasPending` — zero callers.
- **Dead `tab5net` NVS namespace:** `manual_ip`, `use_manual` keys declared in `network_config.h:126-131`, zero producers, zero consumers.
- **Dead WiFiManager stubs:** `shouldUseManualIP`, `getManualIP`, `isMDNSTimeoutExceeded` — hardcoded to false/sentinel.
- **Dead NetworkConfig constants:** `MDNS_FALLBACK_IP_PRIMARY`, `MDNS_FALLBACK_TIMEOUT_MS`, `MDNS_MAX_ATTEMPTS`, `WIFI_ATTEMPTS_PER_NETWORK`, `WIFI_RETRY_TIMEOUT_MS` — declared, never referenced.
- **`OrientationManager`:** defined (8.6 KB .cpp, full tilt/hysteresis/debounce) but never instantiated outside its own .cpp.
- **`DeviceRegistry` / `DeviceSelectorTab`:** referenced in claude-mem memories #23438/#23446/#23485/#23508 but ZERO code presence in tree. Either abandoned or on a branch that was never merged.
- **~90 commented-out `Serial.printf("{\"sessionId\":…}")` debug scaffolds across 3 files** (main.cpp, WebSocketClient.cpp, DisplayUI.cpp). Forensic residue from past investigations.
- **Commented AP_SSID/AP_PASSWORD macros:** `network_config.h:14-15` — dead scaffold (correctly notes "Tab5 should never create its own AP").
- **Total estimated reclaimable:** ~5,000 LOC (non-font) + ~200 `#ifdef` guard lines.

### Config drift / hygiene (from SSA-T)

- `LV_MEM_SIZE=200KB` is IGNORED because `LV_MEM_CUSTOM=1`. Delete to avoid misleading future readers.
- `LV_CONF_INCLUDE_SIMPLE` + `LV_CONF_PATH` both set; only one is needed.
- `LV_USE_DRAW_SW_HELIUM=0` / `LV_USE_DRAW_SW_NEON=0` redundant: RISC-V can't link ARM ASM and `scripts/pio_pre.py` physically deletes the helium/neon directories.
- `LV_USE_BTN=0` but ref.md templates use `lv_btn_create`; code uses `lv_obj_create + CLICKABLE` correctly. Align ref.
- `LV_DRAW_SW_COMPLEX=1` on 1280×720 software draw (no DMA2D/PPA/NEON/Helium) — render cost significant. Audit whether complex draw features are actually used.
- `CORE_DEBUG_LEVEL=3` is a dead flag here — project uses `Serial.print` throughout; arduino `log_*` macros unused.
- `sdkconfig.defaults` has 6 redundant entries duplicating pioarduino baseline; only `SPIRAM_MALLOC_RESERVE_INTERNAL=32768` and `SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y` are effective overrides.
- `[env:tab5_debug]` `-g3 -ggdb -Og` conflict with inherited `-Os`.
- `scripts/pio_pre.py:12` hardcodes absolute log path `.cursor/debug.log` under `/Users/spectrasynq/…`.
- Partition `coredump=64KB` undocumented — may be insufficient for full Tab5 crash frame with PSRAM-resident stacks.
- `LV_INDEV_DEF_READ_PERIOD` not overridden (default 30 ms = 33 Hz touch polling) — may feel laggy.
- `FIRMWARE_VERSION = "1.0.0"` compile-time constant never bumped (`OtaHandler.cpp:21`).
- `cleanup()` (main.cpp:2344-2384) is unreferenced dead code.

### Memory / heap

- `LVGLBridge::gDrawBuf` 160 KB PSRAM never freed (P1-16).
- `ConnectivityTab` three `lv_style_t` heap never `lv_style_reset`-ed (P1-15).
- `NvsStorage::s_handle` / `PresetStorage::s_handle` never `nvs_close()`-ed at shutdown — process-lifetime, OTA-unsafe.
- No `vApplicationStackOverflowHook`; no `configCHECK_FOR_STACK_OVERFLOW` explicit.

### Queue saturation (residual, from SSA-F)

- `SEND_QUEUE_STALE_TIMEOUT_MS=500` with `PARAM_THROTTLE_MS=50` means user final encoder value dropped if channel blocked > 500 ms — should LIFO-pop latest.
- `SimEventQueue` has zero production callers; infrastructure for queue overflow handling exists unused.
- No `[degraded WS]` indicator anywhere in UI (see P0-16).

### Integer overflow (residual, from SSA-G)

- `I2CRecovery::s_recoverySuccesses` uint16_t, no saturation, wraps at 65535 (telemetry only).
- `I2CRecovery::s_errorCount` uint8_t, no saturation, wraps at 255 (misleading status log).
- `_consecutiveSendFailures` uint32_t uncapped (design smell).
- `_mdnsRetryCount` uint8_t wraps at 255 (42+ min of continuous failure → misleading telemetry).

### Touch / UI edge cases

- `ACTION_BUTTON_W=320` hardcoded to divide 1280 into 4; LVGL mode-container has 6 buttons. Wrong mapping even if geometry aligned.
- No `lv_display_set_rotation` — rotation is via `M5.Display.setRotation(3)` at boot only; if IMU-driven rotation is ever wired, LVGL and M5GFX will disagree.
- `LoadingScreen::update` task origin not enforced — if called from non-loopTask, it mutates label text without `lv_lock`.

### Protocol (residual, from SSA-Y)

- `WStype_BIN` handler is empty (line 305) — confirms tab5 is purely JSON; any spontaneous binary frame from K1 dropped without logging.
- No incoming-frame size guard — `deserializeJson` passes full `length`; large `zones.list` responses (~2KB+) risk ArduinoJson v7 pool allocation issues.
- Tab5 sends `mood` / `fadeAmount` via WS `parameters.set`; K1 contract WS schema doesn't list these (REST-only). If K1 drops them, silent failure.

### WiFi / power

- mDNS `log_level` silenced to ERROR at `WiFiManager.cpp:117` — hides future duplicate-hostname collisions.
- `ConnectivityTab::antennaButtonCb` calls `WiFi.disconnect()` directly, bypassing `WiFiManager::reconnect()` — FSM state desync until next `update()` tick.
- `MDNS.begin("tab5encoder")` failure not retried — tab5 becomes unreachable by `tab5encoder.local` if init fails.

---

## Part 6 — Remediation strategy

### 6.1 Recommended execution model

Given the scale, **do NOT** attempt this in a single pass from main context. Each P0/P1 item should be:

1. Triaged for scope (one file vs many).
2. Dispatched to an SSA with a tight prompt (the finding + file:line + proposed fix + test criterion).
3. Verified independently — and because tab5 has NO test infrastructure (per codebase-map.md), verification means on-hardware soak with manual encoder/touch/WS-traffic exercises.
4. Landed as isolated commit with CHANGELOG entry (if tab5-encoder has a CHANGELOG — it currently does not; creating one is P2).

**Cross-project consistency:** Track B on firmware-v3 established patterns for several of these exact issues (AsyncTCP priority, WDT subscription, volatile→atomic, OTA session timeout, rate-limiter wrap). **Port those fix idioms directly** to tab5-encoder rather than re-deriving — this is the highest leverage move per engineering hour.

### 6.2 Three tracks (pick one)

**Track A — P0 sweep** (tightest scope, highest leverage)
- 20 fixes listed in Part 3.
- Closes: UI freezes (HTTP on LVGL task), OTA brick/corruption/wedge, AsyncTCP priority inversion, phantom action-row dispatch, ConnectivityTab broken-by-design, discovery WDT gap, cross-core UAF.
- Can be dispatched as ~12 parallel SSAs (several findings share files and can combine).
- Estimated wall-clock: 1-1.5 days (one parallel dispatch + manual verification round, no automated tests).

**Track B — P0 + P1 sweep** (recommended)
- Track A + 38 P1 items.
- Includes: OTA progress UI, WiFi retry give-up + event handler, mDNS invalidation, WS ping/pong, screen-FSM cleanup, style-reset, unified discovery, backlight/power, boot provisioning, `[degraded WS]` indicator.
- Estimated wall-clock: 2-3 days.

**Track C — P0 + P1 + dead-code cull**
- Track B + delete simulator/ (~1,500 LOC), EncoderService.h, PresetBankWidget.h, OrientationManager, dead WebSocketClient/NvsStorage APIs, `.bak` files, commented scaffolds, dead NetworkConfig constants, dead `tab5net` namespace.
- Recovers ~5,000 LOC (+ ~200 #ifdef guards); simplifies every future audit.
- Estimated wall-clock: 2-4 days.

### 6.3 SSA dispatch template (copy-paste for the next agent)

When dispatching a fix SSA, include:

```
Fix <PN-NN from this document>.

Context:
  File:line: <from finding>
  Severity: <from finding>
  Sister fix on firmware-v3: <reference if one exists, e.g. "mirrors firmware-v3 Track B P1-01">
  Already-known-state: <anything the SSA should not break>

Scope (only these files):
  <list>

Required change:
  <one-paragraph description with the exact fix>

Verification (tab5 has NO test infrastructure — manual soak required):
  <exact steps: build, flash, power-cycle, exercise with K1 and encoder panel, observe>

Hard constraints (CLAUDE.md):
  - British English in comments/logs.
  - Tab5 is WiFi STA — do NOT invert to AP mode.
  - LVGL widget patterns follow docs/reference/lvgl-component-reference.md.
  - No heap alloc in LVGL render tick.
  - clangd-first for C++ symbols; grep only for text literals.

Return contract:
  Files modified: N
  Per-file change summary
  Regression check: <what previously-working behaviour must still work>
  Commit message suggestion
```

### 6.4 Verification strategy for the next agent

After any fix lands:

1. **Build check:** `pio run -e tab5` — must succeed without new warnings.
2. **Flash check:** `pio run -e tab5 -t upload --upload-port /dev/cu.usbmodem1101` — must complete.
3. **Smoke test (manual, no automation exists):**
   - Boot to GLOBAL screen → status bar + preset row + encoder LEDs render.
   - Navigate GLOBAL → CONNECTIVITY → SCAN → observe results populate (post-fix for P0-05).
   - Navigate CONNECTIVITY → back → GLOBAL → tap any mode button → observe correct behaviour (post-fix for P0-10).
   - Spin two encoders for 60 s continuously → observe no UI stutter (post-fix for P0-17).
   - OTA upload a known-good firmware → confirm success on first attempt AND on a second attempt after a simulated disconnect at 50% (post-fix for P0-04).
   - Leave device running 4+ hours with WS traffic → observe heap stable, no reboots.
4. **Regression scope:** each fix category has a specific regression vector — see the per-finding "risk if not fixed" notes to construct its negative test case.
5. **No CHANGELOG.md exists** in `tab5-encoder/` — add one (`tab5-encoder/CHANGELOG.md`) with `[Unreleased] → Fixed` entries per Keep-a-Changelog convention.

### 6.5 What NOT to do

- **Do NOT** attempt to fix everything in one commit. tab5 has no automated tests, so each fix needs its own manual verification window.
- **Do NOT** remove `simulator/` subtree without first confirming no downstream consumer (unit tests, a separate harness). grep confirms zero SIMULATOR_BUILD define, but verify in branches too.
- **Do NOT** delete `OrientationManager` without first confirming the product decision: is Tab5 physically mounted rotation-locked (delete) or is it a planned feature (wire up).
- **Do NOT** enable `LV_USE_SWITCH=1` just to unblock `showNetworkConfigScreen` — that function is unreachable and half-implemented. Delete it.
- **Do NOT** invert tab5 to AP mode. tab5 is STA; K1 is AP. This is architectural.
- **Do NOT** rely on `_discoveryState == SUCCESS` as permanent truth — see P1-04.
- **Do NOT** add LVGL calls from the WS callback path without `lv_lock()/lv_unlock()` — today the single-task invariant works by convention (all LVGL work is on loopTask); adding cross-task calls breaks it silently (see class V P1-10).

---

## Part 7 — SSA returns (detailed appendix)

This part contains the full distilled findings from each SSA. The next agent can re-read any section without re-dispatching. Agent IDs in Part 2 table allow resumption via `SendMessage`.

### 7.A — Blocking primitives (agent a**ab1ee8ade1015f7a2**)

3 P0 findings, 6 P1, 3 P2. Headline: HTTP/mDNS calls inside LVGL event callbacks freeze the UI for seconds (P0-1), `HttpClient::_discoveryMutex` + cross-task `portMAX_DELAY` creates future-widening-risk (P0-2), `I2CRecovery::powerCycleGrove` blocks loopTask ~1.45s each (P0-3). Also: `mdns_query_a` 500 ms blocking at WiFiManager.cpp:200 from loopTask every 10s (P1-2); `Update.begin()` flash erase synchronous on AsyncTCP task without WDT feed (P1-4); `ConnectivityTab` list rendering uses `delay(1)+esp_task_wdt_reset()` every 3 items (P1-6 — symptom of a larger LVGL perf issue). All WebSocketClient send paths correctly bounded to 10ms mutex timeout (anti-finding).

### 7.B — Latching state flags (agent a1f26cf050c2e7853)

22 member bools + 14 static bools inventoried. Zero P0. Two P1: (1) `PresetStorage::s_nvsHealthy` latches false on any NVS error, no retry (silent data loss on reboot); (2) `ConnectivityTab::fallbackAttempted` one-shot lifetime latch. Four P2: `_sendDegraded` queue-only latch (see P0-16); `ConnectivityTab::_state==CONNECTING` no self-clear; `OtaHandler::s_updateStarted` latches on client disconnect (see P0-04); `HttpClient::DiscoveryState::SUCCESS` terminal — stale IP cannot be rediscovered (see P1-04). 14+ flags verified symmetric: all ButtonHandler mode flags, all TouchHandler edge flags, WiFiManager/WebSocketClient FSM state fields, PresetSlotWidget states (500ms timer self-clears on next update).

### 7.C — ISR / IRAM safety (agent a172f5b4ce85ba01b)

**Zero ISRs in application code.** No `IRAM_ATTR` / `DRAM_ATTR` / `attachInterrupt` / `esp_timer_create` / `xTimerCreate` / `gpio_isr_handler_add` / `esp_intr_alloc` / `FromISR` / `portENTER_CRITICAL_ISR` calls anywhere. All library-internal ISRs (ESP-IDF I2C, AsyncTCP LwIP hooks, FreeRTOS tick, WiFi radio, LVGL internal) are vendor code and already IRAM/DRAM-correct. LVGL 9.3 uses `lv_tick_set_cb` polling model (not deprecated `lv_tick_inc` ISR). The Guru Meditation class that triggered the firmware-v3 audit is architecturally impossible in tab5-encoder. **No remediation needed for class C.**

### 7.D — Priority inversion (agent a302028ebd518b970)

3 tasks: `loopTask` (prio 1, core 1, 8 KB), `lw_discovery` (prio 1, unpinned, 6 KB, one-shot), `async_tcp` (prio 10, unpinned, 16 KB). Two P0: AsyncTCP preempts loopTask during OTA (same bug as firmware-v3, not ported); `g_wifiManager.update()` and `g_wsClient.update()` starve during AsyncTCP burst. Two P1: `flush_cb` SPI blit blocks loopTask without yield (no LVGL tick budget documented); `lw_discovery` unpinned xTaskCreate. No WebSockets task spawn (library is polled), no LVGL internal tasks (cooperative via loopTask), no M5Unified mic/speaker tasks (dormant — `M5.Mic.begin()` / `M5.Speaker.begin()` never called). The single urgent action: port firmware-v3's three `CONFIG_ASYNC_TCP_*` overrides.

### 7.E — Watchdog coverage (agent a3d54580a1122830e)

TWDT config: 5s timeout, `trigger_panic=true`, `idle_core_mask=0` (wrong for P4 dual-core — should be 0x3). One subscribed task (`loopTask`) via explicit `esp_task_wdt_add(NULL)` at main.cpp:1443. Two P0: `lw_discovery` unsubscribed with 12 no-op feed calls (up to ~127s silent hang possible); AsyncTCP task unsubscribed (OTA wedge invisible). Three P1: blind top-of-loop feed; DualEncoderService per-4-encoder unconditional feed; LVGL `flush_cb` paired feeds around `pushImage`. Four P2: `idle_core_mask` wrong; IWDT defaults not pinned in sdkconfig; TWDT 5s not documented; no `esp_task_wdt_delete` in cleanup(). The `lw_discovery` fix is a single-line change at task entry.

### 7.F — Queue saturation (agent a46757eab3eb011bb)

~25 dispatch-site callers audited against 1 underlying `_ws.sendTXT` primitive. Four P0: rate-limit-dropped zone commands with no queue fallback (see P0-13); preset save/load/delete void-returning and UI ack-lies (see P0-15); `sendZonesSetLayout` mutex-contention drop; `_sendDegraded` latch (see P0-16). Six P1: NVS parameter save fails silently per-frame; PresetStorage NVS-backup silent latch (see P1-29); `requestZonesState` no queue fallback; `sendEffectParameterChange` drops without queue; WS reconnect during user action loses in-flight command; LVGL button callback issues 4+ sequential WS sends without atomic failure. Three P2: `SimEventQueue` dead in production (see dead-code); HTTP discovery per-candidate failures silent by design; `processSendQueue` single-send-per-update limit. Cross-cut to firmware-v3: symmetrical to the "89% of actor dispatch sites ignore bool" finding — here the bool is captured inside WS layer but wrapper methods are void, UI has no way to observe failure.

### 7.G — Integer overflow (agent a40535bd2d035863f)

Two P0: `_feedback_until_ms` deadline idiom wrap-unsafe at 49.7 days (DisplayUI, see P0-19); HttpClient destructor busy-wait wrap-unsafe (see P0-20). Four P1: `s_recoverySuccesses` uint16 saturates (telemetry); `s_errorCount` uint8 wraps at 255 (misleading status); `_consecutiveSendFailures` uint32 uncapped (design smell); `_mdnsRetryCount` uint8 wraps at 255 (42 min continuous failure). Five P2 mostly correct idioms used. Anti-findings: `s_uiInitEarliestMs` uses correct `int32_t` cast signed-delta; `PresetSlotWidget::_animStart` uses correct elapsed idiom; WS/WiFi backoff saturating `min()`; `pdMS_TO_TICKS` all small constants; LedFeedback breathing animation modulus.

### 7.H — Resource leaks (agent af164a60bc75d1e8b)

Prior audit (ssa-9 2026-04-03) fixed 3 leaks (`g_otaServer`, `_sendMutex`, `lgfx::PPASrm`) — all verified closed. **NEW leaks:** one P0 (`_network_config_screen` leak on shutdown if modal open — see P0-11), three P1 (`ConnectivityTab` style heap; `LVGLBridge::gDrawBuf` 160KB PSRAM never freed; HttpClient force-vTaskDelete + vSemaphoreDelete race). One P2 (NVS handles never closed — process-lifetime, OTA-unsafe). `lv_obj_add_event_cb` callbacks correctly auto-free with object destruction in LVGL 9. DisplayUI has two destructors via #if defined(TAB5_ENCODER_USE_LVGL) — both handle their paths correctly. PresetStorage `s_presets` PSRAM process-lifetime acceptable.

### 7.I — Dead code (agent a788d9abaad9a9de4)

~5,000 reclaimable LOC (non-font). Major: `.bak` files (2,507); simulator subtree (1,568); `EncoderService.h` (382); `PresetBankWidget.h` (356). Medium: dead WebSocketClient/NvsStorage methods (120 LOC total); dead NetworkConfig constants; dead `tab5net` NVS namespace; dead WiFiManager stubs. Small: ~90 commented-out session-JSON debug blobs across main.cpp/WebSocketClient/DisplayUI. Also: `DeviceRegistry` / `DeviceSelectorTab` referenced in claude-mem memories but **zero code presence** — either abandoned or on an unmerged branch. `OrientationManager` (9 KB) never instantiated. `sendColourCorrectionMode` — called but header comment states "K1 has no handlers for these". `FIRMWARE_VERSION = "1.0.0"` compile-time constant never bumped.

### 7.J — Cross-core data races (agent a0b0271585d0b4218)

Only 4 `volatile` declarations in 2 files; zero `std::atomic`. Two P0: `_discoveryState` cross-core volatile with mutex-protected writers but `volatile` adds nothing (false contract); `_discoveryCancelRequested` genuinely unprotected cross-core (see P0-08). One P1: `SimEventQueue::_head/_tail` cross-core-if-used (class is dead code today, latent landmine). No P2 (no "same-core volatile" noise). Zero `FromISR` APIs, zero compiler-only barriers, zero `attachInterrupt` — cleaner surface than firmware-v3. `g_wsConfigured` verified same-task despite claude-mem #23387 flagging it — anti-finding.

### 7.L — NVS + persistence (agent a6d4217c5049d852f)

**Zero P0 — tab5 has been correctly hardened against the firmware-v3 class of bugs.** `NvsStorage::init()` explicitly does NOT call `nvs_flash_erase()` on error; `PresetStorage::init()` is PSRAM-primary with NVS write-behind and refuses to erase. Partition table correctly 64KB (recently enlarged). Five P1: NVS parameter save never flushed before OTA reboot; preset V1→V2 migration doesn't persist; OTA no rollback validation; OTA could flash over NVS page (indirect); no mutex around NVS handles (single-thread invariant not documented). Eight P2: no factory reset endpoint (`eraseAll`/`clearAll` defined but unused); dead `tab5net` namespace; no DeviceRegistry persistence implementation; WiFi creds plaintext in OTA binary; OTA token hardcoded plaintext; no cross-blob checksum; NVS commit logging warn-only on failure; `PresetData::isValid` edge case on `version==0`. tab5's PSRAM-primary preset architecture is the model firmware-v3 should migrate toward.

### 7.M — WiFi STA (agent a5a8b885337eb026e)

Zero P0 — FSM cannot wedge terminally. Nine P1: `WL_IDLE_STATUS` / `WL_DISCONNECTED` not treated as failure (15s stall under auth loops); no `WiFi.onEvent` registration (poll-only, no reason codes); no multi-SSID sequential fallback (4 SSIDs declared, only 1 used); no retry counter / no bounded give-up (enterErrorState dead); `reconnect()` resets backoff bypassing pacing; AP-disappearance recovery functional but no fast-rejoin (no BSSID pinning); `MDNS.begin` failure not retried; mDNS 500ms blocking in main loop every 10s; `setAutoReconnect(false)` disables kernel-level reconnect. Seven P2: TX power never set; power-save never configured; no `setHostname`; antenna switch bypasses WiFiManager; `_ssid`/`_password` raw pointer; race on antenna init vs WiFi init order; mDNS log level silenced to ERROR. `m_forceApOnly` N/A (tab5 is STA, never AP). Anti-findings: FSM is deadlock-free, STA mode correctly set, all paths out of all states exist, mDNS failure doesn't block WS connection (multi-tier fallback in main.cpp).

### 7.Q — Stack + memory budget (agent a680418e74a981d20)

Three tasks: `loopTask` 8KB (Arduino default), `lw_discovery` 6KB, `async_tcp` 16KB (library default). `loopTask` monitored via `uxTaskGetStackHighWaterMark(NULL)` every 30s at CORE_DEBUG_LEVEL>=3 — partial fix from prior audit. Zero P0 (no imminent overflow under normal operation — loopTask estimate 2.5-4 KB / 8 KB, `lw_discovery` 2.5-3.5 KB / 6 KB). Three P1: loopTask combined LVGL+WS+M5GFX chain depth unmeasured; `lw_discovery` thin margin for 254-host scan; LVGL 200 KB heap in internal SRAM. Four P2: `lw_discovery` watermark never captured (task exits); `async_tcp` unmonitored; `configCHECK_FOR_STACK_OVERFLOW` not explicitly set; `vApplicationStackOverflowHook` absent. Significant correction vs prior audit ssa-9: `EXT_RAM_BSS_ATTR` is applied to `s_effectNames`/`s_paletteNames` putting 16.1 KB in PSRAM — SRAM budget is ~16 KB lower than ssa-9 estimated. Also: ssa-9 missed the `async_tcp` task entirely — real task count is 3, not 2.

### 7.R — Log / serial bandwidth (agent ab1a8db7e002c9876)

Two P0: `[Param]` prints on every encoder delta (7 KB/s peak under active use — blocks USB-CDC full); `[NVS] Saved` on every debounced NVS write (blocking burst at encoder-idle moment). Six P1: no `setTxBufferSize` / `setTxTimeoutMs` (boot banner ~3KB close to limit); `[TOUCH]` logs unconditional; `[Param] Synced` per changed parameter per status message (~1.2 KB/s unrate-limited); `[FOOTER DEBUG]` key-dump in WS callback; `[WS] Reconnecting`; `[REPROBE] WARNING`. Seven P2 cleanup (commented scaffolds, dead TRACE blocks, diagnostic tags). **LVGL is silent in production** (LV_USE_LOG=0, PERF/MEM monitors off) — excellent. Cumulative steady-state: ~20 B/s idle, ~1.2 KB/s with status sync, ~7 KB/s active encoder use.

### 7.S — Reentrancy + callback-under-lock (agent a82f9215be139f644)

One P0: ConnectivityTab LVGL event callbacks invoke blocking HTTP (see P0-06 for consolidated writeup). Three P1: `sendJSON()` called from inside `lv_timer_handler()` via LVGL callbacks — safe TODAY only because `_sendMutex` isn't held at that loop-order point, BUT any reversal of the loop() body order introduces deadlock (load-bearing invisible ordering); `_retry_callback` user-supplied invoked inside LVGL event (callback semantics unconstrained); `SidebarWidget::setTabCallback` calls `_wsClient->requestEffectPresetsList()` from inside `lv_timer_handler()` (TCP backpressure can stall LVGL). Three P2: `getDiscoveryState` `portMAX_DELAY` could wedge loopTask if discovery task crashes with mutex held; `DualEncoderService::invokeCallback` fires during `_values[]` mutation (safe same-task today); `new HttpClient()` heap alloc inside LVGL event. `links2004/WebSockets` is polled not async — `handleEvent` fires on loopTask (anti-finding dispels the initial "AsyncTCP → LVGL" worry). LVGL event callbacks do not call `lv_timer_handler` recursively.

### 7.T — Config drift (agent a2bb3c069f3507825)

Two P0: `LV_DISP_DEF_REFR_PERIOD` is LVGL 8 name (dead override, actual 30Hz not 60Hz); `LV_USE_SWITCH=0` but `lv_switch_create` called (build risk). Eight P1: 6 redundant sdkconfig entries; `CORE_DEBUG_LEVEL=3` dead flag here (no `log_*` macros used); `LV_USE_DRAW_SW_HELIUM/NEON=0` redundant (pio_pre.py deletes dirs); `LV_MEM_SIZE` ignored when `LV_MEM_CUSTOM=1`; dual `LV_CONF_INCLUDE_SIMPLE + LV_CONF_PATH`; `[wifi_sta]` block dead code with committed passwords; `LV_DRAW_SW_COMPLEX=1` with no acceleration; AsyncTCP priority not overridden. Seven P2: sdkconfig comment stale; debug env opt-level conflict; pio_pre.py absolute log path; no widget-allowlist comment in lv_conf; coredump 64KB undocumented; LV_CACHE_DEF_SIZE 0; `LV_INDEV_DEF_READ_PERIOD` not tuned.

### 7.V — LVGL widget tree (agent a4ede5402de6b1671)

Compliance with the 12 anti-patterns in `lvgl-component-reference.md`: **9 of 12 fully clean**, 2 deliberately deviated (LoadingScreen and NetworkConfigScreen — anti-pattern #2). One clean exception (LoadingScreen — pre-boot, intentional). Three P0: `showNetworkConfigScreen` creates 5th screen never deleted on nav (see P0-11); `lv_switch_create` with `LV_USE_SWITCH=0` (see P0-12); ConnectivityTab dialog save/cancel unverified (needs textarea→field copy check). Six P1: LoadingScreen create/destroy intentional exception; ConnectivityTab styles never `lv_style_reset`-ed; `refreshNetworkLists` widget thrash; statics inside `loop()`; screen-destroy order in ~DisplayUI; LoadingScreen dots_timer race. Three P2: `LV_FONT_DEFAULT` to disabled font; `ConnectivityTab::ct_make_card` diverges from shared `make_card` (visual inconsistency); ref.md templates mix `lv_btn_create` + CLICKABLE variants. Exemplary: SidebarWidget, ZoneComposerUI destructor-comment, `lvgl_bridge.cpp` flush impl with WDT + 250ms warning. Threading-by-convention: single `LVGLBridge::update()` caller (main.cpp:2404) — consider `configASSERT(xPortGetCoreID() == 1)` to enforce.

### 7.W — Touch input pipeline (agent a2acb4b0c88796a85)

Two P0: dual-dispatch action-row ghost zone (see P0-10 — phantom `handleActionButton` from legacy y-band test while LVGL path also fires); `M5.Touch.getDetail` read twice per loop, dual ownership of indev snapshot. One P1: debounce asymmetric event-timer semantic. Two P2: `ACTION_BUTTON_W=320` divides 1280 into 4 but live UI has 6 buttons; `ClickDetector` is for encoder buttons not touch. Anti-findings: no ISR-based touch (polled); no `LV_SCR_LOAD_ANIM` (atomic screen swaps); `OrientationManager` dormant (never rotated at runtime); header touch pass-through correctly uses `EVENT_BUBBLE`; multi-touch not consumed. Screen-gate (main.cpp:2292 — "if `UIScreen::GLOBAL` proceed else skip TouchHandler") correctly limits the legacy pipeline.

### 7.X — Screen FSM / navigation (agent af4e8dfdc66883687)

Three P0: `ConnectivityTab::loop()` never invoked in LVGL build (see P0-05, highest-value single finding); `showNetworkConfigScreen` unreachable modal (see P0-11); `_screen_*` null-deref risk if begin() fails partway. Five P1: transition-during-transition has no guard; sidebar tab state desynchronises on Back; `_currentScreen` bypass by modal corrupts invariants; WiFi/WS loss has no UI fallback; Control Surface `onScreenEnter` no error path. Five P2: OrientationManager dead (see dead-code); PresetBankWidget orphan; `.bak` files noise; destructor delete order subtle; sidebar tap `ZONES` early-return skips `applyTabColour`. Anti-findings: no back-stack needed (two-level UI); no modal stacking; no race in `_currentScreen` (single-task); no LVGL animation race.

### 7.Y — K1 protocol client (agent a721bb2df9356dc11)

Two P0: `zone.setBrightness`/`setBlend` queued fallback sends `doc["value"]` instead of contract field (see P0-14); REST DELETE `/api/v1/network/networks/{ssid}` not in contract. Five P1: no app-level ping/pong (socket error takes >30s to surface); `HttpClient::_discoveryResult` cached forever (no invalidation); no incoming-frame size guard (zones.list >2KB could exceed ArduinoJson pool); reconnect backoff good but double-raise on ERROR then DISCONNECTED; discovery subnet scan 254×300ms tight on 6KB stack. Seven P2: `zone.status` handler dead (K1 never emits it); `JSON_BUFFER_SIZE=256` small; truncation silently drops without VERBOSE_DEBUG; queue stale-drop 500ms could LIFO-pop latest; auth token bypass (K1 AP open today); no stale-command rejection (fire-and-forget); `WStype_BIN` handler empty confirms JSON-only. Anti-findings: WS command field names match contract (all verified against WsZonesCodec ALLOWED_KEYS); `effectPresets.saveCurrent` correctly uses `slot` not `id`; exponential backoff 1-30s per fsm-reference spec; mutex-guarded `sendJSON`.

### 7.Z — Boot + OTA + power (agent a06fd5f161e5ebdac)

Six P0: no rollback validation (see P0-02); no SHA256/MD5 (see P0-03); OTA session wedge on client disconnect (see P0-04); chunked upload no WDT feed (see class E cross-ref); no crash-loop detector (see P0-18); Wire1 stuck-bus could WDT during setup. Six P1: no backlight/power management (P1-26); OTA progress never surfaces to UI (P1-23); OTA token compile-time plaintext (P1-24); no WDT feed in AsyncTCP lambdas; DisplayUI new before NVS init no null check; watchdog only covers loopTask (P1-25). Six P2: 900-line `setup()` has no numbered phase markers; `WsMessageRouter::init` + inline lambda dual dispatch; no first-boot provisioning; `.bak` files (see dead-code); `FIRMWARE_VERSION="1.0.0"` hardcoded; `cleanup()` unreferenced. Highest-leverage top-3 fixes: `esp_ota_mark_app_valid_cancel_rollback()` (~15 LOC, single highest-value), OTA session timeout (~20 LOC), `RTC_NOINIT_ATTR` crash-loop gate (~30 LOC).

### 7.DD — Discovery + DeviceRegistry (agent afbf7bebb6b7fa147)

Four P0: discovery task leak on re-entry handle-before-create race (ESP-IDF footgun pattern at HttpClient.cpp:100); discovery task WDT-unsubscribed during 75s subnet sweep (same as E P0-1); discovery task blocks resolve path on cancel-race with MDNS I/O (potential mDNS IDF state leak); no DeviceRegistry / no active-device switch mechanism exists. Four P1: two parallel discovery strategies don't share state (P1-06); `shouldUseManualIP()` / `isMDNSTimeoutExceeded()` hardcoded false (P1-07); no device-dropped detection except WiFi loss (P1-08); ConnectivityTab "FAILED after discovery" UI feedback silent. Three P2: device identity entirely IP-based (no MAC/serial probe); subnet sweep expensive and pollutes LAN; hardcoded `MDNS_FALLBACK_IP_PRIMARY` meaningless for non-192.168.1.0/24 LANs. **DeviceRegistry / DeviceSelectorTab referenced in claude-mem memories #23438/#23446/#23485/#23508 but zero code presence** — confirmed via exhaustive grep and glob. Either abandoned or unmerged branch. Memory entries are stale.

---

## Part 8 — Glossary & navigational aids

### Key files by subsystem

| Subsystem | File |
|---|---|
| Entry point | `src/main.cpp` (3,226 lines) |
| LVGL bridge | `src/ui/lvgl_bridge.{cpp,h}`, `src/ui/lv_conf.h` |
| Display UI | `src/ui/DisplayUI.{cpp,h}` (2,647 lines .cpp) |
| Screens | `src/ui/ConnectivityTab`, `ZoneComposerUI`, `ControlSurfaceUI`, `LoadingScreen` |
| Widgets | `src/ui/widgets/*.{cpp,h}`, `src/ui/SidebarWidget.{cpp,h}` |
| WS client | `src/network/WebSocketClient.{cpp,h}`, `src/network/WsMessageRouter.h` |
| REST client | `src/network/HttpClient.{cpp,h}` |
| WiFi STA | `src/network/WiFiManager.{cpp,h}`, `src/network/WiFiAntenna.{cpp,h}` |
| OTA server | `src/network/OtaHandler.{cpp,h}` |
| Input | `src/input/TouchHandler`, `DualEncoderService`, `ButtonHandler`, `ClickDetector`, `I2CRecovery`, `CoarseModeManager` |
| Storage | `src/storage/NvsStorage`, `PresetStorage`, `PresetData.h` |
| Presets | `src/presets/PresetManager.{cpp,h}` |
| Parameters | `src/parameters/ParameterHandler`, `ParameterMap` |
| Config | `src/config/Config.h`, `network_config.h`, `PaletteLedData.h` |
| HAL | `src/hal/EspHal.{cpp,h}` (SimHal is dead code) |
| Hardware | `src/hardware/OrientationManager.{cpp,h}` (dead code) |

### Reference files for the next agent

Read on session start (pre-extracted context, saves ~20K tokens of re-discovery):

- `tab5-encoder/docs/reference/codebase-map.md`
- `tab5-encoder/docs/reference/fsm-reference.md`
- `tab5-encoder/docs/reference/lvgl-component-reference.md` (MANDATORY before any `src/ui/` edit)
- `CLAUDE.md` (root) + `tab5-encoder/CLAUDE.md` + `tab5-encoder/src/CLAUDE.md` + `tab5-encoder/src/ui/CLAUDE.md` + `tab5-encoder/src/input/CLAUDE.md`
- `docs/WORKFLOW_ROUTING.md`
- `docs/protocol/k1-ws-contract.yaml` + `docs/protocol/k1-rest-contract.yaml`
- Sister audit: `firmware-v3/docs/forensic-audit-2026-04-17.md`

### MCP tool routing (enforced — see root CLAUDE.md)

- **clangd first** for C++ symbols. Never grep for C++ symbol names.
- **QMD** for docs search across 1,459 markdown files.
- **Context7** for external library APIs (LVGL, ArduinoJson 7, links2004/WebSockets, ESP-IDF, AsyncTCP, M5Unified, M5GFX).
- grep only for text literals, config constants, comments.

### Commands reference

```bash
# Build:
cd tab5-encoder
pio run -e tab5

# Debug build (-Og -g3 -ggdb):
pio run -e tab5_debug

# Flash:
pio run -e tab5 -t upload --upload-port /dev/cu.usbmodem1101

# Monitor:
pio device monitor -b 115200

# Typical test sequence (manual, no automation):
# 1. Power up — confirm GLOBAL screen renders
# 2. Tap header net-container — confirm CONNECTIVITY opens
# 3. (post P0-05 fix) Tap SCAN — confirm networks populate within 15s
# 4. Back to GLOBAL — tap mode buttons — confirm correct action fires
# 5. Spin 2+ encoders for 60s — confirm no UI stutter
# 6. (post P0-04 fix) OTA upload with mid-upload disconnect — confirm recovery
# 7. 4-hour soak with K1 traffic — confirm heap stable + no reboots
```

### Sister audit cross-references

Many tab5 findings are the same class of bug as firmware-v3. Port the fix idioms directly:

| tab5 finding | firmware-v3 analogue | Fix pattern |
|---|---|---|
| P0-01 (AsyncTCP priority) | firmware-v3 P1-01 | Same 3 build_flags |
| P0-04 (OTA session wedge) | firmware-v3 P0-02 | Max-latch timer mirror |
| P0-08 (cross-core volatile) | firmware-v3 P1-14 | volatile → `std::atomic` with release/acquire |
| P0-09 (`lw_discovery` WDT) | firmware-v3 P0-07 | `esp_task_wdt_add(NULL)` at task entry + liveness-correlated feed |
| P0-13/14/15 (silent drops) | firmware-v3 P1-07 | Propagate bool return, surface to UI |
| P0-17 (log flood) | firmware-v3 P1-11 | `setTxBufferSize` + `setTxTimeoutMs` + rate-limit hot-path logs |
| P0-19/20 (millis deadline wrap) | firmware-v3 P0-04 | Elapsed-subtraction wrap-safe idiom |

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-18 | agent:opus-4.7-1M (session by captain:elroy) | Created. 22 SSA findings consolidated across all hazard classes. Next-agent handover complete. Cross-referenced to firmware-v3 sister audit (2026-04-17). All SSA agent IDs preserved for resumption. |
