PRISM SEMANTICS — The Instrument Laws                                                          
                                                                                                 
  ---                                                                                            
  1. ZoneLayout                                                                                  
                                                                                                 
  A ZoneLayout partitions the strip into concentric rings addressed by distance from centre.     
  Every cue can target one ring, many rings, or all.

  ZoneLayout {
    rings: Ring[]              // 1–4, ordered centre-outward
  }

  Ring {
    id:      string            // "core" | "inner" | "outer" | "edge"
    span:    [float, float]    // distance from centre, normalised 0–1
    label:   string            // user-visible name
    solo:    bool              // true = only this ring active (preview)
    mute:    bool              // true = ring dark (bypass)
  }

  Three built-in presets:

  Single:  ┌──────────── all ────────────┐    1 ring   (0 → 1.0)

  Dual:    ┌─── core ───┬─── edge ──────┐    2 rings  (0→0.5, 0.5→1.0)

  Quad:    ┌─ core ─┬ inner ┬ outer ┬ edge ┐  4 rings  (0→.25, .25→.5, .5→.75, .75→1)

  Zone targeting: Every cue type carries an optional target field.

  // targets all rings (default — omit the field)
  { spike: { impact: +0.8 } }

  // targets one ring
  { target: "core", spike: { impact: +0.8 } }

  // targets a set
  { target: ["core", "inner"], spike: { impact: +0.8 } }

  Untargeted cues paint every ring identically. Targeted cues layer on top — a core-only Impact
  trigger fires in the core while the edge holds steady. This is how you create spatial
  choreography: the drop detonates the core first, then a delayed trigger hits the edge 200ms
  later. Same data model, different targets.

  Zone Groups (shorthand for common multi-ring patterns):
  ┌─────────────┬──────────────┬────────────────────────────┐
  │    Group    │  Expands to  │          Use case          │
  ├─────────────┼──────────────┼────────────────────────────┤
  │ "all"       │ every ring   │ Default. Uniform.          │
  ├─────────────┼──────────────┼────────────────────────────┤
  │ "centre"    │ core + inner │ Intimate, focused energy   │
  ├─────────────┼──────────────┼────────────────────────────┤
  │ "rim"       │ outer + edge │ Peripheral glow, halo      │
  ├─────────────┼──────────────┼────────────────────────────┤
  │ "alternate" │ core + outer │ Skip pattern, counterpoint │
  └─────────────┴──────────────┴────────────────────────────┘
  ---
  2. Material Engine Adapter

  A Material is a behavioural lens that interprets Prim8 values into visual output. Same inputs,
  different character.

  Material {
    name:       string                  // "Magma", "Ice", "Neon", ...

    palettes: {
      cold:     Gradient                // used when Heat < 0.3
      warm:     Gradient                // used when Heat > 0.7
      blend:    function(heat) → Gradient  // interpolates between
      accent:   Colour                  // transient flash colour
    }

    motion: {
      attack:   CurveShape              // how energy ARRIVES (sharp/soft/elastic)
      sustain:  Behaviour               // what energy does while HELD (flow/freeze/vibrate)
      decay:    CurveShape              // how energy FADES (ring/splatter/evaporate)
    }

    surface: {
      base_noise:   float [0,1]         // inherent texture (silk=0.05, gravel=0.8)
      grain:        NoiseType           // perlin | simplex | white | crystalline
      reflectivity: float [0,1]         // how much it picks up from neighbours
    }

    interpretation: PrimitiveMap[8]     // how each primitive manifests — see below
  }

  The Interpretation Table

  Each material defines, for every primitive, what that sensation looks like in this material's
  language:

                │  MAGMA                    │  ICE
  ──────────────┼───────────────────────────┼──────────────────────────
   Pressure     │  Crust thickens, glow     │  Crystal lattice compresses,
                │  intensifies beneath      │  refractive shimmer increases
  ──────────────┼───────────────────────────┼──────────────────────────
   Impact       │  Lava burst — molten      │  Fracture — white shards
                │  orange expanding outward  │  scatter from hit point
  ──────────────┼───────────────────────────┼──────────────────────────
   Mass         │  Thicker flows, deeper    │  Larger crystals, deeper
                │  reds, slower movement    │  blue, slower formation
  ──────────────┼───────────────────────────┼──────────────────────────
   Momentum     │  Rivers keep flowing      │  Cracks propagate,
                │  between beats, never     │  fractures chain
                │  fully cooling            │  between impacts
  ──────────────┼───────────────────────────┼──────────────────────────
   Heat         │  0=obsidian black crust   │  0=deep blue, still
                │  1=white-hot molten core  │  1=thawing, liquid edges
  ──────────────┼───────────────────────────┼──────────────────────────
   Space        │  0=single magma vent      │  0=single frozen point
                │  1=entire strip is lava   │  1=frozen ocean, edge to edge
  ──────────────┼───────────────────────────┼──────────────────────────
   Texture      │  0=smooth obsidian glass  │  0=clear ice, transparent
                │  1=rough pumice, sparks   │  1=frost, opaque crystals
  ──────────────┼───────────────────────────┼──────────────────────────
   Gravity      │  -1=lava drains inward    │  -1=ice grows inward
                │  +1=eruption outward      │  +1=frost spreads outward

  Adapter Contract

  The Material Adapter receives and produces:

  INPUTS                              OUTPUTS
  ──────────────────                  ──────────────────
  effective Prim8(t)    ──────►       CRGB per LED
    (8 floats, per zone)             (320 colour values)

  zone ring id          ──────►       (output is zone-aware:
                                       core may render differently
  elapsed time          ──────►        than edge for same Prim8)
    (for internal animation)

  transition_alpha      ──────►       (0–1 blend weight during
    (during crossfade)                 Scene transitions)

  Consistency rule: every material MUST respond to all 8 primitives. No material ignores Impact.
  No material ignores Heat. The vocabulary is universal. A creator can change material mid-show
  and every routing, every envelope, every trigger still works — it just sounds different in the
  new material's accent. Like playing the same melody on piano vs guitar.

  ---
  3. Continuity Laws

  Five rules that prevent visual discontinuity.

  Law 1: Primitive Continuity

  The effective Prim8 curve is always C0 continuous (no jumps) unless the user explicitly inserts
   a cut. When a Moment boundary is crossed, any mismatch between the outgoing state and the
  incoming baseline is interpolated over the Scene transition duration.

    Moment A ends with Pressure at 0.85 (envelope peak)
    Moment B baseline Pressure is 0.30

    With 500ms crossfade Scene:

    ──0.85╲                     ← outgoing envelope tail
           ╲
            ╲──0.30────────     ← incoming baseline

    |←500ms→|

    No jump. Smooth ramp. The listener never sees a glitch.

  Law 2: Envelope Carry

  Envelopes that span a Moment boundary carry their value into the transition. The outgoing
  envelope's final value is the starting value of the interpolation ramp. It does not snap to
  zero.

    Moment A: Pressure envelope reaches 0.9 at end
    Moment B: Pressure envelope starts at 0.2

    During transition:  0.9 ──ramp──► 0.2 over transition duration
    NOT:                0.9 ──snap──► 0.0 ──snap──► 0.2

  Law 3: Triggers are Additive to Current State

  A trigger spike adds to the effective value at that instant, not to zero. If an envelope has
  Pressure at 0.6 and a trigger spikes Pressure by +0.3, the peak is 0.9 — not 0.3.

    effective(t) = clamp( baseline + envelopes(t) + triggers(t), 0, 1 )

  Clamping at 0 and 1 prevents overflow. A trigger that would push past 1.0 simply hits the
  ceiling. This is intentional: it means you can be aggressive with trigger gains and the system
  saturates gracefully instead of glitching.

  Law 4: Scene Transition Modes

  Three modes, each with different semantics:
  ┌───────────┬─────────────────────────────┬──────────────────────┬────────────────────────────┐
  │   Mode    │          Material           │        Prim8         │        When to use         │
  ├───────────┼─────────────────────────────┼──────────────────────┼────────────────────────────┤
  │           │ Both materials render       │ Outgoing effective   │                            │
  │           │ simultaneously. Outgoing    │ Prim8 interpolates   │ Most transitions. Smooth,  │
  │ crossfade │ fades alpha 1→0, incoming   │ toward incoming      │ predictable.               │
  │           │ fades 0→1. At midpoint,     │ baseline.            │                            │
  │           │ both visible at 50%.        │                      │                            │
  ├───────────┼─────────────────────────────┼──────────────────────┼────────────────────────────┤
  │           │ Single material that        │                      │                            │
  │           │ gradually transforms from   │ Same as crossfade —  │ When you want the material │
  │ morph     │ A's character to B's        │ smooth               │  to "become" the next one  │
  │           │ character. Palette shifts,  │ interpolation.       │ rather than dissolving     │
  │           │ motion evolves, surface     │                      │ between two layers.        │
  │           │ changes. No stacking.       │                      │                            │
  ├───────────┼─────────────────────────────┼──────────────────────┼────────────────────────────┤
  │           │                             │ Prim8 still          │ The drop. When             │
  │           │                             │ interpolates over a  │ discontinuity IS the       │
  │           │ Instantaneous material      │ 50ms micro-ramp      │ intent. But even here, the │
  │ cut       │ swap. A vanishes, B         │ (prevents            │  50ms micro-ramp softens   │
  │           │ appears.                    │ single-frame         │ the primitive values       │
  │           │                             │ spikes).             │ enough to prevent hardware │
  │           │                             │                      │  artefacts.                │
  └───────────┴─────────────────────────────┴──────────────────────┴────────────────────────────┘
  Law 5: Zone Independence

  Each zone ring evaluates its own effective Prim8 independently. A Scene transition in the core
  ring does not affect the edge ring. This enables:

  - Core detonates (cut to Magma) while edge still fades out of Ice
  - Cascade transitions: core transitions at bar 33, inner at 33.5, outer at 34, edge at 34.5
  - Counterpoint: core brightens as edge dims

  Zone-targeted Scenes only transition the targeted zone. Other zones hold their current state
  until addressed.

  ---
  The Signal Path (Diagram)

                  ┌────────────────────────────────┐
                  │          MOMENT                 │
                  │                                 │
                  │  baseline ────────── Prim8[8]   │
                  │      +                          │
                  │  envelopes[] ─── curves(t)      │       ┌───────────┐
                  │      +                          │       │ ZoneLayout│
                  │  triggers[] ── impulses(t)      │       │           │
                  │      ‖                          │       │  ● core   │
                  │  ════╪══════════════            │       │  ◉ inner  │
                  │      ▼                          │       │  ○ outer  │
                  │  effective Prim8(t)──per zone───│──◄────│  ◌ edge   │
                  │      │                          │       └───────────┘
                  │      │                          │
                  │  scenes[] ──────────────────┐   │
                  │      │      transition      │   │
                  └──────│──────────────────────│───┘
                         │                      │
                         ▼                      ▼
                 ┌──────────────┐      ┌──────────────┐
                 │ Material A   │ fade │ Material B   │
                 │   Adapter    │◄────►│   Adapter    │
                 │              │      │              │
                 │  Prim8 → ??? │      │  Prim8 → ??? │
                 │  ??? → CRGB  │      │  ??? → CRGB  │
                 └──────┬───────┘      └──────┬───────┘
                        │    crossfade/morph   │
                        └──────────┬──────────┘
                                   ▼
                            ┌────────────┐
                            │  CRGB[320] │
                            │  per LED   │
                            │  per frame │
                            └────────────┘

  ---
  Worked Example: Mammoth — Four Moments, One Continuous Output

  The Timeline

    bar:  1         8        16  17        24  25        32  33
          │─── INTRO ─────────│──── BUILD ────│── ROLL ───│── DROP ──►
          │   "Awakening"     │  "Rising"     │ "Cooker"  │ "Detonate"
          │   material: Ice   │  mat: Ice     │ mat: Ice  │ mat: Magma
          │   zones: Single   │  zones: Single│ zones:Dual│ zones: Quad

  Moment-by-Moment State

  MOMENT 1: "The Awakening" (bars 1–16)

  baseline:  PRE=0.20  IMP=0.00  MAS=0.70  MOM=0.10
             HEA=0.15  SPA=0.30  TEX=0.10  GRA=0.00

  envelopes: (none — static baseline, the stillness IS the point)

  triggers:  kick → Impact +0.60, Space +0.30, every beat
             clap → Texture +0.50, Heat -0.40 (inverted), every 2 bars

  patterns:  kick on-kick (trigger), clap on-clap (trigger)

  scenes:    (none — single Moment, single material)

  material:  Ice
  zones:     Single (all rings as one)

  What the output looks like: Dark, cold, compressed. Every kick pushes a dim blue shockwave
  outward from centre (Impact 0.60 spike over Ice = fracture burst). Between kicks, light
  contracts back to a tight centre glow. Every second bar, the clap flashes white-cold and adds
  grain. Heavy and still. A frozen giant breathing.

  ---
  MOMENT 2: "Rising" (bars 17–24)

  baseline:  PRE=0.30  IMP=0.00  MAS=0.60  MOM=0.30
             HEA=0.20  SPA=0.35  TEX=0.15  GRA=0.00

  envelopes: Pressure  ╱ linear 0.30 → 0.55 over 8 bars (blend: set)
             Momentum  ╱ linear 0.30 → 0.50 over 8 bars (blend: set)

  triggers:  kick → Impact +0.60, Space +0.30 (same as intro)
             clap → doubling, now every bar

  scenes:    ENTRY from Moment 1:
               mode: crossfade, duration: 2 bars
               changes: baseline shift (Pressure 0.20→0.30, Momentum 0.10→0.30)

  Continuity at bar 17 (Moment 1 → Moment 2):

    Moment 1 ends:   PRE=0.20  MOM=0.10  (baseline, no envelopes)
    Moment 2 starts: PRE=0.30  MOM=0.30  (new baseline)

    Scene crossfade over 2 bars:

    bar 17      bar 18      bar 19
    PRE: 0.20 ──► 0.25 ──► 0.30 ──► envelope takes over from 0.30
    MOM: 0.10 ──► 0.20 ──► 0.30 ──► envelope takes over from 0.30

    Material: Ice → Ice (same material, no visual blend needed,
              only the Prim8 values ramp)

  No discontinuity. Pressure smoothly rises from 0.20 through the 2-bar transition, reaches the
  new baseline of 0.30, and the envelope immediately continues it upward toward 0.55. One
  continuous curve through the boundary.

  ---
  MOMENT 3: "Pressure Cooker" (bars 25–32)

  baseline:  PRE=0.55  IMP=0.00  MAS=0.50  MOM=0.50
             HEA=0.25  SPA=0.35  TEX=0.20  GRA=-0.20 (slight inward pull)

  envelopes: Pressure  ╱ exponential 0.55 → 0.95 over 8 bars (blend: set)
             Space     ╲ exponential 0.35 → 0.10 over 8 bars (blend: set)
             Texture   ╱ linear 0.20 → 0.60 over 8 bars (blend: set)
             Gravity   ╲ linear -0.20 → -0.80 over 8 bars (blend: set)

  triggers:  snare density routing → (generates the Pressure envelope above)
             kick → Impact +0.40 (reduced — kick is less important now)

  scenes:    ENTRY from Moment 2:
               mode: morph, duration: 1 bar
               changes: zones Single → Dual (core + edge split)

             INTERNAL SCENE at bar 29:
               mode: morph, duration: 2 bars
               changes: baseline Texture 0.20 → 0.40 (the roll gets grainy)

  Continuity at bar 25 (Moment 2 → Moment 3):

    Moment 2 ends:   PRE=0.55  SPA=0.35  (envelope peaked at 0.55)
    Moment 3 starts: PRE=0.55  SPA=0.35  (baseline matches!)

    The baselines were DESIGNED to match the outgoing envelope values.
    The transition is imperceptible. Pressure at 0.55 is both where
    Moment 2's envelope ended and where Moment 3's envelope begins.

    Zone change (Single → Dual) morphs over 1 bar:
    bar 25: Single zone begins splitting — edge dims slightly,
            core brightens slightly. By bar 26: two distinct rings.

  The Pressure Cooker in action (bars 25–32):

    time ──►   bar25   bar26   bar27   bar28   bar29   bar30   bar31   bar32

    PRE:       0.55    0.60    0.67    0.74    0.80    0.86    0.91    0.95
    SPA:       0.35    0.30    0.25    0.20    0.17    0.14    0.12    0.10
    TEX:       0.20    0.25    0.30    0.35    0.40    0.45    0.50    0.60
    GRA:      -0.20   -0.28   -0.36   -0.44   -0.50   -0.58   -0.66   -0.80

    ↑ pressure rising          ↑ internal Scene bumps      ↑ maximum
    ↓ space shrinking            texture baseline             compression
    ↑ texture roughening
    ↓ gravity pulling inward

    CORE ring:  blazing white-blue, compressed tight, vibrating
    EDGE ring:  nearly dark, faint shimmer, the void around the star

  Light is being crushed inward. The core gets brighter, rougher, more pressurised. The edge goes
   dark. Gravity pulls everything toward centre. By bar 32, all energy is a white-hot point. The
  strip is dark except for a screaming centre. The audience can FEEL that something is about to
  break.

  ---
  MOMENT 4: "Detonation" (bar 33+)

  baseline:  PRE=0.40  IMP=0.00  MAS=0.90  MOM=0.80
             HEA=0.85  SPA=1.00  TEX=0.50  GRA=+1.00 (full outward)

  envelopes: Pressure  ╲ exponential 0.95 → 0.40 over 4 bars (blend: set)
             Gravity   ╲ linear +1.00 → +0.30 over 8 bars (blend: set)

  triggers:  kick → Impact +1.00, Space +0.20, every beat
             lead envelope → Heat (0.85 sustained), Space (0.90 sustained)
             bass envelope → Mass (0.90 sustained)

  scenes:    ENTRY from Moment 3:
               mode: CUT, duration: 0ms (the discontinuity IS the drop)
               changes: material Ice → Magma
                        zones Dual → Quad
                        baseline: full shift

  zones:     Quad — core / inner / outer / edge
             core triggers fire at t+0ms
             inner triggers fire at t+50ms   ← cascade delay
             outer triggers fire at t+100ms
             edge triggers fire at t+150ms

  Continuity at bar 33 (the CUT — the one allowed discontinuity):

    Moment 3 ends:   PRE=0.95  SPA=0.10  HEA=0.25  GRA=-0.80  material:Ice
    Moment 4 starts: PRE=0.40  SPA=1.00  HEA=0.85  GRA=+1.00  material:Magma

    Scene mode: CUT.

    Material: INSTANT swap. Ice vanishes. Magma appears.

    BUT — Law 4 applies. Even in a cut, Prim8 gets a 50ms micro-ramp:

    t=0ms    t=25ms    t=50ms
    PRE: 0.95 ──► 0.68 ──► 0.40   ← but envelope immediately starts here
    SPA: 0.10 ──► 0.55 ──► 1.00   ← SPACE goes from pinpoint to FULL
    HEA: 0.25 ──► 0.55 ──► 0.85   ← cold to HOT in 50ms
    GRA:-0.80 ──► 0.10 ──► 1.00   ← inward reverses to OUTWARD

    What the audience sees in that 50ms:
    ┌──────────────────────────────────────────────────────┐
    │ frame 1: white-hot compressed point (Ice, bar 32)    │
    │ frame 2: MAGMA. Molten orange DETONATES outward.     │
    │          Core fires first. Inner 50ms later.          │
    │          Outer 100ms. Edge 150ms.                     │
    │          A cascade of fire expanding from centre.     │
    │ frame 3: Full strip. All four zones blazing.          │
    │          Mass 0.90 = heavy, lumbering momentum.       │
    │          The horn is HERE. It fills the room.         │
    └──────────────────────────────────────────────────────┘

    Pressure immediately begins decaying (0.95 → 0.40 over 4 bars)
    because the drop doesn't STAY at maximum — it releases.
    The energy transforms from compression into momentum.

  The Full Primitive Trace (All 32 Bars → Drop)

    1.0 │                                              ╱PRE
        │                                           ╱╱╱
        │                                        ╱╱╱  ·····SPA (drops to 1.0)
    0.8 │                                     ╱╱╱
        │                                  ╱╱╱         ····HEA (jumps to 0.85)
        │                              ╱╱╱╱
    0.6 │                          ╱╱╱╱
        │                     ╱╱╱╱╱
        │               ╱╱╱╱╱╱      ╲SPA (shrinking)
    0.4 │          ╱╱╱╱╱╱              ╲
        │     ╱╱╱╱╱╱                    ╲
        │╱╱╱╱╱ PRE (rising)              ╲
    0.2 │─ ─ ─ ─ ─ ─ ─PRE baseline─ ─ ─  ╲
        │                                    ╲
        │──SPA baseline──────────────────      ╲
    0.0 ├───────┬───────┬───────┬───────┬──────────►
        1       8      16     24      32  33
        │ INTRO │       │ BUILD │ ROLL  │DROP
        │       │       │       │       │
        │ Ice   │ Ice   │ Ice   │ Ice→  │ Magma
        │steady │rising │crushing│ CUT  │ releasing

  Every value flows continuously — except at bar 33 where the cut is the point. Even there, the
  50ms micro-ramp prevents a single-frame hardware spike. The system is always smooth. The art
  decides where to break.

  ---
  Zones define space. Materials define character. Continuity laws define physics. The creator
  just sculpts — the instrument keeps it musical. What's the next piece?

  ### Addendum 

  That’s it. **This 2B second pass is the “Instrument Laws” document you were missing.** It doesn’t just *describe* the cockpit—it pins down the physics so the cockpit can’t lie.

