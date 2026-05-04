---
abstract: Exhaustive archaeological excavation of K1 WiFi-mode evolution from THE FIRST COMMIT in the repository (pre-genesis 2025-06-24 era) through current HEAD (2026-05-03). Backwards-traversal methodology per Captain directive. Captures the pre-firmware-v3 era ("Light Crystals" hardcoded STA + AP fallback), the c8d12479 "Disable WiFi - worthless shit" smoking gun (2025-07-05), the July-December 2025 development hiatus, the 7700aba2 v2 network rebirth (APSTA), the brief Portable Mode AP+STA experiment (2026-02-05), the d13889f8 AP-only doctrinal inflection (2026-02-17), and the firmware-v3 era hardening culminating in the 11e040d6 STA validation profile (2026-05-03). Definitive timeline for Captain round-table on dual-mode reality.
---

# K1 Forensic WiFi Genesis Report

## Methodology

Per Captain directive, this report walks BACKWARDS from HEAD through the entire repository, including all branches reachable via `git log --all`, all tags, and all archived/legacy paths. Forensic-A scoped to `firmware-v3/` only (38 commits from 2025-12-20 forwards). This report extends the scope to:

- The TRUE first commit in the repository (`34baa31d`, 2025-06-24)
- All pre-`firmware-v3/` paths: `src/`, `firmware/K1.8encoderS3/`, `firmware/Tab5.encoder/`, `firmware/v2/`, `v2/`, `assets/source_refactor_final_archive/`
- All branches (47 local + remote, 7 remote-only)
- All tags (32 including `tab5-known-good-2026-01-29`, `v2.0.0`, `v2.1-connectivity-stable`, the `esv11-audio/*` tag series, `stage1-*`)

**Counting note:** A grep on `wifi|wireless|softap|ap-only|portable|wifimanager|network` over commit messages from K1 authors (qaxzy, SpectraSynq, K1 Research Agent, cursoragent) returns **222 commits** across all branches. A content-level search via `git log -G` for the literal symbols `WIFI_MODE_*`, `softAP`, `FORCE_AP_MODE`, `m_forceApOnly`, `WirelessManager`, `WirelessReceiver`, `WirelessProtocol` returns **66 commits**. Forensic-A's number (38 commits in `firmware-v3/` only) is a strict subset. The TRUE WiFi-touching activity since the project began is roughly **5.8x larger** than Forensic-A's scope.

---

## Task 1 — The TRUE first commit

```
git rev-list --all --max-parents=0
```

returns two root commits:

| Hash | Date | Subject | Note |
|---|---|---|---|
| `34baa31d` | **2025-06-24** | `feat: Enhanced Light Crystals ESP32-S3 LED Controller - Comprehensive Visual Effects System` | **TRUE genesis of the Lightwave-Ledstrip codebase.** Author: `qaxzy <32584196+qaxzy@users.noreply.github.com>` (Cursor agent identity). |
| `598369e8` | 2025-09-06 | `Initial release v3.3.8` | Root commit of the claude-mem subtree that was later merged into this repo. **Not** part of the K1 hardware lineage. Author: `Alex Newman` (claude-mem maintainer). |

Captain's anchor `723f4522` (2025-12-29, `feat(bpm): Integrate K1-Lightwave beat tracker from Tab5.DSP`) is **not** the root. It is an integration commit ~6 months after the true genesis. The recon brief was correct that `723f4522` is "the second genesis" — it is closer to a "fresh-start anchor" than a true root.

---

## Task 2 — Full WiFi-touching commit timeline

The complete chronology (consolidated from `git log -i --grep`, `git log -G`, and `git log --diff-filter=AM` across all paths and all branches). Only K1 hardware author commits shown; claude-mem subtree is excluded.

### Pre-genesis & Genesis era (the Light Crystals layer, 2025-06-24 → 2025-07-17)

| Hash | Date | Subject (verbatim) | What it did to WiFi state |
|---|---|---|---|
| `34baa31d` | 2025-06-24 | `feat: Enhanced Light Crystals ESP32-S3 LED Controller — Comprehensive Visual Effects System` | TRUE genesis. Created `src/config/network_config.h` with **hardcoded home router credentials** `WIFI_SSID = "VX220-013F"`, `WIFI_PASSWORD = "3232AA90E0F24"`. AP `LightCrystals` declared as **fallback**. STA was the primary mode by design. |
| `b2578c50` | 2025-06-24 | `fix: Implement M5Stack 8Encoder with performance monitoring` | Encoder pipeline (no WiFi state change). |
| `5e5977d2` | 2025-06-26 | `fix: Remove broken wireless code to fix build errors` | Deleted `src/wireless/WirelessManager.h` (163 lines) and `src/wireless/WirelessProtocol.h` (128 lines). Note: this was the **ESP-NOW encoder mesh**, not WiFi STA/AP. Removed because of compile errors. |
| `16302cae` | 2025-06-26 | `feat: Implement comprehensive wireless encoder system for ESP32-S3` | Re-introduced `WirelessManager`/`WirelessProtocol`/`WirelessTransmitter`/`WirelessReceiver` as ESP-NOW (250-byte packet, <3ms latency) encoder mesh. Distinct from WiFi mode work. |
| `20a396d0` | 2025-06-26 | `feat: Add wireless encoder protocol foundation` | ESP-NOW protocol layer. |
| `17e276c1` | 2025-06-26 | `feat: Complete wireless encoder integration into main project` | ESP-NOW integration into main loop. |
| `a111eff5` | 2025-06-26 | `feat: Finalize wireless encoder system with advanced transition effects` | ESP-NOW finalisation. |
| `1d73c14e` | **2025-07-02** | `feat(network): add mDNS (Bonjour) initialization for lightwaveos.local after WiFi connect` | First appearance of `lightwaveos.local` mDNS hostname. Commit message states: *"Documents that mDNS is not started in AP mode."* This proves STA was the primary live mode at this date — mDNS only fires after `WiFi.connect()` completes. |
| `7393ca4c` | **2025-07-04** | `feat: Implement Genesis Audio Sync and WiFi Optimizer Pro` | Added 906 lines of `WiFiOptimizer.h` and `WiFiOptimizerPro.h` (channel scanning, **adaptive TX power 8-20 dBm**, **802.11 LR mode**, **WPA3 PMF support**, captive portal-style auto-channel selection). Created `docs/hardware/WIFI_ANTENNA_GUIDE.md` (227 lines) and `docs/examples/wifi/WiFiIntegrationExample.cpp`. **Pinnacle of STA-mode investment** in the Light Crystals layer. |
| `c8d12479` | **2025-07-05** | `feat: Disable WiFi by default - remove that worthless shit` | **THE SMOKING GUN** — see Task 3. |
| `1171b511` | 2025-07-05 | `feat: Major audio subsystem refactor with deterministic 8ms task scheduling` | Audio refactor; touched `WiFiManager.cpp` to ensure WiFi runs on Core 0. Did NOT change mode. |
| `73f12937` | 2025-07-05 | `fix: Implement comprehensive I2C mutex protection to resolve multi-core race conditions` | I2C mutex fix; "Fixed WiFiManagerV2 build errors (duplicate methods, friend function)" — kept `WiFiManagerV2.cpp` compilable. STA + AP fallback structure preserved but disabled-by-default. |
| `51f57828` | 2025-07-06 → `574c7b8e` | **2025-07-17** | (effects/encoder work; no WiFi-mode changes) | — |

