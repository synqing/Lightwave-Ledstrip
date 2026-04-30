# Audio Feature Surface v2 Contract

**Date:** 2026-04-27
**Status:** Foundation contract, implementation not yet authorised beyond policy/helper design
**Owner:** Audio pipeline -> visual pipeline contract

This document defines the next contract layer for audio-reactive effects. It is
not a semantic-field implementation plan. It prevents the current spectrum
surface from drifting into raw FFT consumption while preserving existing
transport compatibility.

## 1. Canonical Model

```text
internal substrates -> projection / normalisation / events -> one effect-facing surface
```

There is one canonical effect-facing control surface. It may be transported
through `ControlBusFrame` and exposed through `EffectContext::AudioContext`, but
effect authors should think in terms of named musical controls, not transport
fields.

## 2. Vocabulary

| Term | Definition | Examples |
|------|------------|----------|
| Raw substrate | Internal measurement data used by analysis code, not normal effect-authoring API. | `bins256`, FFT magnitudes, previous-frame spectra |
| Vector | Fixed-length homogeneous array with stable layout and meaning. | `musicalEnergy64`, `chroma12`, `bands8` |
| Scalar feature | Named normalised value with a field contract. | `airEnergy`, `brightness`, `hfFlux` |
| Event | Time-sensitive trigger carrying strength, confidence, age, and validity. | `onset`, `kick`, `snare`, `hat` |
| Envelope | Smoothed stateful scalar derived from events or features. | `cymbalSustain`, `bassEnvelope` |
| Surface | Complete effect-facing control contract composed of vectors, scalars, events, envelopes, metadata, and validity flags. | Audio Feature Surface v2 |

## 3. Raw Access Policy

`bins256` may remain physically present in `ControlBusFrame` for legacy,
diagnostic, STM, and research reasons. It is not normal effect-authoring API.

Allowed:

- audio analysis internals;
- STM and projection code;
- diagnostic visualisers;
- research builds;
- explicitly whitelisted legacy effects during migration.

Disallowed:

- new production effects directly scanning `bins256`;
- render-path feature extraction from `bins256`;
- default WebSocket streaming of expanded raw arrays;
- treating linear FFT bins as the canonical musical spectrum;
- using raw FFT bin indexes as musical note indexes.

New production effect code must use helper accessors and named semantic
controls. Existing raw consumers must be documented, migrated, or kept on an
explicit whitelist until replaced.

## 4. Build Modes

| Build mode | Contract |
|------------|----------|
| Production | `64` musical bins, `chroma[12]`, validated semantic fields, no new raw `bins256` effect access, no expanded telemetry by default. |
| Debug | Optional diagnostic visualisers and reduced-rate telemetry. Raw substrates may be displayed only as diagnostics. |
| Research | `96` / `128` experiments, projection timing instrumentation, fixture replay, and A/B tooling. Research surfaces do not become production API without promotion gates. |

## 5. Numeric Representation

Existing float arrays are not mass-converted in v2 foundation work.

Preferred representation for new compact fields:

- internal analysis: float where useful;
- new effect-facing scalar features: `uint16_t` Q15 where practical;
- new event fields: `uint16_t` Q15 strength/confidence plus age/flags;
- telemetry: compact and quantised by default;
- existing vectors: unchanged until a separate migration is approved.

Q15 range:

```text
0     = no activation
32768 = medium normalised activation
65535 = maximum normalised activation
```

## 6. Event Model

Minimal event shape for new v2 event fields:

```cpp
struct AudioEventQ15 {
    uint16_t strength;     // 0..65535
    uint16_t confidence;   // 0..65535
    uint16_t ageMs;        // time since last trigger
    uint16_t flags;        // active, valid, degraded, clipped-source, etc.
};
```

Events answer "something happened now". They are distinct from:

- energy, which says sound exists in a band;
- flux, which says energy changed;
- envelopes, which say a state is being held or decayed.

## 7. Field Contract Gate

No new production field is allowed without a written field contract.

Required contract items:

| Item | Requirement |
|------|-------------|
| Name | Stable public name. |
| Category | Vector, scalar feature, event, envelope, metadata, or validity flag. |
| Type and range | Exact type and normalised range. |
| Source | Raw substrate or existing semantic source. |
| Formula | Derivation or aggregation rule. |
| Normalisation | Floor, gain, compression, bandwidth correction, and silence handling. |
| Smoothing | Attack/release or event hold policy, dt-correct where temporal. |
| Latency class | Fast event, fast energy, medium spectral, or slow harmonic. |
| Update cadence | Chunk, hop, render, or debug-only cadence. |
| Failure behaviour | Silence, invalid input, clipping, backend absence. |
| Validity flags | How effects know whether the field is trustworthy. |
| Telemetry visibility | None, debug-only, reduced-rate, or production-safe. |
| Consumer | Named effect, helper, validator, or diagnostic visualiser. |
| Fixture test | Required fixture or replay scenario. |
| Promotion gate | Evidence needed before production exposure. |

