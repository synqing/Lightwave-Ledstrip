---
abstract: "Competitive UI research across 13 music/lighting/creative controllers, synthesised into 4 design directions for Tab5 1280x720 touchscreen redesign. Covers information hierarchy, encoder-screen mapping, colour palettes, typography, and aesthetic DNA. Read when planning Tab5 UI redesign or selecting a visual direction."
---

# Tab5 UI Design Research: Controller Interface Patterns

## Context

The Tab5 is the physical controller for the K1 Lightwave visual instrument: 16 rotary encoders (2x M5ROTATE8 units) + a 1280x720 IPS touchscreen running LVGL 9.3 on ESP32-P4. The current UI is a dark card grid with uniform visual weight -- functional but not inspiring. This document researches how professional music, lighting, and creative instrument controllers handle UI design on small embedded screens, then synthesises findings into actionable design directions.

**Current Tab5 UI characteristics (from `Theme.h` and `DisplayUI.h`):**
- Pure black background (`0x0000`) with dark grey panels (`0x0841`)
- 8 neon parameter colours (magenta, cyan, pink, green, purple, yellow, orange, blue)
- 1280x720 landscape, single row of 8 gauge cards (160px wide x 160px tall)
- Preset bank row below, touch action row below that
- Status bar header (80px), footer with BPM/key/mic/uptime metrics
- All elements have uniform visual weight -- no clear hierarchy
- Bebas Neue font family throughout

---

## 1. Music Production Controllers

### 1.1 Ableton Push 3

**Display:** Landscape LCD (estimated ~960x600), non-touch, surrounded by 8 touch-sensitive encoders above and buttons/pads below.

**Information hierarchy:**
- *Always visible:* Track names/colours along the top, parameter names/values aligned to 8 encoders, transport status. The session view grid (8x8 clips) can overlay the full display.
- *Paged/tabbed:* Device parameters, mixing, clip editing, browser -- each is a distinct screen mode. The screen and pad modes are decoupled, so the screen can show device parameters while pads show clips.
- *Hidden:* File management, settings, global preferences.

**Visual hierarchy:**
- Dark charcoal background (not pure black) with coloured accents per track.
- Track colour coding is the primary organising principle -- each track gets a hue, and that hue carries through clips, parameters, and pad illumination.
- Parameter names in small caps above encoder columns, values in larger type below. Active/automated parameters get colour treatment; inactive ones are grey.
- Well-designed animated graphics for built-in devices provide visual personality.

**Encoder-screen mapping:**
- Direct 1:1 mapping: 8 encoders map to 8 parameter columns on screen. Touch-sensitive encoders highlight the corresponding parameter on contact. Banks of 8 allow paging through parameters.
- Macro mapping: parameters can be assigned to specific encoder positions.

**Aesthetic DNA:**
- Professional studio tool aesthetic. Matte black hardware, white pad grid, coloured LED feedback.
- Typography: clean sans-serif, small but readable. Information density is moderate -- enough to work without a computer, but not overwhelming.
- The display feels like a focused *window into a DAW*, not a standalone UI.

**What makes it feel creative:** The session view grid on screen mirrors the physical pad grid, creating a tangible connection between what you touch and what you see. Animated device graphics add personality beyond raw numbers.

### 1.2 Teenage Engineering OP-1 / OP-1 Field

**Display:** AMOLED, 60fps real-time vector graphics. Small but sharp.

**Information hierarchy:**
- *Always visible:* The current engine/effect visualisation dominates the screen. Four encoder values shown as small indicators.
- *Paged:* Four colour-coded modes -- Synth (blue), Drum (red), Tape (green), Mixer (yellow). Each mode completely transforms the screen.
- *Hidden:* Almost nothing is hidden -- the interface is shallow by design.

