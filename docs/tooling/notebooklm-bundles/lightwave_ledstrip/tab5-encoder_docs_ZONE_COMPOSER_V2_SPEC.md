---
abstract: "Final UI spec for Tab5 Zone Composer v2 (LED-Centric layout). 1280x720 LVGL 9.3, 5-col param grid, hero strip visualiser, 3-col zone overview. Encoder mapping, widget tree, pixel budget, and execution plan. Reference before implementing ZoneComposerUI rewrite."
---

# Zone Composer v2 — LED-Centric Layout Spec

## Design Philosophy

The LED strip is the hero. A 1200px-wide strip visualisation dominates the upper portion, with zone-coloured tappable segments and a gold centre marker at 79/80. Below: 5 encoder-mapped parameter cards for the selected zone. Bottom: 3-column zone overview for at-a-glance comparison of all zones.

## Canonical Reference

The iOS app (`lightwave-ios-v2`) is the source of truth for Zone Composer behaviour. This spec brings Tab5 to feature parity.

### iOS Features Missing from Current Tab5

| Feature | iOS | Tab5 Current | Tab5 v2 |
|---------|-----|-------------|---------|
| 3-zone support (Triple Rings) | Yes | No (clamped to 2) | Yes |
| Brightness control | Slider per zone | Deprecated from UI | ENC 3 per zone |
| Blend mode control | Full picker (0-7) | Display-only | ENC 4 per zone |
| Zone boundary adjust | Drag slider | Not available | ENC 5 long-press (stretch) |
| Semantic names | INNER/MIDDLE/OUTER | Generic Zone 1/2/3 | INNER/MIDDLE/OUTER |
| Zone colours | Cyan/Green/Purple | Cyan/Green/Orange | Cyan/Green/Purple |
| All-zones overview | Always visible | Not available | 3-col overview cards |

---

## Screen Dimensions

```
Display: 1280 x 720 (fixed, no responsive scaling)
Content pad: 20px left/right = 1240px usable width
```

---

## Encoder Mapping

| ENC | Function | Range | Click | Long-Press |
|-----|----------|-------|-------|------------|
| 0 | Effect (selected zone) | 0-103, wrap | — | — |
| 1 | Palette (selected zone) | 0-74, wrap | — | — |
| 2 | Speed (selected zone) | 1-100, clamp | — | — |
| 3 | Brightness (selected zone) | 0-255, clamp | — | — |
| 4 | Blend Mode (selected zone) | 0-7, wrap | — | — |
| 5 | Zone Select | cycles 1→2→3→1 | — | Enter boundary adjust mode (stretch) |
| 6 | Zone Count | 2↔3 | — | — |
| 7 | Reserved (rotate) | — | Toggle zone mode ON/OFF | — |

ENC 0-4 always control the **currently selected** zone. ENC 5 changes which zone is selected.

---

## Zone Colours (aligned to iOS canonical)

```cpp
static constexpr uint32_t ZONE_1_COLOUR = 0x00FFFF;  // Cyan  — INNER
static constexpr uint32_t ZONE_2_COLOUR = 0x00FF99;  // Green — MIDDLE
static constexpr uint32_t ZONE_3_COLOUR = 0x9900FF;  // Purple — OUTER
```

Zone roles:
- 2-zone mode: Zone 1 = INNER, Zone 2 = OUTER (Zone 3 hidden)
- 3-zone mode: Zone 1 = INNER, Zone 2 = MIDDLE, Zone 3 = OUTER

---

## Widget Tree