### The hiatus (2025-07-18 → 2025-12-08)

**Five months of zero K1 hardware activity.** The only commits in this window are from `Alex Newman` (claude-mem subtree maintainer) — completely unrelated to K1 firmware. Captain context: this is when K1 development paused; resumed in December 2025 with the v2 architectural rewrite.

### v2 rebirth era (2025-12-09 → 2025-12-29)

| Hash | Date | Subject | What it did to WiFi state |
|---|---|---|---|
| `9b3f24b7` | 2025-12-09 | `feat(zone-composer): Implement multi-zone effect system…` | Resumes K1 work after the hiatus. |
| `53de8f1e` | 2025-12-12 | `feat: VERIFY-WIFI-BUILD - WiFi build verified passing` | Re-validated `pio run -e esp32dev_wifi`. WiFi feature flag still gated, but flagged-on build now passes again. |
| `07a3f168` | 2025-12-12 | `refactor: Reorganize project workspace and archive legacy code` | **Created** `assets/source_refactor_final_archive/wireless.h` (the legacy Emotiscope captive-portal-ish wireless code archived for reference). Diff stat: 1043 lines. |
| `f5d64c7f` | **2025-12-16** | `feat(security,perf,zones): Externalize WiFi credentials, add CORS, enable zone mode by default` | **BREAKING CHANGE.** `WIFI_SSID="VX220-013F"` → `NetworkConfig::WIFI_SSID_VALUE` defaulting to `"CONFIGURE_ME"`. Created `wifi_credentials.ini.template` (gitignored). Hardcoded home router credentials finally purged — though the architectural model (STA primary, AP fallback) was preserved. |
| `54014040` | 2025-12-19 | `feat: Setup v2 monorepo structure for LightwaveOS 2.0` | Birth of `v2/` monorepo. |
| `437d0d66` | 2025-12-19 | `checkpoint: Pre-v2 architecture state` | Pre-v2 freeze marker. |
| `7700aba2` | **2025-12-20** | `feat(v2): Add network layer with WiFiManager and REST/WebSocket API` | **CREATED** `v2/src/network/WiFiManager.cpp` (657 lines) and `WiFiManager.h` (425 lines). Set `WiFi.mode(WIFI_MODE_APSTA)` ("for maximum flexibility"). State machine: `DISCONNECTED → CONNECTING → CONNECTED → AP_MODE (fallback) → RECONNECTING`. AP fallback SSID hardcoded `"LightwaveOS-Setup"`, password `"lightwave123"`. **STA-primary doctrine, this was Forensic-A's earliest commit.** |
| `4657fbaf` | 2025-12-20 | `feat: Add web interface, entry point, and architecture documentation` | v2 main.cpp + WebServer integration. |
| `1311d95b` | 2025-12-21 | `feat(wifi): add multi-network fallback with configurable retry` | Added `WIFI_SSID_2`/`WIFI_PASSWORD_2`. Cycles primary → secondary → AP. Confirms STA-primary architecture at this date. |
| `87a3d89b` | 2025-12-21 | `feat(api): implement REST API v2 with ESP-IDF native http server` | REST v2 surface, no mode change. |
| `fffadd7a` | 2025-12-21 | `chore(v2): add gitignore, credential templates, and multi-network WiFi support` | `wifi_credentials.ini.template`, `network_config.h.template`. Failover doc: `Primary (2 attempts) → Secondary (2 attempts) → AP Mode (if enabled)`. STA-primary doctrine codified. |
| `8c0938f9` | **2025-12-22** | `refactor(arch): Deprecate v1 codebase in preparation for CQRS migration` | **143 files moved from `src/` → `src.v1.deprecated/`**, including all of `src/network/` (the original WiFiManager) and `src/wireless/` (ESP-NOW encoder mesh). v1 and v2 coexist in tree but only v2 is built. End of the Light Crystals lineage. |
| `c691ea30` | 2025-12-24 | `Refactor: Complete dependency injection migration and fix WebServer linker errors` | DI cleanup, no mode change. |
| `e27c008a` | **2025-12-28** | `fix(network): improve WiFi and WebSocket stability` | "Disable WiFi modem sleep after connect (prevents `ASSOC_LEAVE` disconnects). Enable auto-reconnect for better connection recovery." STA-mode hardening. |
| `6fbc1dcf` | 2025-12-28 | `chore: major project root housekeeping - move legacy to external archive` | Final v1 → archive. |
| `eb9ad881` | 2025-12-31 | `feat(network): update web server for audio-reactive API endpoints` | API extensions. |

### Tab5/dual-network era (2026-01-02 → 2026-02-04)

| Hash | Date | Subject | What it did |
|---|---|---|---|
| `4a40ee46` | 2026-01-02 | `feat(K1.8encoderS3): AtomS3 ESP32-S3 encoder controller with I2C recovery` | First Tab5/AtomS3 firmware — adds `firmware/K1.8encoderS3/src/network/WiFiManager.cpp`. WebSocket client connects to K1 via mDNS `lightwaveos.local`. |
| `dcaa89b2` | 2026-01-03 | `feat(Tab5.encoder): WiFi + WebSocket integration with LightwaveOS sync` | First Tab5 encoder firmware with WiFi. Tab5 connects as STA to whatever K1 is on. |
| `9d0ccd44` | 2026-01-01 | `feat(network): complete handler extraction and fix WiFi event handlers` | WiFi event handler refactor. |
| `937c9abc` | **2026-01-04** | `feat(network): add AP-only mode and STA enable/disable API` | **First AP-only doctrinal commit.** Build flag `FORCE_AP_MODE` (default true). Runtime: `m_forceApModeRuntime`, `requestSTAEnable()` for OTA window, `requestAPOnly()` to revert. New REST: `/api/v1/network/sta/enable`, `/api/v1/network/ap/enable`. Commit message verbatim: *"No AP+STA dual-mode (historically unreliable)"*. |
| `a78982b1` | 2026-01-05 | `Save work before controller-only WiFi implementation` | WIP marker before `controller-only` WiFi attempt. |
| `abc74f30` | 2026-01-06 | `feat(tab5-encoder/network): add dual network support with…` | Tab5 dual-network (router and K1 AP). |
| `c4e8b867` | 2026-01-11 | `feat(network): Comprehensive API and WebSocket improvements` | API hardening. |
| `94479036` | 2026-01-09 | `chore: Stage complete tempo tracking phase 0-6 work and audit docs` | **Created `docs/audits/Wireless-Systems-Alignment-Audit-2025-01.md`** (see Task 5). |

### Portable Mode era (2026-02-05 → 2026-02-16) — the AP+STA experiment