**Visual hierarchy:**
- **Colour is modal, not decorative.** The entire screen shifts hue based on the active mode. This is the single strongest hierarchy signal.
- Each synthesis engine has a unique animated visualisation -- a cow, a car race, abstract geometry. These are not informational; they are *emotional*. They make the instrument feel alive.
- Encoder values are secondary to the visualisation. The visual tells you the *character* of the sound; the numbers tell you the *precision*.

**Encoder-screen mapping:**
- Four coloured encoders map to four on-screen parameters. The OP-1 Field shows pop-up graphics when encoder parameters are not obvious.
- The colour of each encoder cap matches the parameter colour on screen.

**Aesthetic DNA:**
- **Radical minimalism meets playfulness.** White aluminium body, coloured accents, no labels on the body -- the screen *is* the interface.
- Typography: custom pixel-influenced fonts, clean but with personality. Not corporate.
- The OP-1 Field's updated palette uses darker blue, ochre, grey, and orange (more mature than the original's primaries).
- Spacing is generous. The screen is never crowded.

**What makes it feel creative:** The animated visualisations are the key. Every engine has a visual personality. You are not tweaking parameters; you are *playing with a character*. The instrument encourages exploration because the visual feedback is delightful, not just informative.

### 1.3 Teenage Engineering TX-6

**Display:** 48x64 pixel monochrome. Extremely constrained.

**Aesthetic DNA:**
- Proves that even a tiny display can feel premium through typography and animation quality.
- Pixelated but carefully crafted fonts, menus, and meters familiar across the TE ecosystem.
- The display automatically sleeps when idle; LED brightness is adjustable.
- Demonstrates that *constraint breeds character* -- the tiny screen's limitations become an aesthetic choice.

### 1.4 Native Instruments Maschine+

**Display:** Two high-resolution colour displays side by side.

**Information hierarchy:**
- *Always visible:* Current group/sound name, pad colours, transport state across both screens.
- *Paged:* Plug-in parameters, browser, mixer, sampler, arranger -- each gets a dedicated screen mode. Eight knobs below the displays map to on-screen parameters.
- *Four pad modes double as screen modes* -- a clever physical-digital coupling.

**Encoder-screen mapping:**
- Eight knobs below each display map directly to parameters shown on that display.
- Knob bank and button bank functions change based on the current screen view.
- Quick Edit buttons (volume, tempo, swing) provide shortcuts to the most-used parameters.

**Aesthetic DNA:**
- Dark professional aesthetic. Colour comes from pad illumination and on-screen accents.
- Dense information display -- Maschine assumes the user is experienced and wants data.
- Typography: clean, functional, information-first.

**What makes it feel creative:** The dual-display setup provides context (left) and detail (right) simultaneously. You never lose the big picture while editing specifics.

### 1.5 Akai MPC Live

**Display:** Large touchscreen (7" on MPC Live, 10.1" on MPC Live II), full-colour.

**Information hierarchy:**
- *Always visible:* Transport controls (on-screen), current track/program name.
- *Paged:* Grid view (piano roll), sample editor, mixer, browser, effects -- each is a full-screen mode.
- Touch interaction is primary -- knobs are secondary.

**Encoder-screen mapping:**
- Four Q-Link knobs are capacitive and map to on-screen parameters, but touch is the primary interaction model.
- The screen *is* the main control surface, with hardware knobs as supplements.

**Aesthetic DNA:**
- Classic MPC dark grey with coloured pad illumination.
- The touchscreen brings a phone/tablet interaction model to hardware.
- Pinch-and-zoom for sample editing -- leveraging touch gestures.

**What makes it feel creative:** The large touchscreen makes the MPC feel like an iPad with hardware pads. Direct manipulation of waveforms and piano rolls creates immediacy.

---

## 2. Lighting Controllers

### 2.1 grandMA3 (MA Lighting)

**Display:** Multiple large multi-touch screens. The "encoder bar" is a dedicated UI zone (screen 8 on full-size, bottom of screen 1 on compact).

