---
abstract: "Handover brief for the next agent resuming Track B Wave 2 of the tab5-encoder forensic-audit remediation. Wave 2 closes three OTA P0 ship-blockers (rollback mark-valid, SHA256 integrity verification, session-timeout wedge recovery) entirely within tab5-encoder/src/network/OtaHandler.{cpp,h}. Includes file:line references, the firmware-v3 sister-fix idioms to port verbatim, the canonical flash procedure (PIO upload is broken on tab5 — use direct esptool), post-session context, scar tissue from 2026-04-18, and the exact verification protocol. Read before touching any OTA code."
type: wave-handover
version: 1.0
project: tab5-encoder
wave: Track B Wave 2 — OTA hardening
prior_session_date: 2026-04-18
author_of_prior: agent:opus-4.7-1M (session by captain:elroy)
blocks_closed_by_wave_2: P0-02, P0-03, P0-04 (all in OtaHandler.cpp)
target_env: tab5 (ESP32-P4 + pioarduino 54.03.21 + Arduino-core 3.2.1 + LVGL 9.3)
---

# Wave 2 Handover — Tab5 OTA Hardening

**Audience:** the next agent or engineer picking up Track B Wave 2.
**Purpose:** full context transfer. You should be able to apply, build, flash, and validate Wave 2 without re-discovering anything from the 2026-04-18 session.

---

## 1. Session state at handover

### 1.1 What landed this session (all pushed to origin/main)

| Commit | Scope |
|---|---|
| `4615978b` | **tab5 Wave 1** — CDC TX buffer (`setTxBufferSize(4096)/setTxTimeoutMs(20)` before `Serial.begin`), `[Param]` log throttle 10 Hz, `LV_DEF_REFR_PERIOD` rename (LVGL 8→9), effect/palette header fallbacks (`#0x<id>` / `Palette <n>`), CHANGELOG created |
| `5bf12d91` | **K1 Round 1 (SSA-A)** — WS heap-shed `closeAll(1013)` removal + threshold widen 20→22 KB shed, 26→32 KB resume, 500 ms post-clear grace |
| `d5cd99a8` | **K1 Round 2 (SSA-C/D/E)** — AsyncTCP priority 3/core 1, extracted `shouldDeferTextAll()` helper + 8 new gate sites, overlap-guard `STALE_ACTIVE_RECOVERY_MS=5000` + IP-map eviction decrement |
| `379467b2` | **P1-09 close-out** — 14 AR effects + 4 TRM family migrated to per-zone `[kMaxZones]` state, SPIRAM instrumentation, EdgeMixer mode widen to 8 |

### 1.2 What is NOT landed (Wave 2 targets — this document)

Three P0 findings in `tab5-encoder/src/network/OtaHandler.{cpp,h}`:

- **P0-02** — OTA has no partition rollback validation. A bad image that boots long enough to clear setup() but crashes later is permanently wedged (no `esp_ota_mark_app_valid_cancel_rollback` anywhere in the tree).
- **P0-03** — OTA has no SHA256/MD5 integrity verification. Corrupted upload (bit flip, truncated TCP, malicious replay) reaches `Update.end(true)` with zero integrity check. OTA token `OTA_UPDATE_TOKEN` is a compile-time `#define` in `network_config.h:136` — recoverable from any firmware dump.
- **P0-04** — OTA session wedges permanently on client disconnect. `s_updateStarted` latches true; no path clears it if the client drops mid-upload. Next upload fails (`Update.begin()` returns false on active session) until cold reboot.

### 1.3 What is explicitly deferred (NOT Wave 2 scope)

- **Tab5 I2C dual-init KNOWN ISSUE** (`M5.Ex_I2C.begin()` + `Wire.begin()` on ESP-IDF 5.4 `i2c_master`). Currently restored with KNOWN ISSUE comment because removing `M5.Ex_I2C.begin()` eliminated the error storm BUT broke encoder polling entirely. Needs its own diagnostic session, not part of Wave 2.
- **P0-05** ConnectivityTab::loop() not wired in LVGL build — audit's highest-value single finding, but separate scope (UI subsystem, different files).
- **P0-06 through P0-20** (17 more P0s in the audit) — future waves.
- **K1 SSA-B frame reassembly** — deferred in Round 2 analysis.

