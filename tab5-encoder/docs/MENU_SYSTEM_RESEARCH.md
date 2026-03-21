---
abstract: "Navigation architecture research for Tab5 parameter access: how Elektron, Ableton Push, grandMA3, ETC Eos, and Resolume handle deep parameter sets on limited physical encoders. Concludes with a concrete menu system proposal mapping 50+ K1 parameters onto 16 encoders using page groups, context switching, and a persistent global row. Read when designing Tab5 encoder-to-parameter navigation."
---

# Tab5 Menu System Research: Navigation Architecture for Deep Parameter Sets

## The Problem

The Tab5 has 16 rotary encoders (2x M5ROTATE8, referred to as Unit A and Unit B) and a 1280x720 touchscreen. The K1 firmware exposes 50+ controllable parameters:

| Category | Parameters | Count |
|----------|-----------|-------|
| Global | effect, palette, speed, mood, fade, complexity, variation, brightness | 8 |
| Effect-specific | varies per effect (2-16 custom keys) | 0-16 |
| EdgeMixer | mode, spread, strength, spatial, temporal | 5 |
| Colour correction | gamma, auto-exposure, brown guardrail, mode | 4 |
| Zone controls | per-zone effect, speed, palette, blend (x4 zones) | 16 |
| Presets | 8 slots with save/load/delete | 8 |
| Camera mode | toggle | 1 |
| **Total addressable** | | **50-58** |

The current approach uses full screen switches (GLOBAL, CONTROL_SURFACE, ZONE_COMPOSER) with flat encoder assignment per screen. Everything is either always visible at equal weight, or requires a full context switch. There is no middle ground -- no paging, no banking, no contextual remapping within a screen.

This research examines how five professional tools solve the same class of problem: mapping far more parameters than physical encoders onto limited hardware with fast, learnable navigation.

---

## 1. Elektron Parameter Pages (Digitakt, Digitakt II, Syntakt)

### The System

Elektron instruments have **8 data-entry knobs** (labelled A-H) and a small OLED/LCD screen. A single audio track on a Digitakt has 40+ parameters spanning sample selection, filter, amplifier envelope, LFO, and effects. The solution is **dedicated parameter page buttons**.

### How It Works

**Physical layout:**
- 8 rotary encoders across the top
- 5-6 dedicated page buttons below the screen: **TRIG**, **SRC**, **FLTR**, **AMP**, **LFO** (Digitakt) or **TRIG**, **SRC**, **FLTR**, **AMP**, **LFO**, **FX** (Syntakt/Digitakt II)
- Each page button illuminates when active (lit = current page)
- Each page maps exactly 8 parameters to the 8 knobs

**Screen layout per page:**
- The screen shows 8 parameter slots arranged left-to-right, directly corresponding to the physical knob positions below
- Each slot displays: parameter name (abbreviated, e.g. "FREQ", "RES", "ATCK") and current value
- The active page name appears in the screen header

**Switching mechanism:**
- Single button press = instant page switch (no hold, no combo)
- Page switches are immediate -- the screen redraws in <50ms
- Press-and-turn a knob for coarse/accelerated adjustment
- There is no animation or transition; the screen simply shows the new page's parameters

**Subpage indicator:**
- Some pages have subpages, indicated by dots on the right side of the display
- Dot count shows how many subpages exist; dot position shows the current one

### Key Design Decisions

| Aspect | Elektron's Answer |
|--------|-------------------|
| Mental model | Parameters live in named categories (Source, Filter, Amp, LFO). Users think "I want to change the filter" and press FLTR. |
| Switching speed | Instant. One button press, zero delay. You can switch pages mid-performance. |
| Visual feedback | Lit page button + screen header shows current page. All 8 parameter names visible simultaneously. |
| Discoverability | Page buttons are labelled. Pressing each one reveals its contents. Low barrier to exploration. |
| Encoder:parameter ratio | 8 encoders : 40+ parameters = **1:5** |

### What Makes It Work

The category names are **semantically meaningful** and **stable**. "Filter" always contains filter parameters. The user builds a spatial memory: "the filter cutoff is the first knob on the FLTR page." This spatial memory persists across sessions and across different tracks because the page structure is consistent.

---

## 2. Ableton Push Encoder Modes (Push 2, Push 3)

