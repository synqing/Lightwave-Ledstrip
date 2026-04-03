---
abstract: "Complete end-to-end memory audit of Tab5 encoder (ESP32-P4). 14-agent parallel investigation covering SRAM/FLASH/PSRAM allocation, font waste, const-correctness, heap lifecycle, LVGL config, and ESP32-P4 deployment. Key findings: 2.68 MB removable fonts, 16 KB reducible name caches, PSRAM correctly used, no leaks in hot path."
---

# Tab5 Encoder Memory Audit — 2026-04-03

**Platform:** ESP32-P4 (RISC-V, dual-core 400 MHz)
**Hardware:** M5Stack Tab5 — 768 KB SRAM, 32 MB PSRAM, 16 MB FLASH
**Methodology:** 14 parallel SSA agents, exhaustive source inspection of 142 files (119K LOC)

---

## Executive Summary

The Tab5 encoder firmware is **well-architected for memory** with one major exception: **2.68 MB of dead font data in FLASH**. Internal SRAM usage is healthy (~330 KB of ~411 KB available), PSRAM is correctly used for the display buffer, and the hot path (loop) is allocation-free. The codebase is already const-correct for most data — only 192 bytes of LVGL grid arrays can be trivially migrated from SRAM to FLASH.

### Verdict: HEALTHY with cleanup opportunities

| Category | Status | Action |
|----------|--------|--------|
| Partition table | **CRITICAL — 12 MB FLASH wasted** | Using 4 MB `huge_app.csv` on 16 MB device |
| Board definition | **CRITICAL — wrong board** | Using generic `esp32-p4-evboard` instead of `m5stack-tab5-p4` |
| FLASH waste (fonts) | CRITICAL — 2.68 MB dead | Remove 19 unused font files + 1 dead TTF |
| SRAM static globals | MODERATE — 18.7 KB | Consider lazy-loading effect/palette name caches |
| Heap management | GREEN | No leaks in hot path, PSRAM correctly placed |
| LVGL configuration | GREEN | 200 KB heap, ~80 KB headroom, 160 KB draw buffer in PSRAM |
| Const-correctness | GREEN | 192 bytes fixable, rest already optimised |
| Monitoring | RED — disabled | Re-enable heap + stack watermark logging |
| ESP32-P4 deployment | YELLOW | Missing sdkconfig optimisations (PSRAM speed, cache, BSS placement) |
| Compiler optimisation | YELLOW | Missing `-flto`, `--gc-sections` not confirmed |

---

## 1. Memory Map — Current State

### Internal SRAM Budget (~411 KB available after boot)

| Consumer | Bytes | % of Available | Source |
|----------|-------|----------------|--------|
| Static globals (BSS/data) | 18,700 | 4.4% | ssa-1, ssa-13 |
| Heap — permanent objects | 82,000–100,000 | 20–24% | ssa-2 |
| LVGL internal heap (LV_MEM_SIZE) | 204,800 | 49.8% | ssa-5 |
| FreeRTOS task stacks | 14,336 | 3.5% | ssa-9 |
| Sync primitives (mutexes) | 120 | <0.1% | ssa-9 |
| **Total committed** | **~330 KB** | **~80%** | |
| **Free headroom** | **~80 KB** | **~20%** | |

### PSRAM Budget (32 MB available)

| Consumer | Bytes | Source |
|----------|-------|--------|
| LVGL draw buffer (1280×64×16bpp) | 163,840 | ssa-5 |
| **Total used** | **160 KB** | |
| **Free** | **~31.8 MB** | **Massively underutilised** |

### FLASH Budget (16 MB — but only 4 MB partitioned!)

**CRITICAL: The `huge_app.csv` partition table only addresses 4 MB. 12 MB of FLASH is inaccessible.**