---

## 2. Wave 2 scope — exact file:line and fix pattern

### 2.1 Finding inventory (all in `tab5-encoder/src/network/OtaHandler.cpp` and `.h`)

#### P0-02 — Rollback mark-valid missing

**Symptom:** tab5 has a dual-OTA partition layout (`partitions_tab5.csv` shows two 6.25 MB app slots). Arduino-ESP32 boots the new app after OTA but does NOT automatically mark it valid. If the new app crashes during or shortly after `setup()`, the ESP-IDF rollback mechanism should boot the previous slot — but only if the app explicitly calls `esp_ota_mark_app_valid_cancel_rollback()`. Tab5 has zero calls to this API anywhere in the tree (confirmed by session grep).

**Result:** a bad OTA that boots through `setup()` but crashes later leaves the device wedged on the broken image. Manual USB re-flash required.

**Reference implementation to port:** `firmware-v3/src/main.cpp` has a `markOtaValidIfNeeded()` pattern that calls `esp_ota_get_state_partition` + `esp_ota_mark_app_valid_cancel_rollback` after N seconds of clean uptime. Port the same idiom to tab5.

**Proposed fix sketch:**
```cpp
// In tab5-encoder/src/main.cpp or OtaHandler.cpp — called from setup() tail OR
// from a post-boot-soak timer after ~30 s of loop() without crash.
#include <esp_ota_ops.h>

static void markOtaValidIfStable() {
    static bool s_marked = false;
    static uint32_t s_bootMs = 0;
    if (s_marked) return;
    if (s_bootMs == 0) { s_bootMs = millis(); return; }
    if ((millis() - s_bootMs) < 30000) return;  // 30 s soak
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
        Serial.println("[OTA] Current image marked valid — rollback cancelled");
    }
    s_marked = true;
}
```

Call `markOtaValidIfStable()` from `loop()` or from a one-shot `esp_timer` 30 s after boot. The `ESP_OTA_IMG_PENDING_VERIFY` state is set automatically by the bootloader for the FIRST boot of a freshly-flashed image; if the app doesn't mark it valid, the next reset triggers rollback.

**Scope boundary:** ~15 LOC in `main.cpp` or `OtaHandler.cpp`. Do NOT touch the partition table.

#### P0-03 — SHA256 integrity verification

**Symptom:** `OtaHandler.cpp:173` calls `Update.begin(s_updateTotal, U_FLASH)` without a preceding `Update.setMD5()`. `Update.end(true)` then writes the partition-select field based on write-count parity only, with zero content integrity check. Combined with P0-02 (no rollback), a corrupted upload boots and the device can't recover.

**Proposed fix sketch:**

1. **Client-side (iOS/dashboard/Tab5-debug):** compute SHA256 of the firmware.bin on the uploader, include it in a new WS frame header or HTTP header `X-Checksum: <hex>`.
2. **Server-side (tab5 OtaHandler):** read the header before `Update.begin()`, call `Update.setMD5(hash)` (actually SHA256 is preferred but Arduino `Update` only supports MD5 natively — use the MD5 API with the SHA256 string or switch to manual verification after `Update.end()`).

A cleaner pattern: compute SHA256 of the received bytes during upload chunks, compare to the header value at session close BEFORE calling `Update.end(true)`. If mismatch, call `Update.abort()` and reset session state.

```cpp
// Inside OtaHandler.cpp upload chunk handler:
static mbedtls_sha256_context s_shaCtx;
static uint8_t s_expectedSha[32];

// On upload start (index == 0):
mbedtls_sha256_init(&s_shaCtx);
mbedtls_sha256_starts_ret(&s_shaCtx, 0);  // 0 = SHA-256 not SHA-224
// Read X-Checksum header; hex-decode into s_expectedSha[32].

// On every chunk:
mbedtls_sha256_update_ret(&s_shaCtx, data, len);

// On final chunk (before Update.end(true)):
uint8_t computed[32];
mbedtls_sha256_finish_ret(&s_shaCtx, computed);
if (memcmp(computed, s_expectedSha, 32) != 0) {
    Serial.println("[OTA] SHA256 mismatch — aborting");
    Update.abort();
    // clear s_updateStarted, reset session state
    return;
}
// else proceed to Update.end(true)
```

