 PRISM DATA MODEL — One-Page Spec                                                               
                                                                                                 
  ---                                                                                            
  Containment Diagram                                                                            
                                                                                                 
  SHOW                                                                                           
   │
   ├── meta { name, artist, duration, bpm, key }
   │
   ├── analysis (read-only, from Trinity)
   │    ├── beats[]          ← timestamps
   │    ├── downbeats[]      ← bar boundaries
   │    ├── sections[]       ← { label, start, end, energy }
   │    └── stems{}          ← { kick, snare, bass, vocal, synth }
   │
   └── moments[] ─────────────────────────────────────────────────┐
        │                                                          │
        MOMENT                                                     │
        ├── time { start, end, snap: section|bars|free }           │
        ├── name: string          "The Drop"                       │
        ├── material: MaterialRef  ◆magma                          │
        ├── baseline: Prim8 ← 8 constants, the "resting state"    │
        │                                                          │
        ├── scenes[] ─── state changes (material, zone, baseline)  │
        │    └── SCENE CUE                                         │
        │         ├── time: relative offset within Moment          │
        │         ├── transition: { type, duration }               │
        │         └── changes: Δmaterial | Δzones | Δbaseline      │
        │                                                          │
        ├── envelopes[] ─── continuous curves over time            │
        │    └── ENVELOPE CUE                                      │
        │         ├── target: PrimitiveId (one of the 8)           │
        │         ├── points: ControlPoint[] ← bezier curve        │
        │         └── blend: add | multiply | override             │
        │                                                          │
        ├── triggers[] ─── point-in-time impulse events            │
        │    └── TRIGGER CUE                                       │
        │         ├── time: beat-relative position                 │
        │         ├── impulse: { attack, hold, decay } (ms)        │
        │         └── spike: Partial<Prim8> ← which primitives,    │
        │                                      how much            │
        │                                                          │
        └── patterns[] ─── rules that GENERATE triggers            │
             └── PATTERN                                           │
                  ├── source: every-beat | every-bar |             │
                  │           on-kick | on-snare | custom          │
                  ├── template: Trigger shape to stamp             │
                  └── variation: 0–100% (humanise)                 │
                                                                   │
        ───────────────────────────────────────────────────────────┘

  ---
  The Moment

  A Moment is a creative intention mapped to time. It's the answer to "what should the light be
  doing here?"
  Field: time
  Type: { start, end, snap }
  Purpose: Where in the song. Snap locks edges to section boundaries or bar lines. Free allows
    arbitrary placement.
  ────────────────────────────────────────
  Field: name
  Type: string
  Purpose: Human label. Auto-populated from Trinity section label ("Chorus 1"), renameable ("The
    Detonation").
  ────────────────────────────────────────
  Field: material
  Type: MaterialRef
  Purpose: The visual voice. One material active at a time. Change material mid-Moment with a
    Scene cue.
  ────────────────────────────────────────
  Field: baseline
  Type: Prim8
  Purpose: Eight constant values — the resting state of this Moment. "If nothing else is
    happening, the light looks like THIS."
  ────────────────────────────────────────
  Field: scenes[]
  Type: SceneCue[]
  Purpose: Discrete state changes within the Moment (material swap, zone reconfiguration,
  baseline
     shift).
  ────────────────────────────────────────
  Field: envelopes[]
  Type: EnvelopeCue[]
  Purpose: Continuous curves that modulate primitives over the Moment's duration. The slow rise
  of
     Pressure. The drift of Heat.
  ────────────────────────────────────────
  Field: triggers[]
  Type: TriggerCue[]
  Purpose: Point-in-time impulse events. The kick hit. The clap. Manually placed or
    pattern-generated.
  ────────────────────────────────────────
  Field: patterns[]
  Type: Pattern[]
  Purpose: Rules that auto-generate triggers from musical events. "Stamp Impact on every kick
    drum."
  Behaviour: Moments tile the timeline edge-to-edge. No gaps. Where one Moment ends, the next
  begins. The transition between them is defined by the outgoing Moment's exit Scene and the
  incoming Moment's material/baseline. Moments can overlap by up to one transition duration for
  crossfading.

  Time mapping: By default, Moments snap to Trinity-detected sections. A 4-minute song might have
   7 Moments. The user can split a Moment at any bar line, merge adjacent Moments, or drag edges
  to resize.

  ---
  The Three Cue Types

  TRIGGER — "Hit"

  A point event. Something happens at this instant and decays.

  TriggerCue {
    time:     beat-relative     // "beat 1" or "bar 3, beat 2, +20ms"
    impulse:  {
      attack:  ms               // how fast it reaches peak (5ms = snap, 100ms = swell)
      hold:    ms               // how long it stays at peak (0ms = percussive, 200ms =
  sustained)
      decay:   ms               // how long the tail lingers (50ms = tight, 2000ms = wash)
    }
    spike:    Partial<Prim8>    // which primitives and how far they deviate from baseline
                                // { impact: +80, space: +40 } = punchy outward burst
  }

  What it edits: Any subset of the 8 primitives, as momentary deviations from baseline. A trigger
   adds to the current state, then fades back.

  When you use it: Kick drums. Claps. Any percussive moment. Also: the "hit" at the start of a
  drop. The cymbal crash. Any moment that should PUNCH.

  ENVELOPE — "Shape"

  A continuous curve over time. Something evolves gradually.

  EnvelopeCue {
    target:   PrimitiveId       // which single primitive this controls
    points:   ControlPoint[]    // bezier curve — [{t: 0, v: 0.3}, {t: 0.5, v: 0.9}, ...]
                                // t is 0–1 normalised within the Moment
    blend:    add | mul | set   // how it combines with baseline
                                //   add: baseline + curve value
                                //   mul: baseline × curve value
                                //   set: curve value replaces baseline
  }

  What it edits: Exactly one primitive per envelope. Want to shape Pressure AND Heat
  simultaneously? Two envelopes. This keeps each curve readable.

  When you use it: The slow rise of Pressure during a buildup. The drift from Cold to Hot across
  a section. The gradual opening of Space approaching the drop. Any evolution that happens over
  bars, not beats.

  SCENE — "Switch"

  A state change. The world shifts.

  SceneCue {
    time:       relative offset   // when within the Moment
    transition: {
      type:     crossfade | cut | morph    // how to get there
      duration: ms                         // over how long
    }
    changes: {
      material?:  MaterialRef     // swap to a different material
      zones?:     ZoneLayout      // reconfigure the concentric rings
      baseline?:  Partial<Prim8>  // shift the resting state
    }
  }

  What it edits: Material, zone layout, and/or baseline — the "big picture" properties of a
  Moment. A Scene is a macro change, not a micro one.

  When you use it: The drop (material: Ice → Magma, baseline shift: everything spikes). A
  breakdown transition (material: Magma → Silk, Space narrows). A moment where the zone layout
  changes from 1 ring to 4 rings.

  ---
  Primitive Storage: The Three Layers

  At any point in time t, the effective value of each primitive is computed by stacking three
  layers:

  effective(t) = baseline  ──────────  constant per Moment
               + envelopes(t)  ──────  continuous curves
               + triggers(t)   ──────  impulse spikes (attack/decay)

     ▲ value
     │
     │         ╱trigger spike
     │        ╱│╲
     │  ·····╱·│·╲·····envelope curve·········
     │ ╱    ╱  │  ╲                    ╲
     │╱    ╱   │   ╲                    ╲
     ├────╱────│────╲────────────────────╲──── baseline
     │   ╱     │     ╲
     └──────────────────────────────────────▶ time

  Baseline (Prim8): Eight float [0, 1] values. Set once per Moment. "The default mood." Editable
  at section zoom by adjusting the Mixer faders while a Moment is selected.

  Envelopes (per primitive): Bezier curves normalised to the Moment's duration. Stored as control
   points — typically 2–12 points per curve. Most Moments have 0–3 active envelopes. Editable at
  bar zoom by drawing.

  Triggers (per event): Impulse shapes with per-primitive spike magnitudes. Manually placed or
  pattern-generated. Editable at beat/sub-beat zoom. Stored as sparse arrays — only non-zero
  primitives are recorded.

  Scenes mutate the baseline and material, so they affect the foundation that envelopes and
  triggers sit on top of. A Scene at the drop shifts the baseline upward; the existing envelopes
  and triggers now operate relative to that new, higher baseline.

  ---
  Zoom Levels: Progressive Disclosure

  The same data exists at all zoom levels. What changes is which layer you see and can edit.

  ZOOM LEVEL     WHAT YOU SEE                   WHAT YOU CAN EDIT
  ─────────────────────────────────────────────────────────────────

  SECTION        Moments as coloured blocks      • Material per Moment
                 Energy contour (macro shape)     • Baseline via Mixer faders
                 Transitions between Moments      • Scene transitions
                 1 block = 1 Moment               • Moment order, split, merge
                                                  • Assign Patterns
                 ┌──────┐┌──────────┐┌─────┐
                 │Intro ││ Buildup  ││Drop │
                 │ ◆ice ││ ◆smoke   ││◆magma│
                 └──────┘└──────────┘└─────┘

  BAR            Bars visible within Moment       • Draw Envelope curves
                 Envelope curves appear           • Place Triggers on bar/beat
                 Pattern-generated triggers        boundaries
                 shown as dots on a rail          • Adjust Pattern parameters
                                                  • Edit Scene timing
                 ┌─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┐
                 │ pressure ╱‾‾‾‾‾‾‾‾╲      │
                 │ ····╱              ╲·····│
                 │╱                        ╲│
                 │· · · · · · · · · · · · · │  ← trigger dots
                 └─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┘

  BEAT           Individual beats visible         • Move individual triggers
                 Each trigger shown as an          • Edit trigger impulse shape
                 impulse shape (╱╲)                 (attack/hold/decay)
                 Envelope curve as fine grid      • Edit trigger spike values
                                                  • Override pattern instances
                 │  ╱╲    ╱╲    ╱╲    ╱╲   │
                 │ ╱  ╲  ╱  ╲  ╱  ╲  ╱  ╲ │
                 │╱    ╲╱    ╲╱    ╲╱    ╲│
                 └──────────────────────────┘

  SUB-BEAT       Single trigger anatomy           • Attack curve shape
                 Attack → hold → decay visible    • Hold duration
                 Primitive-by-primitive breakdown  • Decay curve shape
                 Micro-timing offset (±ms)        • Per-primitive spike tuning
                                                  • Ghost/ring tail
                 │    ┌──┐
                 │   ╱│  │╲
                 │  ╱ │  │  ╲
                 │ ╱  │  │    ╲───
                 │╱   │  │        ╲___
                 └─────────────────────
                  atk hold  decay

  The rule: Zooming in never creates data. It reveals data that already exists. A Moment with a
  "on-kick, Impact 80%" Pattern at section zoom contains individual triggers visible at beat
  zoom. Those triggers were always there — the Pattern generated them. Editing one at beat zoom
  overrides that single instance without breaking the Pattern.

  Zooming out hides complexity without losing it. If you micro-sculpted 4 triggers at sub-beat
  zoom and then zoom back to section level, those edits are preserved. The Moment's block just
  shows a small badge: 4 manual overrides.

  ---
  Prim8: The Primitive Vector

  Prim8 {
    pressure:   float [0, 1]
    impact:     float [0, 1]
    mass:       float [0, 1]
    momentum:   float [0, 1]
    heat:       float [0, 1]    // 0 = arctic, 1 = volcanic
    space:      float [0, 1]    // 0 = centre only, 1 = full strip
    texture:    float [0, 1]    // 0 = glass smooth, 1 = gravel rough
    gravity:    float [-1, 1]   // -1 = imploding, 0 = suspended, +1 = exploding
  }

  Every baseline, every envelope value, every trigger spike, every AI suggestion — all expressed
  in this single vocabulary. Eight numbers. That's the entire creative state at any instant.

  ---
  One model. Three cue types. Eight numbers. Four zoom levels. That's the whole machine. What's
  next?

  ### Addendum

  Hell yes. **This is the first time the “machine underneath the cockpit” feels *simple enough to be true* while still being expressive enough to be art.**