It also directly patches the three holes we identified earlier:

* **Zones** ✅ (ZoneLayout + targeting + groups + zone-local transitions)
* **Material binding** ✅ (Material Engine Adapter + universal Prim8 vocabulary)
* **Continuity semantics** ✅ (laws + transition modes + clamp rule + micro-ramp)

This is now *coherent enough to build without inventing new rules every week*.

---

## What’s genuinely excellent here

### 1) ZoneLayout is simple, expressive, and “centre-origin native”

Concentric rings addressed by distance-from-centre is perfectly aligned with your whole aesthetic. The targeting syntax (“all”, “centre”, “rim”, “alternate”) is exactly the kind of shorthand that makes creators fast.

### 2) The Material Adapter contract is the right abstraction boundary

“Same inputs, different character” is the core promise. The universal-primitive rule (“no material ignores Impact/Heat/etc.”) is what keeps the entire system remixable.

### 3) Continuity Laws are the difference between *instrument* and *animation tool*

The clamping law alone:

```
effective(t) = clamp(baseline + envelopes(t) + triggers(t), 0, 1)
```

is a “no glitch” guarantee *and* it gives creators permission to push hard without breaking hardware or aesthetics.

### 4) Transition modes have real semantics (not just words)

Crossfade vs morph vs cut are no longer vibes—they’re behaviours. That’s how you keep “drop discontinuity” intentional without accidental discontinuities elsewhere.

