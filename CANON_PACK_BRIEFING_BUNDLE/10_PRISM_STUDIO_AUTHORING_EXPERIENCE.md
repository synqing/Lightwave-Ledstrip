# 10 — PRISM STUDIO: THE CREATOR'S INSTRUMENT

> *A design for sculpting volumetric light from the music in your head.*

---

## Prologue: The Feeling Before the Tool

Close your eyes. Mammoth is playing.

The kick drum hits — **BOOM** — and your chest compresses. A stripped-back 4/4 stomp. Cold. Metallic. Like a machine waking up in a frozen factory. Every four bars, a clap cuts through — sharp, white noise, a steam piston firing. You feel the space *between* the kicks. That space is the first canvas. Cold. Minimal. A single point of icy blue light pulsing from the centre of the LGP on each kick, expanding two inches, then dying to black.

Now the build. The snare roll enters, doubling, tripling, accelerating into a continuous blur. The hi-hats shimmer above it. Somewhere a riser pitch-bends upward, pulling tension like a bowstring. The hairs on the back of your neck stand. You KNOW it's coming. And the light should know too — widening, brightening, accelerating, the zones activating from the core outward like a reactor approaching criticality.

Then the silence. One bar. Maybe two. Held breath.

**THE DROP.** The sawtooth horn hits — prehistoric, brass, *Mammoth.* The bass is a physical wall of air. And the light — the light DETONATES from the centre, filling every LED on both strips, the LGP turning into a solid sheet of amber and white energy pulsing with every beat, the zones firing as concentric shockwaves from the core.

You can hear all of this. You always could. You've been hearing it since the first time this track hit you.

What you've never had is the instrument to make it visible.

**PRISM Studio is that instrument.**

---

## Part 1: The First Thirty Seconds

You open PRISM. The screen is dark. A single prompt:

```
╔══════════════════════════════════════════════════╗
║                                                  ║
║         Drop a track. Or pick one.               ║
║                                                  ║
║              ┌──────────────┐                    ║
║              │  Browse...   │                    ║
║              └──────────────┘                    ║
║                                                  ║
║          Or drag a file anywhere.                ║
║                                                  ║
╚══════════════════════════════════════════════════╝
```

You drag `Mammoth.mp3` onto the window. It lands.

Three things happen:

**Second 0–3: The Waveform Appears.**
The stereo waveform unfurls across the timeline like an EKG. You can already SEE the build and the drop — the waveform tells the story. Peaks are loud. Valleys are quiet. The physical shape of the song is visible before a single note plays.

**Second 3–8: The Structure Crystallises.**
Trinity analysis runs. Section markers drop onto a ruler above the waveform:

```
┃ INTRO  ┃  BUILD 1  ┃  DROP 1  ┃  BREAK  ┃  BUILD 2  ┃  DROP 2  ┃  OUTRO ┃
   grey      amber        red       blue       amber        red       grey
```

Each section is colour-coded by energy character. Beat markers appear as thin vertical ticks on the waveform — downbeats brighter than offbeats. You can see the tempo grid: the rhythmic skeleton of the song, laid bare.

**Second 8–15: The Offer.**
A toast appears:

```
 ✨  AI detected 7 sections at 128 BPM.
     Want a generated first draft?
     ┌────────────┐  ┌──────────────────┐
     │  Generate  │  │  I'll do it myself │
     └────────────┘  └──────────────────┘
```

If you hit **Generate**, the AI fills every section with effect, palette, transition, and parameter curves based on the song's structure and energy profile. You have a complete light show in under fifteen seconds. Preview it. Sculpt from there.

If you hit **I'll do it myself**, the timeline is yours. Empty lanes. The song structure is drawn; you are the painter.

Either way, you are **sculpting in under fifteen seconds.** Not configuring. Not reading documentation. Sculpting.

---

## Part 2: Screen Architecture — The Four Territories

The screen is divided into four territories. Each has a job. None is optional.

```
┌──────────────────────────────────────────────────────────────────────┐
│                        THE STAGE                                     │
│  ████████████████████░░●░░████████████████████  ← 320 LEDs, live    │
│  ████████████████████░░●░░████████████████████                       │
├──────────┬───────────────────────────────────────────┬───────────────┤
│          │              THE CANVAS                    │               │
│          │                                           │               │
│  THE     │  Transport: ◀◀  ▶  ▶▶   1:23 / 3:45     │  THE          │
│  RIG     │  ─────────────────────────────────────    │  INSPECTOR    │
│          │  Section: ┃INTRO┃BUILD┃ DROP ┃BREAK┃...   │               │
│  Zone    │  Waveform: ▁▂▃▂▁▂▅▇█▇▅▃▁▂▃▅▇█▇▅▃▁      │  Context-     │
│  diagram │  ─────────────────────────────────────    │  sensitive    │
│  +       │  Effect:  [BtPulse Stack][  Bloom  ][..   │  editing      │
│  spatial │  Palette: [Arctic      ][  Ember   ][..   │  panel        │
│  control │  Brightness: ___╱‾‾╲___╱‾‾‾‾‾‾╲______    │               │
│          │  Speed:      ___╱‾‾╲___╱‾‾‾‾‾‾╲______    │               │
│          │  ...more lanes...                         │               │
│          │                                           │               │
├──────────┴───────────────────────────────────────────┴───────────────┤
│  🪄 INTENT BAR: "What should this feel like?"                [Send]  │
│  Quick: [Explosive] [Building] [Calm] [Dark] [Euphoric] [Raw]       │
└──────────────────────────────────────────────────────────────────────┘
```

**THE STAGE** (top, 80px, fixed): The LED Preview. Always visible. Always showing the current playback frame. This is your monitor speakers. You never lose sight of the output.

**THE RIG** (left panel, collapsible): The Zone Choreographer. A concentric-ring diagram of your hardware. Click a ring to select a zone. The rings pulse in real-time with the preview. Below: zone presets, cross-zone patterns.

**THE CANVAS** (centre, scrollable/zoomable): The Timeline. This is where you spend 90% of your time. Waveform, section ruler, effect lane, palette lane, expression curves, zone lanes. Everything is anchored to the song.

**THE INSPECTOR** (right panel, collapsible): The editing surface. Shows properties of whatever you've selected — a section, an automation point, a zone. Also houses the effect browser, palette browser, and transition picker.

**THE INTENT BAR** (bottom, fixed): The AI interface. Always one keystroke away. Type what you want. Or click a mood chip. The AI proposes; you approve.

---

## Part 3: The Stage

The Stage is a horizontal LED strip visualisation spanning the full width of the screen. It renders all 320 LEDs at their true proportions — two rows of 160, centre-origin at LED 79/80. The centre point is marked with a subtle dot.

```
 ████████████████████░░●░░████████████████████   ← Strip 1 (LEDs 0-159)
 ████████████████████░░●░░████████████████████   ← Strip 2 (LEDs 160-319)
```

Behaviours:

- **During playback**: Shows the live effect output at the current playback position. Updates at 60fps in software preview, or pulls real LED data from the device over WebSocket/UDP if connected.
- **During scrubbing**: As you drag the playhead, the Stage shows the computed frame at that exact timestamp. Instant. No lag. You scrub, you see.
- **Zone overlay**: When a zone is selected in the Rig, thin coloured brackets appear on the Stage marking that zone's LED range. You can see which LEDs belong to which zone.
- **Click-to-inspect**: Click any LED on the Stage to see its exact RGB values and which effect/zone is driving it.

**Why it's fixed at the top**: You never scroll it away. Every edit you make in the Canvas below is immediately reflected in the Stage above. The feedback loop is zero-latency. Change a parameter, see the light change. This is the core of what makes the tool addictive.

**LGP Simulation Mode** (toggle): Since the physical Light Guide Plate diffuses and blends adjacent LED light through the acrylic waveguide, PRISM offers an optional Gaussian-blur overlay that approximates the real-world optical spread. Two viewing modes:

- **Pixel**: Raw LED colours, sharp boundaries. For precision editing.
- **LGP**: Blurred, diffused, closer to what the human eye sees on the physical panel. For vibe-checking.

---

## Part 4: The Canvas — Timeline and Lanes

The Canvas is the heart of the authoring experience. It's a horizontally scrollable, vertically stacked timeline anchored to the song's waveform. Everything in the Canvas is time-indexed to the audio.

### 4.1 The Transport Bar

```
◀◀  ⏪  ▶  ⏩  ▶▶    1:23.4 / 3:45.0    🔁 Loop    128 BPM    [Snap: Beat ▼]
```