**Information hierarchy:**
- *Always visible:* The encoder bar shows active parameter values, feature group selection (Dimmer, Position, Gobo, Colour, etc.), and programmer timing.
- *Paged:* Fixture sheets, layout views, cue lists, effects engine -- all are independently sizable windows that can be arranged across multiple monitors.
- *Seven distinct zones in the encoder bar alone:* user settings, encoder bank buttons, encoder toolbar with layer toolbar, phaser control, MAtricks control, time faders, grand master.

**Visual hierarchy:**
- **Feature groups are colour-coded** -- Dimmer is yellow text when selected. This is the primary organisational signal in a dense interface.
- Inner/outer encoder objects use white when attributes match, and split colour when they differ.
- Colour chips at the top of encoders, fixture sheet values, presets, and groups indicate the active layer.
- The encoder displays the current resolution in the centre of the encoder symbol.

**Encoder-screen mapping:**
- Physical dual encoders map to on-screen encoder widgets in the encoder bar.
- The encoder bar is customisable -- sections can be toggled on/off to match workflow.
- Encoder banks switch which parameter group the encoders control.

**Aesthetic DNA:**
- **Dense, professional, dark background with coloured accents.** This is an information-first interface designed for operators controlling hundreds of fixtures.
- Typography: functional, small, with high information density.
- The interface is deeply customisable -- operators build their own layouts.

**What makes it feel creative:** The extreme customisability means every operator's workspace is unique. The interface adapts to the show rather than forcing the show into a fixed layout.

### 2.2 ETC Eos Family

**Display:** Large multi-touch screens (up to 4K on Apex models). Physical encoder wheels below.

**Information hierarchy:**
- *Always visible:* Channel/fixture selection, intensity wheel value, encoder parameter names/values.
- *Paged:* Encoder category buttons (Intensity, Focus, Colour, Beam, etc.) switch which parameters the four encoder wheels control.
- Parameters displayed change based on selected device(s).

**Encoder-screen mapping:**
- Four encoder wheels plus a dedicated intensity wheel.
- Encoder category buttons on the top row switch the parameter bank.
- Virtual encoder page available in software for remote operation.

**Aesthetic DNA:**
- Clean, professional. The Apex series uses large screens with generous spacing.
- Forty "Target Keys" on larger models provide physical shortcuts.
- The interface prioritises *speed of access* for live show operation.

### 2.3 Resolume Arena/Avenue

**Display:** Full computer screen, typically large monitors.

**Information hierarchy:**
- *Always visible:* Clip grid (horizontal rows with thumbnails), output preview, layer stack.
- *Paged:* Dashboard, autopilot, effect chains, composition settings -- panels can be hidden/shown.
- Clips display animated thumbnails that highlight during playback.

**Visual hierarchy:**
- **Clip thumbnails are the primary visual element** -- colour-coded per deck, group, layer, column, or clip.
- Dark background with colourful clip thumbnails and preview monitors.
- Active/playing clips have bright highlight states.

**Aesthetic DNA:**
- VJ tool aesthetic: dark, video-forward, clip-grid-dominant.
- The interface is *about the visual content* -- the UI frames the media rather than competing with it.
- BPM sync and effect parameters are secondary to the clip grid.

**What makes it feel creative:** The clip grid with animated thumbnails creates a tangible, visual workspace. You can *see* your content, not just parameter names.

---

## 3. Creative Instrument UIs

### 3.1 Monome Norns

**Display:** 128x64 pixel greyscale OLED, 16 brightness levels.

**Information hierarchy:**
- Entirely script-defined. Each Lua script creates its own interface from scratch.
- No fixed elements -- no status bar, no parameter labels, no menus.
- A typical script shows 2-4 values with custom animated visualisations.

**Visual hierarchy:**
- **Anti-aliased greyscale creates depth** on a tiny display. 16 brightness levels enable sophisticated gradients and transparency effects.
- No labels on the physical device. Nothing is labelled -- anywhere. The screen must be self-explanatory.
- Three encoders and three buttons have no predetermined mapping.

