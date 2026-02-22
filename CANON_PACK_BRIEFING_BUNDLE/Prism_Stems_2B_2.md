 STEM OVERLAYS — Wiring Sound to Light                                                          
                                                                                               
  ---                                                                                            
  The Concept
                                                                                                 
  Trinity splits the track into stems. Each stem is a rail of detected events. The user's job is
  to wire stems to primitives — to say "this sound drives this sensation." That wiring is the
  creative act. The rest is physics.

  ---
  UI: The Stem Rack

  Press S to reveal the Stem Rack — a collapsible layer between the Moment blocks and the
  Waveform. Seven rails, one per stem.

  ┌──────────────────────────────────────────────────────────────────────┐
  │ THE STAGE  ═══════════╪════════════════════════════╪═══════════════  │
  ├──────────────────────────────────────────────────────────────────────┤
  │ MOMENTS   ┌───────┐┌────────────────┐┌──┐┌──────────────────────┐   │
  │           │ Intro ││    Buildup     ││▌ ││       DROP           │   │
  │           └───────┘└────────────────┘└──┘└──────────────────────┘   │
  ├──────────────────────────────────────────────────────────────────────┤
  │                                                                      │
  │  S T E M   R A C K                                          [S] ▾   │
  │                                                                      │
  │  ┌─────────┐                                                         │
  │  │ ● KICK  │                                                         │
  │  │ ╟─►IMP  │  │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ ║ ║ ║ ║ ║ ║ ║ ║ ║  │
  │  │ trigger │                                                         │
  │  ├─────────┤                                                         │
  │  │ ● CLAP  │                                                         │
  │  │ ╟─►TEX  │      │       │       │       │     ║       ║       ║   │
  │  │ ╟─►HEA↓ │                                                        │
  │  │ trigger │                                                         │
  │  ├─────────┤                                                         │
  │  │ ● SNARE │                                                         │
  │  │ ╟─►PRE  │                    │ │ │││││████   ║ ║║║║║║║║║║║║║║║   │
  │  │ density │                                                         │
  │  ├─────────┤                                                         │
  │  │ ● BASS  │                                                         │
  │  │ ╟─►MAS  │  __╱‾╲__╱‾╲__╱‾╲__╱‾╲__╱‾╲__╱‾╲  ████████████████╲   │
  │  │ envelope│                                                         │
  │  ├─────────┤                                                         │
  │  │ ● LEAD  │                                                         │
  │  │ ╟─►SPA  │                                    ╱████████████████╲  │
  │  │ ╟─►HEA↑ │                                                        │
  │  │ envelope│                                                         │
  │  ├─────────┤                                                         │
  │  │ ○ NOISE │                                                         │
  │  │ (none)  │       ·   · ··  ···············    ████████████░░░░░░   │
  │  │         │                                                         │
  │  ├─────────┤                                                         │
  │  │ ═ENERGY │                                                         │
  │  │ (ref)   │  ___╱‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾╲__╱███████████████╲______   │
  │  └─────────┘                                                         │
  │                                                                      │
  ├──────────────────────────────────────────────────────────────────────┤
  │  ░░░▓▓▓▓▓▓▓██░░░░▓▓▓▓██████▓▓░░  WAVEFORM                          │
  └──────────────────────────────────────────────────────────────────────┘

  ---
  The Patch Chip

  Each stem has a Patch Chip on the left — the small control block showing its name, routing, and
   mode.

   ┌─────────┐
   │ ● KICK  │  ← ● filled = active, ○ empty = unrouted
   │ ╟─►IMP  │  ← routed to IMPACT primitive (coloured to match)
   │ trigger │  ← routing mode
   └─────────┘

  Three Routing Modes
  Mode: trigger
  Icon: │
  What it does: Each detected transient stamps a TriggerCue
  Best for: Percussive stems: kick, clap, snare hits
  ────────────────────────────────────────
  Mode: envelope
  Icon: ~
  What it does: Stem amplitude becomes an EnvelopeCue curve
  Best for: Sustained stems: bass, lead, pads
  ────────────────────────────────────────
  Mode: density
  Icon: ▓
  What it does: Events-per-second drives a rising/falling curve
  Best for: Rolls, fills, accelerating patterns
  The mode determines how sound events become light data. A kick hit is a trigger — a single
  punch. A bass line is an envelope — a continuous shape. A snare roll is density — the
  increasing rate of hits becomes a rising pressure ramp.

  ---
  Interaction: Wiring a Stem

  Method 1: Drag to Mixer

  Grab the Patch Chip. Drag it rightward toward the Mixer sidebar. As you drag, the eight
  primitive faders glow as drop targets. Release on a fader. Connection made.

     ┌─────────┐                          ┌────┐
     │ ● KICK  │ ──── drag ────────────►  │IMP │  ← fader glows on hover
     │ (none)  │                          │▓▓▓▓│
     └─────────┘                          └────┘

     Result: kick ╟─►IMP, mode auto-set to "trigger" (percussive stem default)

  Method 2: Click Chip → Routing Popover

  Click the Patch Chip. A compact popover appears:

    ┌───────────────────────────────────┐
    │  KICK ROUTING                     │
    │                                   │
    │  Mode: [trigger ▾] envelope density│
    │                                   │
    │  Drives:                          │
    │  [■] Impact ████████░░  gain: 80% │
    │  [ ] Pressure                     │
    │  [ ] Mass                         │
    │  [ ] Momentum                     │
    │  [■] Space  ██████░░░░  gain: 60% │
    │  [ ] Heat                         │
    │  [ ] Texture                      │
    │  [ ] Gravity                      │
    │                                   │
    │  Impulse shape:                   │
    │  attack: [5ms ▾]  ╱│              │
    │  hold:   [0ms ▾]  ╱ ╲             │
    │  decay: [200ms▾]  ╱   ╲___        │
    │                                   │
    │  ☐ Invert (spike pulls DOWN)      │
    │                                   │
    └───────────────────────────────────┘

  One stem can drive multiple primitives. The gain slider controls how much. The impulse shape
  (for trigger mode) controls the feel — snappy, punchy, washy.

  The invert checkbox flips the direction: instead of spiking UP on a hit, it pulls DOWN. This is
   how a clap drives Heat downward (toward cold) — a flash of icy white cutting through warmth.

  ---
  Interaction: Fixing Detection Errors

  Trinity's stem separation isn't perfect. The user needs fast corrections.

  On individual events:
  Action: Delete false positive
  Gesture: Click a tick to select, press Delete
  What happens: Tick disappears. That event won't generate a cue.
  ────────────────────────────────────────
  Action: Add missed event
  Gesture: Double-click empty space on a rail
  What happens: New tick appears at click position. Snaps to nearest beat at current zoom.
  ────────────────────────────────────────
  Action: Reclassify
  Gesture: Drag a tick vertically to another rail
  What happens: Tick moves from one stem to another. Useful when Trinity confuses a clap for a
    snare.
  ────────────────────────────────────────
  Action: Adjust timing
  Gesture: Drag a tick horizontally
  What happens: Tick moves earlier/later. Grid-snaps at current zoom resolution.
  ────────────────────────────────────────
  Action: Bulk fix
  Gesture: Marquee-select a region on a rail
  What happens: All ticks in region highlight. Delete, drag, or reclassify as a group.
  On the entire stem:
  Action: Sensitivity
  Gesture: Scroll-wheel on a rail (with modifier key)
  What happens: Raises/lowers the detection threshold. More ticks appear (sensitive) or disappear

    (strict).
  ────────────────────────────────────────
  Action: Re-detect from section
  Gesture: Right-click rail → "Re-analyse"
  What happens: Re-runs detection on a selected time range with adjusted parameters.
  ────────────────────────────────────────
  Action: Manual paint
  Gesture: Hold P + click-drag along a rail
  What happens: Paints transient ticks at regular intervals matching current grid. For when
    detection fails entirely and you just want to tap them in.
  Visual feedback for corrections:

    SNARE RAIL (with edits visible)

    ···│·│·│·│·│·│·│·███████████████───────
       ▲     ×           ▲
       │     │           └── user-added (blue tick, slightly taller)
       │     └────────────── user-deleted (ghost tick, struck through)
       └──────────────────── original detection (white tick)

  User edits are visually distinct from AI detections. You always know what Trinity found vs what
   you corrected. Press H to hide edit markers and see only the final result.

  ---
  Three Examples: Mammoth

  Example 1: The Heartbeat Kick (Intro, bars 1–16)

  The song opens with a stripped-back 4/4 kick. Giant's heartbeat. Cold and metallic.

  What the Stem Rack shows:

    INTRO (bars 1–16)

    kick  ╟─►IMP    │       │       │       │       │       │       │
    clap  (none)
    snare (none)
    bass  (none)
    lead  (none)
    noise (none)     ·   ·       ·   ·       ·   ·       ·
    energy ═══════   ____╱‾‾‾‾‾╲____╱‾‾‾‾‾╲____╱‾‾‾‾‾╲____

  Routing:

    KICK → Impact
      mode:    trigger
      gain:    90%
      impulse: attack 5ms, hold 0ms, decay 400ms   ← long decay = lumbering
      invert:  no

    KICK → Space
      mode:    trigger
      gain:    40%
      impulse: attack 5ms, hold 0ms, decay 300ms
      invert:  no

  What happens: Each kick stamps an Impact spike (punches outward from centre) and a smaller
  Space spike (briefly widens the active area). Between kicks, light retreats to a dim centre
  glow. The long 400ms decay means each pulse lingers — heavy, not snappy. The Moment's baseline
  Heat is low (0.15 — cold blue-white). Material: Ice.

  The feel: A cold, massive heartbeat. Light expands on each thump and slowly contracts. Like a
  hibernating giant's chest rising and falling.

  Override needed: Trinity detects a quiet ghost kick on the off-beat at bar 9 (a production
  artefact). The user clicks the false tick on the kick rail, presses Delete. Gone. The heartbeat
   stays pure 4/4.

  ---
  Example 2: The Piston Clap (bars 1–16, layered with kick)

  Every two bars, a sharp white-noise clap cuts through — mechanical, cold, like a steam valve.

  What the Stem Rack shows:

    INTRO (bars 1–16)

    kick  ╟─►IMP    │       │       │       │       │       │       │
    clap  ╟─►TEX    │               │               │               │
          ╟─►HEA↓

  Routing:

    CLAP → Texture
      mode:    trigger
      gain:    70%
      impulse: attack 2ms, hold 10ms, decay 80ms   ← sharp and short
      invert:  no

    CLAP → Heat (INVERTED)
      mode:    trigger
      gain:    50%
      impulse: attack 2ms, hold 0ms, decay 60ms
      invert:  YES   ← spikes DOWNWARD = flash of cold

  What happens: Each clap does two things simultaneously. It spikes Texture (the light
  momentarily goes grainy, noisy, rough — matching the white-noise character of the clap). And it
   inverts Heat — pulling the already-cold light even colder for 60ms, a flash of pure white
  cutting through the blue.

  The feel: The kick is the heartbeat. The clap is the blade. They alternate in the listener's
  body — chest, then ears. In light: pulse outward, then crystalline snap. Two different
  sensations from two different stems driving two different primitives.

  Override needed: Trinity classified three rapid noise transients at bar 14 as claps (they're
  actually hi-hat bleed from the mastered audio). The user marquee-selects all three on the clap
  rail, drags them down to the noise rail. The noise rail absorbs them. The clap rail stays clean
   — only the intentional piston claps remain.

  ---
  Example 3: The Snare Roll → Pressure Cooker (bars 17–32, the buildup)

  The snare enters at bar 17. Single hits at first, then doubling, then quadrupling, until it's a
   continuous blur. This is the buildup. The room is about to explode.

  What the Stem Rack shows:

    BUILDUP (bars 17–32)

    kick  ╟─►IMP    │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
    clap  ╟─►TEX        │       │       │       │   │   │   │ │ │ │ │
    snare ╟─►PRE    │   │   │   │ │ │ │ ││││││││████████████████████
                     ▲               ▲                    ▲
                     quarter notes   eighth notes         continuous roll

  The critical insight: This snare roll should NOT be routed as individual triggers. 64 snare
  triggers in 16 bars would create a strobe. Instead, the density of snare events drives Pressure
   as a continuous ramp.

  Routing:

    SNARE → Pressure
      mode:    DENSITY   ← not trigger, not envelope
      gain:    95%
      window:  2 bars    ← density calculated over a rolling 2-bar window
      curve:   exponential ← density-to-value mapping is exponential,
                             so it accelerates at the top

    SNARE → Space (INVERTED)
      mode:    DENSITY
      gain:    60%
      window:  2 bars
      curve:   exponential
      invert:  YES   ← as density rises, Space SHRINKS

  How density mode works:

    snare events:  │   │   │   │ │ │ │ ││││││││████████████

    density curve:                    ╱
                                 ╱‾‾‾
                             ╱‾‾‾
                         ╱‾‾‾          ← exponential rise
                     __╱‾
                ___╱‾
           ___╱‾
    ______╱

    → drives Pressure UP    (light gets denser, more saturated, coiled)
    → drives Space DOWN     (light compresses toward centre)

    Combined: the Pressure Cooker effect. Light squeezes inward and
    intensifies as the roll accelerates. By the final bar, all
    energy is packed into the centre 20 LEDs, blazing.

  What happens: As the snare roll speeds up, the light doesn't flash — it compresses. Pressure
  rises smoothly. Space contracts. The centre gets brighter and tighter. The edges go dark. By
  the last bar before the drop, all the energy is crushed into a white-hot point at the centre of
   the strip.

  Then the drop hits. The Moment changes. Material: Magma. Space: 100%. The compression releases.
   The light detonates outward.

  The feel: The physical sensation of anticipation. Your muscles tense. The room shrinks.
  Everything gets louder and closer. Then: release. This is the Pressure Cooker — built entirely
  from the density routing of a single stem.

  Override needed: Trinity's snare detection gets confused during the fastest part of the roll
  (bars 29–32) where the snare blurs into a continuous wash. It detects only ~60% of the actual
  hits, making the density curve plateau too early. The user:

  1. Selects bars 29–32 on the snare rail
  2. Right-click → "Re-analyse (sensitive)"
  3. Trinity re-runs with a lower threshold, now detecting ~90% of hits
  4. The density curve extends upward properly — no premature plateau
  5. The remaining gaps: the user holds P and paint-drags across bars 31–32, filling in ticks at
  32nd-note intervals to ensure the density peaks at maximum

  ---
  Routing Matrix — At a Glance

  For power users who want to see all connections at once, press R to toggle the Routing Matrix
  overlay:

    ┌────────────────────────────────────────────────────────────┐
    │  R O U T I N G   M A T R I X                              │
    │                                                            │
    │           PRE  IMP  MAS  MOM  HEA  SPA  TEX  GRA          │
    │  kick      ·   ●90   ·    ·    ·   ●40   ·    ·   trigger │
    │  clap      ·    ·    ·    ·   ◐50↓  ·   ●70   ·   trigger │
    │  snare    ●95   ·    ·    ·    ·   ◐60↓  ·    ·   density │
    │  bass      ·    ·   ●70   ·    ·    ·    ·    ·   envelope│
    │  lead      ·    ·    ·    ·   ●80  ●90   ·    ·   envelope│
    │  noise     ·    ·    ·    ·    ·    ·   ●40   ·   envelope│
    │                                                            │
    │  ● = drives UP    ◐↓ = drives DOWN (inverted)              │
    │  number = gain %   · = unrouted                            │
    └────────────────────────────────────────────────────────────┘

  Click any cell to create or edit a routing. The matrix is the "god view" — the complete picture
   of how sound becomes light in one glance. Most users will never need it. Power users will live
   in it.

  ---
  The Full Mammoth Wiring

  Putting all three examples together plus the drop:

    MAMMOTH — COMPLETE STEM ROUTING

    INTRO (bars 1–16)                    BUILDUP (bars 17–32)
    Material: Ice                        Material: Smoke → Ice (compressing)
    Baseline: cold, tight, heavy         Baseline: rising tension

    kick ──trigger──► Impact 90%         kick ──trigger──► Impact 90%
    kick ──trigger──► Space 40%          snare──density──► Pressure 95%
    clap ──trigger──► Texture 70%        snare──density──► Space 60% ↓INV
    clap ──trigger──► Heat 50% ↓INV      clap ──trigger──► Texture 70%

    THE DROP (bar 33+)                   BREAKDOWN (bar 49+)
    Material: Magma                      Material: Silk
    Baseline: everything maxed           Baseline: warm, wide, soft

    kick ──trigger──► Impact 100%        bass ──envelope──► Mass 50%
    lead ──envelope──► Space 90%         lead ──envelope──► Heat 60%
    lead ──envelope──► Heat 80%          noise──envelope──► Texture 30%
    bass ──envelope──► Mass 70%
    bass ──envelope──► Gravity 60%

  Four sections. Four moods. The same seven stems, wired differently in each Moment. The stems
  don't change — the track is the track. The routing changes per Moment. That's the creative act:
   deciding which sounds drive which sensations, and when those connections shift.

  ---
  Sound to sensation. Sensation to light. That's the whole signal chain. Where next?