```
_screen_zone (lv_obj, 1280x720, bg=0x0A0A0B, pad=0, NO_SCROLL)
│
│══ HEADER (50px) ═══════════════════════════════════════════════════
│
├── _header (lv_obj, flex ROW SPACE_BETWEEN, 1240x50, x=20 y=10)
│   │   bg=TRANSP, border=0, NO_SCROLL
│   │
│   ├── _backButton (make_card elevated, 100x44, CLICKABLE)
│   │   │   bg=0x1A1A1C, border=1px 0x1E1E24, radius=14
│   │   └── "< BACK" (lv_label, RAJDHANI_MED_24, 0xFFC700, centre)
│   │       flags: !CLICKABLE, EVENT_BUBBLE
│   │
│   ├── _titleContainer (lv_obj, IGNORE_LAYOUT, absolute centre)
│   │   └── "ZONE COMPOSER" (lv_label, BEBAS_BOLD_40, 0xFFFFFF)
│   │
│   └── _rightControls (lv_obj, flex ROW, gap=10, TRANSP)
│       ├── _zoneCountCard (make_card, 120x44, CLICKABLE) [ENC 6]
│       │   ├── "ZONES:" (lv_label, RAJDHANI_MED_24, 0x9CA3AF)
│       │   └── _zoneCountValue (lv_label, "3", JETBRAINS_MONO_BOLD_32, 0xFFFFFF)
│       │
│       └── _zoneEnableBtn (make_card, 150x44, CLICKABLE) [ENC 7 click]
│           │   border: 2px 0x22C55E (ON) / 0xEF4444 (OFF)
│           └── _zoneEnableLabel (lv_label, RAJDHANI_BOLD_24)
│               text: "ZONES: ON" (0x22C55E) / "ZONES: OFF" (0xEF4444)
│
│══ MODE ROW (36px) ═════════════════════════════════════════════════
│
├── _modeRow (lv_obj, flex ROW CENTER, 1240x36, x=20 y=64, gap=14)
│   │   bg=TRANSP, NO_SCROLL
│   │
│   ├── _zoneSelectorBtns[0] (make_card, 130x36, CLICKABLE)
│   │   │   border: 2px 0x00FFFF (selected) / 1px 0x1E1E24
│   │   └── "ZONE 1 · INNER" (lv_label, RAJDHANI_BOLD_24, 0x00FFFF)
│   │
│   ├── _zoneSelectorBtns[1] (make_card, 150x36, CLICKABLE)
│   │   │   border: 2px 0x00FF99 (selected) / 1px 0x1E1E24
│   │   └── "ZONE 2 · MIDDLE" (lv_label, RAJDHANI_BOLD_24, 0x00FF99)
│   │       [text changes to "ZONE 2 · OUTER" in 2-zone mode]
│   │
│   └── _zoneSelectorBtns[2] (make_card, 140x36, CLICKABLE, HIDDEN in 2-zone)
│       │   border: 2px 0x9900FF (selected) / 1px 0x1E1E24
│       └── "ZONE 3 · OUTER" (lv_label, RAJDHANI_BOLD_24, 0x9900FF)
│
│══ LED STRIP VISUALISER — HERO (130px) ═════════════════════════════
│
├── _stripContainer (lv_obj, 1200x130, x=40 y=108, NO_SCROLL)
│   │   bg=0x0A0A0B, border=0
│   │
│   ├── _stripLabelRow (lv_obj, 1160x24, x=20 y=0, TRANSP, NO_SCROLL)
│   │   ├── _stripZoneLabels[0] ("ZONE 1 · INNER", RAJDHANI_BOLD_24, 0x00FFFF)
│   │   ├── _stripZoneLabels[1] ("ZONE 2 · MIDDLE", RAJDHANI_BOLD_24, 0x00FF99)
│   │   └── _stripZoneLabels[2] ("ZONE 3 · OUTER", RAJDHANI_BOLD_24, 0x9900FF, HIDDEN in 2-zone)
│   │       [positioned: absolute centre of each zone's segment span]
│   │
│   ├── _stripBar (lv_obj, 1160x60, x=20 y=26, bg=0x121214, border=1px 0x2A2A2E, r=8)
│   │   │
│   │   │   Zone segments (coloured overlays, CLICKABLE — tap selects zone):
│   │   │   Selected zone: bg_opa=LV_OPA_60, border=2px zone colour
│   │   │   Unselected:    bg_opa=LV_OPA_30, border=0
│   │   │
│   │   ├── _zoneSegs[0] (Zone 1 — single centre block, bg=0x00FFFF, r=6)
│   │   ├── _zoneSegs[1] (Zone 2 LEFT half, bg=0x00FF99, r=6)
│   │   ├── _zoneSegs[2] (Zone 2 RIGHT mirror, bg=0x00FF99, r=6)
│   │   ├── _zoneSegs[3] (Zone 3 LEFT half, bg=0x9900FF, r=6, HIDDEN in 2-zone)
│   │   ├── _zoneSegs[4] (Zone 3 RIGHT mirror, bg=0x9900FF, r=6, HIDDEN in 2-zone)
│   │   │
│   │   └── _centreMarker (lv_obj, 4x60, bg=0xFFC700, r=2)
│   │       x = (79.5/160)*1160 = 576px — created LAST for z-order
│   │
│   └── _stripTickRow (lv_obj, 1160x24, x=20 y=92, TRANSP, NO_SCROLL)
│       ├── "0" (JETBRAINS_MONO_REG_24, 0x4B5563)
│       ├── "79|80" (JETBRAINS_MONO_REG_24, 0xFFC700, x=556)
│       └── "159" (JETBRAINS_MONO_REG_24, 0x4B5563)
│       [+ dynamic boundary labels at zone split positions]
│
│══ SELECTED ZONE INDICATOR (36px) ══════════════════════════════════
│
├── _selectedZoneRow (lv_obj, flex ROW CENTER, 1240x36, x=20 y=244)
│   ├── _selectedZoneDot (lv_obj, 8x8, r=CIRCLE, bg=zone colour)
│   └── _selectedZoneTitle (lv_label, BEBAS_BOLD_40, zone colour)
│       text: "ZONE 1 PARAMETERS" / "ZONE 2 PARAMETERS" / "ZONE 3 PARAMETERS"
│
│══ PARAMETER GRID — 5 COLUMNS (160px) ══════════════════════════════
│
├── _paramGrid (lv_obj, GRID 5-col, 1240x160, x=20 y=284, NO_SCROLL)
│   │   static lv_coord_t col_dsc[6] = {FR(1), FR(1), FR(1), FR(1), FR(1), LAST};
│   │   static lv_coord_t row_dsc[2] = {150, LAST};
│   │   pad_column=14
│   │   Card width = (1240 - 4×14) / 5 = 236.8px each
│   │
│   ├── _paramCards[0] : EFFECT (col 0) [ENC 0]
│   │   │   make_card, bg=0x121214, border=1px 0x1E1E24, r=14, pad=10
│   │   ├── "EFFECT" (RAJDHANI_MED_24, 0x9CA3AF, TOP_MID y=0)
│   │   ├── _paramValues[0] ("042", JETBRAINS_MONO_REG_32, 0xFFFFFF, TOP_MID y=28)
│   │   ├── _paramNames[0] ("Chroma Wave", RAJDHANI_MED_24, 0x9CA3AF, TOP_MID y=66)
│   │   │   long_mode=SCROLL_CIRCULAR, max_width=210
│   │   └── _paramBars[0] (lv_bar, 0-103, 90%x8, BOTTOM_MID y=-8)
│   │       track=0x2A2A2E, indicator=zone colour, r=4
│   │
│   ├── _paramCards[1] : PALETTE (col 1) [ENC 1]
│   │   │   Same structure as EFFECT
│   │   ├── "PALETTE" label
│   │   ├── _paramValues[1] ("012")
│   │   ├── _paramNames[1] ("Ocean Breeze")
│   │   └── _paramBars[1] (0-74, indicator=zone colour)
│   │
│   ├── _paramCards[2] : SPEED (col 2) [ENC 2]
│   │   ├── "SPEED" label
│   │   ├── _paramValues[2] ("50", JETBRAINS_MONO_REG_32)
│   │   └── _paramBars[2] (1-100, indicator=zone colour)
│   │
│   ├── _paramCards[3] : BRIGHTNESS (col 3) [ENC 3]
│   │   ├── "BRIGHTNESS" label
│   │   ├── _paramValues[3] ("200", JETBRAINS_MONO_REG_32)
│   │   └── _paramBars[3] (0-255, indicator=zone colour)
│   │
│   └── _paramCards[4] : BLEND MODE (col 4) [ENC 4]
│       ├── "BLEND" label
│       ├── _paramValues[4] ("0", JETBRAINS_MONO_REG_32)
│       ├── _paramNames[4] ("Overwrite", RAJDHANI_MED_24, 0x9CA3AF)
│       └── _paramBars[4] (0-7, indicator=zone colour)
│
│══ ZONE OVERVIEW — 3 COLUMNS (170px) ═══════════════════════════════
│
├── _overviewGrid (lv_obj, GRID 3-col, 1240x170, x=20 y=458, NO_SCROLL)
│   │   static lv_coord_t ov_col[4] = {FR(1), FR(1), FR(1), LAST};
│   │   static lv_coord_t ov_row[2] = {160, LAST};
│   │   pad_column=14
│   │   Card width = (1240 - 2×14) / 3 = 404px each
│   │
│   ├── _overviewCards[0] : ZONE 1 (col 0, CLICKABLE → selects Zone 1)
│   │   │   make_card, bg=0x121214, r=14
│   │   │   SELECTED: border=2px 0x00FFFF
│   │   │   UNSELECTED: border=1px 0x1E1E24
│   │   │
│   │   ├── _ovHeaders[0] (flex ROW SPACE_BETWEEN, h=24)
│   │   │   ├── _ovDot[0] (8x8, r=CIRCLE, bg=0x00FFFF)
│   │   │   ├── "ZONE 1 — INNER" (RAJDHANI_BOLD_24, 0x00FFFF)
│   │   │   └── _ovLedRange[0] ("40-79 | 80-119", JETBRAINS_MONO_REG_24, 0x4B5563)
│   │   │
│   │   ├── _ovEffectRow[0] (flex ROW SPACE_BETWEEN, h=24)
│   │   │   ├── "FX" (RAJDHANI_MED_24, 0x9CA3AF)
│   │   │   └── _ovEffectVal[0] ("042 Chroma Wave", JETBRAINS_MONO_REG_24, 0xFFFFFF)
│   │   │       max_width=280, long_mode=SCROLL_CIRCULAR
│   │   │
│   │   ├── _ovPaletteRow[0] (flex ROW SPACE_BETWEEN, h=24)
│   │   │   ├── "PAL" (RAJDHANI_MED_24, 0x9CA3AF)
│   │   │   └── _ovPaletteVal[0] ("012 Ocean Breeze", JETBRAINS_MONO_REG_24, 0xFFFFFF)
│   │   │
│   │   ├── _ovStatsRow[0] (flex ROW SPACE_BETWEEN, h=24)
│   │   │   ├── "SPD 50" (JETBRAINS_MONO_REG_24, 0x9CA3AF)
│   │   │   ├── "BRI 200" (JETBRAINS_MONO_REG_24, 0x9CA3AF)
│   │   │   └── "Overwrite" (RAJDHANI_MED_24, 0x9CA3AF)
│   │   │
│   │   └── _ovLedCount[0] ("80 LEDs", RAJDHANI_MED_24, 0x4B5563, BOTTOM_RIGHT)
│   │
│   ├── _overviewCards[1] : ZONE 2 (col 1, same structure, 0x00FF99 accents)
│   │   └── [in 2-zone mode: header reads "ZONE 2 — OUTER"]
│   │
│   └── _overviewCards[2] : ZONE 3 (col 2, HIDDEN in 2-zone mode, 0x9900FF accents)
│
│══ FOOTER (38px) ═══════════════════════════════════════════════════
│
└── _footer (lv_obj, flex ROW SPACE_BETWEEN, 1240x38, x=20 y=638, NO_SCROLL)
    │   bg=0x1A1A1C, border=1px 0x1E1E24, r=14, pad_h=16
    ├── "ENC 0-4: PARAMS" (RAJDHANI_MED_24, 0x4B5563)
    ├── "ENC 5: ZONE SEL" (RAJDHANI_MED_24, 0x4B5563)
    └── "ENC 6: COUNT  ENC 7: ON/OFF" (RAJDHANI_MED_24, 0x4B5563)
```