**Aesthetic DNA:**
- **Radical minimalism.** The device is a blank canvas. Every script creates a unique instrument.
- Typography: custom bitmap fonts, deliberately low-resolution.
- The constraints (128x64, greyscale) become the aesthetic. It looks like nothing else.
- Generous white space (or black space). The screen is never filled.

**What makes it feel creative:** The blank-canvas philosophy means every script is a new instrument with a new interface. The extreme constraints force designers to distil interfaces to their essence. There is nothing extraneous.

### 3.2 Dirtywave M8

**Display:** 3.5" IPS with capacitive touch (Model:02), customisable colour themes.

**Information hierarchy:**
- Classic tracker layout: hexadecimal spreadsheet grid with columns for notes, instruments, effects.
- *Always visible:* Current pattern/chain, active track, step position.
- *Paged:* Hierarchical navigation -- Shift+Up moves to song-level pages, Shift+Right drills into detail. A miniature navigation map in the bottom-right shows current position.
- Five parameter pages per track: Song, Chain, Phrase, Instrument, Table.

**Visual hierarchy:**
- **Colour themes are user-customisable** -- brightness, font, and all interface colours adjustable in RGB or HSV mode.
- The tracker grid uses colour to distinguish columns (note, velocity, instrument, effects).
- Default aesthetic is deliberately retro -- inspired by Game Boy tracker LSDJ.
- Big Font mode available for accessibility.

**Aesthetic DNA:**
- **Utilitarian pixel art.** Function over decoration. The tracker grid IS the interface.
- Typography: pixel/bitmap fonts, monospaced for column alignment.
- The retro aesthetic is a deliberate design choice, not a limitation.
- Eight mechanical keys handle all input -- the interface is optimised for rapid key combinations.

**What makes it feel creative:** The tracker workflow itself is the creative tool. The constraint of step-sequencing forces a different compositional thinking. The interface speed (once muscle memory develops) creates flow state.

### 3.3 Synthstrom Deluge

**Display:** Small OLED (retrofit option), but the massive RGB pad matrix IS the primary display.

**Information hierarchy:**
- The 128-pad RGB matrix shows: notes (in synth mode), clips (in song mode), audio waveforms (in audio clip mode), automation, probability -- all through pad colour and brightness.
- The OLED displays parameter names and values completely (previously a 4-character 7-segment display).
- Two knobs provide the primary parameter adjustment.

**Aesthetic DNA:**
- **The pads ARE the screen.** The OLED is supplementary. This is the inverse of most controllers.
- Instrument-first philosophy: the Deluge is designed for creating music, not navigating menus.
- The pad matrix provides spatial, visual, and tactile feedback simultaneously.

**What makes it feel creative:** The direct visual feedback through pad illumination creates an immediate, physical connection to the music. You can *see* patterns, notes, and automation as colour.

### 3.4 Elektron Digitakt/Syntakt

**Display:** OLED, small but crisp. Monochrome with high contrast.

**Information hierarchy:**
- *Always visible:* Current track number, pattern name, playback position (via 16 backlit buttons: red=playhead, white=active triggers, dim red=focused track steps).
- *Paged:* Five parameter pages per track (TRIG, SRC, FLTR, AMP, LFO), each exposing 8 parameters mapped to 8 knobs.
- The 16 buttons below the screen double as step sequencer and track selector.

**Visual hierarchy:**
- **The buttons are the primary visual feedback**, not the screen. Playback position (bright red), triggers (white), and track focus (dim red) create a three-level visual hierarchy through button illumination.
- The OLED shows parameter names aligned to the 8 knobs above. Matt black aesthetic with rubberised grey rotaries and minimal coloured LEDs.
- Information density is moderate -- enough to work but not overwhelming. The interface assumes experienced users.

