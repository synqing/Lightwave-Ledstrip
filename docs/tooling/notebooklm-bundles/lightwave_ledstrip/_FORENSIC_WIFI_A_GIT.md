---
abstract: Forensic git code-history excavation for K1 WiFi-mode evolution. Chronological commit timeline + key files traced + identification of 'AP-only-EVER' hardening moment + ESP-IDF version context. Captain-directed evidence for strategic round-table on dual-mode reality.
---

# Forensic Audit A — K1 WiFi-Mode Git History

**Scope:** Pure git code-history archaeology. Read-only. Surfaces evidence; makes no recommendations. Cite-or-don't-claim discipline applied to every assertion.

**Repo head used:** branch `feature/heap-stability-day1`, working dir `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip` at 2026-05-04.

**Search method:** `git log --all -G"WIFI_MODE_|esp_wifi_set_mode|WiFi.mode|WiFi.softAP|WiFi.begin"` plus path-based `git log -- "*WiFi*"`. 50 commits matched the content regex; deeper inspection of the WiFiManager.cpp creation point yields a fuller picture extending back to 2025-12-20.

---

## 1. Chronological commit timeline

Material WiFi-mode-touching commits, ordered oldest → newest. **Direction** = `→STA` (toward STA capability), `→AP` (toward AP-only), `=both` (preserves both), `dev` (Tab5/dev-side, not K1 firmware).

