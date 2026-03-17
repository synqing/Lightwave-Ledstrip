---
abstract: "Design contracts for 5 oscilloscope exploration effects (0x130E-0x1312). Each owns ONE audio dimension with sparse injection (2-20% pixels/frame), crossmodal-compliant mapping, and 5-second verification tests. Based on 3-agent research synthesis."
---

# Oscilloscope Effect Design Contracts

## Principle

Every visual element must make a specific audio feature READABLE to the eye. K1 Waveform (0x1302) owns amplitude + harmony. These 5 effects each own ONE unclaimed dimension.

**The rule:** Stop writing all 80 pixels. Write ONLY the information-carrying pixels. Let the trail buffer + decay create contrast. The dark gaps between injection points ARE the legibility.

## Effect A: Spectral Envelope (0x130E)

- **Owns:** Frequency distribution
- **Contract:** "When bass hits, the centre glows. When treble hits, the edges glow."
- **Audio source:** `bands[0..7]` (8 octave energy bands)
- **Injection:** 8 anchor dots at fixed positions (pixels 0, 10, 20, 30, 40, 50, 60, 79). Brightness = band energy. 16px/frame (20%).
- **Flow:** Outward scroll (~60 px/s). Old readings scroll away, new appear at anchors.
- **Contrast:** Active anchors 150-255, inter-anchor gaps decay to 20-40. Ratio 5:1-10:1.
- **Verify:** Hum low note → centre anchors light. Hi-hat → edge anchors flicker. Music stops → fade to black ~3s.

## Effect B: Beat/Rhythm Pulse (0x130F — REDESIGNED)

- **Owns:** Beat/rhythm timing
- **Contract:** "When a beat lands, a bright ring launches outward from centre. Between beats, it contracts."
- **Audio source:** `musicalGrid.beat_phase01` + `beat_tick` + `beat_strength`. Falls back to RMS breathing when `tempo_confidence < 0.3`.
- **Injection:** 3px wavefront ring at expansion edge on beat. 2-6px/frame (2.5-7.5%).
- **Flow:** Asymmetric expansion/contraction. Fast snap outward on beat (~15% of beat period), slow contraction back (~85%).
- **Contrast:** Wavefront 220-255, background 10-30. Ratio 8:1-20:1. Highest contrast of all 5.
- **Verify:** Clap → ring races outward. Steady kick → rhythmic pumping. No beat → gentle RMS breathing.

## Effect C: Spectral Shape (0x1310 — REDESIGNED)

- **Owns:** Spectral shape (tonal vs noisy)
- **Contract:** "Pure tone → single sharp line. Dense chord → wide shimmering band."
- **Audio source:** `bins64[0..63]` → derived spectral flatness. `saliency.timbralNovelty` for mode transitions.
- **Injection:** Tonal: 1 gaussian blob at dominant frequency (4-6px). Noisy: scattered dots at active bins (8-15px). 5-15% of strip.
- **Flow:** Dominant frequency position drifts with pitch content. Timbral novelty triggers rendering mode transitions.
- **Contrast:** Focal blob/dots at 150-255, background decays to 15-40. Ratio 4:1-9:1.
- **Verify:** Hum single note → one bright blob. Strum chord → wide shimmer. White noise → scattered sparkle.

## Effect D: Dynamic Arc (0x1311 — REDESIGNED)

- **Owns:** Dynamic arc (building vs releasing energy)
- **Contract:** "Crescendo → strip progressively fills outward. Release → light retreats inward."
- **Audio source:** RMS trend (rolling slope over ~4s) + `saliency.dynamicNovelty`. `heavy_bands[0..7]` for fill texture.
- **Injection:** Fill level edge (2-4px wavefront at the fill boundary). Interior already lit from previous fills. Total new injection: 2-4px at the advancing/retreating edge.
- **Flow:** Fill expands outward during building, contracts inward during release. Temporal integrator with inertia — responds to trends over seconds, not frames.
- **Contrast:** Fill edge bright (200-255), filled interior moderate (80-150), unfilled exterior black. Three-level contrast.
- **Verify:** Play song from quiet intro to loud chorus → strip fills over 20-30s. Song ends → light retreats inward. Sudden silence → satisfying collapse.

## Effect E: Onset/Transient Mapper (0x1312 — REDESIGNED)

- **Owns:** Onset/transients (percussion hits)
- **Contract:** "Snare → flash near centre. Hi-hat → flash at edges. Ghost note → subtle flicker."
- **Audio source:** `snareTrigger` + `hihatTrigger` + `snareEnergy` + `hihatEnergy` + `fast_flux`.
- **Injection:** Per-onset flash at frequency-mapped position. Snare: 20px blob near centre. Hihat: 8px blob near edges. 2-6px/frame between onsets. Purely impulsive.
- **Flow:** No spatial flow. Flashes appear, decay in place (tau ~150ms). Ambient flux shimmer between onsets prevents full black.
- **Contrast:** Flash peak 220-255, background shimmer 5-15. Ratio 15:1-40:1. Sharpest transient contrast.
- **Verify:** Tap table → flash at centre. Tap keys → flash at edges. Drum loop → rhythmic flashing pattern matching the drums.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-18 | agent:synthesis | Created from 3 parallel research agents (Q1-Q2 gaps, Q3-Q4 contracts, Q5-Q7 mechanics) |
