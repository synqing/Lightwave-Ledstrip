# Tab5 UI Redesign Brief

## The Problem

The Tab5 GLOBAL screen displays 116 data points with uniform visual weight. Everything is a dark card with a white border. Nothing draws the eye. The UI was designed for 8 gauges and grew to include FX params, presets, action buttons, footer metrics, and diagnostics — all at the same visual priority.

**Hierarchy score: 2/10.** The largest text on screen is the static "LIGHTWAVEOS" brand title, which carries zero operational information.

## What the Audit Found

### Remove from GLOBAL (~70 data points to cut)

| Remove | Why |
|--------|-----|
| FX Row A + B (48 points) | Duplicates CONTROL_SURFACE. Read-only on GLOBAL. Dead "--" cards when effect has <16 params. |
| Preset Row (16 points) | Display-only on GLOBAL. Only interactable on CONTROL_SURFACE. |
| IP address | Never changes (K1 AP at 192.168.4.1). Move to CONNECTIVITY. |
| SSID + RSSI text | Replace with single coloured connection dot. Detail on CONNECTIVITY. |
| UPTIME | Never actionable. Move to CONNECTIVITY. |
| K1 selector buttons | Feature not implemented. Remove placeholders. |
| "LIGHTWAVEOS" title | Static brand. Zero operational value. |

### Keep on GLOBAL (~46 data points)

| Keep | Why |
|------|-----|
| 8 gauge cards (EFFECT→BRIGHTNESS) | Core controls, 1:1 encoder mapping. **ENLARGE with recovered space.** |
| Action row (5 buttons) | System controls (gamma, colour, edgemixer, spatial, zones) |
| Header: effect name + palette name | **Make these the HERO — largest text on screen** |
| Header: connection health dot | Single colour indicator (green/yellow/red) |
| Footer: BPM, KEY, MIC | Audio-reactive state — critical for a visual instrument |
| Footer: battery + WS status | Operational awareness |

### Fix Visual Hierarchy

| Priority | What | Treatment |
|----------|------|-----------|
| **PRIMARY** | Effect + palette name | 40-48px, brightest, top of screen |
| **PRIMARY** | 8 gauge values | 32-40px monospace, prominent bars |
| **SECONDARY** | BPM, KEY, action row states | Medium size, accent colour for active states |
| **TERTIARY** | Battery, WS status, MIC | Smaller, lower contrast |

## Design Direction: Visual Instrument (B+D Hybrid)

From the controller research, the recommended direction combines:
- **Direction B (Visual Instrument):** OP-1 + Resolume inspired — brand-aligned, playful yet professional, medium density
- **Direction D (Performance Grid):** Push + Resolume discipline — proven 8-column grid, clear encoder mapping, reliable layout

### Colour Palette