| # | Hash | Date | Author | Subject (truncated) | Direction | What changed |
|---|------|------|--------|---------------------|-----------|--------------|
| 1 | `7700aba2` | 2025-12-20 | SpectraSynq | feat(v2): Add network layer with WiFiManager and REST/WebSocket API | `=both` | **GROUND ZERO.** WiFiManager.cpp:`WiFi.mode(WIFI_MODE_APSTA)` "for maximum flexibility". AP_MODE listed as state #4 — "Soft-AP fallback mode" after STA reconnect-attempt budget exhausted (max 10 attempts, exponential backoff). |
| 2 | `c4e8b867` | 2026-01-11 | SpectraSynq | feat(network): Comprehensive API and WebSocket improvements | `→STA` | Adds WiFiCredentialsStorage (NVS multi-network), log streaming, color/filesystem handlers. WiFi remains APSTA per (1). |
| 3 | `16fae6ec` | 2026-01-24 | SpectraSynq | feat(network): add WiFiCredentialManager with NVS storage | `→STA` | "Maximum 8 saved networks", XOR obfuscation by MAC, mutex-protected. **Strong evidence the original spec assumed STA-to-router was the primary mode.** |
| 4 | `5cfb688d` | 2026-01-24 | SpectraSynq | feat(wifi): Implement smart network selection from all credential sources | `→STA` | `findBestAvailableNetwork()` ranks scan results across credential pool by RSSI with last-connected boost. |
| 5 | `91d0ec66` | 2026-01-28 | SpectraSynq | refactor(network): centralize v2 network configuration constants | `=both` | Centralises AP password/SSID/timing into `network_config.h`. Default AP password set to `'spectrasynq'`. |
| 6 | `6ce6143e` | 2026-01-28 | SpectraSynq | feat(wifi): add AP-mode fallback after repeated scan failures | `→AP fallback` | **Captain's "AP MUST be possible" requirement, in code.** "When WiFi scanning finds no known networks after 2 consecutive attempts, automatically switch to AP mode for device accessibility." Counter `m_scanAttemptsWithoutKnown`, reset on successful match. STA primary, AP fallback. |
| 7 | `07aa83a9` | 2026-01-28 | SpectraSynq | chore: remove esp32dev_wifi build environment and all references | dev | Renames build env. |
| 8 | `1d1589c6` | 2026-02-05 | SpectraSynq | net: implement Portable Mode - v2 is always the network | `→AP-primary` | **Pivot 1 → "Portable Mode".** Commit message: "v2 now runs AP+STA concurrently so LightwaveOS-AP stays up permanently at 192.168.4.1. Tab5 targets the AP as primary WiFi with deterministic IP." `WIFI_MODE_APSTA`, non-destructive STA retry from AP mode, stub `connectToNetwork`/`requestSTAEnable`/`requestAPOnly` introduced. AP becomes the canonical Tab5/iOS connection path; STA continues in parallel. |
| 9 | `0b270a48` | 2026-02-05 | SpectraSynq | net: stabilise AP+STA and esv11 heap | `=both` | "Skip forced STA reconnect when WiFi runs in AP+STA mode (keeps AP clients stable)." First explicit acknowledgement that **STA scanning destabilises AP clients in concurrent mode.** |
| 10 | `c3f1b426` | 2026-02-06 | SpectraSynq | fix(net): make LightwaveOS-AP open | `→AP-primary` | Drops AP password to open network. Tab5/iOS connection-sheet wording updated. |
| 11 | `9ef320fc` | 2026-02-06 | SpectraSynq | fix(wifi): skip STA scan loops on placeholder creds | `→AP-primary` | Adds guard so the unit doesn't loop on STA scans when only template/placeholder credentials are present. STA still alive, just throttled. |
| 12 | `197234ec` | 2026-02-06 | SpectraSynq | fix(net): reduce WS/UDP churn under low heap | `=both` | Network housekeeping; not a mode change. |
| 13 | `f309f67d` | 2026-02-10 | SpectraSynq | feat(effects): add 11 Shape Bangers effects + switch defa... | `=both` | Effect-side, but touches network paths. |
| 14 | `d13889f8` | 2026-02-17 | SpectraSynq | feat(network): WiFi AP-only and main.cpp integration | **`→AP-ONLY`** | **THE HARDENING MOMENT.** WiFiManager.cpp diff replaces `WIFI_MODE_APSTA` with `WIFI_MODE_AP`. New comment: "Boot into AP-only mode. STA is ONLY activated via serial `wifi connect`. This prevents STA scanning/reconnection loops from destabilising the AP, which is the PRIMARY connection path for Tab5 and iOS clients." Introduces `m_forceApOnly` runtime lock, set true at boot, only cleared by `requestSTAEnable()`. STA event handler now logs "STA disconnected (AP-only mode active)" and stays in `STATE_WIFI_AP_MODE`. |
| 15 | `283c9da3` | 2026-02-17 | SpectraSynq | fix(network): WebServer AP disconnect and beat lifecycle | `→AP-ONLY` | Hardens AP disconnect behaviour for the AP-only path landed in (14). |
| 16 | `afc1e982` | 2026-02-17 | SpectraSynq | feat(Tab5): WiFi antenna, WS client, zone composer UI | dev (Tab5) | Tab5 side; Tab5 connects to K1's AP as STA. |
| 17 | `5ee8aa84` | 2026-02-27 | SpectraSynq | refactor: complete repository restructure (Phases 0-7) | `→AP-ONLY` | `firmware/v2/` moved to `firmware-v3/`. `WiFiManager.cpp` carries the AP-only design from (14). Forensic-audit doc later flags this as "the proven working path". |
| 18 | `d943101a` | 2026-02-27 | SpectraSynq | feat(firmware): integrate 95 commits from fix/stable-effe... | `=AP-ONLY` | 95-commit overlay merge. Brings ESV11 + PipelineCore into firmware-v3. WiFi remains AP-only. |
| 19 | `54765b16` | 2026-03-04 | SpectraSynq | build(platformio): add WIFI_AP_ONLY flag, override template, gitignore updates | **`→AP-ONLY` (build-level)** | Adds `-D WIFI_AP_ONLY=1` to K1v2 build flags. Comment in commit: "enforces AP-only WiFi arch". This formalises the architectural constraint into the build system. |
| 20 | `c3cad71a` | 2026-03-04 | SpectraSynq | feat(tab5): add 16-bit effect IDs, WiFi antenna toggle | dev | Tab5 antenna selection. |
| 21 | `a39a03b8` | 2026-03-04 | SpectraSynq | feat(network): add stimulus override REST/WebSocket API | `=AP-ONLY` | Stimulus override API; uses existing AP. |
| 22 | `d14e2696` | 2026-03-04 | SpectraSynq | perf(core): K1 runtime tuning — heap shedding, stack hyst..., AP fix | `→AP-ONLY` | `WiFiManager: Remove explicit WPA2 auth flags from softAP() — let ESP-IDF infer from password presence. Fixes K1 open network init.` |
| 23 | `032d1a5c` | 2026-03-04 | SpectraSynq | refactor(firmware): clean up main.cpp, harden renderer | `=AP-ONLY` | main.cpp clean-up; AP-only kept. |
| 24 | `5f0cef2f` | 2026-03-05 | SpectraSynq | build(platformio): add WIFI_AP_ONLY flag, override templa... | `→AP-ONLY` | Cherry-pick of (19) onto stage0 baseline. Stage 1 Pick 2. |
| 25 | `19b41ca1` | 2026-03-05 | SpectraSynq | perf(core): surgical K1 runtime tuning from d14e2696 | `=AP-ONLY` | Surgical re-application of (22). |
| 26 | `86abcb12` | 2026-03-05 | SpectraSynq | fix(network): WiFiManager AP auth fix from d14e2696 | `→AP-ONLY` | Re-applies the softAP() simplification. |
| 27 | `b34d4246` | 2026-03-05 | SpectraSynq | perf(network): heap-shedding telemetry and config from d1... | `=AP-ONLY` | Heap-shedding around AP path. |
| 28 | `9cc5f083` | 2026-03-05 | SpectraSynq | revert: back out WebServer heap-shedding telemetry (regre...) | `=AP-ONLY` | Reverts (27) due to regression. |
| 29 | `bccb48fe` | 2026-03-05 | SpectraSynq | refactor(network): WS diagnostics suppression, softAP simplification, configurable heap thresholds | `=AP-ONLY` | "Simplify softAP() re-init call (Arduino handles auth mode from passphrase)". |
| 30 | `701d2b22` | 2026-03-05 | SpectraSynq | ci(firmware): add hot-path safety guardrail script | `=AP-ONLY` | CI guardrails. |
| 31 | `99552461` | 2026-03-14 | (audit author) | Address HIGH audit findings: contract checker, CI matrix | `=AP-ONLY` | Audit response. |
| 32 | `dcb87777` | 2026-03-21 | SpectraSynq | refactor(firmware): Phase 4 — extract SystemInit, main.cpp | `=AP-ONLY` | main.cpp split. |
| 33 | `5beabd76` | 2026-03-21 | SpectraSynq | refactor(firmware): Phase 3 — extract SerialCLI (2,100 lines) | `=AP-ONLY` | Serial CLI split (the path that owns the `wifi connect` escape hatch). |
| 34 | `80787d8e` | 2026-03-24 | SpectraSynq | feat(tab5): sidebar widget, tabbed GLOBAL screen, WiFi antenna toggle | dev | Tab5-side. |
| 35 | `8134d4a4` | 2026-04-18 | (forensic audit) | fix(firmware): forensic-audit stability hardening (Wave 0/1/2 + root yield) | `=AP-ONLY` + dead-code call-out | Wave 1 includes "softAP retry + WiFi TWDT" hardening. The accompanying audit doc (`firmware-v3/docs/forensic-audit-2026-04-17.md:419`) explicitly flags **"~500 LOC of dead STA code in WiFiManager.cpp — all state handlers for STA scanning/connecting/disconnecting are still compiled-in despite WIFI_AP_ONLY=1. findBestAvailableNetwork(), scanNetworks, etc."** Track C remediation proposes "remove dead WiFi STA paths". |
| 36 | `b9c81e85` | 2026-04-30 | (Phase E author) | feat(effects/tools): Phase E audit + I-3 migration tooling | `=AP-ONLY` | Effect-side, touches network paths. |
| 37 | `56110887` | 2026-05-02 | (heap-stability author) | feat: heap-stability mitigation + WS inspector + critical iOS fixes + CI repair | `=AP-ONLY` | Heap-stability work. |
| 38 | `11e040d6` | 2026-05-03 | K1 Research Agent | chore(firmware): add K1v2 STA validation profile | **`→STA-RESURRECTION` (validation-only)** | **Most recent, explicitly walks back AP-only.** Adds `[env:esp32dev_audio_esv11_k1v2_32khz_sta_validation]` which `-UWIFI_AP_ONLY` and applies `${wifi_sta.build_flags}`. Commit body: "Validation: pio run … sta_validation PASS." This is the first commit since (14) that intentionally builds firmware capable of STA. Production default_envs unchanged. |