### The System

Push has **8 touch-sensitive encoders** across the top, a colour display below them, and several mode buttons (Device, Mix, Browse, etc.). A single Live Set can have hundreds of devices, each with 8-100+ parameters. The solution is **modal contexts with parameter bank paging**.

### How It Works

**Mode layer (top-level context):**
- Dedicated buttons switch between fundamentally different encoder contexts:
  - **Device Mode**: encoders control the selected device's parameters
  - **Mix Mode**: encoders control track volume, pan, and sends
  - **Browse Mode**: encoders navigate the browser
- The active mode button is illuminated
- Mode switching is instant (single press)

**Parameter bank paging (within Device Mode):**
- A device's parameters are organised into banks of 8
- Lower display buttons page between banks (left arrow = previous bank, right arrow = next bank)
- The display shows the current bank's 8 parameter names directly above the 8 encoders
- Bank indicators: circle icons before the device name indicate which bank (one circle = bank 1, two circles = bank 2)
- Some devices (e.g. Operator) have more than 8 banks

**Display layout:**
- Upper row of display buttons: device/track selection
- Main display area: 8 columns, each showing parameter name + value, aligned to the encoder below
- Lower row of display buttons: bank/page navigation
- Active/automated parameters get colour treatment; inactive ones are grey
- Touching an encoder highlights its parameter on screen

**Track colour coding:**
- Each track has a colour that carries through clips, parameters, pad illumination, and display elements
- This colour is the primary organising principle for recognising context

### Key Design Decisions

| Aspect | Push's Answer |
|--------|---------------|
| Mental model | Two-tier: first pick your context (Device/Mix/Browse), then page through banks within that context. |
| Switching speed | Mode switch = instant (one press). Bank page = instant (one press on display button). |
| Visual feedback | Illuminated mode button + display shows parameter names + bank indicator circles. Touch-sensitive encoders provide hover feedback. |
| Discoverability | Mode buttons are clearly labelled. Bank arrows visible when more banks exist. Parameter names always visible. |
| Encoder:parameter ratio | 8 encoders : 100+ parameters per complex device = **1:12+** |

### What Makes It Work

The **two-tier model** separates "what kind of thing am I controlling?" (mode) from "which page of that thing?" (bank). This prevents the user from getting lost -- you always know you are in Device Mode on Bank 2 of the Reverb. The display's direct 1:1 spatial alignment between encoder positions and parameter columns eliminates guesswork about which knob controls what.

---

## 3. grandMA3 Encoder Banks (MA Lighting)

### The System

The grandMA3 lighting console has **5 dual encoders** (each with an inner ring and an outer ring) and multiple touchscreens. A modern LED fixture can have 50-200+ parameters (dimmer, pan, tilt, zoom, iris, 6+ colour channels, gobo wheels, prism, frost, focus, etc.). The solution is **feature groups with paged encoder banks and a dual-ring coarse/fine mechanism**.

### How It Works

**Feature Group Control Bar (top-level navigation):**
- A row of radio buttons across the top of the encoder bar display
- Each button represents a feature group: **Dimmer**, **Position**, **Color**, **Gobo**, **Beam**, **Focus**, **Control**, **Shapers**, **Video**
- Tapping a feature group button instantly remaps all 5 encoders to that group's attributes
- Feature groups are dynamic -- patching a fixture with new attribute types automatically adds the corresponding feature group button
- Colour-coded indicators on the buttons show when attributes in that group have active/programmed values

**Encoder Pages (within a feature group):**
- If a feature group has more than 5 attributes, they page across multiple encoder pages
- The encoder page button displays: current feature name + total page count
- Tap to cycle through pages; tap-and-swipe to open a popup showing all available pages
- Each page maps up to 5 attributes to the 5 dual encoders

**Dual encoder mechanism:**
- Inner ring = coarse adjustment (large steps)
- Outer ring = fine adjustment (small steps)
- Press + turn inner ring = accelerated coarse adjustment
- This effectively doubles the control resolution without additional physical space

**Encoder toolbar display:**
- Each of the 5 encoder positions shows: attribute name, current value, channel function selector
- Colour coding matches the active programming layer (e.g. programmer values vs stored cue values)
- A link button per encoder allows one encoder to drive multiple attributes simultaneously

### Key Design Decisions