**Encoder-screen mapping:**
- **Direct spatial correspondence:** the positions of parameters on the screen correspond to the physical locations of the 8 data entry knobs. This 1:1 spatial mapping is the core of Elektron's interaction model.
- Page buttons below the knobs switch parameter banks.

**Aesthetic DNA:**
- **Industrial minimalism.** Matt black, rubberised grey encoders, sparse LED colour.
- Typography: clean monospaced numerics for parameter values, small sans-serif for labels.
- The Digitakt/Syntakt look like professional tools, not toys.

**What makes it feel creative:** The Elektron workflow of parameter locks (per-step automation) and conditional triggers transforms a simple sequencer into a generative composition tool. The interface stays out of the way and lets the workflow shine.

---

## 4. Cross-Cutting Design Patterns

### 4.1 What Separates "Creative Tool" from "Settings Panel"

| Settings Panel | Creative Tool |
|---|---|
| Uniform visual weight for all parameters | Clear hierarchy: some params prominent, others recessed |
| Static labels and values | Animated feedback, visual personality |
| All options visible simultaneously | Focused modes, progressive disclosure |
| Neutral colour palette | Colour carries meaning (modal, categorical, emotional) |
| Grid of identical cards | Varied element sizes, asymmetric layouts |
| Numbers only | Visual representations (arcs, bars, animations) |
| No response to input beyond value change | Micro-animations on interaction, highlight on touch |

### 4.2 Encoder-Screen Mapping Strategies

1. **Direct spatial (Elektron, Push, grandMA3):** Encoder N maps to column N on screen. The most intuitive, but limits parameters to the encoder count per page.
2. **Colour-coded modal (OP-1):** Encoder colour matches screen parameter colour. Mode changes transform the entire screen.
3. **Bank/page (Maschine, ETC Eos):** Buttons switch which parameter bank the encoders control. Labels on screen update accordingly.
4. **Touch-primary (MPC):** Screen is the main input; encoders supplement. Works when screen is large enough for direct touch targets.

### 4.3 Information Density Spectrum

| Low Density | Medium Density | High Density |
|---|---|---|
| Norns (128x64, 2-4 values) | Digitakt (8 params + transport) | grandMA3 (hundreds of fixtures) |
| OP-1 (4 params + viz) | Push 3 (8 params + session grid) | Resolume (clip grid + effects) |
| TX-6 (minimal display) | Maschine (dual displays) | ChamSys MagicQ (multi-window) |

**For Tab5 at 1280x720 with 16 encoders:** Medium-to-high density is appropriate. The screen is large enough for rich visual feedback but constrained enough to require careful information architecture.

### 4.4 Dark Theme Principles (from research)

- Use dark grey or tinted black, NOT pure black, for primary backgrounds. Pure black increases halation and makes white text harsh.
- Desaturated accent colours work better than fully saturated neons on dark backgrounds.
- Reserve the brightest text for the highest hierarchy level. Use grey variants for secondary information.
- Slightly increase font weight on dark backgrounds -- thin fonts wash out.
- Neon-on-dark creates a futuristic, performance-tool aesthetic but must be used sparingly.

---

## 5. Design Direction Options for Tab5

### 5.1 Direction A: "Studio Monitor"

**Inspiration:** Ableton Push 3 + grandMA3 + Maschine+

**Philosophy:** Professional tool for precise control. Information-dense, visually structured, colour used for categorisation rather than decoration.

**Colour palette:**
- Background: `#1A1A1E` (warm near-black)
- Surface: `#252529` (elevated panels)
- Border: `#3A3A3F` (subtle separation)
- Primary accent: `#4B9EFF` (clear blue for active/selected)
- Secondary accent: `#3D3D42` (inactive elements)
- Semantic: green/amber/red for status only

**Typography:**
- Primary: Inter or DM Sans -- clean, professional, excellent at small sizes
- Mono: JetBrains Mono for parameter values (consistent width for numeric displays)
- Hierarchy: 10px labels (dim), 14px values (bright), 18px section headers, 24px mode title