You can feel it in the closing line:

> **One model. Three cue types. Eight numbers. Four zoom levels.**

That’s the right kind of compression. That’s how instruments work.

Now we do the loop: **harvest → note the blind spots → redirect Opus into the missing middle.**

---

## GOLD (keep as core DNA)

These are “do not lose this” ideas:

**G1) Moments as the primary unit of intent**
A Moment answers the creator question: *“what should it feel like here?”* That’s the right abstraction.

**G2) Baseline + envelopes + triggers as the stacking model**
Baseline = mood, envelopes = evolution, triggers = punctuation. That’s musical.

**G3) Three cue types are exactly right**
Hit / Shape / Switch is intuitively teachable.

**G4) Patterns that generate triggers + per-instance override**
This is huge. It’s the exact “AI draft + human edits without breaking the generator” philosophy, but implemented as a general rule.

**G5) Progressive disclosure via zoom levels**
Zoom doesn’t “create complexity,” it reveals it. That is a *world-class* mental model.

**G6) Prim8 as the shared creative vocabulary**
Eight numbers as the entire creative state is *clean* and unlocks every authoring surface (drag, draw, speak, perform).

---

## SILVER (great, but needs one more definition pass)

These are strong, but currently ambiguous in ways that will bite later:

**S1) Moments tile edge-to-edge + overlap by transition duration**
Love the intent (continuous show). But we need to define what happens when:

