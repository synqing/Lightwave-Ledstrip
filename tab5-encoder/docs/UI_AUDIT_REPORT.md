---
abstract: "Comprehensive 6-agent audit of Tab5 encoder LVGL UI implementation. Covers colour tokens (38 unique, 46 inline, 2 CRITICAL mismatches), LVGL anti-patterns (2 CRITICAL: lv_btn with LV_USE_BTN=0, RGB565-in-lv_color_hex), widget tree (198 widgets on GLOBAL, 350-440 total), spec compliance (1 BLOCKER: EM SPREAD/STRENGTH dead, 9 divergences), secondary screens (ConnectivityTab widget-storm, ZoneComposerUI dead code), and widget/font inventory (all widgets/ are M5GFX-only, 36 LVGL fonts ready). Read before any implementation work."
---

# Tab5 UI Implementation Audit Report

**Date:** 2026-03-23
**Method:** 6 specialist agents deployed in parallel (LVGL UI/UX x4, Code Reviewer x1, LVGL UI/UX x1)
**Scope:** All files in `tab5-encoder/src/ui/`, referenced against implementation spec and LVGL component reference

---

## Executive Summary

| Severity | Count | Highlights |
|----------|-------|-----------|
| **CRITICAL** | 4 | lv_btn with LV_USE_BTN=0 (13 calls), RGB565 in lv_color_hex (ZoneComposerUI), LoadingScreen wrong grey, ConnectivityTab widget-storm |
| **BLOCKER** | 1 | EM SPREAD/STRENGTH mode buttons never display values |
| **WARNING** | 8 | 12 missing scrollable clears, dual zone colour palettes, dead FX Row B, sidebar event bubbling, 6 font divergences |
| **INFO** | 6 | Dead code (220 lines M5GFX, 3 dead font headers, 1 dead widget), identity mapping, 100+ disabled debug logs |

---

## 1. Colour Token Audit

### Status: DesignTokens.h exists, migration ~75% complete

| File | Migration % | Issues |
|------|------------|--------|
| SidebarWidget.cpp | 100% | Clean |
| ControlSurfaceUI.cpp | ~98% | 1 inline `0xFFFFFF` |
| DisplayUI.cpp | ~85% | 12 unique inline values, 20 occurrences |
| ConnectivityTab.cpp | ~70% | 9 unique inline values (keyboard colours) |
| ZoneComposerUI.cpp | ~65% | 5+ inline values, dual colour palette |
| LoadingScreen.cpp | 0% | Does not include DesignTokens.h |

### CRITICAL Value Mismatches

1. **LoadingScreen.cpp:148** — subtitle grey is `0x848484`, should be `0x9CA3AF` (DesignTokens::FG_SECONDARY). Visible colour bug.

2. **ZoneComposerUI dual palette** — `ZONE_COLORS` in `.h` (`0x6EE7F3, 0x22DD88, 0xFFB84D, 0x9D4EDD`) vs `zoneColors[]` in `.cpp:1237` (`0x00FF88, 0x00AAFF, 0xFF6600, 0xFF00AA`). Completely different colours for the same zones depending on which part of the UI you're looking at.

### Missing DesignTokens Constants

14 colours used inline with no named constant: `STATUS_GREEN` (0x00FF00), `STATUS_RED` (0xFF0000), `FG_ON_COLOUR` (0x000000), `FEEDBACK_SAVE` (0xFFE066), `FEEDBACK_DELETE` (0xFF3355), 5 keyboard colours, 3 zone styling colours.

---

## 2. LVGL Anti-Pattern Audit

### CRITICAL

| # | Anti-Pattern | Count | Files | Impact |
|---|-------------|-------|-------|--------|
| 1 | `lv_btn_create()` with `LV_USE_BTN=0` in lv_conf.h | 13 calls | ConnectivityTab (9), ControlSurfaceUI (1), DisplayUI (3) | Won't compile unless build system overrides LV_USE_BTN. Must verify platformio.ini or migrate to lv_obj+CLICKABLE. |
| 2 | `Theme::ACCENT` (RGB565 `0x07E0`) passed to `lv_color_hex()` | 1 call | ZoneComposerUI.cpp:626 | Selected zone border renders as near-black instead of intended cyan. Live visual bug. |

### WARNING

| # | Anti-Pattern | Count | Files |
|---|-------------|-------|-------|
| 3 | Missing `LV_OBJ_FLAG_SCROLLABLE` clear | 12 | DisplayUI (9 containers), ControlSurfaceUI (3 grid containers) |
| 4 | Widget creation in render loop | 1 (severe) | ConnectivityTab `refreshNetworkLists()` — destroys and recreates up to 330 widgets per dirty cycle at 10 FPS |
| 5 | SidebarWidget event target ambiguity | 1 | Uses `lv_event_get_target()` instead of `lv_event_get_current_target()` — event bubbling could produce wrong tab index |

### Clean Areas