---

## Vertical Budget

| Layer | Y start | Height | Y end |
|-------|---------|--------|-------|
| Header | 10 | 50 | 60 |
| Mode row | 64 | 36 | 100 |
| Strip container | 108 | 130 | 238 |
| Selected zone indicator | 244 | 36 | 280 |
| Param grid | 284 | 160 | 444 |
| Gap | 444 | 14 | 458 |
| Overview grid | 458 | 170 | 628 |
| Gap | 628 | 10 | 638 |
| Footer | 638 | 38 | 676 |
| Bottom margin | 676 | 44 | 720 |
| **TOTAL** | | **666 used** | **720 avail** |

---

## Zone Segment Geometry (LED→Pixel)

Strip bar = 1160px representing 160 LEDs. Scale: 7.25px/LED.

### 2-Zone Default

| Zone | LEDs | Segments | Width |
|------|------|----------|-------|
| Z1 INNER | 40-119 | 1 centre block | 580px |
| Z2 OUTER | 0-39 + 120-159 | 2 edge blocks | 290px each |

### 3-Zone Default (Triple Rings)

| Zone | LEDs | Segments | Width |
|------|------|----------|-------|
| Z1 INNER | 53-106 | 1 centre block | 392px |
| Z2 MIDDLE | 27-52 + 107-132 | 2 mid blocks | 188px each |
| Z3 OUTER | 0-26 + 133-159 | 2 edge blocks | 196px each |