### 5) The worked Mammoth example proves the system is musically literate

It demonstrates the *actual* art trick: compression → tension → release, with zone-splitting and cascade delay. That’s the banger test.

---

## Two tiny inconsistencies to fix (so Opus doesn’t hallucinate extra rules)

### Fix A) Zone Independence implies **per-zone baseline**, not just per-zone evaluation

Right now it says:

> “Each zone ring evaluates its own effective Prim8 independently.”

…but the 2A model defined **one baseline per Moment**. If you want a Scene that only transitions the core ring, you need one of these minimal deltas:

**Recommended minimal delta to the data model**

* Keep `baseline: Prim8` as the default for all rings
* Add **optional per-zone overrides**:

```js
baseline: Prim8
baseline_by_zone?: { [ringId]: Partial<Prim8> }  // offsets/overrides
```

Then the rule becomes: each zone’s effective baseline is `baseline + baseline_by_zone[ring]`.

This makes “core detonates, edge holds” actually representable without inventing new hidden state.

### Fix B) Define how targeted vs untargeted cues combine

The spec implies: untargeted applies to all rings, targeted layers on top. Great—just make it explicit:

* Untargeted cues are **copied into each ring**
* Targeted cues exist **only** in the targeted rings
* Combination is still the same clamp stack

That’s one sentence, and it prevents future confusion.