- Style state targeting: correct throughout (LV_PART_MAIN vs LV_PART_INDICATOR)
- Grid descriptor lifetime: all `static` — no scope issues
- Screen lifecycle: screens created once, never leaked
- WDT resets: present in DisplayUI (14), ZoneComposerUI (5), ConnectivityTab (7). Missing in ControlSurfaceUI (0 resets, ~96 widgets).

---

## 3. Widget Tree & Layout

### GLOBAL Screen (~198 widgets)

```
_screen_global
├── _header (flex ROW, 1260x66, pos 10,7)
│   ├── effect_container (CLICKABLE → CONTROL_SURFACE)
│   ├── palette_container
│   ├── title_container (IGNORE_LAYOUT, absolute centre)
│   ├── right_spacer (flex_grow=1)
│   ├── net_container (CLICKABLE → CONNECTIVITY)
│   └── retry_button (HIDDEN)
├── SidebarWidget._root (abs pos 0,73, 70x578)
│   └── [x3] tabSlots (pill + accent + label)
├── _top_zone (abs pos 70,73, 1210x232)
│   ├── _gauges_container (GRID 8-col, 8 gauge cards)
│   └── _mode_container (GRID 6-col, 6 mode buttons)
├── _separator (abs pos 70,305, 1210x1)
├── _bottom_zone (abs pos 70,306, 1210x345)
│   ├── _fx_panel (GRID 8-col, 8 FX cards) — VISIBLE default
│   ├── _zones_panel (flex COL, selector + 8 zone cards) — HIDDEN
│   └── _presets_panel (GRID 8-col, 8 preset cards) — HIDDEN
└── _footer (flex ROW SPACE_BETWEEN, 1274x66, pos 3,651)
    ├── leftGroup (BPM, KEY, MIC, UPTIME)
    ├── k1_group (2 K1 selector buttons)
    └── rightGroup (WS status, battery bar)
```

### Other Screens (separate via lv_scr_load)

| Screen | Widget Est. | Owner |
|--------|------------|-------|
| _screen_zone | ~75 | ZoneComposerUI |
| _screen_connectivity | ~45 + dynamic | ConnectivityTab |
| _screen_control_surface | ~96 | ControlSurfaceUI |
| _network_config_screen | ~7 | On-demand |

**Total: ~350-440 widgets, ~60-80KB LVGL heap**

### Dead Code in Widget Tree

- **FX Row B**: 34 member pointers declared, never created. `updateEffectParamSlot()` for slots 8-15 silently no-ops.
- **`kEncoderToDisplayPos[8]`**: Identity mapping (maps i→i). Zero-value indirection.
- **`setConnectionState()`**: Empty function body — all params cast to void.
- **`getHeader()`**: Returns nullptr in LVGL branch.
- **`title_container`**: Local variable not stored as member — can't restyle after begin().
- **Header/footer margin asymmetry**: Header at 10px margins, footer at 3px margins.

---

## 4. Spec Compliance

### BLOCKER

**EM SPREAD and EM STRENGTH mode buttons (indices 4-5) never display values.** Buttons exist, callbacks fire, but `_mode_values[4]` and `_mode_values[5]` stay at "--" forever. No `setEdgeMixerSpreadStrength()` method exists. These are decoration.

### Spec Assumes But Doesn't Exist

| Feature | Status |
|---------|--------|
| FX PARAMS paging (enc 14-15 as page nav) | **Not implemented** — flat 8-card grid, no paging |
| ControlSurfaceUI absorption plan | **Unresolved** — header tap still navigates to separate screen |
| FX Row B (slots 8-15) | **Dead code** — declared but never instantiated |

### Font Divergences (6)

| Element | Spec Says | Implementation Uses | Issue |
|---------|----------|-------------------|-------|
| Gauge labels | Rajdhani Medium 18px | BEBAS_BOLD_24 | Different family + size |
| Gauge values | JetBrains Mono Bold 28px | JETBRAINS_MONO_REG_32 | 28px doesn't exist |
| Sidebar labels | Rajdhani Bold 14px | RAJDHANI_MED_18 | 14px doesn't exist |
| Footer titles | Rajdhani Medium 18px | RAJDHANI_MED_24 | Larger than spec |
| Footer values | JetBrains Mono Regular 18px | RAJDHANI_MED_24 | Wrong family + size |
| Sidebar inactive opacity | 25% | 70% | Deliberate — 20% was invisible on hardware |

---

## 5. Secondary Screens

### ZoneComposerUI (~75 widgets, 1430 lines)

**Issues:**
- CRITICAL: `Theme::ACCENT` RGB565 bug (line 626) — selection border wrong colour
- WARNING: Dual zone colour palettes (header vs LED strip preview)
- 220 lines dead M5GFX rendering code
- 3 dead methods (createZoneCountRow, createPresetRow, createBackButton)
- Zone count label init mismatch ("3" displayed, actual state is 0)
- Fire-and-forget WS commands, no ack or resync

**Redesign readiness (retain as deep-dive):**
- Sidebar must be hidden when active
- Back button callback must restore sidebar + return to ZONES tab
- Only uses encoders 0-3 (correct for 4-zone editing)
- Dead M5GFX code should be stripped first