Centre marker at pixel 576 (LED 79.5).

---

## Touch Targets (all ≥44px effective)

| Element | Size | Effective | Action |
|---------|------|-----------|--------|
| Back button | 100x44 | 100x44 | → GLOBAL screen |
| Zone count card | 120x44 | 120x44 | Cycle count |
| Zone enable btn | 150x44 | 150x44 | Toggle mode |
| Zone selector btns | 130-150×36 | +6px ext_click = 48 | Select zone |
| Strip zone segments | variable×52 | ≥60×52 | Select zone |
| Param cards | 237×150 | 237×150 | Visual only |
| Overview cards | 404×160 | 404×160 | Select zone |

---

## State Transitions

### Zone Selection (ENC 5, strip tap, mode btn tap, overview card tap)
1. Previous zone: strip segment → opa=30%, border=0; overview card border → 1px subtle
2. New zone: strip segment → opa=60%, border=2px zone colour; overview card → 2px zone colour
3. `_selectedZoneTitle` updates text + colour
4. All 5 param cards update values, names, bars to new zone data
5. Param bar indicators change to new zone colour

### Zone Count (ENC 6, zone count card tap)
1. Zone 3 widgets: show/hide (`_zoneSelectorBtns[2]`, `_zoneSegs[3..4]`, `_stripZoneLabels[2]`, `_overviewCards[2]`)
2. Strip segments resize proportionally
3. If Zone 3 was selected and count drops to 2 → auto-select Zone 1
4. `_zoneCountValue` updates "2" / "3"

