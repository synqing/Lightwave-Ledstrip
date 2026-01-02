# Storytelling Framework for Light Shows (LGP)

This document describes how to construct **narrative light shows** using the existing LightwaveOS controls and effect architecture, with a strong emphasis on **LGP optics** and our two hard constraints:

- **Centre origin only** (LEDs 79/80 are the narrative anchor).
- **No rainbows** (no hue‑wheel sweeps or rainbow cycling).

## The core idea

An LGP show is most compelling when it behaves like a physical phenomenon with intention:

- **Cause** (energy appears)
- **Propagation** (it moves with rules)
- **Interaction** (it collides, interferes, organises)
- **After‑image** (memory, decay, residue)

If you can describe an effect as a sentence with those four parts, it will usually feel coherent on the plate.

## Vocabulary: motifs you can reuse

Motifs are the smallest units of story. Reuse them deliberately; repetition is how the audience understands the “language”.

### Motif: Centre ignition
- **Visual**: light appears at 79/80 then expands.
- **Emotion**: beginning, awakening, intention.
- **Controls**: low Speed, moderate Intensity, low‑mid Complexity.

### Motif: Edge approach (inward contraction)
- **Visual**: energy advances from both edges and converges at centre.
- **Emotion**: threat, inevitability, anticipation.
- **Controls**: Speed rising over time; Intensity rising; Saturation often reduced for “white‑light pressure”.

### Motif: Collision (centre event)
- **Visual**: two packets meet; brief bloom; structured fragments emerge.
- **Emotion**: revelation, impact, turning point.
- **Controls**: Intensity spike; diffusion briefly increased; avoid narrow strobe pulses.

### Motif: Interference lattice
- **Visual**: boxes/tiles/bands appear and stabilise.
- **Emotion**: order, intelligence, “the system reveals itself”.
- **Controls**: Complexity controls lattice density; Variation selects phase regime; Speed kept slow.

### Motif: Dissolution (memory fade)
- **Visual**: structure melts into mist; trails linger.
- **Emotion**: relief, aftermath, melancholy.
- **Controls**: lower Intensity; higher diffusion; slower decay.

## Structure: a practical show model

Use a simple model that works for both audio‑reactive and non‑audio shows.

### Three‑act arc (recommended)

1. **Act I – Establish rules (15–30 s)**
   - Show the centre‑origin principle and the palette language.
   - Keep Complexity low so the audience learns the “alphabet”.

2. **Act II – Complicate (30–90 s)**
   - Introduce interference, moiré envelopes, depth swaps.
   - Increase Complexity and introduce controlled parameter motion.

3. **Act III – Resolve (15–30 s)**
   - Return to a recognisable motif (centre ignition or stable lattice).
   - Reduce Complexity and let diffusion/trails carry the ending.

## Pacing and rhythm (mapped to existing controls)

### Pacing rules of thumb

- **Legibility first**: if the audience cannot perceive the motif, it is noise.
- Prefer **slow drift + occasional events** over constant motion.
- When in doubt: keep **Speed low**, and use **Intensity** to create dynamics.

### Control mapping

- **Speed**: how fast the narrative advances (drift rate, packet velocity, envelope speed).
- **Intensity**: tension (contrast, energy, collision strength).
- **Complexity**: density of information (layers, lattice density, particle count).
- **Variation**: scene mode (phase regime, symmetry mode, edge balance preset).
- **Saturation**: realism vs theatre (lower saturation reads as “light”; higher reads as “colour material”).

## Emotional palette (no‑rainbows compliant)

This is not a list of hues; it is a list of *relationships*.

- **Calm / contemplation**: cold family + near‑black, high diffusion, slow Speed.
- **Awe / revelation**: neutral centre highlights, moderate diffusion, slowly increasing Complexity.
- **Tension / threat**: low saturation, high contrast, inward motion; keep colour restrained.
- **Triumph / clarity**: structured lattice, crisp boundaries, diffusion reduced slightly.
- **Melancholy / aftermath**: warm family, low Intensity, long decay, slow drift.

## Scene cards (example storyboards)

These are deliberately expressed in the language of your existing system: effect choice + parameter envelopes. Treat them as patterns to remix.

### Scene card 1: “Awakening crystal”
- **Start**: centre ignition motif, low Complexity.
- **Develop**: introduce a slow interference lattice.
- **End**: dissolve to mist.
- **Suggested components**:
  - A centre‑origin shimmer (diffusion on)
  - A standing structure effect (boxes/tiles)
  - A gentle fade‑out (diffusion increased)

### Scene card 2: “Approach and impact”
- **Start**: edge approach (inward contraction) at low Saturation.
- **Event**: collision bloom at centre (safe pulse width).
- **After**: fragments reorganise into a stable lattice.
- **Parameter envelope**:
  - Speed: ramp up → brief dip at impact → settle mid
  - Intensity: ramp up → peak at impact → settle mid‑low
  - Complexity: low → medium after impact (structure)

### Scene card 3: “Curtains of time” (moiré)
- **Start**: slow moiré envelope (curtains) with diffusion.
- **Middle**: increase Δ slightly to create “pressure”.
- **End**: return Δ to near zero, leaving a calm residual drift.
- **Safety note**: keep envelope < ~1 Hz for comfort.

### Scene card 4: “Depth swap”
- **Start**: one edge dominant (foreground).
- **Middle**: cross‑fade dominance slowly (depth inversion).
- **End**: land on a balanced centre highlight.
- **Key**: avoid hue rotation; tell the story with depth, not colour cycling.

## Transition choreography (centre‑origin compliant)

Transitions are part of the narrative. Use them as punctuation.

- **Crossfade**: “time passes” (soft cut).
- **Morph**: “the same thing becomes something else” (identity retained).
- **Centre wipe** (outward): “reveal” (truth emerges from centre).
- **Centre collapse** (inward): “convergence” (pressure, threat, decision).

Keep transitions slower than you think; the plate is already adding complexity.

## Safety and comfort (minimum guidance)

- Avoid rapid full‑frame flashes.
- Avoid narrow, high‑contrast strobing at uncomfortable rates.
- Prefer diffusion and decay over hard on/off gating.
- Always ensure the emergency stop is functional in the control surface.

## Next document

For engineering‑level specification templates, testing procedures, and operational constraints, see `docs/guides/IMPLEMENTATION_PLAYBOOK_LIGHT_PATTERNS.md`.