| Aspect | grandMA3's Answer |
|--------|-------------------|
| Mental model | Feature groups are the organising principle. "Colour" contains all colour attributes. "Position" contains pan/tilt. Users think in categories, not pages. |
| Switching speed | Feature group switch = instant (one tap). Page within group = instant (one tap). |
| Visual feedback | Radio buttons with colour-coded activity indicators + encoder toolbar showing all 5 attribute names/values + page count display. |
| Discoverability | Feature groups are auto-populated from the fixture library. Colour indicators show which groups have active data. Page count visible at a glance. |
| Encoder:parameter ratio | 5 encoders (10 with dual ring) : 200+ fixture parameters = **1:20+** |

### What Makes It Work

Three things compound: (a) **semantic feature groups** that match how lighting designers think about fixtures (not "page 1, page 2" but "Colour, Position, Beam"); (b) **dual encoders** that double resolution without extra hardware; (c) **activity indicators** that show which feature groups have live data, so you know where to look without paging through every group.

---

## 4. ETC Eos Parameter Wheels

### The System

ETC Eos family consoles (Eos Ti, Gio, Ion Xe) have **4-5 pageable encoder wheels** plus 2 dedicated pan/tilt wheels. Fixtures range from simple dimmers (1 parameter) to complex automated luminaires with 100+ parameters across intensity, colour mixing, beam shaping, gobo selection, and mechanical controls. The solution is **category-based paging with direct-access hardkeys and touchscreen display per encoder**.

### How It Works

**Parameter category hardkeys:**
- 6 hardkeys (5 on Eos) arranged vertically beside the encoder touchscreen
- Categories: **Intensity (I)**, **Focus (F)**, **Color (C)**, **Shutter**, **Image**, **Form**
- These are often referred to by the IFCB acronym (Intensity, Focus, Color, Beam -- where Beam encompasses Shutter, Image, and Form)
- Single press = switch to that category and display its first page of parameters
- Hold + number key = jump directly to a specific page within a category

**Per-encoder touchscreen display:**
- Each encoder position has a touchscreen area showing:
  - Parameter name
  - Current value
  - {Home} button (reset to default)
  - {Min}/{Max} buttons (jump to limits)
  - {Next}/{Last} buttons (step through discrete ranges, e.g. colour scroll positions)
  - {Mode} button (switch between parameter modes, e.g. gobo rotation modes)
- Stepped parameters show limit markers

**Multi-page navigation:**
- Page count per category is displayed at the top of the encoder touchscreen
- Press the category hardkey repeatedly to cycle through pages
- Or use hardkey + number for direct page access
- Pan/tilt have dedicated encoder wheels that do not page

### Key Design Decisions

| Aspect | Eos's Answer |
|--------|--------------|
| Mental model | IFCB categories mirror the lighting designer's workflow: Intensity first, then Focus (position), then Color, then Beam attributes. This matches the order of operations in programming a cue. |
| Switching speed | Instant (single hardkey press). Direct page jump with hardkey + number. |
| Visual feedback | Touchscreen per encoder shows parameter name, value, and utility buttons. Category page count visible at top. Category hardkeys are always visible and labelled. |
| Discoverability | Category names are industry-standard (every LD knows IFCB). Page count tells you how deep a category goes. The {Home}/{Min}/{Max} buttons encourage experimentation. |
| Encoder:parameter ratio | 4-5 encoders : 100+ fixture parameters = **1:20+** |

### What Makes It Work

The **IFCB mental model** is universal in the lighting industry. A designer who learns Eos can operate any Eos-family console because the categories are stable and meaningful. The **per-encoder touchscreen** eliminates the need to look at a separate display -- the information is right where the hand is. The **{Home} button** per encoder reduces the fear of exploration: if you mess up a parameter, one tap resets it.

---

## 5. Resolume Arena Layer Focus

### The System

Resolume Arena is a real-time video performance tool. It has a layer stack (typically 4-20+ layers), each with clips, effects, blend modes, and per-layer parameters. External MIDI controllers with 8-16 encoders are commonly used. The addressable parameter space includes per-clip transport, per-layer opacity/blend/effects, per-effect parameters, and composition-level controls. The solution is **selection-follows-focus with three mapping targets**.

### How It Works

