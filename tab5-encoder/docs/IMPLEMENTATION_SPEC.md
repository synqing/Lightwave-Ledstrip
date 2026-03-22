# Tab5 UI Redesign — Implementation Specification

## Governance

Before modifying any file in `tab5-encoder/src/ui/`, the implementing agent MUST:
1. Read `tab5-encoder/docs/reference/lvgl-component-reference.md`
2. Read this spec in full
3. Output a Decision Readback confirming: problem classified, inventory checked, no duplicates, constraints listed

Violation of any DECIDED item below requires escalation to the Captain. Do not interpret, adapt, or "improve."

---

## Decisions (LOCKED — do not deviate)

### Layout Architecture
- **Inside-frame sidebar** — header/footer full width (1280px), sidebar between them
- **Sidebar:** 70px wide, left edge, backlit slots treatment
- **3 tabs:** FX PARAMS (#FFD000 gold), ZONES (#BF00FF purple), PRESETS (#00FFCC teal)
- **NO 4th tab.** EDGE/CLR eliminated. EM Spread + EM Strength are tap buttons in top zone.

### Content Zones
- **Top zone (persistent, never changes with tab):**
  - 8 gauge cards mapped 1:1 to Unit A encoders 0-7
  - 6 tap-to-cycle mode buttons: GAMMA, COLOUR, EDGEMIXER, SPATIAL, EM SPREAD, EM STRENGTH
  - Background strip: #0C0C0E
- **Bottom zone (contextual, changes with tab selection):**
  - 8 cards mapped to Unit B encoders 8-15 (content varies by tab)
  - Background strip: #08080A

### Visual Treatment
- **Header/footer:** preserved VERBATIM from current design (#1A1A1C, 2px white border, 14px radius, same content layout)
- **Param cards:** #111114 surface, 1px #1E1E24 border, 10px radius. NO white borders.
- **Active card:** 1px border in current tab's neon colour + `box-shadow: 0 0 4px <colour at 30%>`
- **Unit A bars:** always #FFD000 (brand gold), regardless of tab
- **Unit B bars:** current tab's neon colour
- **Row background differentiation:** top zone #0C0C0E, bottom zone #08080A, separator 1px #1E1E24

### Sidebar
- Background: #080809
- Each tab slot: recessed groove with `inset 0 1px 3px rgba(0,0,0,0.6)`
- Inactive: tab's neon colour at 25% opacity text
- Active: full neon text + `text-shadow: 0 0 8px <colour>` + radial gradient backlight at 12% opacity + 3px left accent bar with glow
- Tab labels: "FX", "ZONES", "PRE" (abbreviated to fit 70px width)

### Colour Tokens (neon split-complementary)
```
TOKEN                  HEX        USAGE
BG_DEEP                #08080A    Screen background
SURFACE                #111114    Card surface
SURFACE_ELEVATED       #1A1A1C    Header, footer
BORDER_SUBTLE          #1E1E24    Card borders, separator lines
FG_PRIMARY             #F0F0F0    Primary text
FG_SECONDARY           #8891A0    Labels, secondary text
FG_DIMMED              #3A3F4A    Inactive, disabled
NEON_GOLD              #FFD000    FX tab, brand, Unit A bars
NEON_AZURE             #00BFFF    (reserved — no current tab, available for future)
NEON_PURPLE            #BF00FF    ZONES tab
NEON_TEAL              #00FFCC    PRESETS tab
STATUS_OK              #22C55E    Connected, active preset
STATUS_WARN            #F59E0B    Connecting
STATUS_ERROR           #EF4444    Error, disconnected
SIDEBAR_BG             #080809    Sidebar background
ROW_TOP                #0C0C0E    Top zone background
ROW_BOTTOM             #08080A    Bottom zone background
```

### Parameter Allocation (zero duplicates — verified)

**Unit A gauges (persistent, encoder-controlled):**

| Enc | Card | Param | Control |
|-----|------|-------|---------|
| 0 | EFFECT | effect selector | encoder turn = next/prev |
| 1 | PALETTE | palette selector | encoder turn = next/prev |
| 2 | SPEED | uint8 0-255 | encoder turn |
| 3 | MOOD | uint8 0-255 | encoder turn |
| 4 | FADE | uint8 0-255 | encoder turn |
| 5 | CMPLX | uint8 0-255 | encoder turn |
| 6 | VAR | uint8 0-255 | encoder turn |
| 7 | BRI | uint8 0-255 | encoder turn |

**Mode buttons (persistent, tap-to-cycle):**

| Button | Param | Values | Tap Action |
|--------|-------|--------|------------|
| GAMMA | gamma value | 1.8 / 2.0 / 2.2 / 2.5 / 2.8 | cycle forward |
| COLOUR | CC mode | Standard / Vivid / Muted | cycle forward |
| EDGEMIXER | EM mode | Off / Analogous / Complementary / Triadic / Tetradic | cycle forward |
| SPATIAL | spatial mode | Off / Blend / Gradient | cycle forward |
| EM SPREAD | spread degrees | None / Subtle / Medium / Wide / Max (0/10/20/30/45/60) | cycle forward |
| EM STRENGTH | mix strength | Off / Light / Medium / Full (0/64/128/255) | cycle forward |

**NO param appears in both the mode buttons AND the bottom zone. This is verified and locked.**

**Unit B — FX PARAMS tab (encoder-controlled, paged):**

| Enc | Card | Content |
|-----|------|---------|
| 8-13 | FX1-FX6 | Current effect's specific params (from K1 effects.parameters response) |
| 14 | ◀ | Page back (if >1 page). Shows "X/Y" page indicator. |
| 15 | ▶ | Page forward (if >1 page). Encoder press = no action. |

If effect has ≤6 params, cards 14-15 are empty/dimmed. No paging needed.
If effect has 7-12 params, 2 pages. Encoders 14-15 show page nav.
If effect has 13-16 params, 3 pages. Same paging.

**Unit B — ZONES tab (encoder-controlled after zone selection):**

Zone selector: 3 buttons (220x44px each), touch to select zone.

| Enc | Card | Content (for selected zone) |
|-----|------|---------|
| 8 | EFFECT | zone effect selector |
| 9 | SPEED | zone speed |
| 10 | PALETTE | zone palette selector |
| 11 | BLEND | zone blend mode |
| 12 | BRI | zone brightness |
| 13-15 | — | empty/dimmed |

**Unit B — PRESETS tab (encoder-controlled):**

| Enc | Card | Content |
|-----|------|---------|
| 8-15 | P1-P8 | Preset slots. Turn = select. Press = load. Long-press = save. |

### Typography (unchanged from current)

| Context | Font | Size |
|---------|------|------|
| Header brand | Bebas Neue Regular | 40px |
| Header effect/palette | Bebas Neue Regular | 24px |
| Gauge labels | Rajdhani Medium | 18px |
| Gauge values | JetBrains Mono Bold | 28px |
| Mode button labels | Rajdhani Medium | 18px |
| Mode button values | Rajdhani Bold | 24px |
| Sidebar tab labels | Rajdhani Bold | 14px |
| Footer metrics | Rajdhani Medium | 18px |
| Footer values | JetBrains Mono Regular | 18px |

---

## Implementation Phases

### Phase 1: Design Token Centralisation
**Goal:** Single source of truth for colours. No more per-file duplication.
**Files:**
- CREATE `tab5-encoder/src/ui/DesignTokens.h` — all colour tokens as `constexpr uint32_t`
- MODIFY `tab5-encoder/src/ui/DisplayUI.cpp` — replace inline hex with token references
- MODIFY `tab5-encoder/src/ui/ZoneComposerUI.cpp` — same
- MODIFY `tab5-encoder/src/ui/ConnectivityTab.cpp` — same
- MODIFY `tab5-encoder/src/ui/ControlSurfaceUI.cpp` — same
**Verify:** build succeeds, no visual change (refactor only)
**Constraint:** zero functional change. Pixel-identical before and after.

### Phase 2: Sidebar Widget
**Goal:** Create the sidebar as a standalone LVGL widget.
**Files:**
- CREATE `tab5-encoder/src/ui/SidebarWidget.h` — class with 3 tab slots, backlit glow, tab switch callback
- CREATE `tab5-encoder/src/ui/SidebarWidget.cpp` — LVGL implementation, neon colour per tab
- MODIFY `tab5-encoder/src/ui/DisplayUI.h` — add sidebar pointer, tab state enum
- MODIFY `tab5-encoder/src/ui/DisplayUI.cpp` — instantiate sidebar in begin(), wire tab switch
**Verify:** sidebar renders in correct position, tabs switch on touch, visual matches mockup 19 backlit treatment
**Constraint:** sidebar is display-only at this phase. Tab switching doesn't change content yet.

### Phase 3: Layout Restructure
**Goal:** Move from current flat layout to top-zone / bottom-zone split with sidebar.
**Files:**
- MODIFY `tab5-encoder/src/ui/DisplayUI.cpp` — restructure begin():
  - Header: unchanged (keep verbatim)
  - Create middle section: sidebar (70px) + content area
  - Top zone: 8 gauge cards + 6 mode buttons (add EM SPREAD + EM STRENGTH)
  - Separator: 1px line
  - Bottom zone: placeholder content (will be filled in Phase 4)
  - Footer: unchanged (keep verbatim)
- MODIFY `tab5-encoder/src/ui/DisplayUI.h` — add pointers for new widgets, tab state
- REMOVE from DisplayUI: FX Row A, FX Row B, Preset Row (these move to bottom zone in Phase 4)
**Verify:** layout matches mockup 29 structure. Top zone shows 8 gauges + 6 buttons. Bottom zone is empty. Sidebar visible.
**Constraint:** header and footer are COPIED, not rewritten. Do not change a single property of header/footer.

### Phase 4: Tab Content
**Goal:** Wire bottom zone content to sidebar tab selection.
**Files:**
- MODIFY `tab5-encoder/src/ui/DisplayUI.cpp` — create 3 content panels:
  - FX panel: 8 cards for effect-specific params (reuse existing FX row logic, remove from top zone)
  - ZONES panel: zone selector strip (3 buttons, 220x44px) + 8 zone param cards
  - PRESETS panel: 8 preset slot cards
  - Tab switch: show/hide panels via `lv_obj_add_flag(LV_OBJ_FLAG_HIDDEN)` / `lv_obj_clear_flag`
- MODIFY `tab5-encoder/src/ui/DisplayUI.h` — add panel pointers
**Verify:** switching tabs shows correct content. FX params populate from K1. Zone selector works. Presets display.
**Constraint:** no new WebSocket commands. Reuse existing data flows.

### Phase 5: Neon Colour System
**Goal:** Tab colour identity carries through to bottom zone.
**Files:**
- MODIFY `tab5-encoder/src/ui/DisplayUI.cpp` — on tab switch:
  - Update Unit B bar fill colour to active tab's neon colour
  - Update active card border colour
  - Update sidebar glow colours
- MODIFY `tab5-encoder/src/ui/DesignTokens.h` — add neon colour tokens if not already present
**Verify:** switching from FX (gold bars) to ZONES (purple bars) to PRESETS (teal bars) is visually correct. Unit A always stays gold.
**Constraint:** Unit A colour never changes. Only Unit B responds to tab.

### Phase 6: Encoder Routing
**Goal:** Wire physical encoders to correct params per tab.
**Files:**
- MODIFY `tab5-encoder/src/main.cpp` — update encoder routing:
  - Unit A (enc 0-7): always global params (no change)
  - Unit B (enc 8-15): route based on active tab (FX/ZONES/PRESETS)
- MODIFY `tab5-encoder/src/input/ButtonHandler.cpp` — update button routing for mode buttons (add EM SPREAD + EM STRENGTH tap handlers)
- MODIFY `tab5-encoder/src/network/WebSocketClient.cpp` — add sendEdgeMixerSpread() and sendEdgeMixerStrength() if not already available (check first)
**Verify:** turning encoder 8 on FX tab adjusts first FX param. On ZONES tab adjusts zone effect. On PRESETS tab selects preset slot. Mode buttons cycle through values on tap.
**Constraint:** encoder 0-7 routing must NOT change. Only 8-15 routing changes per tab.

### Phase 7: Card Border Treatment
**Goal:** Replace white borders with subtle treatment + neon active highlight.
**Files:**
- MODIFY `tab5-encoder/src/ui/DisplayUI.cpp` — change card creation:
  - Border: 1px #1E1E24 (not 2px white)
  - Active card: 1px in tab's neon colour + glow
  - Remove `lv_obj_set_style_border_color(card, lv_color_hex(0xFFFFFF))` from ALL param cards
  - Keep white borders ONLY on header and footer
**Verify:** param cards have subtle borders. Active card glows in tab colour. Header/footer retain white borders.
**Constraint:** DO NOT touch header or footer border styles. Only param cards change.

### Phase 8: Build, Flash, Soak
**Goal:** Hardware verification.
**Commands:**
```bash
cd tab5-encoder && pio run -e tab5
pio run -e tab5 -t upload --upload-port /dev/cu.usbmodem1101
```
**Verify:**
- Tab5 boots, connects to K1 AP
- All 3 tabs switch correctly
- Encoders map to correct params per tab
- Mode buttons cycle through values
- Neon colours carry through correctly
- Header/footer identical to previous firmware
- No WDT panics, no visual glitches
- 10+ minute soak

---

## Phase Dependencies

```
Phase 1 (tokens) ─────┐
                       ├──> Phase 3 (layout) ──> Phase 4 (content) ──> Phase 5 (colour)
Phase 2 (sidebar) ────┘                                                      │
                                                                              v
                                                             Phase 6 (encoders) ──> Phase 7 (borders) ──> Phase 8 (verify)
```

- Phase 1 and 2 are INDEPENDENT — can run in parallel
- Phase 3 depends on both 1 and 2
- Phases 4-8 are sequential

---

## Files Manifest

### Files CREATED (3):
```
tab5-encoder/src/ui/DesignTokens.h        — centralised colour tokens
tab5-encoder/src/ui/SidebarWidget.h        — sidebar class declaration
tab5-encoder/src/ui/SidebarWidget.cpp      — sidebar LVGL implementation
```

### Files MODIFIED (7):
```
tab5-encoder/src/ui/DisplayUI.h            — sidebar, tab state, new widget pointers
tab5-encoder/src/ui/DisplayUI.cpp          — layout restructure (biggest change)
tab5-encoder/src/ui/ZoneComposerUI.cpp     — use DesignTokens.h
tab5-encoder/src/ui/ConnectivityTab.cpp    — use DesignTokens.h
tab5-encoder/src/ui/ControlSurfaceUI.cpp   — use DesignTokens.h (may be absorbed)
tab5-encoder/src/main.cpp                  — encoder routing per tab
tab5-encoder/src/input/ButtonHandler.cpp   — EM SPREAD + EM STRENGTH tap handlers
```

### Files NOT MODIFIED:
```
tab5-encoder/src/ui/lvgl_bridge.cpp        — no change
tab5-encoder/src/ui/Theme.h                — legacy, not touched
tab5-encoder/src/ui/fonts/*                — no change
tab5-encoder/src/network/*                 — no change (reuse existing WS commands)
Header and footer in DisplayUI.cpp         — VERBATIM, not one property changed
```

---

## Reference Mockups (the visual truth)

The implementing agent should open these in a browser as the visual spec:
- `tab5-encoder/docs/mockups/29-neon-fx-active.html` — FX tab reference (gold)
- `tab5-encoder/docs/mockups/34-neon-zones-redesigned.html` — ZONES tab reference (purple)
- `tab5-encoder/docs/mockups/35-neon-presets-corrected.html` — PRESETS tab reference (teal)
- `tab5-encoder/docs/mockups/19-sidebar-v6-backlit-slots.html` — sidebar glow reference

If the LVGL output does not match these mockups, the implementation is wrong.

---

*Spec created: 2026-03-22*
*All decisions locked. Deviations require Captain approval.*