| Consumer | Current (4 MB partition) | With `default_16MB.csv` | Source |
|----------|------------------------|------------------------|--------|
| App partition | 3 MB (1 partition) | 6.25 MB × 2 (OTA) | ssa-10 |
| SPIFFS | 896 KB | 3,375 KB | ssa-10 |
| NVS + OTA + PHY | 92 KB | 92 KB | ssa-10 |
| Coredump | 64 KB | 64 KB | ssa-10 |
| Font bitmaps (36 files) | 4,395 KB | 4,395 KB | ssa-6 |
| — of which UNUSED | 2,744 KB | 2,744 KB | ssa-6 |
| **Wasted (unpartitioned)** | **12 MB** | **0** | ssa-10 |

**Fix:** Change `board_build.partitions = huge_app.csv` → `default_16MB.csv` in platformio.ini.
**Also:** Change `board = esp32-p4-evboard` → `m5stack-tab5-p4` for correct Tab5 pin mappings and USB CDC config.

---

## 2. Top 5 SRAM Consumers

| Rank | Item | Bytes | Location | Reducible? |
|------|------|-------|----------|------------|
| 1 | `s_effectNames[256][48]` | 12,288 | main.cpp:155 | YES — lazy-load or LRU cache |
| 2 | `s_paletteNames[80][48]` | 3,840 | main.cpp:175 | YES — lazy-load or LRU cache |
| 3 | `s_fxParamSlots[16]` | 1,296 | main.cpp:204 | NO — active runtime state |
| 4 | `g_wsClient` (WebSocketClient) | ~1,024 | main.cpp:114 | NO — essential |
| 5 | `s_effectKnown[256]` + `s_paletteKnown[80]` | 336 | main.cpp:156,176 | Coupled to name caches |

**The effect/palette name caches (16.1 KB) are the single largest optimisation target.** They pre-allocate space for 256 effects × 48 chars and 80 palettes × 48 chars. If the actual K1 effect list is smaller (e.g., 100 effects), a hash map or LRU cache could save 7–10 KB.

---

## 3. FLASH Waste — 2.68 MB Removable Fonts

**All 36 font files are correctly `const` and in FLASH (.rodata).** Zero SRAM waste from fonts.

However, **19 of 36 font files are never referenced** in any UI code:

| Font Family | Unused Sizes | Total Removable |
|-------------|-------------|-----------------|
| Bebas Neue Light | 24, 32, 40, 48 px | 361 KB |
| Rajdhani Regular | 24, 32, 40, 48 px | 379 KB |
| Rajdhani Medium | 32, 40, 48 px | 350 KB |
| Rajdhani Bold | 40, 48 px | 304 KB |
| JetBrains Mono Regular | 32, 40, 48 px | 462 KB |
| JetBrains Mono Bold | 32, 40, 48 px | 498 KB |
| **Subtotal (19 files)** | | **2,355 KB** |
| Dead TTF: `bebas_neue_regular_ttf.h` | — | **380 KB** |
| **Grand total removable** | | **2,744 KB (2.68 MB)** |

**Edge cases to verify before deletion:**
- `bebas_neue_72px.c` (337 KB) — declared but no grep hits; confirm if used via macro
- `bebas_neue_bold_48px.c` (164 KB) — declared but not explicitly referenced

---

## 4. SRAM→FLASH Migration Candidates

### Confirmed Safe (192 bytes)

12 LVGL grid descriptor arrays in DisplayUI.cpp and ControlSurfaceUI.cpp are `static lv_coord_t` but never written to after initialisation. Adding `const` moves them to FLASH:

| File | Variable | Fix | Savings |
|------|----------|-----|---------|
| DisplayUI.cpp:421 | `col_dsc[9]` | Add `const` | 36 B |
| DisplayUI.cpp:424 | `row_dsc[2]` | Add `const` | 8 B |
| DisplayUI.cpp:490 | `fx_col_dsc[9]` | Add `const` | 36 B |
| DisplayUI.cpp:494 | `fx_row_dsc[2]` | Add `const` | 8 B |
| DisplayUI.cpp:626 | `zone_col_dsc[9]` | Add `const` | 36 B |
| DisplayUI.cpp:630 | `zone_row_dsc[2]` | Add `const` | 8 B |
| DisplayUI.cpp:675 | `preset_col_dsc[9]` | Add `const` | 36 B |
| DisplayUI.cpp:679 | `preset_row_dsc[2]` | Add `const` | 8 B |
| DisplayUI.cpp:732 | `mode_col_dsc[7]` | Add `const` | 28 B |
| DisplayUI.cpp:735 | `mode_row_dsc[2]` | Add `const` | 8 B |
| ControlSurfaceUI.cpp:64 | `col_dsc[9]` | Add `const` | 36 B |
| ControlSurfaceUI.cpp:126 | `row_dsc[2]` | Add `const` | 8 B |