**Layer selection (focus):**
- Clicking a layer's name in the interface (or sending a MIDI note mapped to layer selection) makes it the "focused" layer
- The focused layer is highlighted in blue
- All parameters mapped as "Selected Layer" now point to this layer's controls

**Three mapping targets for MIDI controls:**
1. **By Position**: encoder always controls the same fixed layer/column (e.g. "fader 1 = layer 1 opacity, always")
2. **Selected**: encoder controls whatever layer/clip/group is currently selected (contextual -- focus determines target)
3. **This specific**: encoder is permanently bound to one specific layer/clip regardless of focus

**Per-layer parameters accessible via encoders:**
- Master fader (opacity), volume, pan
- Blend mode, width, height, auto-size
- Transport controls (speed, direction, loop mode)
- Per-layer effects (each effect has its own parameter set)

**Composition-level vs layer-level:**
- Composition-level parameters (master output, crossfader) are always accessible
- Layer-level parameters require either fixed positional mapping or focus-based selection
- The selected layer's parameters track focus -- switching layers instantly remaps "Selected" encoders

### Key Design Decisions

| Aspect | Resolume's Answer |
|--------|-------------------|
| Mental model | "I am controlling THIS layer" -- layer selection is the primary navigation act. Parameters follow focus. |
| Switching speed | Layer selection is instant (one click/note). All "Selected" mapped controls instantly retarget. |
| Visual feedback | Blue highlight on selected layer. MIDI output reflects current state (controller LEDs can follow). |
| Discoverability | The layer list is always visible. "Selected" mapping is explicit in the shortcut editor. |
| Encoder:parameter ratio | 8-16 encoders (typical controller) : 50+ parameters per layer x 20 layers = **1:60+** |

### What Makes It Work

The **three mapping targets** solve the tension between "I always want fader 1 to be layer 1" (positional) and "I want these 8 knobs to control whatever I'm working on right now" (selected). The user decides per-control which behaviour they want. This is relevant to Tab5 because Unit A (global params) wants positional behaviour while Unit B could benefit from selection-follows-focus behaviour.

---

## Cross-System Synthesis

### Universal Patterns

Every system studied uses the same fundamental architecture, expressed differently:

| Pattern | Elektron | Push | grandMA3 | Eos | Resolume |
|---------|----------|------|----------|-----|----------|
| **Category grouping** | Named pages (SRC, FLTR, AMP) | Modes (Device, Mix) | Feature groups (Color, Position) | IFCB categories | Layer focus |
| **Paging within category** | Fixed 8-per-page | Banks of 8, arrow navigation | 5 per page, page count display | Pages per category, hardkey+# | N/A (per-layer) |
| **Switching mechanism** | Dedicated button per category | Mode button + bank arrows | Radio button tap | Category hardkey | Layer click/note |
| **Switch latency** | Instant (<50ms) | Instant | Instant | Instant | Instant |
| **Page indicator** | Lit button + dots | Circle icons + arrows | Page count + feature name | Page count at top | Blue highlight |
| **Spatial memory** | Knob position = parameter position, stable per page | Encoder column = parameter column | Encoder position = attribute position | Encoder = parameter position | Focus determines context |
| **Default to home** | {Home} per parameter (some pages) | N/A | N/A | {Home} button per encoder | N/A |

### Critical Insight: The Two-Level Model

Every successful system uses a **two-level navigation model**:

1. **Level 1 -- Category/Context selection** (high-level, semantic, infrequent switching):
   "What kind of thing am I controlling?" -- Filter, Colour, Mix, Device, Layer, Zone

2. **Level 2 -- Page/Bank within category** (low-level, sequential, frequent switching):
   "Which page of filter parameters?" -- Page 1/2, Bank A/B

Level 1 switching is done with **dedicated buttons or touch tabs** that are always visible. Level 2 switching is done with **page arrows or repeated presses**. Users spend 80% of their time within a single Level 1 context, paging occasionally. They switch Level 1 contexts only when shifting workflow focus.

### Critical Insight: Persistent vs Contextual

All five systems maintain some controls that **never change** and some that **change with context**:

| System | Persistent (always same function) | Contextual (changes with mode/page/focus) |
|--------|----------------------------------|------------------------------------------|
| Elektron | Transport controls, master volume | 8 knobs (change per page) |
| Push | Transport, note mode toggle | 8 encoders (change per mode and bank) |
| grandMA3 | Grand master fader, command line | 5 encoders (change per feature group + page) |
| Eos | Grand master, pan/tilt dedicateds | 4 encoders (change per IFCB category + page) |
| Resolume | Composition master, crossfader | Encoders mapped to "Selected" layer |

This directly maps to Tab5: **Unit A should be persistent (global params)** and **Unit B should be contextual (changes with context/page)**.

---

## Recommendations for Tab5

### Proposed Navigation Architecture

Given 16 encoders (Unit A: 0-7, Unit B: 8-15) + 1280x720 touchscreen:

```
LEVEL 0: Unit A (encoders 0-7) = PERSISTENT GLOBAL PARAMS
         Never remapped. Always: Effect, Palette, Speed, Mood, Fade, Complexity, Variation, Brightness.
         These are the "grand master + transport" equivalent.

LEVEL 1: Context Tabs (touch-selectable, always visible on screen)
         Determines what Unit B (encoders 8-15) controls.
         Tabs: [FX PARAMS] [EDGE/COLOUR] [ZONES] [PRESETS]

LEVEL 2: Pages within context (when a context has >8 parameters)
         Page indicator + encoder button navigation within Unit B.
```

### Context Tab Details

#### Tab: FX PARAMS (default when on GLOBAL screen)

Maps Unit B encoders 8-15 to the current effect's custom parameters.

| Params per effect | Pages | Behaviour |
|-------------------|-------|-----------|
| 0 (no custom params) | 0 | Unit B encoders inactive, display shows "No FX Params" |
| 1-8 | 1 | Direct mapping, no paging needed |
| 9-16 | 2 | Page indicator visible, B0 button = prev page, B7 button = next page |

- On effect change: reset to page 1, fetch metadata, rebind encoders
- Display: 8 cards showing parameter name + value + bar, matching existing ControlSurfaceUI layout
- Matches the existing `ROW2_EFFECT_PARAMETER_SPEC.md` paging model

#### Tab: EDGE/COLOUR

Maps Unit B to EdgeMixer + colour correction parameters (9 total).

| Encoder | Parameter | Page |
|---------|-----------|------|
| B0 | EdgeMixer Mode | 1 |
| B1 | EdgeMixer Spread | 1 |
| B2 | EdgeMixer Strength | 1 |
| B3 | Spatial Blend | 1 |
| B4 | Temporal Blend | 1 |
| B5 | Gamma | 1 |
| B6 | Auto-Exposure | 1 |
| B7 | Brown Guardrail | 1 |
| B0 (pg2) | Colour Mode | 2 |

9 parameters = 2 pages, but page 2 has only 1 parameter. Consider packing all 9 into one page by assigning the 9th to a button-toggle on one of the encoders, or accept the sparse second page.

**Alternative (recommended):** Fit into one page of 8 by combining Spatial + Temporal into a single "Blend" encoder (button toggles which one), giving 8 parameters on one page. Or drop colour mode to a touch-only toggle (it is binary), keeping 8 on one page.

#### Tab: ZONES

Maps Unit B to zone-specific parameters. This is where the **selection-follows-focus** pattern (from Resolume) applies.

**The problem:** 4 zones x 4 parameters = 16 params, but only 8 encoders on Unit B. Currently only 4 encoders are used (one per zone), wasting 12.

**Proposed solution -- Zone Focus model:**

```
Touch-select a zone (0-3) on the Zone Composer display.
Unit B encoders 8-15 now control THAT zone's full parameter set:

B0: Zone Effect
B1: Zone Palette
B2: Zone Speed
B3: Zone Brightness
B4: Zone Blend Mode
B5: (reserved)
B6: (reserved)
B7: (reserved)
```

- Switching zones: touch a zone card on screen, or use a dedicated encoder button (e.g. B7 button cycles zone focus)
- Visual feedback: the focused zone is highlighted with its colour (cyan/green/orange/purple) and the display header shows "ZONE 2" prominently
- Zone count adjustment: touch-only control on the Zone Composer display (not an encoder -- it is rarely changed)
- This gives each zone 5-8 dedicated encoders instead of 1, matching how grandMA3 gives all 5 encoders to the selected fixture's feature group