**Total material commits found:** 38 (50 raw matches when including dev-side and superseded duplicates).

---

## 2. Per-file history

### `firmware-v3/src/network/WiFiManager.cpp`/`.h`

`git log --follow` (firmware/v2 → firmware-v3 path move at 5ee8aa84):

- **Created** at `7700aba2` 2025-12-20 — `WiFi.mode(WIFI_MODE_APSTA)` "for maximum flexibility". STA-primary with 5-state machine (DISCONNECTED / CONNECTING / CONNECTED / AP_MODE / RECONNECTING). AP_MODE is the FALLBACK after STA exhausts 10 attempts.
- **Smart selection** at `5cfb688d` 2026-01-24 — `findBestAvailableNetwork()` introduced.
- **AP fallback wired** at `6ce6143e` 2026-01-28 — `m_scanAttemptsWithoutKnown` counter, falls back to AP after 2 consecutive failed scans.
- **APSTA hardening** at `1d1589c6` 2026-02-05 — Portable Mode; non-destructive STA retry from AP mode. Stubs `connectToNetwork`/`requestSTAEnable`/`requestAPOnly`.
- **APSTA reconnect skip** at `0b270a48` 2026-02-05 — explicit fix that STA forced reconnect destabilises AP clients.
- **AP-ONLY enforced** at `d13889f8` 2026-02-17 — `WiFi.mode(WIFI_MODE_AP)`, `m_forceApOnly = true`, all STA paths gated.
- **softAP auth simplification** at `d14e2696`/`bccb48fe`/`86abcb12` 2026-03-04 / 03-05 — drops explicit WPA2 enum.
- **Path move** at `5ee8aa84` 2026-02-27 — `firmware/v2/src/network/WiFiManager.cpp` → `firmware-v3/src/network/WiFiManager.cpp`.
- **softAP retry + WiFi TWDT** at `8134d4a4` 2026-04-18 — 3-attempt retry with reboot on exhaustion (current-source lines 89–127).
- **Current head:** 1,256 lines `.cpp` + 574 lines `.h`. Boot path forces `WIFI_MODE_AP` (line 83), `m_forceApOnly = true` (line 129). STA state handlers compile but are gated by `m_forceApOnly` checks at lines 261, 472, 569, 592, 569.