**Layout philosophy:**
- Top 80px: status bar with connection state, effect name, BPM/key readout
- Main area: two rows of 8 parameter columns, each with label, value, and horizontal bar indicator. Row A (encoders 0-7) is the primary focus row with larger elements. Row B (encoders 8-15) is secondary with smaller elements.
- Bottom 60px: footer with system metrics, battery, uptime
- Touch targets in the middle zone for mode switching

**Information density:** High. All 16 encoder values visible simultaneously. Effect name, audio metrics, and connection state always present. Parameter names truncated with tooltip on touch.

**Encoder-screen mapping:** Direct spatial. Encoder 0 = leftmost column, encoder 7 = rightmost. Row A and Row B clearly delineated.

**Pros:**
- Maximum information at a glance -- good for experienced users
- Professional appearance befitting a premium instrument
- Familiar to anyone who has used Push, Maschine, or MA consoles
- Scalable to additional parameter pages

**Cons:**
- Can feel clinical/cold for a creative instrument
- Uniform density may overwhelm new users
- Lacks the visual personality that makes instruments inspiring
- Does not leverage the touchscreen's creative potential

---

### 5.2 Direction B: "Visual Instrument"

**Inspiration:** Teenage Engineering OP-1 + Resolume Arena + Synthstrom Deluge

**Philosophy:** The K1 creates visual art from sound. The Tab5 screen should reflect that -- showing the *output* alongside the controls. A live visualisation occupies the centre, with controls framing it.

**Colour palette:**
- Background: `#0D0D12` (deep blue-black)
- Surface: `#16161D` (dark indigo panels)
- Primary accent: from the active LED palette (dynamic, changes with the current effect/palette)
- Secondary accent: `#6E6E80` (muted lavender-grey)
- Highlight: pulled from the current colour palette's dominant colour
- Status: `#2DD4A0` (teal-green) / `#F59E0B` (warm amber) / `#EF4444` (soft red)

**Typography:**
- Primary: Space Grotesk or Outfit -- modern, slightly rounded, friendly but technical
- Mono: IBM Plex Mono for values
- Hierarchy: parameter values large (20px), labels small and dim (10px), mode title in medium weight

**Layout philosophy:**
- **Centre stage (40% of screen area):** Live palette/effect visualisation strip -- a miniature representation of the current LED output, animated in real-time from K1 broadcast data. This is the hero element that makes the Tab5 feel like a *visual instrument controller*, not a settings panel.
- **Left column (8 parameters):** Row A encoder values as slim vertical bars with labels, stacked vertically along the left edge.
- **Right column (8 parameters):** Row B encoder values along the right edge.
- **Top bar:** Minimal -- effect name, connection dot, battery.
- **Bottom strip:** BPM, key, mode selector as touch targets.

**Information density:** Medium. Only the most relevant parameter value is large; others are present but recessed. The visualisation strip communicates *what the instrument is doing* more effectively than 16 numerical values.

**Encoder-screen mapping:** Vertical columns left/right, with the active encoder's value expanding to show detail. Colour of the active encoder's bar matches the parameter accent.

**Pros:**
- Directly represents the K1's visual output -- makes the Tab5 feel connected to the instrument
- Dynamic palette integration means the UI *looks different* for every effect/palette combo
- Emotionally engaging -- the centre visualisation creates delight
- Unique in the market -- no other controller does this

**Cons:**
- Lower information density -- trades data for visual impact
- Centre visualisation requires real-time data from K1 (WebSocket bandwidth)
- Dynamic colour palette means accessibility contrast ratios vary
- More complex rendering (animated visualisation on LVGL/ESP32-P4)

---

### 5.3 Direction C: "Minimal Instrument"

**Inspiration:** Monome Norns + Dirtywave M8 + Elektron Digitakt

**Philosophy:** Strip everything to the essential. Typography and spacing create the hierarchy. No decoration, no colour gradients, no visual noise. The interface disappears and the musician focuses on *listening and feeling*.