#### Tab: PRESETS

Maps Unit B to preset operations.

```
B0-B7: Each encoder represents a preset slot (0-7).
  - Rotate: browse/preview (if implemented)
  - Short press: load preset
  - Double press: save to slot
  - Long press: delete from slot
```

Display shows 8 preset slot cards with occupied/empty state, effect name, and visual feedback on save/load/delete. This matches the existing preset interaction model but gives it dedicated encoder space instead of sharing with FX params.

### Switching Mechanism: Touch Tabs

The context tabs should be implemented as a **persistent touch tab bar** on the display, visible on the GLOBAL screen:

```
┌─────────────────────────────────────────────────────────────┐
│ ● [Effect Name ──────────] [Palette ────]          138 BPM  │  HEADER
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐    │
│  │EFFECT│ │PALETT│ │SPEED │ │ MOOD │ │ FADE │ │CMPLX │    │  UNIT A
│  │ 0x1B │ │  31  │ │  25  │ │ 128  │ │  64  │ │ 200  │    │  GAUGES
│  │ ████ │ │ ████ │ │ █░░░ │ │ ██░░ │ │ █░░░ │ │ ███░ │    │  (persistent)
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘    │
│  ┌──────┐ ┌──────┐                                          │
│  │ VAR  │ │ BRI  │                                          │
│  └──────┘ └──────┘                                          │
├──────────────────────────────────────────────────────────────┤
│ [FX PARAMS]  [EDGE/CLR]  [ZONES]  [PRESETS]   ◆◇  p1/2    │  CONTEXT TAB BAR
├──────────────────────────────────────────────────────────────┤
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐    │
│  │ csq  │ │dampin│ │edge  │ │impuls│ │fwd   │ │rev   │    │  UNIT B
│  │ 0.15 │ │ 0.04 │ │ 0.09 │ │  96  │ │ 6.0s │ │ 3.75 │    │  CONTEXTUAL
│  │ ████ │ │ ██░░ │ │ █░░░ │ │ ███░ │ │ ████ │ │ ███░ │    │  CARDS
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘    │  (changes per tab)
│  ┌──────┐ ┌──────┐                                          │
│  │intro │ │intro │                                          │
│  └──────┘ └──────┘                                          │
├──────────────────────────────────────────────────────────────┤
│ ● KEY: Am  ● MIC: -12dB  ● BAT: 87%              WS: ●    │  FOOTER
└──────────────────────────────────────────────────────────────┘
```

### Tab Bar Design

The context tab bar occupies approximately 40px of vertical space between the Unit A gauges and Unit B cards. It contains:

1. **4 touch tabs** (left-aligned): FX PARAMS, EDGE/CLR, ZONES, PRESETS
   - Active tab: filled background in brand yellow (`#FFC700`), white text
   - Inactive tabs: transparent background, grey text (`#8891A0`)
   - Touch target: minimum 120x40px per tab

2. **Page indicator** (right-aligned): shows current page and total pages
   - Format: `p1/2` or filled/hollow diamonds `◆◇`
   - Only visible when the current context has >1 page
   - Hidden (or shows `p1/1` dimmed) for single-page contexts

3. **Page navigation**: Unit B encoder buttons (B0 = prev page, B7 = next page)
   - Matches the `ROW2_EFFECT_PARAMETER_SPEC.md` button contract
   - Touch-swipe left/right on the Unit B card area as an alternative

### Why NOT Full Screen Switches

The current approach (separate GLOBAL, CONTROL_SURFACE, ZONE_COMPOSER screens) forces the user to leave the global parameter view entirely. This creates two problems:

1. **Loss of global context**: when on CONTROL_SURFACE, you cannot see or adjust global params without switching back
2. **High switching cost**: a full screen transition is cognitively expensive (new layout, new spatial relationships, reorientation time)

The proposed architecture keeps Unit A gauges **always visible** and changes only Unit B. The cognitive cost of switching a context tab is much lower than switching an entire screen because the top half of the display is stable anchor context.

The full-screen ZONE_COMPOSER and CONNECTIVITY screens should remain as separate screens (accessed via the existing action row buttons) because they have specialised layouts that cannot be compressed into an 8-encoder card row. But CONTROL_SURFACE should be **eliminated as a separate screen** -- its functionality is absorbed by the FX PARAMS context tab.