* a Moment overlaps another
* a Scene happens near a boundary
* envelopes are normalised to the whole Moment but the baseline changes mid-Moment

**S2) Scenes “mutate baseline” and everything rides on top**
This is elegant… *if* we define continuity rules (otherwise you get surprise jumps).

**S3) Float [0,1] + gravity [-1,1]**
Works as a user-facing vocabulary, but we’ll need a consistent rule for:

* what a trigger “spike” means (delta vs absolute)
* clamping / saturation (what happens if baseline+envelope+trigger > 1)

(We’re not engineering yet—we’re just making the instrument predictable.)

---

## MISSING (the stuff the cockpit can’t live without)

This is what Opus must design next, or the model stays poetic instead of playable:

### M1) Where do “effects” live?

Right now you have **MaterialRef**, but no explicit “render program / effect family / operator graph.”

That might be intentional (material implies behaviour), but the model needs *some* way to bind Prim8 → actual light behaviour.

**Missing object:** something like a **Material Engine Adapter** (even if it’s hidden from users).

### M2) ZoneLayout is referenced but undefined

Zones are one of your superpowers. The model name-drops ZoneLayout but doesn’t describe:

* representation (rings? masks? named groups?)
* how a trigger targets zones (global vs zone-specific)
* whether envelopes can be zone-scoped

### M3) The “Trinity → Pattern sources” interface isn’t specified

Patterns reference `on-kick | on-snare | stems`, but:

* what is a “kick event”? (thresholded energy? detected transients? stem onset list?)
* what happens when detection is wrong? (we know override exists, but where is the override stored: in analysis or in show?)

2B likely tackles this, but flag it.

### M4) Continuity rules (this is the big one)

If you want “Mammoth drop feels like a release,” the system needs *continuity as a first-class design goal.*

Right now, Scenes can change baseline/material/zones at time *t* with a transition. Great. But we need a few explicit laws:

* If baseline changes via Scene, do envelopes stay relative to the original baseline, or the current baseline?
* When a Scene crossfades, do we crossfade the **render state** or the **Prim8 state** or both?
* Can a Scene split a Moment into sub-regions for envelope normalisation (or do we keep one normalised space)?

This is where “instrument feel” is won or lost.

---