### Already Optimised

The rest of the codebase is already well const-qualified:
- `paletteLedColors[75][9][3]` — const (2,025 B in FLASH)
- `PARAMETER_TABLE[14]` — const (~420 B in FLASH)
- All DesignTokens/Theme colours — constexpr
- All config constants — constexpr
- All font data — const with `LV_ATTRIBUTE_LARGE_CONST`

---

## 5. Heap Health

### Permanent Allocations (~90 KB)

| Object | Size | Location | Leak? |
|--------|------|----------|-------|
| DisplayUI + child UIs | ~70 KB | Internal heap | No — cleanup() exists |
| WebSocketClient + HTTP | ~18 KB | Internal heap | **g_otaServer never freed** |
| DualEncoderService | ~6 KB | Internal heap | No |
| Other (presets, params, buttons) | ~4 KB | Internal heap | No |
| Display draw buffer | 160 KB | **PSRAM** | No |

### Hot Path (loop) — Allocation-Free

The main loop is **effectively zero-allocation** in steady state:
- Encoder polling: pre-allocated buffers
- LVGL render: internal pool (no per-frame malloc)
- WebSocket: JSON parsing only on message receipt (debounced)
- Serial: only allocates on command receipt (rare)

### Issues Found

1. **`g_otaServer` (AsyncWebServer)** — allocated on WiFi connect, never freed. ~10-20 KB leak. Low priority (allocates once).
2. **`WebSocketClient._sendMutex`** — not freed in destructor. ~60 B leak. Trivial fix.
3. **`lgfx::PPASrm` in LoadingScreen** — allocated, never freed. Fix: delete after loading screen hidden.

---

## 6. LVGL Configuration Assessment

| Setting | Current | Assessment |
|---------|---------|------------|
| LV_MEM_SIZE | 200 KB | Adequate. ~80 KB free after widgets. |
| LV_MEM_CUSTOM | 1 (custom allocator) | Good — allows PSRAM routing |
| Draw buffer | 1280×64×16bpp = 160 KB | Efficient partial render. In PSRAM. |
| Render mode | PARTIAL | Correct for memory efficiency |
| Refresh period | 16 ms (62.5 FPS) | Fine for control surface |
| Colour depth | 16-bit RGB565 | Correct |
| Widget count | ~350-420 total, ~200 active | Pre-allocated at boot (good) |
| Styles | 6 lv_style_t (static) | Excellent — no dynamic allocation |
| Label pattern | 154× set_text, 3× fmt, 46× snprintf | Safe. ~150 B convertible to _static() |
| Disabled features | 24 unused widget types | Good curation |

### LVGL Recommendation
Consider a custom PSRAM-aware allocator that routes allocations >4 KB to PSRAM. This would free ~35-42 KB of widget memory from internal SRAM to PSRAM. See section 8.

---

## 7. Subsystem Memory Summary

| Subsystem | SRAM | PSRAM | FLASH | Agent | Health |
|-----------|------|-------|-------|-------|--------|
| Input (I2C, encoders) | 1.4 KB | — | — | ssa-7 | EXCELLENT |
| Network (WiFi, WS, HTTP) | ~1 KB + WiFi stack | — | — | ssa-8 | GOOD (WiFi disabled) |
| UI (LVGL widgets, styles) | ~35-42 KB (in LVGL heap) | 160 KB buffer | 264 KB fonts | ssa-5, ssa-14 | GOOD |
| Fonts | 0 | — | 4,395 KB (2,744 removable) | ssa-6 | NEEDS CLEANUP |
| Main lifecycle | 18.7 KB static | — | — | ssa-13 | MODERATE |
| FreeRTOS tasks | 14.3 KB stacks | — | — | ssa-9 | GOOD (no watermarks) |

