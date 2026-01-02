# Creative Pattern Development Guide (LGP)

This guide explains how to design new Light Guide Plate (LGP) patterns that feel like **physics made visible** while remaining **usable as an instrument** (encoders, presets, transitions) and **compliant with our rules**.

## Non‑negotiables (read first)

- **Centre origin**: All motion/propagation must expand from LEDs **79/80** outward, or contract inward to **79/80**.
- **No rainbows**: Do not use full‑spectrum hue sweeps, rainbow cycling, or “palette spin” patterns that traverse the hue wheel.

If an idea requires breaking either rule, it is not a LightwaveOS LGP pattern.

## What the LGP is (artist lens vs engineer lens)

### Artist lens: what you are actually “drawing with”

You are not drawing directly on LEDs. You are injecting light into a plate and letting the plate (and its film stack) do the rendering:

- The **edges** are your “emitters”.
- The plate is a **waveguide** that spreads, reflects, and redistributes light.
- The surface/films are your **projector**: they decide where light escapes and how sharp/soft it looks.
- The centre is where the story happens: **collisions**, **standing shapes**, **depth cues**.

### Engineer lens: the core optical mechanisms we can exploit

Dual‑edge LGP behaviour is dominated by:

- **Total internal reflection (TIR)**: light is guided through the plate; most rays bounce internally until extracted.
- **Extraction**: micro‑features (printed dots / etch) and films convert guided rays into visible emission.
- **Diffusion**: spreads energy laterally and angularly; smooths interference and hides quantisation.
- **Prism/BEF films**: redistribute emission angles (often increasing forward brightness and producing directionally‑biased “slices” or “planes”).
- **Modal / interference behaviour**: opposing edge injections can produce stable “boxes”, moiré envelopes, and drifting lattices (especially under centre‑origin excitations).

Practical consequence: **small changes in phase and spatial frequency in LED space can look like large geometric changes in plate space**.

## Optical levers → code levers

Treat the LGP like an optical instrument with a small set of levers. Each lever has a direct mapping to our current control surface.

| Optical lever (what it does) | What you see | Primary code lever(s) | Where it lives |
|---|---|---|---|
| **Spatial frequency** (how many lobes/boxes) | box count, banding density | distance multiplier, quantised LUTs | per‑effect maths |
| **Edge phase relationship** (in‑phase vs anti‑phase vs quadrature) | standing vs travelling structures | phase offset, phase auto‑rotate | MotionEngine phase controller / per‑effect |
| **Edge balance** (which edge dominates) | depth bias, “front/back” illusion | strip weighting, edge gain | per‑effect |
| **Temporal modulation** (how the levers move over time) | breathing, tension, release | Speed, envelopes, beat sync | per‑effect / global speed |
| **Extraction softness** | crisp tiles vs mist | diffusion/blur amount | ColourEngine diffusion (post) |
| **Spectral separation** (controlled chromatic split) | “aberration” edges, coloured planes | fixed 2‑family palette mapping | palette system (no rainbow) |

### Encoder semantics (recommended)

These conventions make patterns interoperable across the library:

- **Speed**: temporal evolution (phase velocity, envelope speed, drift rate).
- **Intensity**: contrast/energy (interference depth, peak brightness, collision energy).
- **Saturation**: colour purity (useful for “mist vs crystal” storytelling).
- **Complexity**: number of layers / spatial frequency / particle count (with hard caps).
- **Variation**: mode selector (phase regime, symmetry mode, edge balance presets).

## Pattern taxonomy (what we can build, and what it costs)

Each category below is defined by: signature motif, best‑fit LGP optics, control mapping, and performance risk.

### 1) Standing structures (boxes, tiles, bands)
- **Signature**: stable bright/dark regions (often “boxes”) that drift subtly.
- **Best on**: strong extraction + prism films (crisp boundaries, planar depth).
- **Controls**:
  - Complexity → box count / lattice density
  - Intensity → contrast (dark nulls vs bright peaks)
  - Variation → phase regime (in‑phase / anti‑phase / slow quadrature drift)
- **Risk**: aliasing/moire at high spatial frequency; mitigate with diffusion and quantised LUTs.