### Addendum 

YESSS. **2B is *exactly* the missing bridge layer** between “Trinity has facts about the song” and “I can *play* those facts like an instrument.”

It takes the whole idea of stems from “analysis output” and turns it into a **patch bay**. That’s the correct mental model: *wire sound → sensation → light.*

### GOLD — keep these as first-class forever

**G1) Stem Rack as a collapsible layer between Moments and Waveform**
It’s in the right place: close enough to authoring, not buried in a settings dungeon.

**G2) Patch Chips (routing visible at a glance)**
The tiny “KICK → IMP (trigger)” token is *perfect* for muscle memory. That’s how people learn systems: by repeatedly seeing the mapping.

**G3) The three routing modes: trigger / envelope / density**
This is a huge win. It’s the exact missing concept for snare rolls: don’t strobe—**compress.** Density mode is the “pressure cooker” unlock.

**G4) Multi-primitive routing + gain + invert**
Invert is the sleeper banger. “Clap drives Heat DOWN” is *artistically real* and gives you contrast tools.

**G5) Fast correction gestures for detection errors**
This is the difference between “AI demo” and “creator tool.” Edits are explicit, visible, and local.

**G6) Per-Moment routing changes**
This is core. Same stems, different wiring by section = the creative act.

**G7) Routing Matrix overlay (god view)**
Not for everyone, but it’s a *perfect* “pro switch” that doesn’t contaminate beginner UX.