### `firmware-v3/src/network/WiFiCredentialManager.cpp`/`.h`

- **Created** at `16fae6ec` 2026-01-24 — 758 LOC across `.cpp`/`.h`. NVS-backed, MAC-XOR obfuscation, CRC32 integrity, mutex-protected, max-8 networks. **Specifically designed to support STA reconnection across multiple networks. The very existence of this 758-LOC subsystem in the current tree contradicts pure AP-only intent — it has no use under AP-only.**
- Touched again at `5cfb688d` (smart selection helper).

### `firmware-v3/src/network/WiFiCredentialsStorage.cpp`/`.h`

- Added alongside or as part of (3); appears as separate file in the post-restructure tree. Same purpose: STA credential pool.

### `firmware-v3/include/wifi_config.h` / `network_config.h`

- `network_config.h` introduced via `91d0ec66` 2026-01-28; centralises AP SSID, password, channel, WS port, mDNS hostname.
- `wifi_credentials.ini` + `.template` exist at firmware-v3 root: gitignored credentials file for STA SSIDs (per `5f0cef2f` gitignore entry). Implies STA was — and remains — wired into the build at credential ingestion level.

### `firmware-v3/src/main.cpp` (WiFi blocks)

- Major WiFi-init block introduced at `d13889f8` 2026-02-17 alongside AP-only switch.
- Phase 4 refactor at `dcb87777` 2026-03-21 extracted to `SystemInit`.
- Currently calls `WiFiManager.begin()`, which executes the AP-only path.

