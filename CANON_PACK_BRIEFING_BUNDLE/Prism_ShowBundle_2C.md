PRISM 2C — ShowBundle + Playback Semantics            

  ---                                                                                            
  1. ShowBundle Contents
                                                                                                 
  A ShowBundle is a single self-contained package. Everything the device needs. Nothing it
  doesn't.

  ShowBundle
  │
  ├── manifest ──────────────────────────────────────────────────
  │     id:             uuid
  │     name:           "Mammoth — Volcano Cut"
  │     artist:         "SpectraSynq user @pyroclast"
  │     track:          { title, artist, duration_ms, bpm, key }
  │     prism_version:  "1.0"
  │     created:        timestamp
  │     device_compat:  ["lightwave-v2"]
  │
  ├── show ──────────────────────────── the authored composition
  │     moments: Moment[]               ordered by time
  │     global_baseline: Prim8           show-wide defaults
  │
  ├── routing ───────────────────────── sound-to-light wiring
  │     per_moment: {
  │       moment_id → StemRouting[]      which stems drive which primitives
  │     }
  │
  ├── analysis_snapshot ─────────────── Trinity output, frozen
  │     beats:      float[]              beat timestamps (ms)
  │     downbeats:  float[]              bar boundaries
  │     sections:   Section[]            { label, start, end, energy }
  │     stems:      StemEvents{}         { kick: float[], snare: float[], ... }
  │     energy:     ControlPoint[]       composite energy curve
  │     hash:       string               fingerprint for edit provenance
  │
  ├── edits ─────────────────────────── user corrections (patch layer)
  │     base_hash:    string             which analysis these correct
  │     corrections:  EditOp[]           add / delete / move / reclassify / threshold
  │
  └── material_refs ─────────────────── material declarations
        used: MaterialId[]               list of built-in IDs referenced ("magma", "ice")
        custom: MaterialDef[]            any user-created materials (full definition inline)

  What is NOT in the bundle: the audio file. The track plays on the source (phone, browser,
  computer). The device receives sync pulses. The bundle is choreography, not media.

  Bundle size: A typical 4-minute show compresses to 2–8 KB. Moments are recipes, not rendered
  frames. A 120fps rendered output would be ~4 MB. The bundle is 500x smaller because it stores
  instructions, not pixels.

  ---
  2. Precompiled vs Live
  ┌────────────────────────────────────────────────┬───────────────────────────────────────────┐
  │ Precompiled (in bundle, read-only at playback) │   Computed live (per frame, on device)    │
  ├────────────────────────────────────────────────┼───────────────────────────────────────────┤
  │ Moment boundaries + baselines                  │ Envelope interpolation at current t       │
  ├────────────────────────────────────────────────┼───────────────────────────────────────────┤
  │ Envelope control points (bezier)               │ Trigger impulse evaluation (attack/decay) │
  ├────────────────────────────────────────────────┼───────────────────────────────────────────┤
  │ Trigger event list (time + shape + spike)      │ Prim8 stacking (base + env + trig)        │
  ├────────────────────────────────────────────────┼───────────────────────────────────────────┤
  │ Scene transitions (time + mode + duration)     │ Material rendering (Prim8 → CRGB)         │
  ├────────────────────────────────────────────────┼───────────────────────────────────────────┤
  │ Zone layouts per Moment                        │ Transition blending (alpha mixing)        │
  ├────────────────────────────────────────────────┼───────────────────────────────────────────┤
  │ Routing table per Moment                       │ Sync drift compensation                   │
  ├────────────────────────────────────────────────┼───────────────────────────────────────────┤
  │ Beat grid + section markers                    │ Cascade delay offsets                     │
  ├────────────────────────────────────────────────┼───────────────────────────────────────────┤
  │ Material IDs                                   │ Zone compositing → final CRGB[320]        │
  └────────────────────────────────────────────────┴───────────────────────────────────────────┘
  The device is a recipe evaluator, not a frame player. It reads the recipe and cooks every frame
   fresh. This means playback quality scales with device capability — a faster chip renders
  smoother, a slower chip drops frames, but the show data is identical.

  ---
  3. Resolution Pipeline — Moment to Frame

  Every frame (~8.3ms at 120fps), the runtime executes this pipeline:

  ┌─────────────────────────────────────────────────────────────┐
  │                    PER FRAME (120fps)                        │
  │                                                             │
  │  ① LOCATE                                                   │
  │     current_time → which Moment? (binary search)            │
  │     active Scene transition? (check ±transition_duration)   │
  │                                                             │
  │  ② EVALUATE PRIM8 (per zone ring)                           │
  │                                                             │
  │     For OUTGOING Moment (if in transition):                 │
  │       out_prim8 = out.baseline                              │
  │                 + Σ out.envelopes[i].sample(t)              │
  │                 + Σ out.triggers[j].impulse(t)              │
  │       clamp each channel to [0, 1]                          │
  │                                                             │
  │     For INCOMING / CURRENT Moment:                          │
  │       in_prim8  = in.baseline                               │
  │                 + Σ in.envelopes[i].sample(t)               │
  │                 + Σ in.triggers[j].impulse(t)               │
  │       clamp each channel to [0, 1]                          │
  │                                                             │
  │  ③ BLEND (if Scene transition active)                       │
  │                                                             │
  │     transition_alpha = elapsed / scene.duration             │
  │     apply easing curve to alpha                             │
  │                                                             │
  │     crossfade: effective = lerp(out_prim8, in_prim8, α)     │
  │     morph:     effective = lerp(out_prim8, in_prim8, α)     │
  │     cut:       effective = lerp(out_prim8, in_prim8, α)     │
  │                (α ramps 0→1 over 50ms micro-ramp)           │
  │                                                             │
  │     If no transition: effective = in_prim8, α = 1.0         │
  │                                                             │
  │  ④ MATERIAL RENDER (per zone ring)                          │
  │                                                             │
  │     adapter.render(                                         │
  │       prim8:    effective Prim8[8]                           │
  │       zone:     { id, span, led_start, led_count }          │
  │       elapsed:  ms since Moment start                       │
  │       alpha:    transition_alpha                             │
  │       mat_out:  outgoing MaterialRef  (if transitioning)    │
  │       mat_in:   incoming MaterialRef                        │
  │     ) → CRGB[zone.led_count]                                │
  │                                                             │
  │  ⑤ COMPOSITE                                                │
  │     Stitch zone outputs into CRGB[320]                      │
  │     Apply global brightness ceiling                         │
  │     Write to LED buffer                                     │
  │                                                             │
  └─────────────────────────────────────────────────────────────┘

  Step-by-step for zone-targeted cues

  Zone targeting is resolved at step ②. Each zone ring evaluates only the cues that target it:

  ring "core" receives:
    → all cues with target: null (global)
    → all cues with target: "core"
    → all cues with target containing "core" (e.g., ["core", "inner"])
    → IGNORES cues targeting "edge" only

  ring "edge" receives:
    → all cues with target: null (global)
    → all cues with target: "edge"
    → IGNORES cues targeting "core" only

  This means each ring can have a different effective Prim8 at the same instant. The core can be
  at Impact 0.9 while the edge sits at Impact 0.0 — because the Impact trigger targeted core
  only.

  ---
  4. Material Adapter — Frame Contract

  MaterialAdapter.render()
  ─────────────────────────────────────────────────────────────
  INPUTS:
    prim8           Prim8[8]        the 8 effective values for this zone
    zone_id         string          "core" | "inner" | "outer" | "edge"
    zone_span       [int, int]      LED index range (e.g., [60, 79] for core)
    elapsed_ms      float           time since Moment start (drives internal anim)
    transition_α    float [0, 1]    0.0 = fully outgoing, 1.0 = fully incoming
    mat_outgoing    MaterialRef     outgoing material (same as incoming if no transition)
    mat_incoming    MaterialRef     incoming material

  OUTPUT:
    pixels          CRGB[zone.led_count]   colour per LED in this zone
  ─────────────────────────────────────────────────────────────

  BEHAVIOUR BY TRANSITION MODE:

    crossfade:
      pixels_out = mat_outgoing.interpret(prim8, zone, elapsed)
      pixels_in  = mat_incoming.interpret(prim8, zone, elapsed)
      pixels     = blend(pixels_out, pixels_in, α)    ← per-LED alpha blend

    morph:
      interp_table = lerp(mat_out.interpretation, mat_in.interpretation, α)
      merged_mat   = Material(interp_table)
      pixels       = merged_mat.interpret(prim8, zone, elapsed)
      (single render pass — no stacking, the material itself transforms)

    cut:
      if α < 1.0:    ← during 50ms micro-ramp
        pixels = mat_incoming.interpret(prim8, zone, elapsed)
        (material swaps instantly; prim8 is what ramps)
      else:
        pixels = mat_incoming.interpret(prim8, zone, elapsed)

    no transition:
      pixels = mat_incoming.interpret(prim8, zone, elapsed)

  The key distinction: in crossfade, two material renderers run simultaneously and their pixel
  outputs are blended. In morph, the interpretation table itself interpolates and a single
  renderer runs. In cut, only the incoming material renders — the discontinuity is in the
  material swap, while the Prim8 micro-ramp provides the 50ms softening.

  ---
  5. Edit Provenance

  User corrections to Trinity are stored as a patch layer — never modifying the original
  analysis.

  edits {
    base_hash:    "a7f3c..."          ← SHA of the analysis this patches

    corrections: [
      ── STRUCTURAL OPS ──
      { op: "delete",      stem: "kick",  time: 14302 }
      { op: "add",         stem: "snare", time: 29847 }
      { op: "move",        stem: "clap",  from: 7201, to: 7215 }
      { op: "reclassify",  stem: "snare", to: "clap", time: 3502 }

      ── SENSITIVITY OPS ──
      { op: "threshold",   stem: "snare", range: [25000, 32000], sensitivity: 0.85 }

      ── SECTION OPS ──
      { op: "split_section",  at: 48000 }                          ← split a section
      { op: "merge_sections", from: "chorus_1", into: "chorus_2" } ← merge two
      { op: "relabel",        section: "verse_2", label: "bridge" } ← rename
    ]
  }

  Resolution at playback

  effective_stems = analysis.stems                    ← start with Trinity output
                    .apply(edits.corrections)          ← layer user patches

  for each correction:
    delete  → remove event at matching time (±5ms tolerance)
    add     → insert synthetic event
    move    → remove at .from, insert at .to
    reclassify → remove from source stem, add to target stem
    threshold → re-filter events in range using new sensitivity

  Re-analysis safety

  When Trinity re-analyses (new model version, user re-imports):

  new_hash ≠ old_hash → corrections may conflict

  For each correction:
    If the event it references still exists (within ±10ms): apply normally
    If the event is gone:                                   flag for review
    If a new event appears near a "delete" correction:      flag for review

  User sees: "3 of your edits need review after re-analysis" + highlighted conflicts

  Provenance chain: analysis (Trinity) → edits (human) → effective (merged). The ShowBundle
  stores all three. A recipient can see exactly what the AI detected, what the human corrected,
  and what the final result is. Attribution is always clear.

  ---
  6. Playback Trace — Mammoth Drop (bar 32.4 → bar 33.2)

  128 BPM. Beat = 469ms. Frame = 8.3ms. Zone layout transitions from Dual to Quad at the drop.

  t = -100ms  [bar 32, beat 4, +369ms]
  ══════════════════════════════════════════════════════════
  LOCATE:  Moment "Pressure Cooker"  |  no active transition
  ZONES:   Dual — core [60..79,80..99], edge [0..59,100..159]

  EVALUATE (both zones receive same global cues):
    baseline:   PRE=.55 IMP=.00 MAS=.50 MOM=.50 HEA=.25 SPA=.35 TEX=.20 GRA=-.20
    envelopes:  PRE→.94  SPA→.11  TEX→.58  GRA→-.78  (exponentials near peak)
    triggers:   kick IMP decaying, +.08 remaining (fired 300ms ago)
    ─────────────────────────────────────────────
    effective:  PRE=.94 IMP=.08 MAS=.50 MOM=.50 HEA=.25 SPA=.11 TEX=.58 GRA=-.78

  MATERIAL:  Ice, α=1.0, no transition
    → core: white-blue compressed point, crystalline fractures, pulling inward
    → edge: nearly dark, faint shimmer at periphery

  COMPOSITE: 320 LEDs. Centre 20 blazing. Rest dark. Maximum compression.


  t = 0ms  [bar 33, beat 1 — THE DROP]
  ══════════════════════════════════════════════════════════
  LOCATE:  Moment boundary! "Cooker" → "Detonation"
           Scene: CUT, 50ms micro-ramp
           Zone layout: Dual → Quad (instant in cut mode)

           OUTGOING final state captured:
           PRE=.95 IMP=.00 MAS=.50 MOM=.50 HEA=.25 SPA=.10 TEX=.60 GRA=-.80

           INCOMING baseline loaded:
           PRE=.40 IMP=.00 MAS=.90 MOM=.80 HEA=.85 SPA=1.0 TEX=.50 GRA=+1.0

           Incoming envelopes begin:
           PRE: .95 → .40 over 4 bars (catches outgoing value, decays)
           GRA: +1.0 → +.30 over 8 bars

           Kick trigger fires: IMP +1.00, target: "core", attack 5ms
           (inner/outer/edge triggers scheduled at +50/+100/+150ms)

  MICRO-RAMP:  α = 0.00 (just started)

  EVALUATE:
    blended = lerp(outgoing, incoming, α=0.00) = outgoing state
    + trigger: core IMP spike just starting (attack phase, +.10)

    core:  PRE=.95 IMP=.10 MAS=.50 MOM=.50 HEA=.25 SPA=.10 TEX=.60 GRA=-.80
    inner: PRE=.95 IMP=.00 MAS=.50 MOM=.50 HEA=.25 SPA=.10 TEX=.60 GRA=-.80
    outer: same as inner
    edge:  same as inner

  MATERIAL:  CUT → Magma immediately (material swaps at α=0)
    → Magma receives compressed-cold-inward Prim8
    → Magma interprets: dense obsidian core, dark crust, inward pull
    → Visually: the strip is DARK MAGMA. Dormant volcano. One frame.


  t = 8ms  [frame 1 after drop]
  ══════════════════════════════════════════════════════════
  MICRO-RAMP:  α = 0.16  (8/50)

  EVALUATE:
    blended:  PRE=.86 IMP=.00 MAS=.57 MOM=.55 HEA=.35 SPA=.24 TEX=.58 GRA=-.51

    core trigger: attack complete (5ms), now decaying
      IMP spike: +.97
    inner/outer/edge: no triggers yet

    core:  PRE=.86 IMP=.97 MAS=.57 MOM=.55 HEA=.35 SPA=.24 TEX=.58 GRA=-.51
    inner: PRE=.86 IMP=.00  ...same blended values, no spike
    outer: same as inner
    edge:  same as inner

  MATERIAL: Magma
    → core: ERUPTION. IMP=.97 in Magma = molten orange shockwave
      expanding from centre. SPA only .24 so it hasn't spread far yet.
      HEA at .35 = warming but not fully hot.
    → inner/outer/edge: dark obsidian, dormant. The wave hasn't arrived.

  COMPOSITE: Centre 20 LEDs blazing orange. Everything else dark.
             The explosion has begun but is still contained.


  t = 25ms  [frame 3]
  ══════════════════════════════════════════════════════════
  MICRO-RAMP:  α = 0.50

  EVALUATE:
    blended:  PRE=.68 SPA=.55 HEA=.55 GRA=+.10

    core:   IMP decaying → +.82
    inner:  trigger fires (50ms cascade, but we're at 25ms... wait)

    Correction: cascade delays are from the Moment start, not from micro-ramp.
    Inner trigger fires at t=50ms. Not yet.

    core:  PRE=.68 IMP=.82 MAS=.70 MOM=.65 HEA=.55 SPA=.55 TEX=.54 GRA=+.10
    inner: PRE=.68 IMP=.00 MAS=.70 MOM=.65 HEA=.55 SPA=.55 TEX=.54 GRA=+.10
    outer: same as inner
    edge:  same as inner

  MATERIAL: Magma
    → core: spreading molten burst. SPA=.55 = wave reaching mid-strip.
      GRA flipped positive (+.10) = energy now flowing OUTWARD.
      HEA=.55 = orange deepening, warmth arriving.
    → inner: warm magma glow, no impact yet. Waiting.

  COMPOSITE: Centre half of strip orange-bright. Outer half warming from black.
             The wave is visibly expanding outward.


  t = 50ms  [micro-ramp complete]
  ══════════════════════════════════════════════════════════
  MICRO-RAMP:  α = 1.00. Transition done. Pure incoming state.

  EVALUATE:
    baseline:   PRE=.40 MAS=.90 MOM=.80 HEA=.85 SPA=1.0 TEX=.50 GRA=+1.0
    envelopes:  PRE→.94 (catches outgoing, barely started decaying)
                GRA→+.99 (barely started decaying)

    core:   IMP decaying → +.68
    inner:  trigger fires NOW. IMP +1.00, attack 5ms begins.
    outer:  scheduled at t=100ms
    edge:   scheduled at t=150ms

    core:  PRE=.94 IMP=.68 MAS=.90 MOM=.80 HEA=.85 SPA=1.0 TEX=.50 GRA=+.99
    inner: PRE=.94 IMP=.10 MAS=.90 MOM=.80 HEA=.85 SPA=1.0 TEX=.50 GRA=+.99
    outer: PRE=.94 IMP=.00 ...
    edge:  PRE=.94 IMP=.00 ...

  MATERIAL: Magma, no transition
    → core: blazing molten river flowing outward. IMP still elevated.
      Full heat. Full space. Full outward gravity. VOLCANIC.
    → inner: catching fire. Impact spike just starting.
    → outer/edge: hot magma glow, full space, but no impact spike yet.
      The lava is THERE but hasn't been HIT yet.

  COMPOSITE: Inner 60% bright molten orange. Outer 40% deep red glow.
             The cascade wave is visibly sweeping outward.


  t = 100ms
  ══════════════════════════════════════════════════════════
    core:   IMP → +.42 (decaying, the punch is fading into momentum)
    inner:  IMP → +.82 (peaked, now decaying)
    outer:  trigger fires NOW. IMP +1.00 begins.
    edge:   scheduled at t=150ms

    The wave has reached the outer ring.
    Core is settling into sustained heat.
    Inner is blazing.
    Outer is detonating.
    Edge: hot glow, waiting for the final slam.


  t = 150ms
  ══════════════════════════════════════════════════════════
    core:   IMP → +.22 (almost settled, pure momentum sustain)
    inner:  IMP → +.42
    outer:  IMP → +.82
    edge:   trigger fires NOW. IMP +1.00 begins.

    THE WAVE HITS THE EDGE.

    All four zones active. All 320 LEDs blazing.
    Molten orange from centre to edge.
    The cascade took 150ms to sweep the full strip.

    MOM=.80 means the energy doesn't die between beats.
    The strip stays alive, flowing outward, until the next kick
    fires 319ms from now and the whole cascade repeats.


  t = 469ms  [beat 2]
  ══════════════════════════════════════════════════════════
    All zones: IMP has fully decayed. MOM=.80 keeps residual glow.
    PRE envelope: .93 (barely moved, 4-bar decay is slow)

    Next kick fires. New cascade begins: core → inner → outer → edge.
    Each kick is a fresh volcanic eruption expanding outward.

    The horn (lead stem) sustains HEA=.85 and SPA=1.0 via envelope routing.
    Between kicks, the strip isn't dark — it's a flowing river of magma.
    The kicks add IMPACT on top of that river. Punches on a sustained burn.

  ---
  THE DROP — 150ms CASCADE

    t=0ms     t=50ms    t=100ms   t=150ms   t=469ms

    edge  ░░░░░░░░  ░░░░░░░░  ░░░░▓▓▓▓  ████████  ▓▓▓▓▓▓▓▓
    outer ░░░░░░░░  ░░░░░░░░  ████████  ▓▓▓▓▓▓▓▓  ▓▓▓▓▓▓▓▓
    inner ░░░░░░░░  ████████  ▓▓▓▓▓▓▓▓  ▓▓▓▓▓▓▓▓  ▓▓▓▓▓▓▓▓
    core  ████████  ▓▓▓▓▓▓▓▓  ▓▓▓▓▓▓▓▓  ▓▓▓▓▓▓▓▓  ▓▓▓▓▓▓▓▓
          │         │         │         │         │
          detonate  spreading reaching  FULL      next
          core      outward   edge      STRIP     kick

    ░ = dark/dormant   ▓ = hot glow   █ = impact peak

  The bundle is 4KB. The playback is 120fps. The cascade is 150ms. The audience feels the room
  explode from the centre outward. That's the whole chain — authored in Moments, stored as
  recipes, evaluated live, rendered through Magma. Where to next?


  ### Addendum

[FACT] **2C is the missing keystone.** It finally nails the thing Opus needs to “see”: *the platform is an instrument that compiles to a tiny, deterministic recipe; the device is a live evaluator; Trinity is frozen provenance + patch layer; playback is a tight per-frame pipeline.*

[INFERENCE] This is now **actually architecture-relevant** (not vibes-only), *and* it still keeps the creative cockpit intact. That’s the sweet spot.

Now—two tiny inconsistencies are worth addressing, but **as a micro-patch, not a rewrite**, because they affect whether Opus invents extra rules later.

## What’s strong (don’t touch)

* **ShowBundle as choreography, not media**: clean licensing stance + clean device responsibility.
* **Precompiled vs live** table: instantly communicates “recipe engine” to anyone.
* **Edits as patch layer**: provenance + re-analysis conflict handling = chef’s kiss.
* **Playback trace**: this is the “I can *feel* it” proof.

## The two nits (fix in ONE LINE each)

### Fix A — Zone Independence vs baseline storage

Right now, you’ve got *per-zone evaluation*, but baseline is written like a single Prim8 per Moment.

**Minimal semantic fix (no model change):**

> **Zone baseline at time t** is `moment.baseline` **plus any Scene baseline deltas whose target includes that zone** (plus global deltas). If a zone isn’t targeted, it carries forward under continuity laws.

That preserves your “independence” law **without** forcing `baseline_by_zone` into the data model.

### Fix B — Targeted vs untargeted cue combination

You *implicitly* say “zone receives global + its targeted cues,” but you don’t explicitly define conflict/precedence.

**One-line rule that prevents hallucinations:**

> For each zone: **apply all global cues, then zone-targeted cues**; combination is by cue type rules (**Scenes mutate baseline**, **Envelopes use blend add/mul/set**, **Triggers add then clamp**). Where multiple `set` operations compete, **latest cue in time wins**.

That’s it. Opus won’t invent a fourth blend mode or a hidden precedence ladder.

## One more thing (not a “fix”, just an explicit missing contract)

[INFERENCE] 2C mentions “sync pulses” but doesn’t define the **timebase contract** (what `current_time` *is* and who is authoritative). That’s the one place Opus might start improvising.

Add a single sentence somewhere in 2C:

> `current_time` is **track position in ms since start**, supplied by the source (browser/iOS/desktop) and smoothed on-device via drift compensation.

No protocol details needed yet—just the authority.

## Recommendation

Run with 2C as-is **but** append those **two one-liners** (A + B) **immediately after** the “Zone targeting is resolved at step ②” section. That keeps momentum *and* seals the semantic leaks.

You’ve now got: **cockpit (2A)** → **data model (2B)** → **executable artifact + runtime (2C)**.
That’s the whole instrument, end-to-end, in human language.