### Zone Enable (ENC 7 click, enable btn tap)
1. Label: "ZONES: ON" (0x22C55E) ↔ "ZONES: OFF" (0xEF4444)
2. Border: 2px green ↔ 2px red
3. When OFF: strip + params + overview dim to opa=30%

---

## STRETCH: Zone Boundary Adjust Mode

**Trigger:** Long-press ENC 5 (zone selector encoder)

**Behaviour:**
1. Footer text changes to "BOUNDARY MODE — ENC 5: ADJUST | CLICK: EXIT"
2. Selected zone's inner boundary becomes adjustable via ENC 5 rotation
3. Strip visualiser shows a blinking gold handle at the boundary position
4. Minimum zone width: 4 LEDs per side (matching iOS)
5. Cascade validation: adjusting one boundary pushes adjacent zones
6. Click ENC 5 or tap strip → exit boundary mode, return to zone select

**WS Command:** `zones.setLayout` with updated segment array

---

## ASCII Mockup

```
+═══[ < BACK ]═══════════[ ZONE COMPOSER ]══════════[ ZONES 3 ][ ON ]═══+
│                                                                         │
│   [ZONE 1 · INNER]   [ZONE 2 · MIDDLE]   [ZONE 3 · OUTER]            │
│                                                                         │
│       ZONE 1 · INNER      ZONE 2 · MIDDLE      ZONE 3 · OUTER         │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │▓▓Z3▓▓│░░░Z2░░░│████████ Z1 ████████│██│░░░Z2░░░│▓▓Z3▓▓│      │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│   0       27    53         79|80        106   132      159             │
│                                                                         │
│                    ● ZONE 1 PARAMETERS                                  │
│                                                                         │
│  ┌─EFFECT──┐  ┌─PALETTE─┐  ┌──SPEED──┐  ┌──BRIGHT─┐  ┌──BLEND──┐     │
│  │  042    │  │  012    │  │   50    │  │  200    │  │   0     │     │
│  │Chroma   │  │Ocean    │  │         │  │         │  │Overwrite│     │
│  │▓▓▓▓░░░░│  │▓▓░░░░░░│  │▓▓▓▓▓░░░│  │▓▓▓▓▓▓▓░│  │▓░░░░░░░│     │
│  └─────────┘  └─────────┘  └─────────┘  └─────────┘  └─────────┘     │
│    ENC 0        ENC 1       ENC 2        ENC 3        ENC 4           │
│                                                                         │
│  ┌─── ZONE 1 — INNER ─ 40-119 ─┐ ┌─ ZONE 2 — MID ─ 27-52 ──┐ ┌─Z3─┐│
│  │ FX  042 Chroma Wave         │ │ FX  017 Stellar Drift     │ │... ││
│  │ PAL 012 Ocean Breeze        │ │ PAL 003 Lava Flow         │ │    ││
│  │ SPD 50  BRI 200  Overwrite  │ │ SPD 75  BRI 180  Additive │ │    ││
│  │                     80 LEDs │ │                    52 LEDs │ │    ││
│  └──────────────────────────────┘ └───────────────────────────┘ └────┘│
│                                                                         │
│  ENC 0-4: PARAMS        ENC 5: ZONE SEL       ENC 6: COUNT  7: ON/OFF │
+═════════════════════════════════════════════════════════════════════════+
```