---

## 3. The "original AP requirement" commit

**Earliest evidence — `7700aba2` 2025-12-20** ("feat(v2): Add network layer with WiFiManager"):

```cpp
// In WiFiManager::begin():
// Set WiFi mode to STA+AP for maximum flexibility
WiFi.mode(WIFI_MODE_APSTA);
```

State #4 enumerated as `AP_MODE - Soft-AP fallback mode` after STA reconnect budget (max 10 attempts) is exhausted.

**Codified AP-as-fallback — `6ce6143e` 2026-01-28** ("feat(wifi): add AP-mode fallback after repeated scan failures"):

> "When WiFi scanning finds no known networks after 2 consecutive attempts, automatically switch to AP mode for device accessibility."

**Verdict on Captain's recollection:** Confirmed. The original requirement was **dual-mode with STA primary and AP as a graceful fallback for the "no known network in range" scenario.** This matches Captain's "AP MUST be possible, but not AP-only-EVER".

---

## 4. The "AP-only-EVER" hardening moment

**Single-commit answer:** `d13889f8` 2026-02-17 ("feat(network): WiFi AP-only and main.cpp integration").

**Evidence (diff excerpt from the commit):**

```diff
-    // Set WiFi mode. Default is AP+STA concurrent (Portable Mode).
-    // AP starts immediately so Tab5/iOS can connect within seconds of boot.
-    // STA connects in parallel without affecting AP clients.
+    // Boot into AP-only mode. STA is ONLY activated via serial `wifi connect`.
+    // This prevents STA scanning/reconnection loops from destabilising the AP,
+    // which is the PRIMARY connection path for Tab5 and iOS clients.
 #ifdef WIFI_AP_ONLY
     LW_LOGW("WIFI_AP_ONLY enabled - starting in AP mode only");
-    startSoftAP();
-    setState(STATE_WIFI_AP_MODE);
-#else
-    if (m_apEnabled) {
-        // Portable Mode: AP always on, STA connects in parallel
-        WiFi.mode(WIFI_MODE_APSTA);
+#endif
+    {
+        WiFi.mode(WIFI_MODE_AP);
        ...
+        m_forceApOnly = true;
+        setState(STATE_WIFI_AP_MODE);
     }
-#endif
```

**Build-level enforcement** added at `54765b16` 2026-03-04 (`-D WIFI_AP_ONLY=1`) and `5f0cef2f` 2026-03-05 (cherry-pick onto stage0 baseline). `WIFI_AP_ONLY=1` is now applied to **5 build envs** in `platformio.ini` (lines 203, 237, 414, 1134, 1163).

**Was the transition gradual or sudden?**

- **Sudden in code:** `d13889f8` flips the runtime default from `APSTA → AP` in a single 59-line diff to `WiFiManager.cpp`.
- **Gradual in surrounding ecosystem:** preceded by progressive APSTA stabilisation (`1d1589c6`, `0b270a48`, `9ef320fc`) over Feb 5–6 that recognised AP-client stability suffered when STA scanned/reconnected. The Feb 17 commit closed off the residual instability by making `m_forceApOnly` the default state and only escapable via the serial `wifi connect` command.
- **Vestigial STA infrastructure was NOT deleted.** `WiFiCredentialManager` (~758 LOC), `WiFiCredentialsStorage`, `findBestAvailableNetwork`, `scanNetworks`, all 5-state STA handlers, and the `wifi_credentials.ini` ingestion remain in the current tree. The forensic-audit-2026-04-17 doc (`firmware-v3/docs/forensic-audit-2026-04-17.md:419`) explicitly catalogued **"~500 LOC of dead STA code in WiFiManager.cpp"** as a Track C dead-code-cull candidate; that cull was never executed.

