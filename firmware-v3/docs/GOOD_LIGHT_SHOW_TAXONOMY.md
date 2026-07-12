---
abstract: "K1 Good Light Show Taxonomy. Defines how effects are judged as light shows rather than only as code: LGP fusion, colour quality, motion grammar, audio coupling, silence behaviour, VP stack risk, and promotion class."
---

# Good Light Show Taxonomy

**Status:** DRAFT - Captain-approved for documentation/tooling package on 2026-05-06. No firmware behaviour changes are made by this document.

**Authority anchors:**
- Project effect constraints define centre-origin, no-rainbow, no-heap, timing, latency, and language requirements.
- `firmware-v3/docs/effects-catalog/PATTERN_TAXONOMY.md:9-23` defines the current four-axis effect catalogue and centre-origin baseline.
- `firmware-v3/docs/EFFECTS_BEHAVIORAL_REFERENCE.md:11-54` documents live behavioural gates: colour correction bypass, tone mapping, silence gate, and palette caveats.
- `firmware-v3/docs/research/PHASE5_EFFECTS_RESEARCH_SYNTHESIS_2026-04-27.md:172-211` records the de-facto brand filter: visceral curiosity, liquid centre-origin fields, bounded palette arcs, and rejection of graph-like or fragmented visuals.
- `firmware-v3/docs/audit/phase_5_visual_sign_off_2026-04-28.md:15-22` states that static/trace evidence is not the same thing as hardware visual sign-off.

## Purpose

This taxonomy gives agents a shared language for judging whether an effect is a good K1 light show.

The key distinction:

- **Code quality** says the effect is safe and correct enough to run.
- **Light-show quality** says the effect is worth showing to a human.

Both are required for production promotion.

## The Top-Level Question

Would a layperson seeing the K1 at normal viewing distance understand it as a cohesive, saturated, musical light sculpture, without needing the engineer to explain the algorithm?

If not, the effect may still be useful as diagnostic, experimental, or research material, but it should not be promoted as production light-show material.

## Judgement Axes

Score each axis from 0 to 3.

| Score | Meaning |
|---:|---|
| 0 | Fails the product intent or violates a hard rule. |
| 1 | Technically present but weak, confusing, or visually degraded. |
| 2 | Works and is usable, but not yet distinctive. |
| 3 | Strong K1-native behaviour; obvious visual payoff. |

### A1 - LGP Fusion

Does the acrylic read as one optical object?

| Score | Criteria |
|---:|---|
| 0 | Looks like raw LED strips, isolated dots, bars, or electronics. |
| 1 | Has visible light but poor spatial coherence. |
| 2 | Reads as a field through the LGP most of the time. |
| 3 | Produces a cohesive liquid, glass, caustic, or holographic field. |

### A2 - Centre-Origin Legibility

Does the motion respect and reveal the centre pair?

| Score | Criteria |
|---:|---|
| 0 | Linear sweep or off-centre origin. |
| 1 | Centre-origin in code, but visually ambiguous. |
| 2 | Clear outward/inward centre relation. |
| 3 | Centre-origin is part of the visual signature. |

### A3 - Colour Quality

Does the effect produce saturated, intentional colour?

| Score | Criteria |
|---:|---|
| 0 | White haze, pastel wash, random white flashes, or rainbow sweep. |
| 1 | Colour exists but is muddy, overcorrected, or disconnected from intent. |
| 2 | Stable palette or chroma-driven colour with acceptable saturation. |
| 3 | Strong authored colour with clear hue, tone, temperature, and contrast. |

### A4 - Motion Grammar

Does motion look physically or musically meaningful?

| Score | Criteria |
|---:|---|
| 0 | Flicker, jitter, wagon-wheel aliasing, or dead static output. |
| 1 | Motion exists but feels arbitrary or mechanical. |
| 2 | Motion is smooth and readable. |
| 3 | Motion has layered temporal structure: bed, pulse, accent, memory. |

### A5 - Audio Coupling

Does audio change the right visual dimensions?