No contract, no production field. No consumer or validator, no production
semantic field.

## 8. Latency Classes

| Class | Examples | Behaviour |
|-------|----------|-----------|
| Fast event | `kick`, `snare`, `hat`, `onset` | Minimal smoothing, single-hop trigger semantics. |
| Fast energy | bass, mid, high-frequency, air energy | Short attack, moderate release. |
| Medium spectral | brightness, tilt, centroid-like controls | Stable but responsive. |
| Slow harmonic | chroma, tonal confidence, harmonic density | Smoother; never twitch like a transient detector. |

Sub-8ms scheduling pressure applies hardest to events and fast envelopes.
Harmonic summaries may integrate over longer windows if their field contract
states the latency.

## 9. Tier 1 HF Field Contracts To Draft

These are contracts only. They do not authorise new production fields until the
Phase 1B timing, stability, and fixture gates have passed.

### `hfEnergy`

| Item | Contract |
|------|----------|
| Category | Scalar feature |
| Meaning | High-frequency content exists. |
| Allowed behaviour | May rise on hats, cymbals, sibilance, noise, bright dense music, and bright synth material. |
| Forbidden behaviour | Must not be treated as a hi-hat trigger by default. |
| Fixture gate | Positive on bright material; explicitly tolerated on sibilance/noise without implying `hatEvent`. |

### `hfFlux`

| Item | Contract |
|------|----------|
| Category | Scalar feature |
| Meaning | High-frequency content changed quickly. |
| Use | Motion, edge, sparkle, and fast spectral movement. |
| Forbidden behaviour | Must not be treated as a confirmed hat event by itself. |
| Fixture gate | Rises on sharp hat/cymbal attacks; does not continuously trigger on steady noise or cymbal wash. |

### `hatEvent`

| Item | Contract |
|------|----------|
| Category | Event |
| Shape | `AudioEventQ15` with strength, confidence, age, and flags. |
| Meaning | Short, transient, hat-like high-frequency event. |
| Required gates | HF flux, short attack, adaptive floor, HF-to-low/mid ratio, refractory/holdoff, validity/clip suppression. |
| Bias | Prefer missed weak hats over persistent false hats. |
| Fixture gate | Must not continuously fire on cymbal wash, spoken S/T/SH sibilance, noise, clipped music, compressed bright music, or dense bass. |

### `cymbalSustain`

| Item | Contract |
|------|----------|
| Category | Envelope |
| Meaning | Sustained noisy high-frequency decay. |
| Use | Shimmer trails, bloom, persistent upper texture. |
| Forbidden behaviour | Must not retrigger like closed hats. |
| Fixture gate | Holds and decays smoothly on crash/ride wash. |

### `airEnergy`

| Item | Contract |
|------|----------|
| Category | Scalar/envelope feature |
| Meaning | Smooth upper-air shimmer bed. |
| Use | Atmosphere, upper glow, fine sparkle. |
| Forbidden behaviour | Must not act as a transient trigger. |
| Fixture gate | Smooth on upper-air material; no single-hop trigger semantics. |

### `spectralBrightness`

| Item | Contract |
|------|----------|
| Category | Scalar feature |
| Meaning | Overall spectral tilt / upper-balance proxy. |
| Use | Colour and visual intensity shaping. |
| Naming | Uses `spectralBrightness` to avoid confusion with LED/render brightness. |

### `spectralBrightnessDelta`

| Item | Contract |
|------|----------|
| Category | Scalar feature |
| Meaning | Change in spectral brightness. |
| Use | Spectral movement, transitions, and controlled texture motion. |

## 10. Helper API Direction

Effects should consume named helpers and bounded vector operations, not raw
transport fields.

Foundation helper design targets:

```cpp
audio.airEnergy();
audio.hfFlux();
audio.hatEvent();
audio.cymbalSustain();
audio.brightness();
audio.musicalBin(i);
audio.musicalRange(lo, hi);
```

Helper requirements:

- allocation-free;
- inline/static where practical;
- no render-path heap use;
- no unbounded raw spectrum scanning;
- valid fallback when audio is unavailable;
- stable semantics across backends.

## 11. Telemetry Policy

Expanded arrays are never streamed by default.

Diagnostic telemetry must be:

- opt-in;
- rate-limited;
- AP-local only;
- disabled in production unless explicitly enabled;
- compact or quantised where practical.

## 12. Promotion Gates

Any new surface element must pass:

- field contract review;
- fixture validation;
- timing p99 and p999 gates;
- zero deadline-miss gate during burn-in;
- frame-size and copy-budget review;
- effect-level A/B where the output is more musical, not merely busier.

`96` musical bins are compile-time gated research until they beat:

```text
64 musical bins + semantic high-frequency controls + good normalisation
```

`128` musical bins remain research-only unless they beat `96` visually and pass
stricter timing, memory, and stability gates.