| Hash | Date | Subject | What it did |
|---|---|---|---|
| `1d1589c6` | **2026-02-05** | `net: implement Portable Mode - v2 is always the network` | **CRITICAL.** v2 reverts to `WIFI_MODE_APSTA`. Commit message verbatim: *"v2 now runs AP+STA concurrently so LightwaveOS-AP stays up permanently at 192.168.4.1. Tab5 targets the AP as primary WiFi with deterministic IP (no mDNS needed)."* AP SSID `LightwaveOS-AP`, password `SpectraSynq`. iOS introduces NWPathMonitor + UserDefaults persistence. This is the SECOND attempt to make AP+STA work. |
| `0b270a48` | 2026-02-05 | `net: stabilise AP+STA and esv11 heap` | Within hours of the previous commit: *"Skip forced STA reconnect when WiFi runs in AP+STA mode (keeps AP clients stable)."* First sign of AP+STA contention symptoms. |
| `9ef320fc` | 2026-02-06 | `fix(wifi): skip STA scan loops on placeholder creds` | Fix for endless STA scan loops when `wifi_credentials.ini` was at `CONFIGURE_ME` defaults — the placeholder-creds path destabilised AP. |
| `78601d7a` | 2026-02-07 | `fix(mem): harden memory allocation across WebSocket/HTTP under AP+STA load` | Memory hardening under combined-mode pressure. |

### AP-only doctrinal era (2026-02-17 → present)

| Hash | Date | Subject | What it did |
|---|---|---|---|
| `d13889f8` | **2026-02-17** | `feat(network): WiFi AP-only and main.cpp integration` | **THE DOCTRINAL INFLECTION.** Diff verbatim from `firmware/v2/src/network/WiFiManager.cpp`: removed the comment *"Set WiFi mode. Default is AP+STA concurrent (Portable Mode)"*; replaced with *"Boot into AP-only mode. STA is ONLY activated via serial `wifi connect`. This prevents STA scanning/reconnection loops from destabilising the AP, which is the PRIMARY connection path for Tab5 and iOS clients."* `WiFi.mode(WIFI_MODE_AP)` becomes the boot default. New private `m_forceApOnly = true` flag. |
| `283c9da3` | 2026-02-17 | `fix(network): WebServer AP disconnect and beat lifecycle` | Same-day hardening: handlers return 503 when `!WiFi.isAPUp()`. |
| `afc1e982` | 2026-02-17 | `feat(Tab5): WiFi antenna, WS client, zone composer UI` | Tab5 AP-targeting, antenna config. |
| `5ee8aa84` | 2026-02-27 | `refactor: complete repository restructure (Phases 0-7)` | **`firmware/v2` → `firmware-v3`.** This is when `firmware-v3/src/network/WiFiManager.cpp` came into existence — as a path rename of the AP-only-doctrine `v2` file. |
| `d943101a` | 2026-02-27 | `feat(firmware): integrate 95 commits from fix/stable-effect-ids branch` | Mass merge that brought stable effect IDs + (incidentally) WiFiManager touches. |
| `a39a03b8` | **2026-03-04** | `feat(network): add stimulus override REST/WebSocket API, harden AP-only docs` | **Doctrine codified in code.** Added AP-only banner blocks to `WiFiCredentialManager.h`, `WiFiCredentialsStorage.h`, `NetworkHandlers`. **Created `firmware-v3/src/network/CLAUDE.md`** with the canonical AP-ONLY warning text that survives in HEAD today. |
| `54765b16` | 2026-03-04 | `build(platformio): add WIFI_AP_ONLY flag, override template, gitignore updates` | Added build flag `WIFI_AP_ONLY` (defaults true). |
| `032d1a5c` | 2026-03-04 | `refactor(firmware): clean up main.cpp, harden renderer, add diagnostics` | Renderer hardening, side touches WiFiManager. |
| `d14e2696` | 2026-03-04 | `perf(core): K1 runtime tuning — heap shedding, stack hysteresis` | Heap shedding (later identified as cause of fragmentation latch). |
| `5f0cef2f` | 2026-03-05 | `build(platformio): add WIFI_AP_ONLY flag, override template, gitignore updates` | Cherry-pick of 54765b16 onto stage0. |
| `86abcb12` | 2026-03-05 | `fix(network): WiFiManager AP auth fix from d14e2696` | Removed explicit auth params from `softAP()` to fix WPA handshake failures with some clients. |
| `19b41ca1` | 2026-03-05 | `perf(core): surgical K1 runtime tuning from d14e2696` | Performance tuning carried forward. |
| `8134d4a4` | 2026-04-18 | `fix(firmware): forensic-audit stability hardening (Wave 0/1/2 + root yield)` | Wave 1 P0-04: *"softAP retry + WiFi TWDT"*. Added `kSoftApMaxAttempts` retry loop with restart on failure. The "AP-ONLY INVARIANT" comment in HEAD's `WiFiManager.cpp` traces to this commit. |
| `c126fbd9` | 2026-04-18 | `fix(firmware): WS cooldown timestamp refreshed on reject — cleared` | WS cooldown bug found during tab5↔K1 reconnect-churn investigation. |
| `d5cd99a8` | 2026-04-18 | `fix(firmware): WS Round 2 — AsyncTCP priority/core, textAll gating, overlap-guard staleness recovery` | Pinned AsyncTCP to Core 1 (away from WiFi/audio Core 0). Three parallel SSAs (-C/-D/-E). |
| `0cc34478` | 2026-05-01 | `fix(network): heap-shed fragmentation latch — release on largest-block recovery` | Fixed iOS WS reconnect storm caused by heap-shed latch never releasing under fragmentation. iPhone↔K1-AP reliability fix. |
| `11e040d6` | **2026-05-03** | `chore(firmware): add K1v2 STA validation profile` | **Latest WiFi commit at HEAD.** Adds `esp32dev_audio_esv11_k1v2_32khz_sta_validation` env (validation-only). Production default remains AP-only. *"This profile is for REST/WS validation only after the approved AP failure criteria are met; it is not a production default."* |

---

## Task 3 — The smoking gun: `c8d12479`

### Verbatim commit message (full)

> ```
> feat: Disable WiFi by default - remove that worthless shit
> 
> WiFi is now DISABLED BY DEFAULT because it's fucking worthless:
> 
> Resource savings:
> - RAM usage: 21.2% → 13.4% (saved 25KB)
> - Flash usage: 65.1% → 33.4% (saved 519KB)
> - Build time: Much faster
> - Performance: Better LED performance without WiFi interference
> 
> Changes:
> 1. Set FEATURE_WEB_SERVER=0 in features.h by default
> 2. Commented out FEATURE_WEB_SERVER=1 override in platformio.ini
> 3. Added feature flag guards to all network files
> 4. Created multiple build environments:
>    - esp32dev (default, no WiFi)
>    - esp32dev_wifi (WiFi enabled for masochists)
>    - esp32dev_debug (debug, no WiFi)
> 
> How to enable WiFi if you really want that garbage:
> - Method 1: Change FEATURE_WEB_SERVER to 1 in features.h
> - Method 2: Uncomment build flag in platformio.ini
> - Method 3: Use 'pio run -e esp32dev_wifi'
> 
> The system runs much better without WiFi interference.
> Serial control and encoders work perfectly.
> All LED effects perform optimally.
> 
> 🤖 Generated with [Claude Code](https://claude.ai/code)
> 
> Co-Authored-By: Claude <noreply@anthropic.com>
> ```

### Diff summary (verified via `git show c8d12479 --stat`)