**Colour palette:**
- Background: `#000000` (true black -- OLED-optimised, even on IPS this is a statement)
- Text primary: `#E0E0E0` (not pure white -- reduces glare)
- Text secondary: `#666666` (dim grey for labels and inactive values)
- Single accent: `#00D4AA` (teal-green -- the one colour that means "active/selected")
- Warning: `#FF6B35` (warm orange, sparingly)
- No other colours. Monochrome with one accent.

**Typography:**
- Primary: a high-quality monospaced font (Berkeley Mono, Iosevka, or Cascadia Code) -- the tracker aesthetic
- ALL CAPS for labels, mixed case for values
- Hierarchy through size and brightness alone: 32px for the focused parameter value, 12px for labels, 16px for secondary values
- Generous letter-spacing (tracking) for labels

**Layout philosophy:**
- **Single focused parameter** fills the upper-centre of the screen: name (small, dim), value (huge, bright), unit.
- **15 secondary parameters** arranged in a minimal grid below: name and value only, no bars, no gauges. The currently-tweaked encoder's value animates to the focus position.
- **No status bar** -- connection state shown as a single dot (green/amber/red) in the top-left corner.
- **No footer** -- BPM and key shown only when the audio-metrics mode is active.
- Encoder interaction highlights: when you turn encoder N, its value smoothly animates to the focus position, displacing the previous focus.

**Information density:** Low-to-medium. One parameter in sharp focus, the rest present but deliberately recessed. This is a performance-mode interface.

**Encoder-screen mapping:** The active encoder takes centre stage. Other parameters are arranged in a spatial grid that roughly corresponds to encoder positions but prioritises visual balance over strict alignment.

**Pros:**
- Maximum focus on the parameter being adjusted -- ideal for live performance
- Extremely clean, distinctive aesthetic -- unlike any competitor
- Low rendering cost -- simple geometry, single accent colour
- Accessibility: high contrast, large focused values
- The monochrome-plus-one-accent approach is timeless

**Cons:**
- Cannot see all 16 values simultaneously -- requires trust in the interface
- May feel too sparse for users who want dashboard-level overview
- The "tracker aesthetic" may not communicate "visual instrument"
- Mode switching (overview vs focus) adds cognitive load

---

### 5.4 Direction D: "Performance Grid"

**Inspiration:** Resolume Arena clip grid + Ableton Push session view + ChamSys MagicQ layout windows

**Philosophy:** The Tab5 screen is a modular grid of *performance blocks* -- each block can be a parameter gauge, a mini-visualisation, a preset bank, an audio meter, or a mode selector. The layout is reconfigurable for different performance contexts.

**Colour palette:**
- Background: `#121215` (cool near-black)
- Grid lines: `#1E1E22` (barely visible, structural)
- Card surface: `#1A1A1F` (subtle lift from background)
- Active card border: `#7C5CFC` (electric purple -- the "Lightwave" accent)
- Secondary accent: `#3B82F6` (blue for secondary interactive elements)
- Parameter colours: a curated set of 8 desaturated neons, one per parameter category
- Mode indicator: colours shift per mode (Global = purple, Zone = teal, FX = amber)

**Typography:**
- Primary: Geist or Inter -- crisp at all sizes, variable weight
- Mono: Geist Mono for values
- Hierarchy: card titles in SMALL CAPS (11px, 600 weight, letter-spaced), values in regular weight (16-20px)

**Layout philosophy:**
- **8-column grid** (matching encoder count per unit) with flexible row heights.
- **Row A (top):** 8 parameter cards, each showing name, value, mini-bar. Colour-coded by parameter category.
- **Row B (middle):** 8 slots for presets/zones/FX params. Cards can show different content types based on the active mode.
- **Row C (bottom):** Touch action row -- mode buttons, colour correction, EdgeMixer. Contextual to the active mode.
- **Header strip:** Effect name (left), palette name (centre), connection/audio metrics (right).
- Cards have subtle borders that glow on encoder interaction. The active encoder's card border brightens for 500ms on rotation.