### ConnectivityTab (~45 widgets + dynamic, 1420 lines)

**Issues:**
- CRITICAL: `refreshNetworkLists()` destroys and recreates up to 330 widgets per dirty cycle at 10 FPS. Worst performance pattern in the codebase.
- Zero encoder support (touch-only)
- All networking via REST (HttpClient), not WebSocket
- Arduino String heap allocations for state (_errorMessage etc.)
- 6 separate `new HttpClient()` allocation points

---

## 6. Widget & Font Inventory

### widgets/ Directory: ALL M5GFX-ONLY

| Widget | Purpose | LVGL? | Reusable? |
|--------|---------|-------|-----------|
| GaugeWidget | Parameter gauge cell | NO — M5GFX sprite | NO |
| ActionRowWidget | Touch action buttons | NO — M5GFX sprite | NO |
| UIHeader | Status bar header | NO — M5GFX sprite | NO |
| PresetSlotWidget | Preset bank indicator | NO — M5GFX sprite | Logic only (state machine) |
| PresetBankWidget | Alt preset display | NO — M5GFX sprite | DEAD CODE — never instantiated |
| PresetTypes.h | PresetSlotState enum | N/A | YES |

**The production LVGL UI is built entirely inline in DisplayUI.cpp (2300+ lines). Zero LVGL widget components exist.**

### Font System: READY

36 LVGL font .c files across 3 families:
- **Bebas Neue**: Regular 24-72px, Bold 24-48px, Light 24-48px
- **Rajdhani**: Regular 24-48px, Medium 18-48px, Bold 24-48px
- **JetBrains Mono**: Regular 24-48px, Bold 24-48px

2 active headers (experimental_fonts.h, bebas_neue_fonts.h), 3 dead headers (bebas_simple.h, bebas_neue_lvgl.h, bebas_neue_regular_ttf.h — 61KB dead TTF binary).

### Theme.h: PARTIALLY DEAD

- **Still useful:** `PARAM_NAMES[16]`, `SCREEN_W`, `SCREEN_H`, `STATUS_BAR_H`, layout constants
- **Dead for LVGL:** All 13 RGB565 colour constants, `dimColor()` helper, `PARAM_COLORS[8]`
- Included by 11 files but only for layout constants and param names

---

## Priority Remediation

### Before Redesign (must fix)

1. **Resolve lv_btn vs LV_USE_BTN=0** — verify build config or migrate 13 call sites to lv_obj+CLICKABLE
2. **Fix Theme::ACCENT RGB565 in ZoneComposerUI.cpp:626** — change to DesignTokens constant or 0x00FFFF
3. **Wire EM SPREAD/STRENGTH values** — add data path from K1 to `_mode_values[4]` and `_mode_values[5]`
4. **Consolidate zone colour palettes** — pick one palette, put it in DesignTokens.h
5. **Fix LoadingScreen.cpp:148** wrong grey value

### During Redesign (clean up)

6. Add scrollable clear to 12 containers
7. Add WDT resets to ControlSurfaceUI::begin()
8. Fix SidebarWidget to use `lv_event_get_current_target()`
9. Refactor ConnectivityTab::refreshNetworkLists() to update-in-place (not destroy+rebuild)
10. Delete dead code: FX Row B (34 pointers), 220 lines M5GFX in ZoneComposerUI, 3 dead font headers, PresetBankWidget, .bak2 file
11. Complete DesignTokens migration: add 14 missing constants, migrate LoadingScreen.cpp
12. Reconcile font usage with spec typography table

---

## Source File Reference

| File | Lines | Key Finding |
|------|-------|-------------|
| `src/ui/DisplayUI.cpp` | ~2300 | All LVGL UI inline, 198 widgets, 34 dead pointers, mixed abs/grid/flex |
| `src/ui/DisplayUI.h` | 299 | Dead FX Row B arrays, dead setConnectionState/getHeader |
| `src/ui/ControlSurfaceUI.cpp` | 682 | 0 WDT resets, 1 lv_btn call, semantic contracts documented in CONTROLSURFACE_RESEARCH.md |
| `src/ui/ZoneComposerUI.cpp` | 1430 | RGB565 bug line 626, 220 dead lines, dual colour palettes |
| `src/ui/ConnectivityTab.cpp` | 1420 | Widget-storm in refreshNetworkLists, 9 lv_btn calls, 9 inline colours |
| `src/ui/SidebarWidget.cpp` | 177 | 100% tokenised, event target fragility |
| `src/ui/widgets/` | ~1500 | ALL M5GFX-only, not used in LVGL builds |
| `src/ui/DesignTokens.h` | 84 | make_card factory, 24 tokens, partially adopted |
| `src/ui/Theme.h` | ~200 | Partially dead — layout constants useful, RGB565 colours dead |

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-23 | agent:audit-council (6 specialists) | Created — comprehensive UI audit from 6 parallel LVGL specialist agents |