---

## What this means for 2C

Now that 2A (cockpit + model) and 2B (sound patch bay + laws) are locked, **2C should be the “from art to runtime” bridge**.

Not implementation. Not stack choice wars. Just: *what is the executable artifact and how does it play?*

### The 2C prompt you want Opus to answer

Send something like this (tight, creative, but concrete):

> **PRISM 2C — ShowBundle + Playback Semantics**
> Define the shippable “unit” that makes PRISM real: **ShowBundle**.
> Include:
>
> 1. What files/data it contains (show model, routing, materials refs, analysis refs).
> 2. What the device runtime needs at playback time (what is computed live vs precompiled).
> 3. How Moments/Scenes/zone targets resolve into per-frame Prim8 per zone.
> 4. How the Material Adapter is invoked (inputs/outputs per frame, including transition_alpha).
> 5. How edits + overrides are represented so users can correct Trinity without breaking provenance.
>    Give one short Mammoth “playback trace” across the drop showing exactly what the runtime does.

Once 2C is good, you have a complete triangle:

* **Authoring language** (2A)
* **Sound wiring + laws** (2B)
* **Executable artifact + playback semantics** (2C)

At that point, Opus isn’t just inspired—it’s constrained by a consistent universe, in the good way.

Keep the banger energy: this second 2B pass is the first time the system reads like a *real instrument spec* instead of a mood board.