```
WiFi_CONTROL.md                            | 65 ++++++++++++++++++++++++++++++
platformio.ini                             | 13 +++++-
src/config/features.h                      |  7 +++-
src/core/LightshowEngine.h                 | 65 ++++++++++++++++++++++++++++++
src/effects/audio/LightshowEngineModes.cpp | 37 +++++++++++++++++
src/main.cpp                               | 23 +++++------
src/network/WebServer.cpp                  |  6 ++-
src/network/WiFiManager.cpp                |  6 ++-
src/network/WiFiManagerV2.cpp              |  6 ++-
9 files changed, 208 insertions(+), 20 deletions(-)
```

### Code-level effect

`src/config/features.h` BEFORE (`c8d12479^`):
```c
// Network features
#ifndef FEATURE_WEB_SERVER
#define FEATURE_WEB_SERVER 1            // Web interface enabled by default (can override via build flags)
#endif
```

`src/config/features.h` AFTER (`c8d12479`):
```c
// Network features - DISABLED BY DEFAULT because WiFi is fucking worthless
// To enable WiFi/WebServer, either:
// 1. Change FEATURE_WEB_SERVER to 1 below
// 2. Or add to platformio.ini: build_flags = -D FEATURE_WEB_SERVER=1
#ifndef FEATURE_WEB_SERVER
#define FEATURE_WEB_SERVER 0            // Web interface DISABLED by default (that WiFi shit is worthless)
#endif
```

### Project state interpretation

- **BEFORE:** WiFi was the default-ON path. Hardcoded SSID `VX220-013F` (the developer's home router), STA primary mode, AP `LightCrystals` as fallback. Active hardening campaign 4 days earlier (`7393ca4c` Genesis Audio Sync + WiFi Optimizer Pro adding 802.11 LR mode, adaptive TX power 8-20 dBm, WPA3 PMF) — the **most invested phase** of the original Light Crystals WiFi work.
- **AFTER:** WiFi is OFF by default. The `WiFiManager.cpp` and `WiFiManagerV2.cpp` files are still present in the tree but each gets a `#if FEATURE_WEB_SERVER` guard added. The flag default flips from `1` to `0`. Three build envs created so users opt in: `esp32dev` (no-WiFi default), `esp32dev_wifi` (described as "WiFi enabled for masochists"), `esp32dev_debug` (debug, no WiFi). Resource savings cited: RAM 21.2% → 13.4%, Flash 65.1% → 33.4%.
- **Emotional texture:** The commit message uses "fucking worthless", "that garbage", "for masochists" four separate times. Author is `qaxzy` (Cursor agent identity used by the original developer in the Light Crystals era). This was a frustration moment — the WiFi Optimizer Pro work landed Friday 2025-07-04, and by Saturday 2025-07-05 the author had given up on STA reliability after only ONE day of trying the new optimizer code.

### Why this is a smoking gun for Captain's round-table

The "K1 is AP-ONLY because STA never worked" doctrine that lives in HEAD's `firmware-v3/src/network/CLAUDE.md` and the user-level CLAUDE.md `Hard Constraints` section traces an unbroken behavioural lineage to **THIS commit on 2025-07-05**. The first time anyone in this codebase decided "WiFi router connection is a problem, not a feature" was 7 months and 3 days before the AP-only doctrine was formally codified at `d13889f8` (2026-02-17). The doctrine is not new — only the wording is.

---

## Task 4 — `1d73c14e` mDNS commit

### Verbatim commit message

> ```
> feat(network): add mDNS (Bonjour) initialization for lightwaveos.local after WiFi connect
> 
> - Uses hostname from network_config.h
> - Advertises HTTP service for Bonjour discovery
> - Adds serial output for mDNS status
> - Documents that mDNS is not started in AP mode
> 
> This enables access via http://lightwaveos.local on mDNS/Bonjour-capable networks.
> ```

(Note: the commit message escapes newlines as literal `\n` in the body — viewable via `git show 1d73c14e --pretty=format:"%B"` returns the full text on a single line.)

### Code-level WiFi state at this commit

- **WiFi mode at this commit's state:** `WIFI_MODE_STA` (primary), `WiFi.softAP()` only as fallback. Confirmed by reading `src/config/network_config.h` at this commit — the file still contains `WIFI_SSID = "VX220-013F"` (developer's home router) as the primary, with `AP_SSID = "LightCrystals"` listed under "Access Point settings (fallback)".
- **The smoking-gun textual evidence:** *"Documents that mDNS is not started in AP mode."* This wording explicitly states mDNS only fires when the device is in STA mode and has obtained an IP from the router. It is reasonable to infer that AP mode was a degraded/error fallback rather than the intended operating mode — Bonjour discovery was the marketed UX path.

### Track forward from `1d73c14e`

The next commit that changed WiFi state was **`c8d12479`** (`Disable WiFi by default - remove that worthless shit`) on 2025-07-05 — exactly **3 days later**. The doctrinal pivot (STA primary → WiFi disabled-by-default) happened in those 3 days, with `7393ca4c` (Genesis Audio Sync + WiFi Optimizer Pro on 2025-07-04) sandwiched in between as a final desperate optimisation attempt before the author gave up.

---

## Task 5 — Pre-existing audit docs

### `docs/audits/Wireless-Systems-Alignment-Audit-2025-01.md`

- **Created:** `94479036` (2026-01-09) — `chore: Stage complete tempo tracking phase 0-6 work and audit docs`. Despite the filename suffix `2025-01`, the file was **created in January 2026**, not January 2025. The filename is misleading.
- **Contents (verbatim opening):**
  > ```
  > # Wireless Systems Alignment Audit
  > **Date**: 2025-01-XX
  > **Systems Audited**: firmware/v2 (server) and firmware/Tab5.encoder (client)
  > **Audit Type**: Network Configuration Alignment
  > 
  > ## Executive Summary
  > 
  > **Status**: ✅ **ALL ISSUES RESOLVED** (2025-01-XX)
  > 
  > **Initial State**: 🔴 **CRITICAL MISALIGNMENTS DETECTED**
  > ```
- **Configuration matrix recorded at audit time:** AP SSID `"LightwaveOS-AP"`, AP Password `"SpectraSynq"` (now visible in current public repo as a leaked credential), mDNS hostname `"lightwaveos"`, WebSocket port 80, path `/ws`. Tab5 client checks gateway IP fallback `"192.168.4.1"` when SSID matches `"LightwaveOS-AP"`.
- **Posture revealed by this audit (early 2026):** v2 was operating with AP **as a functional mode** (not just fallback) — the audit calls out v2 server's mDNS hostname as `"lightwaveos"` and Tab5 expecting `"lightwaveos.local"`. AP-only doctrine had not yet been formalised; the server still ran APSTA and Tab5 was still trying to use mDNS even though mDNS was documented as not starting in AP mode.

### `docs/agent/NETWORK.md`

- **Created:** `76e00be7` (2026-02-04) — `docs: add component documentation and agent context`. Created exactly **one day before** `1d1589c6` Portable Mode (2026-02-05) and **13 days before** `d13889f8` AP-only inflection (2026-02-17). This is the agent-onboarding doc written in the brief Portable-Mode-AP+STA window.
- **Note:** This file no longer exists in HEAD — it was removed during the `5ee8aa84` (2026-02-27) repository restructure that flattened `docs/`.

### `docs/WiFiManagerV2_Integration.md`

- **Created:** `7393ca4c` (2025-07-04) — `feat: Implement Genesis Audio Sync and WiFi Optimizer Pro`. Originally part of the Light Crystals layer alongside the WiFi Optimizer Pro hardening campaign.