---

## 8. ESP32-P4 Deployment — Recommended Configuration

### Missing sdkconfig Optimisations

Based on ssa-11 (P4 architecture) and ssa-12 (deployment best practices), the current build is missing several memory optimisations:

```ini
# PSRAM — currently only -DBOARD_HAS_PSRAM, needs explicit config
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_HEX=y                        # P4 HEX mode (only option)
CONFIG_SPIRAM_SPEED_200M=y                       # Max bandwidth (currently unknown)
CONFIG_SPIRAM_USE_MALLOC=y                       # malloc() falls back to PSRAM
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096         # Keep <4KB in internal SRAM
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768      # Reserve 32KB for DMA
CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y    # Large static arrays → PSRAM

# Cache (P4-specific)
CONFIG_ESP32P4_INSTRUCTION_CACHE_SIZE=32KB
CONFIG_ESP32P4_DATA_CACHE_SIZE=32KB
CONFIG_CACHE_L2_CACHE_SIZE=256KB

# Compiler
CONFIG_COMPILER_OPTIMIZATION_SIZE=y              # -Os (better cache efficiency)

# lwIP (when WiFi enabled)
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=2920             # Reduce from 5744
CONFIG_LWIP_TCP_WND_DEFAULT=2920
```

### Build Flag Additions (platformio.ini)

```ini
build_flags =
    ; existing flags...
    -ffunction-sections -fdata-sections    ; Dead code elimination
    -Wl,--gc-sections                      ; Linker garbage collection
```

### Key P4 Facts for Future Development

- **PROGMEM is a no-op** on ESP32-P4. `const` data is automatically in FLASH via MMU cache.
- **PSRAM is inaccessible during FLASH writes** (OTA, NVS). Don't place real-time buffers there.
- **32 MB PSRAM at 200 MHz** is massively underutilised (only 160 KB used currently).
- **8 KB TCM** available for zero-wait deterministic access (unused by Tab5 currently).
- **768 KB SRAM** (vs 512 KB on S3) — 50% more headroom than K1.

---

## 9. Prioritised Action Items

### P0 — Quick Wins (< 1 hour, zero risk)

| # | Action | Savings | Risk |
|---|--------|---------|------|
| 1 | **Switch partition table to `default_16MB.csv`** | **12 MB FLASH unlocked** | None — config change |
| 2 | **Switch board to `m5stack-tab5-p4`** | Correct pin mappings | Low — verify USB CDC still works |
| 3 | Delete 19 unused font .c files + 1 dead TTF | **2.68 MB FLASH** | None — verified unused |
| 4 | Add `const` to 12 LVGL grid arrays | 192 B SRAM | None — confirmed read-only |
| 5 | Re-enable heap monitoring (uncomment existing code) | Visibility | None |
| 6 | Add `uxTaskGetStackHighWaterMark()` calls | Visibility | None |

### P1 — Medium Effort (1-4 hours, low risk)

| # | Action | Savings | Risk |
|---|--------|---------|------|
| 7 | Replace `String` members in HttpClient structs with `char[]` | ~1.1 KB heap fragmentation | Low — struct API change |
| 8 | Fix 3 minor memory leaks (otaServer, sendMutex, PPASrm) | ~20 KB | Low |
| 9 | Add `-flto=auto -Wl,--gc-sections` to build flags | Est. 5-15% code size | Low |
| 10 | Create `sdkconfig.defaults` with PSRAM/cache optimisations | Performance | Low |

### P2 — Significant Effort (4+ hours, moderate risk)