**Most recent state (24 hours before this audit):** `11e040d6` 2026-05-03 added a `_sta_validation` build env that `-UWIFI_AP_ONLY` to **explicitly re-enable STA for validation purposes**, signed off as RBDO-GROUNDED. The doctrine has therefore already begun walking back from "AP-only-EVER" at the build-system level, even though the production default_envs entry is still AP-only.

---

## 5. ESP-IDF version + known-bug context

**Pinned version (current platformio.ini):** `platform = espressif32@6.9.0` at lines 113, 256, 1072, 1182, 1211, 1236.

**Framework:** `framework = arduino` (NOT pure ESP-IDF). Arduino-ESP32 v3.x for the espressif32 6.9.0 platform package corresponds to **ESP-IDF 4.4.x** (project memory `firmware_build_envs.md` records IDF 4.4.7 specifically; this aligns with the espressif32 6.9.0 Arduino-ESP32 vendor pin).

**Known-bug evidence in commit corpus:**

- `25522`–`25542` claude-mem observation block (Feb 5, 2026 indexed) — root cause for Tab5 "4WAY_HANDSHAKE_TIMEOUT" identified as **"ESP32 WiFi stack PMK cache retention"**. Mitigation (`25550`) — "AP re-initialization to flush WPA2 PMK cache on client disconnect".
- `25523` — "WiFi AUTH_EXPIRE root cause identified: auth mode mismatch between ESP32-S3 AP and ESP32-P4 STA" (ESP32-P4 = Tab5).
- `25475` — "UDP recovery logic skips WiFi reconnect in AP+STA mode" → confirms AP+STA concurrent mode resource contention.
- Forensic-audit doc `firmware-v3/docs/forensic-audit-2026-04-17.md` references AP retry hardening but does NOT cite a specific ESP-IDF JIRA / GitHub issue number.

**`platform_packages` overrides:** none found in `firmware-v3/platformio.ini` (no IDF version override beyond what `espressif32@6.9.0` ships). No commit comment in the corpus mentions an Arduino-ESP32 / espressif32 platform-version bump tied to a "AP+STA fix" upstream.

**Captain's hypothesis ("might have gotten solved recently"):** No evidence in this repo's git history that an ESP-IDF / Arduino-ESP32 patch relating to AP+STA concurrent corruption has been adopted. The platform pin `6.9.0` was effectively frozen during the AP-only window. Verifying whether espressif32 ≥ 6.10.x or Arduino-ESP32 v3.3+ (referenced at `f7e78fc2` Tab5 commit as "NOT YET RELEASED" on 2026-01-17) actually carries an AP+STA fix would require pulling release notes from outside this repo (out of scope for SSA-A).

---

## 6. Vestigial STA infrastructure findings

Currently compiled-in but unreachable on production builds (`WIFI_AP_ONLY=1`):