### Encoder Button Semantics

Unit B encoder buttons gain context-dependent behaviour:

| Context | Short Press (Bn) | Long Press (Bn) | Double Press (Bn) |
|---------|-------------------|------------------|---------------------|
| FX PARAMS | B0: prev page, B7: next page, B1-6: no-op | B0: first page, B7: last page | Reset param to default |
| EDGE/CLR | Toggle sub-mode (e.g. B3 toggles spatial/temporal) | Reset to default | N/A |
| ZONES | B7: cycle zone focus (0->1->2->3->0) | N/A | N/A |
| PRESETS | Load preset slot | Delete preset slot | Save to preset slot |

### How This Maps to the Five Research Systems

| Tab5 Concept | Inspired By |
|-------------|-------------|
| Unit A persistent / Unit B contextual | All five systems (persistent grand master + contextual encoders) |
| Context tabs = Level 1 | Elektron page buttons, grandMA3 feature group bar, Eos IFCB hardkeys |
| Pages within FX PARAMS = Level 2 | Elektron subpage dots, Push bank arrows, grandMA3 page count |
| Zone focus model | Resolume "Selected Layer" mapping, grandMA3 fixture selection |
| Per-encoder display cards | Push column alignment, Eos per-encoder touchscreen |
| Instant switching | Universal -- every system studied achieves <50ms page switch |

### Parameter Accessibility Summary

With this architecture, every K1 parameter is reachable in at most 2 actions (context tab + optional page):

| Parameters | Access Path | Actions |
|-----------|-------------|---------|
| 8 global params | Always on Unit A | 0 (always visible) |
| FX params (page 1) | FX PARAMS tab (default) | 0-1 |
| FX params (page 2) | FX PARAMS tab + B7 next page | 1-2 |
| EdgeMixer + colour | EDGE/CLR tab | 1 |
| Zone params (focused zone) | ZONES tab + zone touch selection | 1-2 |
| Presets | PRESETS tab | 1 |
| Camera mode | Touch-only toggle (action row or header) | 1 |

Maximum depth: 2 actions. No parameter requires more than tab switch + page. This matches Elektron (button + optional subpage) and Push (mode + bank page).

### What This Eliminates

- CONTROL_SURFACE screen (absorbed into FX PARAMS tab)
- Duplicate FX param rows on GLOBAL screen (already removed per DESIGN_BRIEF.md)
- Unused encoders on ZONE_COMPOSER (all 8 Unit B encoders now active per zone)
- Full screen switches for parameter access (tab bar is always visible)

### What This Preserves

- GLOBAL screen layout with Unit A gauges (DESIGN_BRIEF.md direction)
- ZONE_COMPOSER as a dedicated screen (specialised layout)
- CONNECTIVITY as a dedicated screen (utility)
- Existing preset interaction model (button semantics)
- Existing FX param paging model (`ROW2_EFFECT_PARAMETER_SPEC.md`)

### Open Questions

1. **Should context tabs be switchable by encoder buttons on Unit A?** E.g. pressing encoder 0's button cycles context tabs. This would add a non-touch switching path for heads-down use. Downside: overloads Unit A button semantics.

2. **Should the ZONES tab auto-activate when entering Zone Composer?** This would create automatic context tracking: switching to Zone Composer screen sets Unit B to ZONES tab. Returning to GLOBAL restores the previous tab.

3. **Should there be a "quick page" gesture?** E.g. pressing and holding a context tab shows a popup with all pages, allowing direct page jump (like grandMA3's tap-and-swipe). For effects with only 2 pages this is overkill, but future effects could have more.

4. **Dual-encoder behaviour on M5ROTATE8?** The M5ROTATE8 has rotation + button per encoder, but no inner/outer ring like grandMA3. Could button-hold + rotate provide a "fine" mode? This would mirror grandMA3's dual encoder concept with existing hardware.

5. **Should the tab bar replace the action row, or coexist?** The current action row holds gamma, colour correction, EdgeMixer, spatial, and zones toggles. If EDGE/CLR becomes a context tab, should the action row shrink to just [ZONES] (screen switch) and [CONNECTIVITY] (screen switch)?

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-21 | agent:ux-researcher | Created. Research across 5 systems + Tab5 navigation architecture proposal. |