| # | Action | Savings | Risk |
|---|--------|---------|------|
| 11 | Replace `s_effectNames[256][48]` with LRU cache or lazy-load | Up to 12 KB SRAM | Medium — changes name resolution |
| 12 | Route LVGL heap to PSRAM via custom allocator | Up to 200 KB SRAM freed | Medium — latency change |
| 13 | Implement `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY` | Auto-move large statics to PSRAM | Medium — needs testing |
| 14 | Convert 150+ `lv_label_set_text()` to `_static()` where safe | ~150 B LVGL heap | Low but tedious |

### P3 — Future Architecture

| # | Action | Benefit |
|---|--------|---------|
| 13 | Lazy-load ZONE_COMPOSER/CONNECTIVITY screens (not pre-allocated) | ~13 KB LVGL heap on demand |
| 14 | Font subsetting (reduce character ranges if not using full ASCII) | FLASH savings per font |
| 15 | PPA hardware acceleration for display (LGFX_PPA already in deps) | CPU time, not memory |

---

## 10. Monitoring Gaps (Must Fix)

The firmware currently has **zero runtime memory visibility**:

- `ESP.getFreeHeap()` — 10 calls exist, **all commented out**
- `ESP.getMinFreeHeap()` — 10 calls exist, **all commented out**
- `ESP.getFreePsram()` — **never called anywhere**
- `uxTaskGetStackHighWaterMark()` — **never called anywhere**
- `heap_caps_get_free_size()` — **never called**
- `LV_USE_MEM_MONITOR` — **disabled** (0)
- `LV_USE_PERF_MONITOR` — **disabled** (0)

**Recommendation:** Add periodic heap/PSRAM/stack logging (every 30s) gated behind `CORE_DEBUG_LEVEL >= 3`. The infrastructure already exists — just uncomment the 10 existing call sites and add PSRAM + watermark checks.

---

## Appendix: Agent Coverage Matrix

| # | Agent | Domain | Files | Tokens | Key Finding |
|---|-------|--------|-------|--------|-------------|
| 1 | ssa-1-globals | Census | 80+ | 72K | 18.7 KB static SRAM, effect caches dominate |
| 2 | ssa-2-heap | Census | 121 | 61K | 90 KB heap, no leaks in hot path, PSRAM correct |
| 3 | ssa-3-const-flash | FLASH | 89 | 66K | 192 B fixable, rest already const |
| 4 | ssa-4-strings | FLASH | 80 | 56K | 15 String objects, format strings in FLASH |
| 5 | ssa-5-lvgl-config | LVGL | 6 | 49K | 200 KB heap, 160 KB PSRAM buffer, 80 KB free |
| 6 | ssa-6-fonts | LVGL | 36 | 51K | 2.68 MB removable, all const (FLASH) |
| 7 | ssa-7-i2c-input | Subsystem | 10+ | 73K | 1.4 KB total, 35 B/encoder, excellent |
| 8 | ssa-8-network | Subsystem | 8 | 71K | ~1 KB buffers, WiFi disabled (SDIO conflict) |
| 9 | ssa-9-stacks | RTOS | 85 | 54K | 14.3 KB stacks, no watermarks, 2 tasks |
| 10 | ssa-10-build | Build | 8 | 42K | 12 MB FLASH wasted (wrong partition table), missing -flto |
| 11 | ssa-11-p4-memory | Research | web | 54K | 768 KB SRAM, 32 MB PSRAM, PROGMEM no-op |
| 12 | ssa-12-p4-deploy | Research | web | 64K | sdkconfig, PSRAM 200 MHz, -Os, gc-sections |
| 13 | ssa-13-main | Lifecycle | 1 | 56K | 18.7 KB globals, allocation-free loop, cleanup exists |
| 14 | ssa-14-ui-widgets | LVGL | 20 | 84K | 350-420 widgets, pre-allocated, styles static |

**Total agent work: ~853K tokens across 14 parallel investigations.**

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-03 | agent:memory-audit | Created — 14-agent parallel memory audit synthesis |
| 2026-04-03 | agent:memory-audit | Updated — added ssa-10 build config findings (partition table, board def, -flto) |