| Component | Location | LOC | Original purpose | Reachable today? |
|-----------|----------|-----|------------------|------------------|
| `WiFiCredentialManager` | `firmware-v3/src/network/WiFiCredentialManager.{h,cpp}` | 752 | NVS multi-network credential pool with MAC-XOR + CRC32 | Compiled, mutex-protected. Enumeration callable but stored credentials feed only `findBestAvailableNetwork()` → STA scan. Effectively dead under AP-only. |
| `WiFiCredentialsStorage` | `firmware-v3/src/network/WiFiCredentialsStorage.{h,cpp}` | (small) | Adjacent persistence helper | Compiled, dead under AP-only. |
| `findBestAvailableNetwork()` | `WiFiManager.cpp` (handleStateScanning path) | ~80 | RSSI-ranked pool selection | Gated by `m_forceApOnly` early-return at line 261 |
| 5-state STA handlers (`handleStateScanning`, `handleStateConnecting`, `handleStateConnected`, `handleStateReconnecting`) | `WiFiManager.cpp` | ~250 | STA state machine | Each state's first action is `if (m_forceApOnly) return;` (lines 472, 569, 592) |
| `requestSTAEnable()` / `requestAPOnly()` runtime API | `WiFiManager.cpp` lines ~1122/1132 | small | Runtime toggle | `requestSTAEnable()` clears `m_forceApOnly` and is callable. **Serial `wifi connect` is the documented escape hatch** but project memory `firmware_wifi_architecture.md` records this as KNOWN UNRELIABLE. |
| `wifi_credentials.ini` + `.template` | `firmware-v3/wifi_credentials.ini[.template]`, `firmware-v3/data/wifi_credentials.ini.template` | — | STA SSID/password ingestion at build time | Files present, gitignored, but the ingestion plumbing into the build is alive. |
| `[wifi_sta]` build_flags template | `firmware-v3/platformio.ini:25-32` | small | Empty-by-default placeholder so `${wifi_sta.build_flags}` resolves on fresh checkouts | Active. Comment: "K1 is AP-ONLY in production (per CLAUDE.md hard constraint); the _sta env is dev-only and must not be used on K1 hardware." |
| `esp32dev_audio_pipelinecore_sta` build env | `firmware-v3/platformio.ini` | small | dev-only STA capture env | Pre-existed and built around `${wifi_sta.build_flags}`. |
| `esp32dev_audio_esv11_k1v2_32khz_sta_validation` build env | `firmware-v3/platformio.ini:225-241` (added `11e040d6`) | small | **Validation-only STA fallback** for "Portal REST/WS closeout" | **Live as of 2026-05-03**. Explicitly `-UWIFI_AP_ONLY`. |

**Forensic-audit 2026-04-17 explicit dead-code call-out** (`firmware-v3/docs/forensic-audit-2026-04-17.md:419`):

> "**~500 LOC of dead STA code in WiFiManager.cpp** — all state handlers for STA scanning/connecting/disconnecting are still compiled-in despite WIFI_AP_ONLY=1. findBestAvailableNetwork(), scanNetworks, etc."

Track C remediation ("P0 + P1 + dead-code cull") proposed removing them. **This cull was not executed** — the lines persist at HEAD.

**Interpretation marker:** The persistence of ~750 LOC of NVS credential management, ~250 LOC of STA state handlers, an `[wifi_sta]` build template, and STA-capable build envs (including one added 24 hours ago) suggests that **the system was never fully committed to AP-only-EVER at the architectural level — only at the runtime-default level.** The STA path was disabled, not removed.

---

## 7. Evidence quality verdict (per Captain question)

| Question | Verdict | Reasoning |
|----------|---------|-----------|
| Q1 — total commits found | **HIGH** | 38 material commits with full hash + date + body inspected. Verified across both content-regex (`-G`) and path-based (`-- "*WiFi*"`) git log queries. |
| Q2 — per-file history | **HIGH** | All four current network files have explicit creation-commit + major-modification hashes. Archive scan found no buried older WiFi sources. |
| Q3 — original AP requirement | **HIGH** | `7700aba2` 2025-12-20 is unambiguous (`WIFI_MODE_APSTA` + AP listed as fallback state). `6ce6143e` 2026-01-28 codifies AP-as-fallback explicitly. Diff text quoted verbatim. |
| Q4 — when AP-only-EVER hardened | **HIGH** | Single-commit answer `d13889f8` 2026-02-17 (runtime), reinforced by `54765b16` 2026-03-04 (build flag), with the diff producing both the `WIFI_MODE_AP` switch and the `m_forceApOnly` lock. Comment text quoted verbatim. The transition is sudden in code, gradual in surrounding ecosystem stabilisation (Feb 5–6 APSTA fixes that preceded the cut). |
| Q5 — ESP-IDF + known-bug context | **MEDIUM** | Platform pin (`espressif32@6.9.0`) confirmed by direct grep. claude-mem observations cite "PMK cache retention" and "AUTH_EXPIRE auth-mode mismatch ESP32-S3 AP ↔ ESP32-P4 STA" as root causes, but these were not cross-verified to a specific ESP-IDF JIRA or upstream patch. **No commit in this repo references an upstream fix for AP+STA concurrent corruption.** Whether espressif32 ≥ 6.10.x carries such a fix requires external sources beyond SSA-A's scope. |
| Q6 — vestigial STA infrastructure | **HIGH** | `forensic-audit-2026-04-17.md:419` quotes ~500 LOC explicitly. Confirmed by direct grep of current WiFiManager.cpp (158 STA-related references in current source). Components catalogued with file:line in §6 above. |