**Information density:** High, but organised through consistent card sizing and colour coding. Each card is a self-contained information unit.

**Encoder-screen mapping:** Strict spatial correspondence. Column N = encoder N. Row A = Unit-A encoders (0-7). Row B = Unit-B encoders (8-15). This mirrors the physical encoder layout.

**Pros:**
- Clear 1:1 mapping between physical encoders and screen columns
- Colour-coded parameter categories reduce cognitive load
- Cards-based layout is native to LVGL (efficient implementation)
- The grid aesthetic resonates with both Resolume (VJ) and Push (music) users
- Mode-dependent content in Row B provides flexibility without extra screens
- Card glow on interaction provides satisfying micro-feedback

**Cons:**
- The grid can feel rigid -- less fluid than the Visual Instrument approach
- 8-column layout means each card is only 160px wide (tight for parameter names)
- Could still feel like a "settings panel" without careful animation and colour work
- Requires careful typographic discipline to prevent clutter at 160px column width

---

## 6. Recommendation Matrix

| Criterion | A: Studio Monitor | B: Visual Instrument | C: Minimal Instrument | D: Performance Grid |
|---|---|---|---|---|
| Information density | High | Medium | Low-Medium | High |
| Visual personality | Low | Very High | High (through restraint) | Medium-High |
| Implementation complexity (LVGL) | Medium | High | Low | Medium |
| 16-encoder mapping clarity | Excellent | Good | Fair (focus model) | Excellent |
| Live performance suitability | Good | Very Good | Excellent | Good |
| First-impression impact | Professional | Stunning | Intriguing | Polished |
| Accessibility (contrast/readability) | Very Good | Variable | Excellent | Good |
| Uniqueness in market | Low | Very High | High | Medium |
| "Creative tool" feeling | Medium | Very High | High | Medium-High |
| Rendering performance (ESP32-P4) | Good | Demanding | Excellent | Good |

**For the K1 Lightwave -- "the world's first visual instrument" -- Direction B (Visual Instrument) is the most brand-aligned choice**, but it carries the highest implementation risk. **Direction D (Performance Grid) is the safest high-quality option** with the clearest encoder mapping.

A hybrid approach is also viable: Direction D's grid layout as the foundation, with Direction B's centre-stage visualisation strip replacing the middle section when real-time data is available. This gives the instrument personality while maintaining information density, and degrades gracefully when the K1 connection is not active.

---

## 7. Implementation Notes for LVGL 9.3

- **Dark backgrounds:** Use `lv_obj_set_style_bg_color(obj, lv_color_hex(0x1A1A1E), 0)` -- avoid pure black for panel surfaces.
- **Desaturated neons:** Convert the current full-neon PARAM_COLORS to ~70% saturation equivalents. Example: Magenta `#FF00FF` to `#D946EF`, Cyan `#00FFFF` to `#22D3EE`.
- **Font rendering:** LVGL supports anti-aliased fonts at any size. Pre-render the chosen font family at 10, 12, 14, 16, 20, 24, 32px during build. Use `lv_font_t` with 4bpp anti-aliasing.
- **Card glow effect:** Use `lv_obj_set_style_shadow_color()` and `lv_obj_set_style_shadow_spread()` with a short animation (`lv_anim_t`) to create the encoder-interaction highlight.
- **Visualisation strip (Direction B):** Render as an `lv_canvas_t` or custom draw callback. The K1's `capture stream` can send LED frame data at 30fps; downscale 320 LEDs to screen width with interpolation.
- **Mode transitions:** Use `lv_scr_load_anim()` with `LV_SCR_LOAD_ANIM_FADE_IN` for screen changes (200-300ms).

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-21 | agent:ui-designer | Created -- competitive UI research across 13 controllers, synthesised into 4 design directions for Tab5 redesign |