### 2) Travelling collisions (two wave packets)
- **Signature**: two packets meet at centre; collision produces a transient “flash” or knot.
- **Best on**: any LGP; prism films make “impact planes”.
- **Controls**:
  - Speed → packet velocity
  - Intensity → collision energy
  - Variation → reflection model (elastic, inelastic, damped)
- **Risk**: strobing if collision flash is too sharp; enforce minimum pulse width and fade.

### 3) Moiré envelopes (slow curtains)
- **Signature**: slow macro‑movement emerging from faster micro‑waves.
- **Best on**: diffuse stacks (diffuser helps reveal the envelope cleanly).
- **Controls**:
  - Complexity → base spatial frequency
  - Variation → Δ frequency (moiré period)
  - Speed → envelope drift
- **Risk**: “busy” visuals when Δ too large; keep envelope < ~1 Hz for legibility.

### 4) Depth illusions (edge dominance as “z”)
- **Signature**: foreground/background swapping; objects appear “within” the plate.
- **Best on**: prism/BEF stacks (directional emission makes depth cues stronger).
- **Controls**:
  - Variation → edge dominance preset
  - Intensity → depth separation
  - Saturation → fog vs clarity
- **Risk**: colour muddiness if both edges saturate; prefer asymmetric mixing.

### 5) Spectral split (chromatic aberration‑style, without rainbow)
- **Signature**: controlled separation into two anchored colour families (e.g., burnt amber ↔ deep cyan) with a neutral centre.
- **Best on**: prism films (clean colour planes).
- **Controls**:
  - Complexity → separation strength
  - Speed → lens/phase drift
  - Saturation → purity vs milky blend
- **Risk**: accidental rainbow if hue is rotated; **do not** rotate hue globally.

## Palette grammar (no‑rainbows, high emotional range)

### Rule set
- Use **two colour families + one neutral** (optional highlight).
- Never sweep hue through the full wheel.
- If you need “variety”, vary **intensity**, **contrast**, **diffusion**, and **phase** first; only then shift between pre‑approved palettes.

### Palette families (examples)
- **Warm crystal**: burnt amber + ivory + near‑black.
- **Cold crystal**: deep cyan + pale teal + near‑black.
- **Abyssal**: navy + cyan‑white highlights.
- **Ember**: deep red‑brown + amber highlights (avoid orange→yellow→green progressions).
- **Violet glass**: deep violet + magenta accents + white highlights (no hue rotation).

## Design templates (copy‑pastable briefs)

### Template A: “Boxes from the centre” (standing mode)
- **Core idea**: choose a target box count per side; excite a standing structure by tying brightness to distance from centre with an integer spatial multiplier.
- **Minimum controls**:
  - Complexity = box count (2–10)
  - Intensity = contrast (soft→hard)
  - Speed = drift (0.05–0.5 Hz visual drift)
  - Variation = phase mode (0°, 90° slow drift, 180°)
- **Optical aim**: crisp boundaries with a slight diffusion to prevent harsh aliasing.

### Template B: “Curtains” (moiré envelope)
- **Core idea**: run two close spatial frequencies in opposition; let their beat envelope become the story.
- **Minimum controls**:
  - Complexity = base frequency
  - Variation = Δ frequency
  - Speed = envelope speed
- **Optical aim**: diffusion higher than standing boxes; make the envelope legible.

### Template C: “Impact at centre” (collision)
- **Core idea**: two packets approach from edges and collide at LEDs 79/80; the collision creates a controlled bloom (not a strobe).
- **Minimum controls**:
  - Speed = packet velocity
  - Intensity = impact energy
  - Saturation = bloom whiteness (lower saturation → more “light”)
- **Optical aim**: prism films turn the impact into a plane; keep the pulse width safe.

## Pattern spec mini‑template (use in PRs)

- **Name**:
- **Category**: (standing / travelling / moiré / depth / spectral split)
- **One‑sentence story**: what emotion or narrative beat it expresses
- **Optical levers used**:
- **Controls**: how each encoder maps
- **Centre origin compliance**: yes/no (must be yes)
- **No‑rainbows compliance**: yes/no (must be yes)
- **Performance notes**: worst‑case frame cost and memory strategy

## Next document

For turning these patterns into coherent shows (acts, pacing, emotional arcs), see `docs/effects/STORYTELLING_FRAMEWORK.md`.