| Token | Hex | Usage |
|-------|-----|-------|
| `BG_DEEP` | `#08080A` | Screen background (darker than current #0A0A0B) |
| `SURFACE` | `#111114` | Card background |
| `SURFACE_ELEVATED` | `#1A1A1E` | Header, footer, active cards |
| `BORDER_SUBTLE` | `#1E1E24` | Card borders (softer than current white 2px) |
| `BORDER_ACTIVE` | `#FFC700` | Active/selected card border (brand yellow) |
| `FG_PRIMARY` | `#F0F0F0` | Primary text (slightly warmer than pure white) |
| `FG_SECONDARY` | `#8891A0` | Labels, secondary text |
| `FG_DIMMED` | `#3A3F4A` | Inactive/disabled |
| `BRAND` | `#FFC700` | Accent, active indicators, bars |
| `BRAND_DIM` | `#665200` | Inactive brand elements |
| `STATUS_OK` | `#22C55E` | Connected, enabled |
| `STATUS_WARN` | `#F59E0B` | Connecting, degraded |
| `STATUS_ERROR` | `#EF4444` | Error, disconnected |

### Typography

| Context | Font | Size | Colour |
|---------|------|------|--------|
| Effect/palette name (hero) | Bebas Neue Regular | 44px | `FG_PRIMARY` |
| Gauge values | JetBrains Mono Bold | 36px | `FG_PRIMARY` |
| Gauge labels | Rajdhani Medium | 20px | `FG_SECONDARY` |
| Action row labels | Rajdhani Medium | 20px | `FG_SECONDARY` |
| Action row values | Rajdhani Bold | 28px | `FG_PRIMARY` |
| Footer metrics | JetBrains Mono Regular | 20px | `FG_SECONDARY` |
| BPM (hero metric) | JetBrains Mono Bold | 28px | `BRAND` |

### Layout Concept

```
┌─────────────────────────────────────────────────────────────┐
│ ● [Effect Name ──────────────────] [Palette Name ───] 138♩ │  HEADER (60px)
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐   │
│  │EFFECT│ │PALETT│ │SPEED │ │ MOOD │ │ FADE │ │CMPLX │   │  GAUGE ROW
│  │      │ │      │ │      │ │      │ │      │ │      │   │  (280px)
│  │ 0x1B │ │  31  │ │  25  │ │ 128  │ │  64  │ │ 200  │   │  ENLARGED
│  │ ████ │ │ ████ │ │ █░░░ │ │ ██░░ │ │ █░░░ │ │ ███░ │   │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘   │
│  ┌──────┐ ┌──────┐                                         │
│  │ VAR  │ │ BRI  │                                         │
│  │ 128  │ │ 192  │                                         │
│  │ ██░░ │ │ ███░ │                                         │
│  └──────┘ └──────┘                                         │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│ [GAMMA] [COLOUR] [EDGEMIXER] [SPATIAL] [ZONES]            │  ACTION ROW (80px)
├─────────────────────────────────────────────────────────────┤
│ ● KEY: Am  ● MIC: -12dB  ● BAT: 87%     WS: ●            │  FOOTER (40px)
└─────────────────────────────────────────────────────────────┘
```

**Key changes from current:**
- Gauge cards get ~280px height (was 125px) — values are readable from arm's length
- Hero text: effect + palette name dominate the header at 44px
- BPM integrated into header right side (not buried in footer)
- Footer compressed to one line with dot indicators
- No FX rows, no presets, no brand title, no IP/SSID text
- Softer borders (1px `BORDER_SUBTLE` instead of 2px white)
- Active gauge highlighted with `BORDER_ACTIVE` (brand yellow)

### What Makes It Feel Like an Instrument

1. **The gauges are the interface** — big, prominent, directly mapped to the encoders under your fingers
2. **The effect name is the identity** — like seeing the patch name on a synth
3. **BPM is alive** — pulsing or accent-coloured, showing the music is being heard
4. **Negative space** — removing 70 data points creates breathing room that says "this is intentional, not a spreadsheet"
5. **Colour is earned** — brand yellow only appears on active/selected elements, not everywhere

## Implementation Notes

- All changes are LVGL-only (`TAB5_ENCODER_USE_LVGL=1`)
- Read `tab5-encoder/docs/reference/lvgl-component-reference.md` before any implementation
- The gauge row layout changes from 8-column single-row grid to 8-column 2-row grid (6 top + 2 bottom, or 4+4)
- Header simplification: remove the centre title container, make effect+palette full-width
- Footer simplification: single flex row with dot indicators
- Card factory: reduce border width from 2px to 1px, change colour from white to `BORDER_SUBTLE`
- Active card: 1px `BORDER_ACTIVE` (brand yellow) border on the gauge currently being turned

## Not in This Redesign

- CONTROL_SURFACE screen (already well-structured)
- ZONE_COMPOSER screen (density is appropriate)
- CONNECTIVITY screen (utility screen, low priority)
- New screens or features
- Encoder LED coordination (separate task)
- Audio-reactive UI elements (future exploration)

---

**Source research:**
- `tab5-encoder/docs/UI_DESIGN_RESEARCH.md` — 13 controllers, 4 design directions
- UX audit (session research) — 116→46 data point reduction
- Brand research (partial — agent timed out, key findings captured above)

---
*Created: 2026-03-21*