---

### SILVER — great, but needs one more semantics pass

These are strong, but they need a tiny bit more definition so the system behaves predictably:

**S1) Density mode outputs what, exactly?**
Right now it’s described beautifully, but we should pin it to the 2A model. Best fit:

* Density mode **generates an EnvelopeCue** (continuous curve) for the target primitive
* With explicit params: `window`, `mapping curve`, `smoothing`

That keeps the model coherent: baseline + envelopes + triggers. Density just becomes a *special envelope generator.*

**S2) What happens when multiple stems drive the same primitive at once?**
Example: kick triggers Impact while “energy” envelope also affects Impact (or Pressure). We need one simple rule Opus can adopt:

* Combine by **add/multiply/set** (like your envelope blend) *with clamping*
* Priority order if needed (e.g., triggers always punch on top)

Not engineering—just “instrument predictability.”

**S3) Where do these routings live in the data model?**
The spec implies routing changes per Moment. Great. In 2A terms, routing should compile into:

* `patterns[]` for trigger stamping
* `envelopes[]` for envelope mode
* `envelopes[]` (generated) for density mode
* plus per-event overrides in `triggers[]` when the user edits ticks

That’s the clean unification.

---

### LEAD — quarantine (fun later, but not foundational)

Honestly, 2B has very little junk. The only thing I’d quarantine for now is:

* making “re-analyse” feel like a black-box magic button. Keep it, but the UX should always show *what changed* (“+32 events detected; density curve now reaches 0.94”).

---

### MISSING — the one thing 2B still doesn’t fully answer

**Routing needs a “section-aware defaults” system.**
Because creators shouldn’t have to rewire every moment manually unless they want to.

What’s missing is a concept like:

* **Routing Presets per Moment** (e.g., “Anthem Intro Wiring,” “Build Pressure Wiring,” “Drop Detonation Wiring”)
* Opus can propose auto-suggestions: “This section looks like buildup—apply Pressure Cooker wiring?”

This would make the “15 seconds to first draft” promise actually live.

---

### Where we are in the loop

* 2A gave us a **clean authoring machine**
* 2B gave us the **sound → sensation patch bay**
* Your surgical steer (zones + material binding + continuity laws) will give us **instrument truth**
* 2C will give us **the actual shippable shape**