| Score | Criteria |
|---:|---|
| 0 | No meaningful reaction, or reaction is only global dimming. |
| 1 | Reacts, but as a level display, blink, or graph. |
| 2 | Audio drives energy, colour, or motion in a readable way. |
| 3 | Musical structure is visible without turning into a diagnostic display. |

### A6 - Silence Behaviour

What happens when music stops?

| Score | Criteria |
|---:|---|
| 0 | Random flashes, stuck garbage, or misleading hidden global behaviour. |
| 1 | Drops abruptly or keeps noisy activity that reads as a fault. |
| 2 | Fades, breathes, or idles according to effect class. |
| 3 | Silence behaviour is deliberate and improves perceived quality. |

### A7 - VP Stack Robustness

Does the effect survive the real output path?

| Score | Criteria |
|---:|---|
| 0 | Depends on accidental VP behaviour or corrupts output buffers. |
| 1 | Looks acceptable only under one hidden correction state. |
| 2 | VP interactions are known and controlled. |
| 3 | Authored output remains strong with minimal protective post-processing. |

### A8 - Product Distinctiveness

Would this earn a place in the K1?

| Score | Criteria |
|---:|---|
| 0 | Generic LED effect, test pattern, or obvious clone. |
| 1 | Pleasant but forgettable. |
| 2 | K1-appropriate and useful. |
| 3 | K1-native: makes the LGP, dual strip, or centre-origin topology feel necessary. |

## Promotion Thresholds

| Class | Minimum Evidence | Score Guidance |
|---|---|---|
| Hero | Full hardware visual sign-off | Mostly 3s; no 0s; no unresolved VP risk. |
| Production | Hardware pass plus trace/build evidence | No 0s; average at least 2.0. |
| Experimental | Build-safe and tagged | May contain 0s or 1s if isolated from production. |
| Diagnostic | Serves a test purpose | Product score can be low; diagnostic purpose must be explicit. |
| Parked | Build-safe but not worth current rescue | One or more 0s in core product axes. |
| Retired | Removed or hidden | Repeated failure or superseded by a stronger effect. |

## Positive Patterns

These are good K1-native signs:

- centre-emitting motion that becomes one LGP field;
- bounded chroma or palette movement, not full-wheel hue cycling;
- saturated colour with dark room for contrast;
- layered movement: ambient bed, musical structure, impact accent, memory tail;
- phase, parity, or interference that is visible without becoming a chart;
- top/bottom strip relationships that fuse into one optical phenomenon;
- silence behaviour that feels intentional, not broken.

## Rejection Patterns

Park or redesign the effect when it shows:

| Pattern | Why It Fails |
|---|---|
| White haze | Reduces contrast and hides authored colour. |
| Random vertical flashes | Reads as transport or buffer corruption. |
| Graph/oscilloscope look | Turns the K1 into a measurement display. |
| Metronome blink | Audio-reactive but not musical. |
| Fragmented agents | Several dots or objects do not fuse through the LGP. |
| Rainbow sweep | Violates the product and repo hard rule. |
| Hidden global reactivity | Makes ambient effects appear reactive by accident. |
| Over-tuned correction | The VP stack compensates for weak authored colour. |
| Manual-only interpretation | If the viewer needs the algorithm explained, the effect is not yet product-grade. |

## Captain Visual Verdict Form

Use this for hardware reviews:

| Field | Answer |
|---|---|
| Effect ID / name |  |
| Track/window or test input |  |
| First impression |  |
| Strongest visible feature |  |
| Worst visible defect |  |
| Does it read as one LGP object? | PASS / FAIL / DEGRADED |
| Is colour saturated and intentional? | PASS / FAIL / DEGRADED |
| Is motion meaningful? | PASS / FAIL / DEGRADED |
| Is audio coupling musical rather than metering? | PASS / FAIL / DEGRADED |
| Promotion class | Hero / Production / Experimental / Diagnostic / Parked / Retired |
| Required next action |  |

## Relationship To Existing Gates

- This taxonomy does not replace static hard constraints.
- This taxonomy does not replace VP validation.
- This taxonomy does not make K1_Testbed simulation a hardware pass.
- This taxonomy provides the language for deciding whether a technically safe effect is worth polishing, parking, or promoting.
