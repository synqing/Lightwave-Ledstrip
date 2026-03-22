# Parameter Allocation Audit

Every controllable parameter must have exactly ONE location. No duplicates. No waste.

## Complete Parameter Inventory

### Global Params (8) — Unit A encoders 0-7, ALWAYS visible

| Enc | Param | Type | Range | Home |
|-----|-------|------|-------|------|
| 0 | Effect | selector | 0x0000-0xFFFF | Unit A gauge card |
| 1 | Palette | selector | 0-255 | Unit A gauge card |
| 2 | Speed | uint8 | 0-255 | Unit A gauge card |
| 3 | Mood | uint8 | 0-255 | Unit A gauge card |
| 4 | Fade | uint8 | 0-255 | Unit A gauge card |
| 5 | Complexity | uint8 | 0-255 | Unit A gauge card |
| 6 | Variation | uint8 | 0-255 | Unit A gauge card |
| 7 | Brightness | uint8 | 0-255 | Unit A gauge card |

**Status: clean. No overlap possible — these are persistent.**

### Mode Buttons (4) — persistent touch buttons, always visible

| Button | Param | Current value display | Action |
|--------|-------|----------------------|--------|
| GAMMA | Gamma value | "2.2" | Tap to cycle/toggle |
| COLOUR | Colour correction mode | "RGB" / "Standard" | Tap to cycle |
| EDGEMIXER | EdgeMixer mode | "Triadic" / "Complementary" | Tap to cycle |
| SPATIAL | Spatial+Temporal mode | "Blend" / "Decay" | Tap to cycle |

**These are TOGGLE buttons, not encoder-controlled params. They show the current mode and cycle on tap.**

### EdgeMixer Params (5) — via WS `edge_mixer.set`

| Param | Type | Range | Currently shown in |
|-------|------|-------|--------------------|
| Mode | enum | 0-N (Triadic, Complementary, etc.) | Mode button EDGEMIXER ✓ |
| Spread | uint8 | 0-255 | NOWHERE — needs a home |
| Strength | uint8 | 0-255 | NOWHERE — needs a home |
| Spatial | enum | modes | Mode button SPATIAL ✓ |
| Temporal | enum | modes | Mode button SPATIAL (shares button) ✓ |

**Spread and Strength are the ONLY EdgeMixer params without a home.** Mode/Spatial/Temporal are already in the persistent mode buttons.

### Colour Correction Params — via WS `colorCorrection.setConfig`

| Param | Type | Range | Currently shown in |
|-------|------|-------|--------------------|
| Gamma enabled | bool | on/off | Mode button GAMMA ✓ |
| Gamma value | float | 1.0-4.0 | Mode button GAMMA ✓ |
| CC Mode | uint8 | 0-N | Mode button COLOUR ✓ |
| Auto Exposure | REMOVED | — | — |
| Brown Guardrail | REMOVED | — | — |

**ALL colour correction params are already in mode buttons. Zero need a tab home.**

### Effect-Specific Params (0-16 per effect) — varies

| Param | Type | Range | Home |
|-------|------|-------|------|
| Varies per effect | float/int | varies | FX PARAMS tab, paged |

**These are the primary use case for contextual Unit B encoders.**

### Zone Params (per zone, max 3 zones) — via WS `zone.*`

| Param | Type | Per-zone | Home |
|-------|------|----------|------|
| Effect | selector | yes | ZONES tab |
| Speed | uint8 | yes | ZONES tab |
| Palette | selector | yes | ZONES tab |
| Blend mode | enum | yes | ZONES tab |
| Brightness | uint8 | yes | ZONES tab |

**5 params × 3 zones = 15 total. Zone focus model: select zone, 5 params on encoders.**

### Preset Operations (8 slots)

| Operation | Type | Home |
|-----------|------|------|
| Select slot | encoder turn | PRESETS tab |
| Load preset | encoder press | PRESETS tab |
| Save preset | encoder long-press | PRESETS tab |
| Delete preset | UI action | PRESETS tab |

**Self-contained in PRESETS tab.**

---

## The EDGE/CLR Tab Problem

After removing duplicates, the EDGE/CLR tab contains:
- EM Spread (encoder-controlled)
- EM Strength (encoder-controlled)
- That's it. 2 params.

**2 params do not justify a dedicated tab with 8 encoder slots.**

### Options:

**A) Absorb into FX PARAMS tab.**
Add EM Spread and EM Strength as a "page" within FX PARAMS. When the current effect has 6+ params, they page. EM Spread/Strength become a dedicated page that's always available regardless of effect.

Problem: mixing "effect-specific" and "global enhancement" params in one tab is confusing. The user expects FX PARAMS to change when they switch effects. EM Spread/Strength don't change.

**B) Absorb into mode buttons.**
Replace EDGEMIXER and SPATIAL mode buttons with EDGEMIXER (cycles mode on tap, encoder 8 controls Spread) and SPATIAL (cycles mode on tap, encoder 9 controls Temporal blend). Long-press or double-tap could access Strength.

Problem: overloading the mode buttons with encoder control creates a hidden interaction.

**C) Put them on the persistent top zone.**
Add EM Spread and EM Strength as two additional small gauge cards in the mode button row, making it 4 buttons + 2 mini-gauges.

Problem: adds density to the persistent zone.

**D) Eliminate the EDGE/CLR tab. Go to 3 tabs: FX, ZONES, PRESETS.**
EM Spread and EM Strength become encoders 6-7 on a dedicated "enhancement" page within FX PARAMS (page that's always last, labelled "ENHANCE" not "FX 3/3"). Or they map to physical encoders that are always active regardless of tab (e.g. encoder 14 = Spread, encoder 15 = Strength when not in PRESETS tab).

**E) The mode buttons BECOME encoders.**
Instead of 4 tap-only buttons, make them encoder-responsive:
- GAMMA button: tap cycles enable, encoder 8 adjusts value (2.0→2.8)
- COLOUR button: tap cycles mode
- EDGEMIXER button: tap cycles mode, encoder 9 adjusts Spread
- SPATIAL button: tap cycles spatial, encoder 10 adjusts Strength

The mode buttons are already in the persistent zone. This keeps everything in one place without a dedicated tab. The button shows current mode AND the encoder adjusts the continuous param.

---

## Revised Tab Structure (if Option D or E)

| Tab | Content | Encoders 8-15 |
|-----|---------|---------------|
| FX PARAMS | Effect-specific params, paged | 8 params per page |
| ZONES | Zone selector + per-zone params | 5 active + 3 empty |
| PRESETS | 8 preset slots | turn=select, press=load |

3 tabs instead of 4. Cleaner. EM Spread/Strength resolved by Option E (mode buttons become encoder-responsive) or Option D (enhancement page in FX PARAMS).

---

## Decision Needed

Which option for EM Spread + EM Strength?