---

## WebSocket Commands (Tab5 → K1)

All commands per the protocol contract. Tab5 sends:

| Command | Fields | When |
|---------|--------|------|
| `zone.enable` | `{enable: bool}` | ENC 7 click / enable btn tap |
| `zone.setEffect` | `{zoneId, effectId}` | ENC 0 rotate |
| `zone.setPalette` | `{zoneId, paletteId}` | ENC 1 rotate |
| `zone.setSpeed` | `{zoneId, speed}` | ENC 2 rotate |
| `zone.setBrightness` | `{zoneId, brightness}` | ENC 3 rotate |
| `zone.setBlend` | `{zoneId, blendMode}` | ENC 4 rotate |
| `zone.loadPreset` | `{presetId}` | Zone count change (presetId = count - 1) |
| `zones.setLayout` | `{segments[], zoneCount}` | Boundary adjust (stretch) |
| `zones.get` | `{requestId}` | On screen enter, sync state |

Tab5 listens for: `zones.list`, `zones.changed`, `zones.stateChanged`, `zones.enabledChanged`

Rate limiting: 100ms per encoder index (existing infrastructure).

---

## Execution Plan

### Phase 1: Foundation (delete + scaffold)

1. **Delete** `ZoneComposerUI.cpp` and `ZoneComposerUI.h` entirely
2. **Create** new `ZoneComposerUI.h` with member variables from widget tree above
3. **Create** new `ZoneComposerUI.cpp` with `begin()`, `update()`, `handleEncoderChange()`, `handleEncoderClick()`
4. **Update** `DesignTokens.h`: add `ZONE_1_COLOUR`, `ZONE_2_COLOUR`, `ZONE_3_COLOUR`
5. **Verify** `DisplayUI.cpp` screen instantiation still works (should — same class name)

### Phase 2: Header + Mode Row + Strip Visualiser

1. Build header bar (back button, title, zone count, zone enable)
2. Build mode row (3 zone selector buttons)
3. Build strip visualiser container with zone segments + centre marker
4. Implement `updateStripSegments()` — positions/sizes segments from zone LED boundaries
5. Implement strip tap → zone selection
6. WDT resets between sections

### Phase 3: Parameter Grid

1. Build 5-column param grid with cards
2. Implement `updateParamCards(zoneId)` — populates all 5 cards from zone state
3. Wire ENC 0-4 to `adjustZoneParameter()` → sends WS command → updates card
4. Implement bar indicator colour following selected zone colour

### Phase 4: Overview Cards

1. Build 3-column overview grid
2. Implement `updateOverviewCards()` — populates all 3 cards from zone state cache
3. Wire overview card tap → zone selection
4. Implement selected zone border highlight
5. Show/hide Zone 3 card based on zone count

### Phase 5: Zone Count + Enable + State Sync

1. Wire ENC 6 → zone count change → `zone.loadPreset` WS command
2. Wire ENC 7 click → zone enable toggle → `zone.enable` WS command
3. Implement `handleZonesList(JsonObject&)` — full state sync from K1
4. Implement `handleZonesChanged()` → triggers `zones.get` refresh
5. Implement 2-zone ↔ 3-zone mode transitions (show/hide/resize)

### Phase 6: Polish + Stretch

1. Selected zone indicator row updates
2. Footer encoder hint labels
3. Echo prevention (don't update from WS if we just sent the command)
4. **(Stretch)** Boundary adjust mode via ENC 5 long-press

### Phase 7: Firmware Alignment

1. **Verify** `zones.get` response includes `blendModeName` string (Tab5 needs it for overview cards)
2. **Verify** Triple Rings preset (presetId=2) works correctly on K1
3. **Verify** all WS field names match protocol contract
4. Update `docs/protocol/k1-ws-contract.yaml` if any gaps found

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-03 | agent:orchestrator | Created. LED-Centric layout selected from 5 SSA proposals. Refined encoder mapping to 5-param + zone select + count + enable. |