**Token security sidebar (P1-24, not Wave 2 scope but note it):** `OTA_UPDATE_TOKEN` at `tab5-encoder/src/config/network_config.h:136` is a plaintext `#define`. Recoverable from any firmware dump. Move to NVS-stored, user-settable, per-device-unique token is a P1 follow-up, not part of Wave 2.

#### P0-04 — Session timeout wedge recovery

**Symptom:** `OtaHandler.cpp:156` sets `s_updateStarted = true` on first chunk; no code path clears it if the WS OTA client drops TCP mid-upload (half-open socket, no FIN received). `Update` library holds the app partition open. Next upload attempt fails with "session already active". Cold reboot required.

**Reference implementation to port:** `firmware-v3` has a `OtaSessionLock` watchdog pattern (committed within the forensic-audit stability hardening). Tab5 version should be simpler: a single `s_updateStartedAtMs` timestamp + a `loop()` check that aborts stale sessions.

**Proposed fix sketch:**

```cpp
// tab5-encoder/src/network/OtaHandler.cpp — add near s_updateStarted:
static uint32_t s_updateStartedAtMs = 0;
static constexpr uint32_t OTA_SESSION_TIMEOUT_MS = 30000;  // 30 s

// In handleUpload(index==0) where s_updateStarted is set:
s_updateStarted = true;
s_updateStartedAtMs = millis();

// Add a new OtaHandler::loop() method (or call from main.cpp loop()):
void OtaHandler::loop() {
    if (!s_updateStarted) return;
    const uint32_t elapsed = millis() - s_updateStartedAtMs;
    if (elapsed > OTA_SESSION_TIMEOUT_MS) {
        Serial.printf("[OTA] Session timeout (%lu ms) — aborting\n",
                      static_cast<unsigned long>(elapsed));
        Update.abort();
        s_updateStarted = false;
        s_updateStartedAtMs = 0;
    }
}
```

Wire `OtaHandler::loop()` into the main `loop()` path. Simple.

**Edge case:** a legitimate very-slow-client upload that takes >30 s must be handled. 30 s is arbitrary — consider 60 s, or reset the timestamp on every received chunk (`s_updateStartedAtMs = millis()` in the chunk handler) so the timeout is "time since last data" not "time since session start".

### 2.2 Files touched (exactly 2, maximum 3)

- `tab5-encoder/src/network/OtaHandler.h` — add `loop()` method signature; declare `s_updateStartedAtMs`, SHA context statics. Possibly add `markOtaValidIfStable()` forward declaration if placed here.
- `tab5-encoder/src/network/OtaHandler.cpp` — all three fix implementations.
- *(Optional 3rd file)* `tab5-encoder/src/main.cpp` — wire `OtaHandler::loop()` into the main loop; wire `markOtaValidIfStable()` call into setup-tail or a post-boot timer.

Do NOT touch any UI file, any network file other than OtaHandler, the protocol contract YAML, or the tab5 partition table.

---

## 3. Hard constraints (read-back required before first edit)