---

## 8. What I did NOT verify (scope limits)

- **Hardware reality.** Whether STA mode actually fails today on K1 V2 hardware — not tested, not flashed, not measured. Pure code-history audit.
- **ESP-IDF upstream bug status.** Did not pull ESP-IDF / Arduino-ESP32 release notes / GitHub issues to verify whether AP+STA concurrent corruption has been patched in espressif32 ≥ 6.10.x or any backport. Captain's "might have gotten solved recently" is neither confirmed nor refuted by this audit.
- **Tab5-side WiFi behaviour.** Tab5 is a STA-to-K1's-AP client; Tab5 firmware history was tangentially included but not exhaustively traced (`f7e78fc2`, `afc1e982`, `c3cad71a`, `80787d8e` are noted but not deep-dived).
- **iOS-side discovery and reconnection logic.** The iOS connection flow (DeviceDiscoveryService, SSID detection, NWPathMonitor) is referenced in `1d1589c6` but iOS commits beyond that were not traced.
- **Memory / claude-mem deep search.** SSA-C's scope. I observed three memory pointers (project memory `firmware_wifi_architecture.md`, claude-mem observation IDs `25422`–`25710`, the forensic-audit doc) but did not fan out across the full memory archive.
- **Doctrine / CLAUDE.md drift.** SSA-D's scope. I noted that `firmware-v3/src/network/CLAUDE.md` carries the "AP-ONLY hard constraint" doctrine but did not chart its evolution.
- **Current-source state.** SSA-B's scope. Snapshots cited only as needed to anchor git claims.
- **Pre-2025-12-20 history.** Only one v2 WiFiManager (`7700aba2`) found as the creation point. Earlier WiFi work in `_archive/` was scanned via `find` but not deep-dived. Pre-restructure work in `firmware/` and the genesis CN3 vendor SDK is inventoried but not traced.

---

## Appendix: One-line summary for synthesis

> Original 2025-12-20 spec was **APSTA "for maximum flexibility"** with AP as STA-failure fallback (`7700aba2`, `6ce6143e`). Pivot to **AP-as-primary** Feb 5 (`1d1589c6` Portable Mode) followed by hard cut to **AP-ONLY at runtime** Feb 17 (`d13889f8`) to stop STA scanning destabilising AP clients, formalised at build level Mar 4 (`-D WIFI_AP_ONLY=1` at `54765b16`). ~500 LOC of STA infrastructure remained dead-but-compiled. 24 hours ago (`11e040d6` 2026-05-03), an `_sta_validation` build env was added that `-UWIFI_AP_ONLY` — first explicit walk-back since the Feb hardening. ESP-IDF pinned at `espressif32@6.9.0` (~IDF 4.4.x) throughout; no commit references a specific upstream AP+STA-corruption patch.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:SSA-Forensic-A | Created. 38-commit timeline, four files traced, AP-only hardening pinned to `d13889f8` 2026-02-17, vestigial STA inventory enumerated, ESP-IDF pin verified at `espressif32@6.9.0`. |