- **Play/Pause** (Space): Starts playback from the playhead. The Stage updates in real-time.
- **Jump** (< / >): Jump to previous/next section boundary.
- **Loop** (L): Toggle loop around the selected section or custom range.
- **Snap**: Magnetic snapping for all cue placement. Options: Off, Beat, Bar, Section.
- **BPM display**: From Trinity analysis. Not editable (the song's tempo is a fact, not a preference).

### 4.2 The Section Ruler

```
┃ INTRO  ┃  BUILD 1  ┃  DROP 1  ┃  BREAK  ┃  BUILD 2  ┃  DROP 2  ┃  OUTRO ┃
```

Sections come from Trinity structural analysis (AllInOne-DiNAT with Harmonix labels). Each section is a coloured block you can:

- **Click** to select (highlights the section; Inspector shows its properties).
- **Right-click** to rename, split, merge, or change the label.
- **Drag boundaries** to adjust section start/end times (snaps to beats).
- **Double-click** to type an intent for that section in the Inspector.

Sections are the highest-level organisational unit. They map directly to **Moments** (see Part 5).

### 4.3 The Waveform

The stereo waveform rendered across the full timeline width. Beat markers appear as thin vertical lines (downbeats brighter). This lane is read-only — it's the song's anatomy. You don't edit the waveform; you paint on top of it.

Visual cues baked into the waveform view:
- **Energy envelope**: A faint filled area showing the RMS energy contour. You can see where the song is loud (drops) and quiet (intros, breaks).
- **Beat grid**: Vertical lines at every beat, with downbeats emphasised. When zoomed in, sub-beat divisions appear.
- **Playhead**: A bright vertical line showing the current position. Draggable for scrubbing.

### 4.4 The Lane Stack

Below the waveform, lanes are stacked vertically. Each lane represents one dimension of your light show. Lanes are grouped into three tiers:

**Tier 1 — Master Lanes** (always visible):

| Lane | Shows | Interaction |
|------|-------|-------------|
| **Effect** | Coloured blocks showing which effect is active in each time range | Drag effects from browser; resize blocks; right-click to change |
| **Palette** | Coloured gradient blocks showing the active palette | Drag palettes from browser; the block renders the actual palette gradient |
| **Transition** | Diamond markers at section boundaries | Click to pick transition type; drag to adjust timing |

**Tier 2 — Expression Lanes** (collapsible group, toggle with `E`):

| Lane | Parameter | Range | What it sculpts |
|------|-----------|-------|-----------------|
| **Brightness** | Master luminous intensity | 0–255 | How bright. The volume knob of light. |
| **Speed** | Animation velocity | 1–100 | How fast patterns move and evolve. |
| **Mood** | Reactive ↔ Smooth | 0–255 | 0 = snappy, beat-locked. 255 = dreamy, flowing. The most important creative parameter. |
| **Intensity** | Effect amplitude | 0–255 | How much energy the effect injects. |
| **Saturation** | Colour purity | 0–255 | 0 = greyscale. 255 = full colour. |
| **Complexity** | Pattern detail | 0–255 | How intricate the visual structure is. |
| **Variation** | Randomness seed | 0–255 | How much organic randomness enters the pattern. |
| **Fade** | Trail persistence | 0–255 | 0 = infinite trails. 255 = no persistence. Controls the "memory" of the light. |

Each expression lane shows an **automation curve** — a line graph you can draw, edit, and shape over time.

**Tier 3 — Zone Lanes** (visible when zones are active):

| Lane | Shows |
|------|-------|
| **Zone 1** (Core) | Per-zone effect and palette override blocks |
| **Zone 2** | Per-zone effect and palette override blocks |
| **Zone 3** | Per-zone effect and palette override blocks |
| **Zone 4** (Outer) | Per-zone effect and palette override blocks |

Zone lanes let you override the master effect/palette for individual concentric rings. If a zone lane is empty, it inherits from the master lanes. If it has a block, that zone uses its own effect.

**Lane visibility modes:**

- **Compact** (default): Section ruler + Waveform + Effect + Palette (4 lanes). Clean overview.
- **Expression**: + All expression lanes (12 lanes). For parameter sculpting.
- **Full**: + Zone lanes + Transition lane (up to 17 lanes). For detailed choreography.
- **Custom**: Toggle any individual lane with a checkbox in the lane header area.

---

## Part 5: Moments — The Human Unit of Light

You don't think in cues. You think in **moments.**

"The intro should feel cold and mechanical."
"The build should feel like rising pressure."
"The drop should detonate."

A **Moment** is the user-level unit of composition. It maps to a time region (defaults to a section, but you can create custom regions) and bundles everything that defines the visual experience in that window:

| Property | What it is |
|----------|-----------|
| **Name** | User-defined or AI-suggested: "Cold Stomp", "The Pressure", "DETONATION" |
| **Time range** | Start and end timestamps, snapped to section boundaries by default |
| **Intent** | Optional natural language: "cold metallic pulse on kicks" |
| **Effect** | Which visual instrument runs here |
| **Palette** | Which colour vocabulary |
| **Transition in** | How you enter this moment (which of the 12 transition types) |
| **Parameters** | Static values or automation curves for all 8 expression params |
| **Zone overrides** | Optional per-zone effect/palette/parameter overrides |

When you click a section in the ruler, you're selecting a Moment. The Inspector shows all its properties. You edit there, or you use the Intent Bar.

**The critical abstraction:** Under the hood, each Moment compiles to one or more ShowCues — EFFECT cues, PALETTE cues, PARAMETER_SWEEP cues, TRANSITION cues, ZONE_CONFIG cues. But the creator never sees cues unless they switch to Direct Mode (Part 11). Moments are the creative interface; cues are the machine output.

**Moment interactions on the timeline:**

- **Click** a section → selects the Moment.
- **Double-click** → opens the Intent Bar pre-focused for that section.
- **Right-click** → context menu: Duplicate, Clear, Generate with AI, Copy Style, Paste Style.
- **Drag the boundary** between two Moments → adjusts timing (snaps to beats).
- **Split** (B at playhead) → cuts a Moment into two at the playhead position.
- **Merge** (M with two selected) → combines adjacent Moments.

### The Moment-to-Cue Pipeline

```
CREATOR                    PRISM                        FIRMWARE
────────                   ─────                        ────────
Moment: "DETONATION"  →  Compiles to:               →  ShowBundle JSON:
  Effect: BeatPulseBloom     CUE_EFFECT(121, NUCLEAR)     chapters + cues
  Palette: Ember             CUE_PALETTE(58)               (10 bytes each)
  Transition: Nuclear        CUE_TRANSITION(8, 2500ms)
  Brightness: 255            CUE_PARAMETER_SWEEP(0, 255, 0ms)
  Speed: 85                  CUE_PARAMETER_SWEEP(1, 85, 0ms)
  Mood: 30                   CUE_PARAMETER_SWEEP(...) × N
  Intensity: 240
  Zone 2: Holographic        CUE_ZONE_CONFIG(2, 0b0011)
                             CUE_EFFECT(zone2, 16, FADE)
```

The creator sculpts in Moments. PRISM compiles to ShowCues. The firmware consumes a flat array of 10-byte cues sorted by `timeMs`. The abstraction is clean. The creator never needs to know that a "Moment" is actually seven cues.

---

## Part 6: Expression Curves — Drawing the Energy

Expression lanes are where the fine detail lives. Each lane is a drawable automation curve spanning the timeline.

### Drawing interactions

| Action | Gesture | Result |
|--------|---------|--------|
| **Set a point** | Click on lane at time position | Creates a breakpoint at that value |
| **Draw a ramp** | Click-drag horizontally | Linear interpolation from start to end |
| **Freehand draw** | Hold Shift + drag | Pencil mode: traces your mouse movement as a curve |
| **Adjust a point** | Drag existing breakpoint | Moves value up/down, time left/right |
| **Delete a point** | Right-click breakpoint → Delete | Removes the point; curve interpolates neighbours |
| **Clear region** | Select region + Delete | Removes all automation in the selection |
| **Interpolation type** | Right-click between points | Linear / Ease In / Ease Out / Ease In-Out / Step |

### Quick Shapes

Right-click on any empty region of a lane → **Insert Shape**:

| Shape | Behaviour | Use case |
|-------|-----------|----------|
| **Ramp Up** | Linear increase over selected region | Building tension: brightness rising through a build |
| **Ramp Down** | Linear decrease | Releasing energy: speed dropping into a break |
| **Pulse** | Spike on every beat, decaying between | Beat-locked parameter pumping |
| **Swell** | Gradual rise, peak at 75%, gradual fall | Natural breathing shape for mood or intensity |
| **Step** | Instant value change at a single point | Hard cuts: sudden brightness shift at a drop |
| **Sawtooth** | Rise then instant drop, repeating per bar | Rhythmic tension-and-release |

### Section-Level Shortcuts

When you don't want to draw curves, you can set parameters for an entire Moment:

1. Select a section.
2. In the Inspector, set Brightness = 255.
3. The entire section shows a flat line at 255.
4. Later, if you want detail within that section, switch to the lane and draw over the flat line.

This is the "coarse then fine" workflow. Set the feel per section. Then sculpt within sections.

---

## Part 7: The Rig — Zone Choreography

The left panel shows a top-down diagram of the Light Guide Plate's concentric zone geometry:

```
    ┌───────────────────────┐
    │       THE RIG         │
    │                       │
    │      ┌─────────┐      │
    │      │ ┌─────┐ │      │
    │      │ │ ┌─┐ │ │      │
    │      │ │ │●│ │ │      │   ● = centre (79/80)
    │      │ │ └─┘ │ │      │   Inner ring = Zone 1
    │      │ └─────┘ │      │   Middle ring = Zone 2
    │      └─────────┘      │   Outer ring = Zone 3/4
    │                       │
    │  Zone 1: BeatPulseBloom│
    │  Zone 2: Holographic   │
    │  Zone 3: Star Burst    │
    │  Zone 4: Breathing     │
    │                       │
    │  Layout: [Quad ▼]     │
    │                       │
    │  Cross-Zone Pattern:  │
    │  ○ Independent         │
    │  ● Cascade Out         │
    │  ○ Cascade In          │
    │  ○ Synchronised        │
    │  ○ Counterpoint        │
    │                       │
    │  Zone Presets:         │
    │  [Unified        ]    │
    │  [Dual Split     ]    │
    │  [Triple Rings   ]    │
    │  [Quad Active    ]    │
    │  [Heartbeat Focus]    │
    └───────────────────────┘
```

### Interactions

- **Click a ring**: Selects that zone. The Inspector switches to zone-specific editing. The Stage highlights that zone's LED range.
- **Drag an effect from browser onto a ring**: Sets that zone's effect override.
- **Drag a palette swatch onto a ring**: Sets that zone's palette override.
- **Real-time glow**: Each ring glows/pulses in sync with the LED preview, giving you a live minimap of what each zone is doing.

### Cross-Zone Patterns

These control how zones relate to each other temporally. They're presets for common zone choreography:

| Pattern | Behaviour | When to use |
|---------|-----------|-------------|
| **Independent** | Each zone runs its own effect independently | Complex multi-texture compositions |
| **Cascade Out** | Changes propagate centre → outer with a configurable delay (100–500ms per zone) | Builds and drops: energy radiates from core |
| **Cascade In** | Changes propagate outer → centre | Implosion moments: energy collapses inward |
| **Synchronised** | All zones change simultaneously | Hard cuts at section boundaries |
| **Counterpoint** | Adjacent zones alternate (zone 1+3 vs zone 2+4) | Call-and-response rhythmic patterns |

### The Mammoth Example

For the DROP, you configure:
- Layout: **Quad** (4 concentric zones)
- Cross-Zone: **Cascade Out** with 150ms delay
- Zone 1 (core): BeatPulseBloom, Ember palette, brightness 255
- Zone 2: Holographic, Copper palette, brightness 220
- Zone 3: Star Burst, Ember palette, brightness 200
- Zone 4 (outer): Breathing, Arctic palette, brightness 180

When the drop hits: Zone 1 detonates first at the centre. 150ms later Zone 2 fires. Then Zone 3. Then Zone 4. Four concentric shockwaves rippling outward from the core, each carrying a different visual texture. The LGP turns these into overlapping wavefronts of light. This is not something you get from a linear LED strip. This is volumetric.

---

## Part 8: The Inspector — Touch, Tune, Transform

The right panel is context-sensitive. It shows editing controls for whatever is currently selected.

### When a Moment is selected:

```
┌─────────────────────────┐
│  THE DROP                │
│  1:12.0 → 1:48.0  36s   │
│  ────────────────────────│
│                          │
│  Intent:                 │
│  "Full-force mammoth     │
│   explosion from centre" │
│  [Edit]                  │
│                          │
│  ─── EFFECT ───          │
│  ┌────────────────────┐  │
│  │ 🔮 BeatPulse Bloom │  │
│  │ Audio-reactive      │  │
│  │ ★★★★☆ Intensity     │  │
│  └────────────────────┘  │
│  [Change...]             │
│                          │
│  ─── PALETTE ───         │
│  ┌────────────────────┐  │
│  │ ████████████ Ember  │  │  ← actual gradient swatch
│  └────────────────────┘  │
│  [Change...]             │
│                          │
│  ─── TRANSITION IN ───   │
│  ┌────────────────────┐  │
│  │ 💥 Nuclear   2.5s  │  │
│  └────────────────────┘  │
│  [Change...]   Duration: │
│  ░░░░░░████░░░  2500ms   │
│                          │
│  ─── PARAMETERS ───      │
│  Brightness   ████████ 255│
│  Speed        ██████░░  85│
│  Mood         ██░░░░░░  30│
│  Intensity    ███████░ 240│
│  Saturation   ██████░░ 200│
│  Complexity   ████░░░░ 128│
│  Variation    ███░░░░░  96│
│  Fade         ██████░░ 180│
│                          │
│  ─── ZONES ───           │
│  ☑ Zone overrides active │
│  [1:Bloom] [2:Holo]      │
│  [3:Star ] [4:Breathe]   │
│                          │
│  ────────────────────────│
│  🤖 [AI: Suggest changes] │
│  📋 [Copy style]          │
│  📋 [Paste style]         │
└─────────────────────────┘
```

### When an automation point is selected:

```
┌─────────────────────────┐
│  BREAKPOINT              │
│  Parameter: Brightness   │
│  Time: 1:24.320          │
│  Value: 212              │
│                          │
│  Curve: [Ease In-Out ▼]  │
│                          │
│  [Delete point]          │
└─────────────────────────┘
```

### When a zone ring is selected in the Rig:

```
┌─────────────────────────┐
│  ZONE 2 — Middle Ring    │
│  LEDs: 40-59, 100-119    │
│  (40 LEDs per strip)     │
│                          │
│  Override Effect:         │
│  [Holographic        ▼]  │
│                          │
│  Override Palette:        │
│  [████████ Copper    ▼]  │
│                          │
│  Blend Mode:             │
│  [Additive           ▼]  │
│                          │
│  Zone Brightness:        │
│  ░░░░░██████░░  220      │
│                          │
│  Zone Speed:             │
│  ░░░░░████░░░░  75       │
│                          │
│  [Clear overrides]       │
└─────────────────────────┘
```

The Inspector always shows exactly what you need. Nothing more.

---

## Part 9: Effect and Palette Browsers

### 9.1 The Effect Browser

Opened via the Inspector's [Change...] button or by pressing `F`. Not a text list. A visual gallery.

```
┌─────────────────────────────────────────────┐
│  EFFECTS                          [🔍     ] │
│                                             │
│  [All] [🎵 Audio] [✨ Ambient] [💎 LGP]    │
│  [Fluid] [Geometric] [Optical] [Organic]   │
│  [Quantum] [Physics] [BeatPulse] [Shapes]  │
│                                             │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐      │
│  │  ◉ ))) │ │  ◉~~~  │ │  ◉ /// │      │
│  │         │ │         │ │         │      │
│  │BeatPulse│ │  Bloom  │ │ Ripple  │      │
│  │ Stack   │ │         │ │Enhanced │      │
│  │ 🎵      │ │ 🎵      │ │ 🎵      │      │
│  └─────────┘ └─────────┘ └─────────┘      │
│                                             │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐      │
│  │  ≋≋≋≋  │ │  ✦✦✦✦  │ │  ◎◎◎◎  │      │
│  │         │ │         │ │         │      │
│  │  Holo-  │ │  Star   │ │  Wave   │      │
│  │ graphic │ │  Burst  │ │Collision│      │
│  │ 💎      │ │ ✨      │ │ 💎      │      │
│  └─────────┘ └─────────┘ └─────────┘      │
│                                             │
│    Drag onto timeline, zone ring, or click  │
│    to apply to selected moment.             │
└─────────────────────────────────────────────┘
```

Each card contains:
- A **mini canvas thumbnail** showing a 3-second loop of the effect at default parameters. This is rendered in software using the same engine as the Stage preview.
- The **effect name**.
- A **badge**: 🎵 audio-reactive, ✨ ambient, 💎 LGP-specific.
- **Hover**: shows the full description, tags, and "related effects" from PatternRegistry metadata.

You can:
- **Drag** a card onto the timeline → creates an effect block at the drop position.
- **Drag** onto a zone ring → overrides that zone's effect.
- **Click** while a Moment is selected → sets that Moment's effect.
- **Search** by name or tag.
- **Filter** by family or audio-reactive vs ambient.
- **Preview**: hover for 0.5s to hear/see the effect in the Stage with the current song playing underneath.

### 9.2 The Palette Browser

Opened via [Change...] on palette or pressing `P`.

```
┌─────────────────────────────────────────────┐
│  PALETTES                         [🔍     ] │
│                                             │
│  [All] [Warm] [Cool] [Vivid] [Calm] [CVD]  │
│                                             │
│  ▓▓▓▓░░▓▓▓▓░░▓▓▓▓  Arctic          cool   │
│  ████░░████░░████    Ember           warm   │
│  ▓▓▓▓░░▓▓▓▓░░▓▓▓▓  Deep Ocean      cool   │
│  ████░░████░░████    Copper          warm   │
│  ▓▓▓▓░░▓▓▓▓░░▓▓▓▓  Crameri Batlow  CVD    │
│  ████░░████░░████    Viridis         CVD    │
│  ...                                        │
│                                             │
│    75 palettes • Drag or click to apply     │
└─────────────────────────────────────────────┘
```

Each row renders the **actual CRGBPalette16 gradient** as a colour bar. You see the real colours that will appear on the LEDs. Filter by warmth, intensity, or CVD-friendliness. The palette metadata flags (`PAL_WARM`, `PAL_COOL`, `PAL_VIVID`, `PAL_CALM`, `PAL_CVD_FRIENDLY`) drive the filter tabs.

---

## Part 10: AI Co-Pilot — Three Tools

The AI is a co-pilot. It assists, proposes, translates. It never writes without your permission. Every suggestion is a diff you can accept or reject.

### 10.1 The Intent Bar

Always visible at the bottom of the screen. One `Tab` keystroke to focus it.

**The interaction:**

1. Select a section on the timeline (or leave it global).
2. Type in the Intent Bar: `"cold metallic pulse on kicks"`
3. Press Enter.
4. AI proposes:

```
┌────────────────────────────────────────────────────────┐
│  🤖 AI Suggestion for INTRO:                           │
│                                                        │
│  Effect:     BeatPulse Stack  (beat-locked, minimal)   │
│  Palette:    Arctic           (cold, icy tones)        │
│  Brightness: 40               (low, atmospheric)       │
│  Speed:      BPM-locked       (128 BPM pulse)          │
│  Mood:       15               (very reactive/snappy)   │
│  Intensity:  60               (restrained energy)      │
│  Fade:       200              (fast decay to black)    │
│                                                        │
│  Reasoning: "Metallic = cool palette, minimal =        │
│  low brightness/intensity, pulse on kicks = low        │
│  mood for snappy beat response, fast fade for          │
│  rhythmic definition against darkness."                │
│                                                        │
│  ┌──────────┐  ┌────────┐  ┌──────────────┐          │
│  │  Accept  │  │ Reject │  │ Adjust & Ask │          │
│  └──────────┘  └────────┘  └──────────────┘          │
└────────────────────────────────────────────────────────┘
```

5. Hit Accept: the Moment updates. The Stage shows the change immediately.
6. Or hit Adjust: "make it even colder, almost black between kicks" → AI refines.

**Quick mood chips** below the Intent Bar provide one-click intents:
- **Explosive**: max brightness, max intensity, Nuclear transition, BeatPulseBloom
- **Building**: ramping brightness/speed/intensity, Cascade Out zones
- **Calm**: low brightness, high mood (smooth), Breathing effect, cool palette
- **Dark**: minimal brightness, high fade, black between beats
- **Euphoric**: high brightness, high saturation, warm palette, wide zones
- **Raw**: low mood (reactive), high intensity, no persistence, sharp

Clicking a chip immediately applies the preset to the selected Moment. Faster than typing, but less precise.

### 10.2 Section Autopilot

Each section in the ruler has a small ✨ icon. Click it:

```
 ✨ Generate visuals for BUILD 1?
 The AI will suggest effect, palette, parameters,
 and transitions based on the song structure and
 surrounding sections.
 ┌───────────┐  ┌──────────┐
 │  Generate │  │  Cancel  │
 └───────────┘  └──────────┘
```

The AI considers:
- The section label (BUILD → ramping energy is expected).
- The Trinity macro curves for that time range (energy, percussiveness, bass_weight).
- What the adjacent Moments look like (it won't duplicate; it will create contrast and continuity).
- The existing effect and palette catalogue (it will only suggest effects/palettes that exist in the firmware).

The result appears as a preview in the Stage. Accept, reject, or refine.

### 10.3 The Magic Wand

The precision tool. Select any time range in the timeline. Press `W`.

A small prompt appears inline:

```
🪄 What should change here? ________________________________
```

Type: "make this 4 bars a stutter effect" or "ramp brightness down to 50" or "add a nuclear transition at the downbeat."

The AI interprets your intent and modifies **only the selected region**. It doesn't touch anything outside your selection. The diff is shown in the timeline as highlighted changes. Accept or reject.

---

## Part 11: The Three Speeds

### Magic Mode — For the Listener Who Just Wants a Show

The screen is stripped down:

```
┌──────────────────────────────────────────────┐
│  ████████████████●████████████████  Stage     │
├──────────────────────────────────────────────┤
│                                              │
│  Mammoth - Dimitri Vegas & Like Mike         │
│  ▁▂▃▅▇█▇▅▃▁▂▃▅▇█▇▅▃  3:45                  │
│                                              │
│  How should this feel?                       │
│                                              │
│  ┌────────┐ ┌───────┐ ┌──────┐ ┌─────────┐  │
│  │Energetic│ │ Chill │ │ Dark │ │Cinematic│  │
│  └────────┘ └───────┘ └──────┘ └─────────┘  │
│                                              │
│         ┌──────────────────┐                 │
│         │    ▶ Generate    │                 │
│         └──────────────────┘                 │
│                                              │
└──────────────────────────────────────────────┘
```

Drop track. Pick mood. Generate. Play. Done. Thirty seconds to a complete light show.

No timeline lanes. No zones. No curves. Just the song, a vibe choice, and a button. The AI does everything. The preview plays. If you like it, export it. If you want to adjust, you can nudge section moods one at a time by clicking on sections and picking from a simplified palette of intents.

This is the Kickstarter demo mode. This is the "show your friend at a party" mode. This is the gateway drug.

### Guide Mode — For the Creator (DEFAULT)

The full layout described in Parts 2–10. Section-based Moment editing. Intent Bar. Effect and palette browsers. Expression curves when you want them. Zone choreography when you need it. AI assists but you drive.

This is where most users will spend most of their time once they've outgrown Magic Mode. The creative sweet spot: enough control to express your vision, enough assistance to stay in flow.

### Direct Mode — For the Engineer

Everything from Guide Mode, plus:

- **Cue list view**: A scrollable table showing every ShowCue with `timeMs`, `type`, `targetZone`, and raw `data[4]` bytes. You can edit individual cues.
- **ShowBundle JSON editor**: Raw JSON with syntax highlighting. Edit the machine format directly.
- **Frame-accurate timeline**: Timestamps to the millisecond. No snapping unless you enable it.
- **Parameter sweep visualiser**: See exactly where ActiveSweep interpolations are active, with start/target values and duration overlays.
- **Device diagnostics**: Real-time frame timing, render budget usage, PSRAM allocation visible in a sidebar.
- **Transition timeline**: Exact transition buffer state visible during playback (source, target, blend progress).

You switch modes via a three-position toggle in the top bar. Your work is preserved across switches — switching to Direct reveals the cues that Magic or Guide mode generated beneath the Moment abstractions. Switching back to Guide hides them but doesn't destroy them.

---

## Part 12: The Keyboard — Flow-State Shortcuts

When you're in flow, your hands shouldn't leave the keyboard. Every frequent action has a single-key shortcut:

### Transport

| Key | Action |
|-----|--------|
| `Space` | Play / Pause |
| `<` | Jump to previous section |
| `>` | Jump to next section |
| `,` | Previous beat |
| `.` | Next beat |
| `Home` | Jump to start |
| `End` | Jump to end |
| `L` | Toggle loop on selection |
| `[` / `]` | Zoom in / out on timeline |

### Editing

| Key | Action |
|-----|--------|
| `B` | Split Moment at playhead |
| `M` | Merge selected Moments |
| `F` | Open effect browser |
| `P` | Open palette browser |
| `T` | Open transition picker |
| `W` | Magic Wand on selection |
| `Tab` | Focus the Intent Bar |
| `1`–`4` | Select Zone 1–4 |
| `0` | Select Global (no zone) |
| `Delete` | Remove selected item |
| `Cmd+Z` | Undo |
| `Cmd+Shift+Z` | Redo |
| `Cmd+C` | Copy style |
| `Cmd+V` | Paste style |
| `Cmd+G` | Generate AI draft for selected section |
| `Cmd+Enter` | Preview from playhead position |

### View

| Key | Action |
|-----|--------|
| `E` | Toggle expression lanes |
| `Z` | Toggle zone lanes |
| `R` | Toggle Rig panel |
| `I` | Toggle Inspector panel |
| `S` | Solo selected zone in preview |
| `A` | Toggle LGP simulation mode |

---

## Part 13: The Performance — Live Override with Tab5

The Tab5 encoder is the MIDI controller of light.

When connected, PRISM shows a **Record** button in the Transport bar. Here's the workflow:

1. You have a show built in Guide Mode. The structure is there. But you want *human feel* on the brightness during the drop — you want to ride the energy with your hands.

2. Press **Record**. The record indicator glows red.

3. Press **Play**. The song starts. The Stage shows the show.

4. As the drop hits, you **turn the brightness encoder on Tab5**. Push it up on the heavy kicks. Pull back in the gaps. Feel the music in your fingers.

5. Press **Stop**. The automation curve for brightness now has your performance baked in — not a programmed ramp, but an organic, human envelope that breathes with the music.

6. Play it back. The brightness curve has the imperfections and the feel that only a human hand can create. If it's too rough, apply a smoothing filter. If it's perfect, keep it raw.

This is **automation recording** — the same workflow that every music producer uses with MIDI controllers, but applied to light parameters. The Tab5's 8 encoders map directly to the 8 expression parameters. You can record all 8 simultaneously.

The Tab5 also works in **live performance mode** without recording — you're at a party, the show is playing, and you ride the parameters in real-time over the top of the composed show. The Tab5 overrides become "live offsets" that are applied on top of the automation curves. When you release an encoder, the parameter fades back to the composed value over 2 seconds.

---

## Part 14: The Addictive Loop

What makes you come back tomorrow.

**The 30-second payoff.** Drop Mammoth. Hit Generate. See the entire anthem translated into light in the time it takes to pour a drink. Instant gratification. You've never been able to do this before. The first time it works, you're hooked.

**The "one more section" hook.** You finished the drop, and it looks incredible. But the build before it is too plain — it doesn't earn the drop. "Let me just try ramping the intensity..." And now the build earns it, but the intro doesn't set up the build properly. "One more tweak..." Thirty minutes later, you've sculpted every section and you didn't notice time passing.

**The happy accident.** You drag the wrong effect onto the break — Holographic instead of Breathing — and it looks AMAZING. The light guide plate's interference patterns during the stripped-back section create this ethereal, haunting quality you never planned. You keep it. The best creative tools produce surprises.

**The A/B moment.** You hold `Cmd+Z` to see the before. Release to see the after. The difference is visceral. Your version is better. That feeling of improvement is addictive.

**The preview-on-device moment.** You hit "Send to Device". The physical LGP lights up with YOUR creation. The software preview was good; the real thing is transcendent. The acrylic waveguide does things to the light that no screen can simulate. This is the moment that sells the Kickstarter.

**The share moment.** You export the ShowBundle. Post it to the community feed. Someone in Berlin loads your Mammoth show on their device. They send you a video. Your creation, their hardware, their room. You're a light artist.

**The live performance.** Friday night. Friends over. Mammoth comes on. You have Tab5 in front of you and the show loaded. You ride the brightness through the build, punch up the intensity on the drop, pull everything back for the break. Your friends don't know what they're watching. They just know the light is *alive.* That is the feeling that makes you open PRISM again on Saturday.

---

## Part 15: Questions This Design Must Answer

These are the hard product questions that flow from this design. Each one requires a decision before implementation.

### Creation

1. **How precise is the software preview?** The Stage renders 320 LEDs accurately. But it cannot simulate the physical LGP's diffusion, interference, and depth. How critical is the LGP simulation mode? Is a Gaussian blur sufficient, or do we need a physically-based optical model?

2. **Offline-first or device-connected?** Can you author a complete show without the physical hardware? (Answer should be yes — the software preview must be sufficient for 90% of creation. Device is for the final 10% of validation.)

3. **Effect parameter preview latency**: When you adjust a slider in the Inspector, how quickly does the Stage update? Target: <50ms. If software rendering is too slow for real-time preview, we need a GPU-accelerated preview pipeline (PixiJS/WebGL).

4. **Trinity analysis time**: How long does it take to analyse a 4-minute track? If >15 seconds, we need a progress indicator and the ability to start working with just the waveform before the structure analysis completes.

### Scope

5. **Multi-device authoring**: When a creator has 3 K1 devices, does the Rig panel show 3 separate hardware diagrams? Can zones span devices? This is Stage 5 (post-Kickstarter), but the UI architecture should not prevent it.

6. **Video export**: Can the creator export a screen recording of the Stage as a video for social media? This is the primary viral loop. A 15-second Instagram reel of Mammoth's drop in light would sell units.

7. **Palette editor**: Can creators design custom palettes, or only choose from the 75 built-in? Custom palettes require a gradient editor and a way to deploy to the device. Likely post-Kickstarter.

### Platform

8. **Web vs native**: This design assumes a web app (React + PixiJS + WaveSurfer). Does it need to run on iPad natively? A Progressive Web App with OPFS storage could work on iPad Safari. True native would require reimplementation in SwiftUI.

9. **Collaboration**: Can two creators work on the same show simultaneously? Real-time collaboration (a la Figma) is a massive engineering lift. Probably post-Kickstarter. But the file format (ShowBundle JSON) should support it architecturally.

10. **The blank-canvas problem**: The empty timeline is intimidating. Besides the AI Generate button, what other onboarding scaffolding reduces the first-creation friction? Starter templates? A guided "Create your first show in 60 seconds" tutorial?

### Identity

11. **What is this NOT?** PRISM Studio is not a VJ tool (no live video mixing), not a DMX controller (no fixture patching), not a music production DAW (no audio editing). It is specifically an authoring environment for translating music into volumetric LED light shows on SpectraSynq hardware. Every feature that doesn't serve this purpose is scope creep.

12. **What is the minimum loveable product?** Which subset of this design ships first? Suggested MVP: Magic Mode + Guide Mode (no Direct Mode), Intent Bar (no Magic Wand), Effect/Palette browsers, 4 expression lanes (brightness, speed, mood, intensity), single-zone only (no zone choreography), AI generation, ShowBundle export. This is enough to deliver the "30-second show" promise.

---

## Appendix A: Mammoth — The Complete Worked Example

Here is the entire Mammoth light show, section by section, as a creator would build it in Guide Mode:

### INTRO (0:00–0:24) — "Cold Stomp"

```
Effect:     BeatPulse Stack
Palette:    Arctic
Transition: (none — show start)
Brightness: 40
Speed:      BPM-locked (128)
Mood:       10  (ultra-reactive)
Intensity:  50
Saturation: 120 (desaturated, icy)
Complexity: 30  (minimal)
Fade:       220 (fast decay to near-black)
Zones:      Single (unified)
```

*Rationale:* A cold, minimal pulse from the centre on each kick. The light appears as a brief icy flash that expands two inches from the centre point and immediately dies. Between kicks: near-total darkness. The space between the pulses IS the effect — it creates anticipation.

### BUILD 1 (0:24–0:55) — "The Pressure"

```
Effect:     BeatPulse Shockwave
Palette:    Copper
Transition: Fade (800ms)
Brightness: 40 → 200 (ramp over section)
Speed:      50 → 90  (ramp)
Mood:       10 → 40  (gradually smoothing)
Intensity:  50 → 220 (ramp)
Saturation: 120 → 200 (warming)
Complexity: 30 → 180 (increasing detail)
Fade:       220 → 100 (trails lengthening)
Zones:      Dual — Zone 1 (core): Shockwave; Zone 2 (outer): Breathing
Cross-zone: Cascade Out (200ms delay)
```

*Rationale:* Every parameter ramps. The light grows outward, brightens, speeds up, and develops longer trails as the snare roll intensifies. Zone 2 (outer ring) is breathing slowly while Zone 1 (core) fires shockwaves — creating the sense that the outer zone is "charging up" while the core drives the rhythm. Copper palette adds warmth as the section builds toward the drop. The riser synth should make the brightness feel like it's being pulled upward.

### DROP 1 (0:55–1:48) — "DETONATION"

```
Effect:     BeatPulse Bloom (Zone 1), Holographic (Zone 2), Star Burst (Zone 3)
Palette:    Ember (Zone 1+3), Copper (Zone 2)
Transition: Nuclear (2500ms)
Brightness: 255
Speed:      85
Mood:       25  (mostly reactive, slight smoothing)
Intensity:  240
Saturation: 240
Complexity: 200
Fade:       140 (moderate trails — light lingers)
Zones:      Triple
Cross-zone: Cascade Out (150ms delay)
```

*Rationale:* Maximum everything. The Nuclear transition detonates from centre, replacing the build's controlled ramp with an explosive transformation. BeatPulseBloom's subpixel transport creates liquid light that advects outward from the centre with every kick. Holographic on Zone 2 exploits the LGP's interference properties to create depth — the drops feel three-dimensional. Star Burst on the outer ring fires radial lines that look like the light is trying to escape the edges of the plate. Every beat is a shockwave cascade: core → middle → outer.

### BREAK (1:48–2:12) — "The Held Breath"

```
Effect:     Breathing (global)
Palette:    Deep Ocean
Transition: Implosion (1500ms)
Brightness: 60
Speed:      20
Mood:       230 (very smooth)
Intensity:  40
Saturation: 160
Complexity: 60
Fade:       40  (long trails — ghostly persistence)
Zones:      Single (unified)
```

*Rationale:* The Implosion transition collapses the drop's energy inward to the centre — a visual exhale. Then Breathing takes over: a slow sinusoidal expansion and contraction from centre, like the room itself is breathing. Deep Ocean palette is cool and restful. Long trails mean the light ghosts and smears rather than cutting sharp. This section's purpose is relief — your eyes and nervous system need the rest to make DROP 2 hit even harder.

### BUILD 2 (2:12–2:40) — "The Pressure (Reprise)"

```
Same as BUILD 1, but:
- Starting brightness: 60 (higher than Build 1's 40)
- Effect: BeatPulse Spectral (frequency-split rings)
- Additional: Zone 3 activates at 75% through the build (Cascade Out, 100ms)
- Saturation peaks at 255 (hotter than Build 1)
```

*Rationale:* The second build should feel MORE intense because you know what's coming. Starting from a higher brightness floor and adding a third zone partway through gives the sense that the energy is exceeding what the system can contain. Spectral effect splits bass/mid/treble into different radial zones — the build literally spreads across more of the strip as it intensifies.

### DROP 2 (2:40–3:24) — "DETONATION II"

```
Same as DROP 1, but:
- Zones: Quad (all four active)
- Zone 4: Kuramoto Transport (the emergent dynamics effect)
- Cross-zone: Cascade Out with 100ms delay (tighter than Drop 1's 150ms)
- Transition: Stargate (3000ms) — a wormhole opens at centre
```

*Rationale:* The second drop needs to top the first. Four zones instead of three. Tighter cascade delay means the shockwaves hit faster. Kuramoto Transport on the outer ring brings genuinely emergent behaviour — an oscillator field whose phase dynamics create patterns no algorithm explicitly programmed. The Stargate transition is slower but more dramatic than Nuclear, creating a visual event that makes the listener go "how did it do THAT."

### OUTRO (3:24–3:45) — "The Afterglow"

```
Effect:     Breathing → Fade to black
Palette:    Arctic (return to cold)
Transition: Wipe In (1200ms)
Brightness: 60 → 0 (ramp to black)
Speed:      20 → 5
Mood:       240
Intensity:  30 → 0
Zones:      Single
```

*Rationale:* The Wipe In transition collapses the drop inward. Breathing at low intensity creates a dying heartbeat. The return to Arctic palette bookends the show — we started cold, we end cold. Brightness ramps to zero. The last thing visible is a tiny pulse at the centre point. Then black.

---

## Appendix B: ShowBundle Export Format

The Mammoth show, when exported, produces a ShowBundle JSON that the firmware can consume:

```json
{
  "id": "mammoth-dvlm",
  "name": "Mammoth — DVLM",
  "version": 1,
  "bpm": 128,
  "totalDurationMs": 225000,
  "chapters": [
    {
      "name": "Cold Stomp",
      "startTimeMs": 0,
      "durationMs": 24000,
      "narrativePhase": "REST",
      "tensionLevel": 30
    },
    {
      "name": "The Pressure",
      "startTimeMs": 24000,
      "durationMs": 31000,
      "narrativePhase": "BUILD",
      "tensionLevel": 150
    },
    {
      "name": "DETONATION",
      "startTimeMs": 55000,
      "durationMs": 53000,
      "narrativePhase": "HOLD",
      "tensionLevel": 240
    }
  ],
  "cues": [
    { "timeMs": 0,     "type": "EFFECT",    "zone": 255, "data": "BeatPulseStack" },
    { "timeMs": 0,     "type": "PALETTE",   "zone": 255, "data": "arctic" },
    { "timeMs": 0,     "type": "SWEEP",     "zone": 255, "param": "brightness", "target": 40, "durationMs": 0 },
    { "timeMs": 24000, "type": "EFFECT",    "zone": 255, "data": "BeatPulseShockwave" },
    { "timeMs": 24000, "type": "PALETTE",   "zone": 255, "data": "copper" },
    { "timeMs": 24000, "type": "TRANSITION","zone": 255, "data": "fade", "durationMs": 800 },
    { "timeMs": 24000, "type": "SWEEP",     "zone": 255, "param": "brightness", "target": 200, "durationMs": 31000 },
    { "timeMs": 24000, "type": "SWEEP",     "zone": 255, "param": "speed",      "target": 90,  "durationMs": 31000 },
    { "timeMs": 24000, "type": "SWEEP",     "zone": 255, "param": "intensity",  "target": 220, "durationMs": 31000 },
    { "timeMs": 55000, "type": "EFFECT",    "zone": 0,   "data": "BeatPulseBloom" },
    { "timeMs": 55000, "type": "EFFECT",    "zone": 1,   "data": "LGPHolographic" },
    { "timeMs": 55000, "type": "EFFECT",    "zone": 2,   "data": "LGPStarBurst" },
    { "timeMs": 55000, "type": "PALETTE",   "zone": 0,   "data": "ember" },
    { "timeMs": 55000, "type": "PALETTE",   "zone": 1,   "data": "copper" },
    { "timeMs": 55000, "type": "TRANSITION","zone": 255, "data": "nuclear", "durationMs": 2500 },
    { "timeMs": 55000, "type": "SWEEP",     "zone": 255, "param": "brightness", "target": 255, "durationMs": 0 },
    { "timeMs": 55000, "type": "ZONE",      "zone": 255, "count": 3, "enabled": 7 }
  ]
}
```

Note: The JSON ShowBundle uses **name-based** effect and palette references. The firmware resolves these via `PatternRegistry::getPatternMetadata(name)` and palette name lookup at load time. This decouples the authoring format from firmware-internal integer IDs, making ShowBundles portable across firmware versions.

---

## Appendix C: Technology Stack

PRISM Studio is a web application, optimised for desktop Chrome/Safari with tablet (iPad) as a secondary target.

| Layer | Technology | Why |
|-------|-----------|-----|
| Framework | React 19 + TypeScript | Matches existing lightwave-dashboard. Component model suits the panel-based layout. |
| State | Zustand | Lightweight, no boilerplate. Show state + UI state in separate stores. |
| Timeline rendering | PixiJS v8 (WebGL) | Canvas/DOM too slow for 60fps timeline scrubbing with waveform + automation curves + beat grid. PixiJS gives GPU-accelerated 2D rendering. |
| Waveform | WaveSurfer.js v7 | Battle-tested waveform rendering with plugin ecosystem. Provides beat marker overlay, zoom, scrub. |
| Audio playback | Web Audio API | Precise time-synchronised playback. `AudioContext.currentTime` drives the playhead with sub-millisecond accuracy. |
| LED preview | Custom Canvas/WebGL | 320-LED strip rendered at native resolution. Same colour maths as firmware (FastLED's `ColorFromPalette` ported to JS). |
| AI integration | REST to hosted Claude/GPT | Structured output: AI returns JSON with effect names, parameter values. The frontend validates against the effect/palette catalogue before applying. |
| File storage | OPFS (Origin Private FS) | Browser-local project storage. No server needed. ShowBundle JSON + audio files persisted locally. |
| Build | Vite | Matches existing dashboard. Fast HMR during development. |
| Design system | Glass V4 (existing) | Dark frosted-glass aesthetic already established in lightwave-dashboard. |

---

## Appendix D: Minimum Viable Product — What Ships for Kickstarter

The MVP is the smallest subset of this design that delivers the "drop a song, get a light show" promise.

### Ships:

| Feature | Scope |
|---------|-------|
| Track import | Drag-and-drop MP3/WAV. Trinity analysis via local Python or pre-computed. |
| Waveform timeline | Stereo waveform with beat grid and section markers. Scroll, zoom, scrub. |
| Magic Mode | Drop track → pick mood → Generate → Play. Full AI-generated show. |
| Guide Mode (basic) | Section-level Moment editing. Effect/palette assignment per section. |
| Effect browser | Visual card grid with thumbnails. 147 effects with family filters. |
| Palette browser | Gradient swatch list with warm/cool/CVD filters. 75 palettes. |
| Intent Bar | Natural language → parameter suggestion. Accept/reject. |
| 4 expression lanes | Brightness, Speed, Mood, Intensity. Ramp drawing + section-level values. |
| LED Preview (Stage) | Software-rendered 320-LED strip. Pixel mode only (no LGP sim). |
| ShowBundle export | JSON export. Upload to device via REST API. |
| Device playback | Send ShowBundle to K1 via WebSocket. Play/pause/stop control. |

### Does NOT ship:

| Feature | Why deferred |
|---------|-------------|
| Zone choreography | Zones work on device, but per-zone authoring in PRISM adds significant UI complexity. Single-zone MVP is sufficient for the "30-second show" demo. |
| Direct Mode | Power-user cue-level editing. Not needed for the core promise. |
| Magic Wand | Region-specific AI edits. Intent Bar covers 80% of AI use cases. |
| Tab5 automation recording | Requires Tab5 ↔ PRISM WebSocket bridge. Not essential for authoring. |
| Video export | Would require headless rendering pipeline. Use screen recording for now. |
| Collaboration | Post-Kickstarter. |
| Multi-device | Post-Kickstarter. |
| Custom palette editor | Post-Kickstarter. |
| Saturation/Complexity/Variation/Fade lanes | Available as section-level values in Guide Mode, but not as drawable automation curves. 4 lanes is enough for MVP. |

### The Kickstarter Demo (2 minutes)

**0:00–0:15**: Open PRISM. Drag Mammoth onto the window. Waveform appears. Sections detected. "AI detected 7 sections at 128 BPM."

**0:15–0:25**: Click "Generate". Full light show appears in the timeline. Hit Play. The Stage preview shows the show synced to the music. It already looks good.

**0:25–0:50**: Click on the DROP section. Type in the Intent Bar: "make this more explosive." AI suggests Nuclear transition + max brightness. Accept. Preview again. The drop hits harder.

**0:50–1:10**: Click on the INTRO. Drag Arctic palette from the browser. Drag brightness ramp from 0 to 40. The intro is now cold and minimal. Play from start — the contrast between cold intro and explosive drop is dramatic.

**1:10–1:25**: Hit "Send to Device." Cut to a shot of the physical LGP. The light show plays. It's real. It's on hardware. The creator made this in 60 seconds.

**1:25–1:45**: "But this is just the beginning." Show the Guide Mode timeline with expression lanes visible. Show someone sculpting brightness curves. Show the palette swapping. Show the section-by-section refinement. "PRISM Studio gives you the tools to sculpt every detail."

**1:45–2:00**: Final payoff shot. Mammoth's drop. The LGP in a dark room. Every zone firing. The light is ALIVE. Text: "Drop a song. Get a light show. In 30 seconds." SpectraSynq logo.