- **British English** in every new comment, log string, doc line. `centre` not `center`, `colour` not `color`, `initialise` not `initialize`, `behaviour` not `behavior`.
- **Tab5 is WiFi STA** (client of K1's AP at 192.168.4.1). Do NOT invert to AP mode.
- **No heap alloc in LVGL render tick.** Wave 2 doesn't touch the render path, but confirm no OTA callback fires during render.
- **clangd first** for any C++ symbol navigation (`esp_ota_*`, `Update.*`, `mbedtls_sha256_*`). Grep only for text literals and config constants.
- **`compile_commands.json`** lives in `tab5-encoder/` after `pio run -t compiledb` — refresh it after Wave 2 edits land.

---

## 4. Canonical flash procedure (critical — PIO upload is BROKEN on tab5 for custom partitions)

### 4.1 Why PIO upload is broken

`tab5-encoder/platformio.ini` pins `board_build.partitions = partitions_tab5.csv`. Tab5's partition layout is the Arduino-ESP32 default (app0 at `0x10000`, otadata at `0xe000`) — **so PIO upload actually works for tab5**. This caveat applies to **K1v2** (firmware-v3 with grown NVS layout), NOT tab5. For tab5 you can use `pio run -t upload` normally.

### 4.2 Tab5 flash — standard PIO upload

```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/tab5-encoder
pio run -e tab5
pio run -e tab5 -t upload --upload-port /dev/cu.usbmodem1101
```

**Port discovery:** tab5 enumerates as `/dev/cu.usbmodem1101` or `/dev/cu.usbmodem2101` depending on USB re-enumeration state. Check with `ls /dev/cu.usbmodem*` before the upload command. If both exist, the K1v2 is also connected — tab5 is typically on 1101, K1v2 on 2101 (but verify MAC before flash — tab5 MAC is `30:ed:a0:e0:c1:a0`, K1v2 is `b4:3a:45:a5:87:f8`).

### 4.3 If PIO upload fails mid-write

Session scar (2026-04-18): PIO upload to K1v2 aborted at ~30 % with "Could not configure port: Device not configured" on default baud (460800). Dropping to 115200 via direct esptool succeeded. If tab5 ever exhibits the same symptom, the escape hatch is:

```bash
esptool.py --chip esp32p4 --port /dev/cu.usbmodem1101 --baud 115200 write-flash \
  0x0      .pio/build/tab5/bootloader.bin \
  0x8000   .pio/build/tab5/partitions.bin \
  0xe000   /Users/spectrasynq/.platformio/packages/framework-arduinoespressif-src-*/tools/partitions/boot_app0.bin \
  0x10000  .pio/build/tab5/firmware.bin
```

**Note the offsets are Arduino defaults** (`0xe000` otadata, `0x10000` app0) — tab5 does NOT have the grown-NVS layout that K1v2 has. Do NOT use the K1v2 offsets (`0x19000`, `0x20000`) on tab5.

### 4.4 MAC verification (mandatory before every flash)

```bash
esptool.py --port /dev/cu.usbmodem1101 read-mac
# Must show: 30:ed:a0:e0:c1:a0  (tab5 ESP32-P4)
# If it shows b4:3a:45:a5:87:f8, that's K1v2 — STOP and use correct port.
```

See `~/.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip/memory/feedback_verify_mac_before_flash.md`.

---

## 5. Build & verification protocol

### 5.1 Build

```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/tab5-encoder
pio run -e tab5
```

Expected: exit 0, firmware.bin ~2.3 MB in `.pio/build/tab5/`. RAM ~3.3 %, Flash ~33 %. Wave 2 should add < 2 KB flash (mbedtls SHA already linked via WiFi stack) and < 200 B RAM.

### 5.2 Pre-flash grep gate

Before flashing, confirm Wave 2 changes are isolated to OtaHandler + main.cpp:

```bash
git diff --stat tab5-encoder/
# Should show exactly: OtaHandler.h, OtaHandler.cpp, (optionally) main.cpp, CHANGELOG.md
```

### 5.3 Hardware verification (tab5 has NO test infrastructure — manual soak)

After flash, run these tests in order:

**Test 1 — P0-02 rollback mark-valid basic:**
1. Flash Wave 2 firmware.
2. Boot. Wait 35 s (> 30 s soak window).
3. Serial should show `[OTA] Current image marked valid — rollback cancelled` exactly once.
4. Reset the device (USB re-plug). On boot, no rollback should fire — image stays current.

**Test 2 — P0-03 SHA256 verify:**
1. Compute SHA256 of a known-good firmware.bin on the host: `shasum -a 256 .pio/build/tab5/firmware.bin`.
2. Upload with correct SHA256 header — OTA must succeed.
3. Upload with **incorrect** SHA256 header — OTA must `abort()` and log `[OTA] SHA256 mismatch — aborting`.
4. Upload with a deliberately truncated firmware.bin with recomputed SHA256 of the truncated bytes — should succeed the integrity check (that's what integrity means) but the device should fail to boot (because the image is not valid ESP-IDF), triggering rollback from Test 1. This tests the rollback path end-to-end.

**Test 3 — P0-04 session timeout recovery:**
1. Start an OTA upload; immediately abort the client mid-upload (Ctrl+C or drop TCP).
2. Wait 35 s.
3. Serial should show `[OTA] Session timeout (>30000 ms) — aborting`.
4. Start a new OTA upload immediately — must succeed (not "session already active").

**Test 4 — Regression sanity:**
1. Full OTA upload at normal speed — must succeed with SHA256 verify, no timeout trigger, no rollback.
2. Spin encoders for 60 s — no regression on Wave 1's CDC buffer / `[Param]` throttle / LVGL 60 Hz behaviour.
3. Confirm tab5 WS to K1 remains `WS:OK` throughout (don't introduce a WS regression via OTA work).

### 5.4 Serial port busy during flash (known annoyance)

If `pio run -t upload` reports "port busy":
1. Close any active `pio device monitor` / serial terminal / Cursor serial view.
2. Retry. If still busy: `lsof /dev/cu.usbmodem1101` to find the holder.

---

## 6. Commit format (mandatory)

Each Wave 2 fix lands as **one atomic commit** (or bundled commit if the three fixes are tightly coupled). Commit messages must:

- Start with `fix(tab5): ...` scope prefix (Keep-a-Changelog friendly).
- Include the forensic-audit P0 number(s) in the title or body (`P0-02`, `P0-03`, `P0-04`).
- End with `Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>` (the trailer is mandatory per CLAUDE.md).

Example:

```
fix(tab5): Wave 2 OTA hardening — rollback + SHA256 + session timeout

Closes forensic-audit-2026-04-18 P0-02, P0-03, P0-04.

[body with details]

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
```

Also update `tab5-encoder/CHANGELOG.md` under `## [Unreleased]` → `### Fixed` with one line per fix.

---

## 7. Scar tissue from 2026-04-18 — what NOT to do

### 7.1 Do NOT add a hello-time WS burst to prime caches

Previous attempt (reverted) added `requestEffectsList` + `requestPalettesList` + `requestCurrentEffect` to `sendHelloMessage()`. The 6-frame WS burst triggered K1 to drop the TCP connection with "Connection reset by peer". If Wave 2 needs to signal K1 that OTA just completed, do NOT burst multiple WS frames — send one compact frame and wait.

### 7.2 Do NOT remove M5.Ex_I2C.begin() or reorder I2C init

Previous attempt (reverted) removed the redundant `M5.Ex_I2C.begin()` call hoping to eliminate the ESP-IDF 5.4 `i2c_master` dual-init error storm. It eliminated the storm but broke encoder polling entirely — M5Unified requires the call for reasons beyond raw driver handle ownership (not fully characterised; needs its own investigation). The call is restored with a KNOWN ISSUE comment in `tab5-encoder/src/main.cpp` around the Ex_I2C init block. Wave 2 does not touch I2C.

### 7.3 Do NOT touch K1 (firmware-v3) in Wave 2

Wave 2 is **tab5-only**. The K1 WS server accepts whatever integrity header tab5 sends. If Wave 2 introduces a new WS frame type for checksum delivery, extend the protocol contract (`docs/protocol/k1-ws-contract.yaml`) and the K1 handler in a SEPARATE commit, not bundled into the tab5 Wave 2 commit.

### 7.4 Do NOT skip the MAC verification

Workspace feedback memory `feedback_verify_mac_before_flash.md` exists for a reason — wrong-device flash incident 2026-03-24. Always verify MAC before uploading, even if you're "sure" the port is right.

### 7.5 Do NOT trust AwsFrameInfo single-event assumptions

Session investigation on K1 found that AsyncWebServer emits `WS_EVT_DATA` in multi-event mode for TCP-split frames (Mode B in SSA-B analysis). If OTA protocol work on K1 side ever needs to handle large frames, the P1-16 fragmentation gate on K1 side rejects legitimate multi-event single-frame payloads — SSA-B reassembly fix is the proper solution but was deferred. This is not a Wave 2 concern for tab5 (tab5 receives no OTA frames; K1 doesn't OTA tab5 — OTA is from uploader client to tab5).

---

## 8. Reference documents (read in this order before first edit)

1. `tab5-encoder/docs/forensic-audit-2026-04-18.md` §P0-02, §P0-03, §P0-04 — the findings themselves.
2. `tab5-encoder/docs/reference/codebase-map.md` — directory structure.
3. `tab5-encoder/docs/reference/fsm-reference.md` — if OTA touches any state machine.
4. `firmware-v3/src/main.cpp` `markOtaValidIfNeeded()` — rollback mark-valid reference pattern (grep for it).
5. `firmware-v3/src/network/webserver/ws/WsOtaCommands.cpp` + `WsOtaCodec.cpp` — K1's SHA256 OTA pattern for reference (already landed in commit `5e37daa0` P1-10).
6. `CLAUDE.md` (root) + `tab5-encoder/CLAUDE.md` + `tab5-encoder/src/network/CLAUDE.md` — hard constraints.
7. `docs/WORKFLOW_ROUTING.md` — tool routing.
8. `~/.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip/memory/feedback_k1v2_flash_procedure.md` — flash procedure (not directly relevant to tab5 since its partition layout is default, but the MAC-verify and baud-fallback rules apply universally).

---

## 9. Token & time budget

- Reading scope: ~10 K tokens (this doc + OtaHandler.{cpp,h} + firmware-v3 references).
- Editing: ~3 K tokens (three focused fixes in one file).
- Build + flash + verify: ~5 K tokens (serial captures, MAC verify, test output).
- Total Wave 2: ~20 K tokens, ~1 hour wall-clock.

---

## 10. Completion criteria

Wave 2 is done when:

1. `tab5-encoder/src/network/OtaHandler.cpp` contains the three fixes and compiles with `pio run -e tab5` cleanly (no new warnings beyond pre-existing LVGL 9 enum warnings).
2. All four hardware tests in §5.3 pass on the actual tab5 device (MAC-verified).
3. One or two commits landed with the mandatory trailer and `P0-02/03/04` references.
4. `tab5-encoder/CHANGELOG.md` `[Unreleased]` → `Fixed` updated with three lines.
5. This handover doc's §1 table extended with a new row for the Wave 2 commit (optional — document evolution).
6. (Optional) A short post-Wave-2 note appended here summarising what the implementation actually looked like, for the agent picking up Wave 3.

---

## 11. When Wave 2 is done — what's next (Wave 3 hint)

Wave 3 candidates ranked:

1. **P0-05 — ConnectivityTab::loop() wire-up in LVGL build.** Audit's highest-value single finding. Two-line fix in `tab5-encoder/src/ui/DisplayUI.cpp:1466`. Closes the "SCAN results never surface" UI bug. Low risk.
2. **P0-10 — Phantom `handleActionButton` from legacy TouchHandler.** Remove the `g_touchHandler.onActionButton(...)` registration entirely; LVGL now owns action buttons. Eliminates spurious mid-screen tap dispatches.
3. **P0-11 + P0-12 — Delete `showNetworkConfigScreen` entirely.** Unreachable stuck modal with no Save/Cancel callbacks and `lv_switch_create` against `LV_USE_SWITCH=0`. Pure deletion.
4. **P0-13 through P0-16 — WS command correctness** (rate-limit drops, contract mismatches, silent preset failures, `_sendDegraded` latch). Cluster these into one "WS command hardening" wave.
5. **Tab5 I2C dual-init root fix** (KNOWN ISSUE). This is a research task, not a mechanical fix — needs a proper dive into M5Unified's `I2C_Class::begin()` behaviour on ESP-IDF 5.4 and whether `end()` between the two `begin()` calls is the right approach.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-18 | agent:opus-4.7-1M (session by captain:elroy) | Created. Full Wave 2 handover produced at session close after Wave 1 + K1 Round 1/2 + P1-09 close-out landed. |