### `docs/WIRELESS_ENCODER_GUIDE.md` and `docs/hardware/WIFI_ANTENNA_GUIDE.md`

- **WIFI_ANTENNA_GUIDE.md created:** `7393ca4c` (2025-07-04) — alongside Genesis Audio Sync. 227 lines covering antenna selection, channel scanning, RSSI thresholds.
- **Posture revealed:** January 2026 was the dominant audit window. The 2025-07-04 docs were aspirational STA-mode hardening (immediately followed by `c8d12479` "worthless shit" capitulation 1 day later); the Jan-Feb 2026 docs were configuration-alignment audits between v2 server and Tab5 client during the brief APSTA Portable Mode window.

---

## Task 6 — Legacy firmware tree investigation

### `firmware/K1.8encoderS3/src/network/WiFiManager.cpp` history

- **Created:** `4a40ee46` (2026-01-02) — `feat(K1.8encoderS3): AtomS3 ESP32-S3 encoder controller with I2C recovery`. This is the **separate AtomS3 encoder controller**, not the K1 main firmware. It was Tab5's predecessor before Tab5.encoder was unified.
- **Original mode setting:** STA primary (it's a CLIENT that connects to the K1 AP). Resolves K1 via mDNS `lightwaveos.local` then falls back to gateway `192.168.4.1`. WebSocket client to K1 with exponential backoff.
- **Deleted:** `0c797e32` (2026-01-17) — `chore: remove obsolete encoder projects (K1.8encoderS3, Tab5.8encoder)`. Verbatim commit message: *"These legacy encoder controller projects have been superseded by the unified Tab5.encoder implementation"*. End of K1.8encoderS3 lineage. Total deletion: `firmware/K1.8encoderS3/` + `firmware/Tab5.8encoder/` = 8267 lines removed.

### `assets/source_refactor_final_archive/wireless.h`

- **Created:** `07a3f168` (2025-12-12) — `refactor: Reorganize project workspace and archive legacy code`.
- **Contents:** ESP-IDF native HTTP server WiFi code. Uses `esp_http_server.h`, `esp_wifi.h`, `esp_event.h`. Targets `emotiscope.rocks` discovery server (`#define WEB_URL "https://app.emotiscope.rocks/discovery/"`) — this is **Lixie Labs Emotiscope** legacy code that was archived as a reference. NOT K1 code. Contains a discovery + websocket server with retry/backoff logic. Reason for archival: structural reference for the v2 rewrite.

### `firmware/K1.8encoderS3/src/network/WebSocketClient.h/.cpp`

- Same history as the parent directory. Created `4a40ee46` (2026-01-02), deleted `0c797e32` (2026-01-17). Lifespan: 15 days.

---

## Task 7 — The c8d12479 → 7700aba2 gap (2025-07-05 → 2025-12-20)

### Activity in the gap

**Almost none.** The K1 hardware author (`qaxzy`) is silent from `574c7b8e` (2025-07-17) through `9b3f24b7` (2025-12-09) — a **5-month hiatus**. The only commits in this window come from `Alex Newman` (claude-mem subtree maintainer) and `Copilot` / `claude[bot]` — all working on the unrelated claude-mem package.

### When K1 work resumed and what happened to WiFi

| Hash | Date | Subject | What happened to WiFi |
|---|---|---|---|
| `9b3f24b7` | 2025-12-09 | `feat(zone-composer): Implement multi-zone effect system…` | K1 work resumes after the hiatus. **WiFi still disabled by default** (the `c8d12479` flag flip is still in effect). |
| `53de8f1e` | 2025-12-12 | `feat: VERIFY-WIFI-BUILD - WiFi build verified passing` | First sign of WiFi being touched again. `pio run -e esp32dev_wifi` rebuilt and verified passing — re-validated the opt-in WiFi build env from `c8d12479`. RAM 19.3%, Flash 31.3%. **WiFi still off-by-default.** |
| `f5d64c7f` | **2025-12-16** | `feat(security,perf,zones): Externalize WiFi credentials, add CORS, enable zone mode by default` | **Crucial pivot.** Hardcoded credentials purged (`VX220-013F` → `CONFIGURE_ME` template). CORS added. WiFi treated as a real product feature again — implies a decision was made to re-invest in the network path. |
| `54014040` | 2025-12-19 | `feat: Setup v2 monorepo structure for LightwaveOS 2.0` | Birth of v2/. |
| `7700aba2` | **2025-12-20** | `feat(v2): Add network layer with WiFiManager and REST/WebSocket API` | New `WiFiManager` from scratch. `WIFI_MODE_APSTA` ("for maximum flexibility"). **STA-primary doctrine restored.** |

### Was WiFi re-enabled?

**Yes — but as STA primary.** From `7700aba2` (2025-12-20) until `937c9abc` (2026-01-04), the v2 network layer ran APSTA-mode with STA primary and AP fallback. That is a **5.5-month doctrinal cycle**:

- 2025-07-05: STA primary disabled ("worthless shit")
- 2025-07-05 → 2025-12-20: dormant
- 2025-12-20: STA primary re-enabled in fresh v2 codebase
- 2026-01-04: AP-only build flag added (FORCE_AP_MODE)
- 2026-02-05: Reverted to APSTA Portable Mode
- 2026-02-17: AP-only doctrine codified in code
- 2026-03-04: AP-only doctrine codified in docs (`network/CLAUDE.md`)

The author at the v2 rebirth (SpectraSynq, in 2025-12) appears to have not been aware of (or chose not to honour) the `c8d12479` capitulation. The team needed to learn the same lesson again.

---

## Task 8 — Pre-genesis branches and tags

### Branches with their own WiFi history

- **`experimental/wireless-encoder-development`** — exists in remote, contains files `WIFI_NETWORK_FIXES_IMPLEMENTED.md`, `WIFI_V2_IMPLEMENTATION_SUMMARY.md`, `WiFi_CONTROL.md` at the root. This is the original ESP-NOW + STA development branch from 2025-06/07.
- **`feat/Development-Progress-Continues`** — contains `docs/examples/wifi/WebServerEnhanced.cpp`, `docs/examples/wifi/WiFiIntegrationExample.cpp`, `docs/hardware/WIFI_ANTENNA_GUIDE.md`, `docs/implementation/WiFi_CONTROL.md`. Forked from the late Light Crystals era.
- **`codex/p4-onwards-fixes`** — same Light Crystals docs preserved.
- **`backup-before-reset`** — contains `LightwaveOS_Temp_Files/K1.LightwaveS3/docs/WIFI_AUDIT_FINDINGS.md` and `WIFI_FIX_PROPOSAL.md` — pre-reset workspace state with WiFi audit artefacts.

### Tags

- `v2.0.0` (Dec 2025) — first v2 hardware-tagged build
- `v2.0.6-phase6-complete` — pre-restructure v2 milestone
- **`v2.1-connectivity-stable`** — milestone tag suggesting connectivity stabilisation around the v2.1 mark; needs a `git show` to confirm exact date but the tag name itself is doctrine evidence
- `tab5-known-good-2026-01-29` — Tab5 client+server working state, from the Portable Mode era
- `stable-with-critical-fixes` (no date in name)
- The `esv11-audio/100-wifi-ap-align` tag — explicit "WiFi AP align" milestone in the audio rebuild track

---

## Task 9 — The actual `723f4522` anchor

### Verbatim commit message (header)

> ```
> feat(bpm): Integrate K1-Lightwave beat tracker from Tab5.DSP
> 
> Migrates the 4-stage K1-Lightwave beat tracking pipeline from Tab5.DSP
> (ESP32-P4) to Lightwave-Ledstrip v2 (ESP32-S3). This provides stable
> tempo detection and phase-locked beat events for visual synchronization.
> ```

### Why is this Captain's anchor?

It is **not** a merge or squash — it is a regular commit (`Author: SpectraSynq`, single parent). Its significance:

- It is the migration of the K1 beat-tracking pipeline FROM the Tab5.DSP (ESP32-P4) project INTO the Lightwave-Ledstrip v2 (ESP32-S3) project. **It marks the moment K1 became "the K1 we know today".**
- WiFi state at this commit: unchanged from `7700aba2` 9 days prior — still `WIFI_MODE_APSTA` with STA primary. Beat tracker has nothing to do with WiFi mode.
- The orchestrator's brief said `723f4522` is "the genesis"; this report shows it is more accurately the **v2 audio-architecture anchor**. The WiFi-mode lineage runs through different commits.

### WiFi state immediately before vs after `723f4522`

| | Hash | Date | WiFi state |
|---|---|---|---|
| Before | `b4d41166` | 2025-12-29 | APSTA, STA primary, AP fallback. No mode change. |
| After | `eb9ad881` | 2025-12-31 | APSTA, STA primary. Web server gets audio-reactive endpoints — no mode change. |

The first WiFi-mode change after `723f4522` was `937c9abc` on **2026-01-04** (FORCE_AP_MODE flag added) — 6 days later.

---

## Task 10 — Verbatim doctrine evolution

| Phrase | First appearance | Last appearance / disappearance | Notes |
|---|---|---|---|
| `WIFI_MODE_AP` (broad — includes APSTA) | `1171b511` (2025-07-05) | active in HEAD | Continuous presence, but mode VALUE shifted: APSTA → STA-only-fallback → APSTA → AP-only. |
| `WIFI_MODE_APSTA` | `7700aba2` (2025-12-20, comment "for maximum flexibility") | active in HEAD's `WiFiManager.cpp` line where `requestSTAEnable()` temporarily sets it | Has appeared and disappeared multiple times. Currently exists ONLY inside the `requestSTAEnable()` escape hatch. |
| `WIFI_MODE_STA` only | implicitly the project default 2025-06-24 → 2025-07-05 (via `WiFi.begin()` calls) | Removed from default boot path at `d13889f8` (2026-02-17) | The escape hatch `wifi connect` serial command can still flip into STA-equivalent mode but it is not the boot default. |
| `WIFI_MODE_AP` (single, exclusive) | `937c9abc` (2026-01-04, build flag `FORCE_AP_MODE`) and `d13889f8` (2026-02-17, runtime `m_forceApOnly`) | active in HEAD | The doctrinal mode. |
| `WiFi.begin` | `1d73c14e` (2025-07-02) — first appearance after mDNS init | various touches; last appearance live in `WiFiManager.cpp` is gated behind `m_forceApOnly` check | — |
| `softAP` / `WiFi.softAP` | `1d73c14e` (2025-07-02) | active in HEAD as the boot path | First mentioned as fallback only; promoted to primary in 2026-02-17. |
| `lightwaveos.local` | `1d73c14e` (2025-07-02) — first appearance, mDNS hostname | active in HEAD's `WiFiCredentialManager` docs but **deprecated** because mDNS doesn't run in AP mode | Captain's calibration debt: the `.local` hostname appears in CHANGELOG/docs but is functionally unreachable. |
| `wifi_credentials` / `wifi_creds` | `f5d64c7f` (2025-12-16) — externalisation BREAKING CHANGE | active in HEAD — `firmware-v3/wifi_credentials.ini.template` | Replaced hardcoded `VX220-013F`. |
| `captive portal` / `captiveportal` | NEVER APPEARED in K1 firmware | N/A | Captain's reference list mentioned this; the `git log -G` search returned only claude-mem-related results, never K1. The pattern was **considered** in the Light Crystals docs but never built. |
| `dual-mode` / `Portable Mode` | `1d1589c6` (2026-02-05) | killed `d13889f8` (2026-02-17) — 12-day lifespan | Brief experiment. |
| `mDNS` / `Bonjour` | `1d73c14e` (2025-07-02) | active in HEAD but documented as not running in AP mode | Functionally dormant since `d13889f8`. |
| `AP-only` / `AP_ONLY` / `WIFI_AP_ONLY` | `937c9abc` (2026-01-04) — build flag `FORCE_AP_MODE` | active in HEAD | The doctrine name. Codified in build flag, then in code, then in docs (`network/CLAUDE.md` 2026-03-04). |
| `STA validation` | `11e040d6` (2026-05-03) — separate validation profile | active in HEAD | Latest tag — STA exists ONLY as a validation env, not production. |
| `WPA3` / `PMF` | `7393ca4c` (2025-07-04) — WiFi Optimizer Pro | dormant since `c8d12479` | Was implemented, never reached production. |
| `WirelessProtocol` / `WirelessManager` (ESP-NOW) | `16302cae` (2025-06-26) — first appearance | `8c0938f9` (2025-12-22) — moved to `src.v1.deprecated/wireless/`. Gone from current builds. | Distinct from WiFi STA/AP. ESP-NOW encoder mesh, never reached production. |

---

## DEFINITIVE chronological timeline (all eras combined)

| Date | Hash | What happened to WiFi | Era |
|---|---|---|---|
| 2025-06-24 | `34baa31d` | TRUE genesis. STA primary (`VX220-013F` hardcoded), AP `LightCrystals` fallback. | Light Crystals |
| 2025-06-26 | `5e5977d2` | Removed broken ESP-NOW wireless. | Light Crystals |
| 2025-06-26 | `16302cae` | Re-introduced ESP-NOW for encoder mesh (250B/<3ms). | Light Crystals |
| 2025-07-02 | `1d73c14e` | mDNS `lightwaveos.local` after `WiFi.connect()`. Documented as **not started in AP mode**. | Light Crystals |
| 2025-07-04 | `7393ca4c` | WiFi Optimizer Pro: 802.11 LR mode, adaptive TX power, WPA3 PMF, channel scanning. **Pinnacle of STA investment.** | Light Crystals |
| **2025-07-05** | **`c8d12479`** | **SMOKING GUN.** WiFi disabled by default. "Remove that worthless shit." RAM 21.2%→13.4%, Flash 65.1%→33.4%. | **WiFi-disabled** |
| 2025-07-17 | `574c7b8e` | Last K1 commit before hiatus. | WiFi-disabled |
| 2025-07-18 → 2025-12-08 | (none) | **5-month K1 hiatus.** | WiFi-disabled (dormant) |
| 2025-12-09 | `9b3f24b7` | K1 work resumes. WiFi still disabled by default. | WiFi-disabled |
| 2025-12-12 | `53de8f1e` | `pio run -e esp32dev_wifi` re-validated. | WiFi-disabled |
| 2025-12-16 | `f5d64c7f` | Hardcoded creds purged. `VX220-013F` → `CONFIGURE_ME`. Externalised template. | WiFi-rebuild |
| 2025-12-20 | `7700aba2` | **NEW v2 WiFiManager from scratch.** `WIFI_MODE_APSTA`, STA primary, AP `LightwaveOS-Setup` / `lightwave123` fallback. | v2 STA-primary |
| 2025-12-21 | `1311d95b` | Multi-network fallback (`WIFI_SSID_2`). | v2 STA-primary |
| 2025-12-22 | `8c0938f9` | v1 → `src.v1.deprecated/`. End of Light Crystals lineage. | v2 STA-primary |
| 2025-12-28 | `e27c008a` | Disabled WiFi modem sleep, enabled auto-reconnect. STA hardening. | v2 STA-primary |
| 2025-12-29 | `723f4522` | Captain's anchor. Beat tracker migration. **No WiFi-mode change.** | v2 STA-primary |
| 2026-01-02 | `4a40ee46` | First Tab5/AtomS3 client (K1.8encoderS3). | v2 STA-primary |
| 2026-01-04 | **`937c9abc`** | **AP-only build flag introduced.** `FORCE_AP_MODE` (default true). REST endpoints `/api/v1/network/sta/enable`, `/api/v1/network/ap/enable`. Verbatim: "No AP+STA dual-mode (historically unreliable)." | AP-only (build flag) |
| 2026-01-09 | `94479036` | Wireless Systems Alignment Audit doc created (filename says 2025-01, file actually from 2026-01). | AP-only (build flag) |
| 2026-01-17 | `0c797e32` | Removed K1.8encoderS3 + Tab5.8encoder. Tab5.encoder unified. | AP-only (build flag) |
| 2026-02-05 | **`1d1589c6`** | **Portable Mode reverts to APSTA.** "v2 is always the network." Hardcoded `LightwaveOS-AP` / `SpectraSynq`. | Portable Mode (APSTA) |
| 2026-02-05 | `0b270a48` | Same-day: skip forced STA reconnect when in APSTA. AP-stability symptoms. | Portable Mode (APSTA) |
| 2026-02-06 | `9ef320fc` | Skip STA scan loops on placeholder creds. | Portable Mode (APSTA) |
| 2026-02-07 | `78601d7a` | Memory hardening under combined-mode load. | Portable Mode (APSTA) |
| **2026-02-17** | **`d13889f8`** | **DOCTRINAL INFLECTION.** Boot into AP-only. `m_forceApOnly = true` runtime invariant. STA only via serial `wifi connect` escape. **Doctrine codified in code.** | AP-only doctrinal |
| 2026-02-17 | `283c9da3` | WebServer: 503 when `!WiFi.isAPUp()`. | AP-only doctrinal |
| 2026-02-17 | `afc1e982` | Tab5 hardcodes K1 AP target. | AP-only doctrinal |
| 2026-02-27 | `5ee8aa84` | Repository restructure. `firmware/v2` → `firmware-v3`. | AP-only doctrinal |
| **2026-03-04** | **`a39a03b8`** | **AP-only doctrine codified in docs.** Created `firmware-v3/src/network/CLAUDE.md` with the "K1 IS AP-ONLY. NEVER ENABLE STA MODE" warning that survives in HEAD. | AP-only doctrinal |
| 2026-03-04 | `54765b16` | `WIFI_AP_ONLY` build flag (default true). | AP-only doctrinal |
| 2026-03-05 | `86abcb12` | softAP() auth params removed (WPA handshake fix). | AP-only doctrinal |
| 2026-04-18 | `8134d4a4` | Forensic audit Wave 1: softAP retry + WiFi TWDT. "AP-ONLY INVARIANT" comment lands. | AP-only doctrinal |
| 2026-04-18 | `c126fbd9` | WS cooldown bug from tab5↔K1 reconnect churn investigation. | AP-only doctrinal |
| 2026-04-18 | `d5cd99a8` | AsyncTCP pinned to Core 1. Three parallel SSAs. | AP-only doctrinal |
| 2026-05-01 | `0cc34478` | Heap-shed fragmentation latch fix. iOS WS reconnect storm root cause. | AP-only doctrinal |
| **2026-05-03** | **`11e040d6`** | **Latest WiFi commit.** STA validation profile env added. Production stays AP-only. **STA finally returns — but only as a validation env, not a production default.** | AP-only doctrinal (with STA validation) |

---

## Era boundaries

1. **Light Crystals era (2025-06-24 → 2025-07-05).** STA primary, hardcoded home router `VX220-013F` credentials, AP `LightCrystals` as fallback. WiFi treated as a feature to be optimised — pinnacle was `7393ca4c` (Genesis Audio Sync + WiFi Optimizer Pro 802.11 LR / WPA3 PMF / adaptive TX power, 2025-07-04). 12 days. **11 days of investment, then capitulation in 24 hours.**
2. **WiFi-disabled era (2025-07-05 → 2025-12-15).** `c8d12479` flipped `FEATURE_WEB_SERVER` default to 0. WiFi opt-in only (`pio run -e esp32dev_wifi`). 5.4 months — but 5 months of those were the K1 development hiatus (no commits at all from the K1 author).
3. **v2 STA-primary rebirth era (2025-12-16 → 2026-01-03).** Hardcoded creds purged at `f5d64c7f`, fresh `WiFiManager` written at `7700aba2` with `WIFI_MODE_APSTA`. STA-primary doctrine restored. **The new author (SpectraSynq) appears not to have read or to have ignored `c8d12479`'s capitulation lesson — the same problem will be re-discovered.**
4. **AP-only build-flag era (2026-01-04 → 2026-02-04).** `937c9abc` introduced `FORCE_AP_MODE` build flag. STA still possible at runtime via `requestSTAEnable()`. 31 days. The doctrine was *available* but not yet enforced.
5. **Portable Mode AP+STA experiment (2026-02-05 → 2026-02-16).** `1d1589c6` reverted to APSTA "v2 is always the network". 12 days. **Failed within hours of landing** — `0b270a48` same-day fix shows AP-stability symptoms, `9ef320fc` next day skips STA scan loops on placeholder creds, `78601d7a` 2 days later hardens memory under combined-mode load.
6. **AP-only doctrinal era (2026-02-17 → present).** `d13889f8` codified AP-only in code; `a39a03b8` (2026-03-04) codified it in docs. Has held since with continuous hardening (`8134d4a4` Apr 18 forensic audit Wave 1, `0cc34478` May 1 heap-shed fix, `11e040d6` May 3 STA validation profile as a separate env). 2.5+ months as of HEAD. **The current era.**

---

## The transition narrative

The project moved from "WiFi disabled — worthless shit" through to "AP-only-EVER doctrine" by repeating a 4-stage cycle **twice**:

1. **Investment.** Build STA-primary infrastructure with strong network features (channel scanning, WPA3, mDNS, multi-network fallback).
2. **Failure.** Discover that ESP32-S3's STA implementation is unreliable in real deployments (router auth failures, AP+STA contention, scan-loop AP disruption, placeholder-cred regressions).
3. **Capitulation.** Disable STA mode, often with vivid frustration in commit messages.
4. **Doctrinal hardening.** Codify "AP-only" first as a flag, then as a runtime invariant, then as documentation, then as a hard global constraint in CLAUDE.md.

Cycle 1 was extremely fast (June-July 2025): the STA-primary design was set in place at genesis on 2025-06-24, hardened with WiFi Optimizer Pro on 2025-07-04, and abandoned 24 hours later on 2025-07-05 with `c8d12479` "remove that worthless shit". The total investment-to-capitulation cycle was **11 days**. The author was `qaxzy` (the original Cursor-driven developer).

Cycle 2 was slower (Dec 2025 - Feb 2026): SpectraSynq rebuilt the network layer from scratch on 2025-12-20 with `7700aba2`, ran APSTA for two weeks, added the AP-only flag on 2026-01-04 with `937c9abc` (commit message: *"No AP+STA dual-mode (historically unreliable)"* — a textual nod to the lessons of cycle 1, even if not a direct citation), then attempted Portable Mode APSTA again on 2026-02-05 (`1d1589c6`), failed within hours, and finalised AP-only on 2026-02-17 (`d13889f8`). Total cycle: **62 days**.

The third reference — `11e040d6` (2026-05-03) — is the **escape hatch institutionalisation**. STA returns only as a validation env, separated from production by a different `default_envs` line. This is the project saying: "We accept STA as a debug tool. We do not accept it as a product feature."

The verbal evidence from commit messages preserves the human texture across both cycles:

- 2025-07-05 `c8d12479`: *"WiFi is now DISABLED BY DEFAULT because it's fucking worthless"* — Cycle 1 capitulation.
- 2026-01-04 `937c9abc`: *"No AP+STA dual-mode (historically unreliable)"* — Cycle 2 first acknowledgement.
- 2026-02-17 `d13889f8`: *"This prevents STA scanning/reconnection loops from destabilising the AP, which is the PRIMARY connection path for Tab5 and iOS clients"* — Cycle 2 codification.
- 2026-03-04 `a39a03b8` `network/CLAUDE.md`: *"K1 is an Access Point. It does NOT connect to external WiFi routers. This is a HARD architectural constraint. STA authentication FAILS at the ESP-IDF 802.11 driver level (AUTH_EXPIRE reason 2, AUTH_FAIL reason 202). This has been reproduced across multiple routers and 6+ firmware mitigation attempts — NONE worked."* — Cycle 2 doctrine.
- 2026-05-03 `11e040d6`: *"This profile is for REST/WS validation only after the approved AP failure criteria are met; it is not a production default."* — Cycle 2 institutionalised.

---

## Evidence-quality verdict for each era

| Era | Evidence quality | Notes |
|---|---|---|
| Light Crystals (2025-06-24 → 2025-07-05) | **GROUNDED.** Direct file reads of `network_config.h` at `34baa31d`, full diff stat of `c8d12479`, verbatim commit messages preserved. | Hardcoded `VX220-013F` SSID is unambiguous evidence of a real-world STA deployment expectation. |
| WiFi-disabled (2025-07-05 → 2025-12-15) | **GROUNDED.** Author silence verified via `git log --author=qaxzy --since=2025-07-18 --until=2025-12-08` (zero commits). | Hiatus is genuinely captured. |
| v2 STA-primary (2025-12-16 → 2026-01-03) | **GROUNDED.** `7700aba2` diff inspected; `WiFi.mode(WIFI_MODE_APSTA)` verbatim. | Mode-setting line directly observed. |
| AP-only build-flag (2026-01-04 → 2026-02-04) | **GROUNDED.** `937c9abc` commit message captured verbatim including the "No AP+STA dual-mode (historically unreliable)" line. | Flag default is observable in code at the commit. |
| Portable Mode AP+STA (2026-02-05 → 2026-02-16) | **GROUNDED.** `1d1589c6` commit message captured; `0b270a48` same-day stabilisation fix shows the AP-stability symptoms. | The 12-day lifespan is bounded by two well-observed commits. |
| AP-only doctrinal (2026-02-17 → present) | **GROUNDED.** `d13889f8` diff captured at line level; `network/CLAUDE.md` text in HEAD verified to match the `a39a03b8` introducing commit. | Continuous hardening commits preserved. |

**Verdict on the c8d12479-as-smoking-gun hypothesis:** **STRONGLY GROUNDED.** The commit (a) flips the WiFi-default flag in `features.h` from 1 to 0 with a 9-file diff, (b) carries verbatim emotional language ("fucking worthless", "for masochists", "that garbage") signalling a frustration moment from the original Light Crystals author, (c) lands exactly **24 hours** after the most-invested STA-hardening commit (`7393ca4c` WiFi Optimizer Pro 2025-07-04), and (d) sits at the head of an unbroken behavioural lineage — disabled-by-default 2025-07-05, dormant for 5 months, fresh start 2025-12-20 still STA-primary, repeats the same lesson, finally codified AP-only 2026-02-17, doctrine hardened 2026-03-04, hard-constraint reaffirmed in K1 Research Agent's `chore(firmware): add K1v2 STA validation profile` 2026-05-03. **It is the first instance of the doctrine that lives in HEAD.**

---

## What this report did NOT verify

- **Hardware behaviour assertions.** The CLAUDE.md doctrine claims STA fails with `AUTH_EXPIRE reason 2`, `AUTH_FAIL reason 202`, "across multiple routers and 6+ firmware mitigation attempts". This report did NOT validate those reason codes against ESP-IDF documentation, did NOT enumerate the 6+ mitigation attempts, and did NOT observe the actual hardware failure. Those claims live in `firmware-v3/src/network/CLAUDE.md` as authoritative but their underlying evidence (router model, firmware version, 802.11 capture) is not present in git history this audit examined.
- **Whether `c8d12479`'s "RAM 21.2% → 13.4%" measurement was reproduced.** The commit asserts the savings; the build envs did exist (verified in the diff stat including `platformio.ini`), but the actual binary size deltas are not reproduced here.
- **Captain's hypothetical alternative WiFi-mode work** that may exist on the 7 remote-only branches (`feature/dt-correction-upgrade`, `feature/emotiscope-tempo-replacement`, `feature/voice-control-exploration`, `infallible-perlman`, etc.). This report ran `git log --all` which includes them, but did not exhaustively examine each branch's tree state.
- **Whether the "VX220-013F" / "3232AA90E0F24" credentials still expose private network info in public history.** They are visible at `34baa31d` and remained committed until `f5d64c7f` (2025-12-16). Whether this constitutes a credential leak under current Captain governance is out of scope for this archaeological pass.
- **The `0c797e32` (2026-01-17) deletion of `firmware/K1.8encoderS3/` and `firmware/Tab5.8encoder/`.** Those subprojects had their own WiFiManager / WebSocketClient lifetimes. This report only summarised them; the full per-line evolution within their 15-day lifespan was not enumerated.
- **The `Tab5.encoder` lineage post-`0c797e32`.** This report focused on K1-side WiFi mode. Tab5's `WiFiManager.cpp` exists in HEAD (`tab5-encoder/src/network/`); its mode-setting evolution was not traced.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:SSA-Archaeology | Created. Exhaustive backwards-traversal WiFi archaeology from `34baa31d` (TRUE genesis 2025-06-24) through HEAD (2026-05-03). 6 eras identified, 222 K1-author commits scanned, 66 content-touching commits, c8d12479 verbatim verified, AP-only doctrine traced from build flag (2026-01-04) through code (2026-02-17) through docs (2026-03-04) through STA validation profile separation (2026-05-03). |
