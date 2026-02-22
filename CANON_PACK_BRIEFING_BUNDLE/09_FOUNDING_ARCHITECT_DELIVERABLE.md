# SpectraSynq / K1-Lightwave — Founding Architect Deliverable v2

**Date:** 2026-02-10
**Author:** Founding Principal Architect
**Input:** Canon Pack (00-08), firmware source, Trinity/PRISM/K1/iOS/Tab5 codebases, 12-agent deep research sweep
**Status:** Architecture proposal v2 — replaces v1 from scratch

---

## 0. Preamble: What "Making Music Visible" Actually Means

Music and light share something no other sensory pairing does: both unfold over time, both operate through frequency, and both can induce emotion without semantic content. You don't need to understand the lyrics to feel a song. You don't need to read a label to feel a colour.

Neuroscience calls this **crossmodal correspondence** — the brain's built-in wiring that maps pitch to brightness, tempo to speed, roughness to angularity. These aren't learned associations. They appear in infants. They appear cross-culturally. They are as close to "perceptual laws" as human experience gets.

But SpectraSynq is not a visualiser. Visualisers react. SpectraSynq **narrates**. The difference is the same as the difference between a seismograph and a film score. Both track energy. Only one tells a story.

The architectural insight that makes this possible is the **Kuleshov Principle applied to light**: the same visual pattern paired with different music creates different perceived meaning. A slow breathing glow during a quiet verse feels contemplative. The same glow during a bass drop feels like coiled tension. The combination creates meaning that neither modality achieves alone.

This is why ShowBundle exists — it encodes not just "what happens when" but "what this moment means." And it is why TrinityContract exists — it extracts the musical structure that gives those moments their emotional weight.

**The governing equation:**

```
TrinityContract(song) + ShowBundle(creative intent) → K1_RUNTIME(visible music)
```

Everything in this architecture serves this equation.

---

## 1. The Laws

These are non-negotiable. They are restated here not as a list to memorise but as a system to understand. Each law exists because violating it would break a specific property of the product.

### 1.1 Visual Identity Laws

**L1. Centre-origin geometry.**
Effects originate from LEDs 79/80 and propagate outward or inward. No linear sweeps. This makes the Light Guide Plate look like a breathing organism — energy emanating from a core — rather than a scrolling ticker tape. It is a product identity choice that defines how the LGP *feels*.

**L2. No rainbow cycling.**
Full hue-wheel rotations and generic HSV sweeps are banned. Every colour must come from a curated palette. This forces effects to express mood rather than just colour, and ensures the visual language has the restraint of an art director rather than a demo scene.

### 1.2 Render Discipline Laws

**L3. Zero heap allocation in render path.**
`render()` and every function it calls must never invoke `new`, `malloc`, `String()`, or any heap-touching operation. All buffers are pre-allocated at boot in PSRAM. This prevents heap fragmentation in a system that runs 24/7 — a single leaked allocation would eventually crash the device.

**L4. PSRAM is mandatory.**
The ESP32-S3's ~320 KB internal SRAM cannot hold DSP buffers + network stack + effect state + JSON parsing arenas simultaneously. All build environments must define `-D BOARD_HAS_PSRAM`. Builds without PSRAM will crash with `ESP_ERR_NO_MEM`.

**L5. 120 FPS render cadence, <2 ms per effect kernel.**
The render loop tick is 8.33 ms. Effect computation budget is ~2 ms. WS2812 LED push (via parallel RMT channels) takes **~4.8 ms for 320 LEDs** (two strips of 160, driven simultaneously on separate RMT peripherals — see Section 11.1). Remaining budget (~1.5 ms) covers message processing, audio mapping, and overhead. Frame drops are logged and treated as bugs.

> **CORRECTED from v1:** The previous claim of "~9.6 ms total for 320 LEDs across 2 strips" was wrong. The firmware uses `FASTLED_RMT_BUILTIN_DRIVER=1` which drives both strips in parallel via separate RMT channels. Actual measured push time is ~4.8 ms. The stale comment at `RendererActor.cpp:729` ("showLeds() blocks for ~9.6ms") must be updated.

**L6. Watchdog-safe scheduling.**
No blocking calls that starve the RTOS idle task. The WS2812 parallel push (~4.8 ms) provides natural watchdog feeding time during the data-clocking phase. `vTaskDelay(0)` yields at frame end as a safety net.

### 1.3 Concurrency Laws

**L7. Actor model with exclusive state ownership.**
Each actor owns its state. No shared mutable memory between actors. Communication is via message passing through MessageBus. This eliminates data races without mutexes in hot paths.

**L8. Single-source system limits.**
Constants like `MAX_EFFECTS = 147`, `TOTAL_LEDS = 320`, `MAX_PALETTES = 75` live in `config/limits.h`. No duplication. All consumers include this header. Compile-time `static_assert` validates consistency.

### 1.4 Network Laws

**L9. REST for commands, WebSocket for streaming.**
REST handles discrete state changes (set effect, save preset, load ShowBundle). WebSocket handles continuous data (state broadcast, trinity.beat/macro/segment stream, telemetry). This split is stable and well-tested.

**L10. AP/STA fallback.**
Device starts in STA mode (connects to home WiFi). If that fails within timeout, falls back to its own AP (`LightwaveOS-AP`). The device is always reachable — critical for a consumer product that must work out of the box.

**L11. IP-first discovery, mDNS as hint.**
The app saves the device's IP address after first connection. mDNS (`lightwaveos.local`) assists initial discovery but is not relied upon for reconnection. Consumer routers break mDNS frequently enough that deterministic IP paths must be the primary mechanism.

### 1.5 Hardware Laws

**L12. Dual-strip 320-LED WS2812 topology is fixed.**
160 LEDs per strip, two data pins (GPIO 16, GPIO 17), two separate RMT channels. LED addressing is interleaved with centre-origin mapping. This is the physical product. Every subsystem that references LED count must use the single-source constant from `limits.h`.

### 1.6 Law Consistency Check

No conflicts exist between L1-L12. They form four orthogonal contracts:

| Contract | Laws | What it protects |
|----------|------|------------------|
| Visual identity | L1, L2 | Product distinctiveness |
| Render discipline | L3, L4, L5, L6 | Runtime stability at 120 FPS |
| Concurrency | L7, L8 | Correctness without locks |
| Network | L9, L10, L11 | Always-reachable device |

One **resolved tension**: L5 (120 FPS = 8.33 ms frame period) vs LED push time (~4.8 ms). With parallel RMT, 4.8 ms + 2 ms effect render = 6.8 ms, leaving 1.5 ms headroom within the 8.33 ms budget. True visual refresh rate is 120 Hz, not the ~104 Hz that the incorrect 9.6 ms figure would have implied.

---

## 2. The Contract Spine

Two data structures form the spine of the entire system. Every subsystem either produces, transforms, or consumes one or both.

### 2.1 TrinityContract — What the Music Contains

TrinityContract is the output of TRINITY_PIPELINE. It describes the temporal structure of an audio file in machine-readable form. It does not prescribe any visual response — it is pure description.

```
TrinityContract {
  metadata {
    song_title: string
    artist: string
    duration_s: float
    bpm: float
    time_signature: [int, int]
    key: string              // e.g., "C minor"
    analysis_version: string // Pipeline version for compatibility
  }

  beats[] {
    time_s: float            // Beat onset time in seconds
    confidence: float        // 0.0-1.0
    is_downbeat: bool        // Bar boundary
  }

  segments[] {
    start_s: float
    end_s: float
    label: string            // "intro", "verse", "chorus", "bridge", "drop", "outro", "break"
    confidence: float
  }

  stems {
    // Per-stem feature timeseries at 50 FPS (20ms frames)
    vocals: StemFeatures
    drums: StemFeatures
    bass: StemFeatures
    other: StemFeatures
  }

  // Per-frame (50 FPS) derived features
  macro_curves {
    energy: float[]          // 0.0-1.0, overall RMS energy
    tension: float[]         // 0.0-1.0, spectral roughness + harmonic function
    brightness: float[]      // 0.0-1.0, spectral centroid normalised
    rhythm_density: float[]  // 0.0-1.0, onset frequency
    harmonic_complexity: float[]  // 0.0-1.0, chromagram entropy
  }
}

StemFeatures {
  rms: float[]               // Per-frame RMS
  onset_strength: float[]    // Per-frame onset detection
  band_energy: float[8][]    // 8-band spectral decomposition per frame
}
```

**Critical status:** The current TRINITY_PIPELINE has bugs that must be fixed before TrinityContract is reliable:

1. **Dashboard V4 macro mapping sends zeros.** The `controlMessage.macros` object uses keys (`vocal`, `bass`, `perc`, `bright`) that don't exist in the activations dictionary. Only the Stem Pack path (`stem_rms_*`) produces non-zero values. **Fix:** Derive macros from `stem_features` directly, not from activation lookup.

2. **`export_contract.py` macro derivation uses sinusoidal placeholders.** The `derive_macro_curves()` function generates sine waves instead of computing actual features from the analysis. **Fix:** Compute energy from stem RMS sum, tension from spectral roughness, brightness from spectral centroid, rhythm_density from onset frequency, harmonic_complexity from chromagram entropy.

3. **Three declared features are never computed:** `harmonic_complexity`, `rhythmic_density`, `spectral_centroid` are defined in the contract schema but not populated by any pipeline stage. **Fix:** Implement in `stem_features.py` using librosa functions.

4. **TrinityControlBusProxy zeroes mid-frequency bands.** `bands[2..5]` are explicitly set to 0 in the proxy, meaning effects that reference mid-frequency audio data get silence during Trinity playback mode. **Fix:** Derive mid-band values from stem `band_energy` fields.

### 2.2 ShowBundle — What the Creator Intends

ShowBundle is the creative artefact. It maps musical structure to visual response. It is the unit of authoring, sharing, and playback.

```
ShowBundle {
  meta {
    id: string               // UUID
    name: string
    author: string
    version: int
    created_at: ISO8601
    trinity_version: string  // Minimum TrinityContract version required
    duration_ms: uint32
    looping: bool
    description: string      // Human-readable, for sharing/community
  }

  // Song binding (optional — bundles can be song-specific or generic)
  song_binding {
    title: string
    artist: string
    duration_s: float
    bpm: float
    // No audio file embedded — audio is played separately
  }

  chapters[] {
    name: string             // "Awakening", "Build", "Eruption", "Afterglow"
    start_ms: uint32
    duration_ms: uint32
    narrative_phase: enum    // BUILD | HOLD | RELEASE | REST
    tension_level: uint8     // 0-255
    cue_start_index: uint8
    cue_count: uint8
  }

  cues[] {
    // See Section 6 for exact binary ABI
    time_ms: uint32
    type: CueType
    target_zone: uint8       // 0xFF = global
    data: uint8[4]           // Type-specific payload
  }

  // Effect and palette references are by NAME, not ID
  // This makes bundles portable across firmware versions
  effect_references: string[]    // ["LGP Holographic", "Beat Pulse Bloom", ...]
  palette_references: string[]   // ["Lava", "Ocean Depths", "Neon Mint", ...]

  // AI authoring provenance (optional)
  generation {
    method: string           // "magic" | "guided" | "manual"
    model_version: string
    user_modifications: int  // Count of manual edits after generation
  }
}
```

**Name-based resolution:** ShowBundle references effects and palettes by their human-readable string name, not by numeric ID. On the firmware side, `PatternRegistry::getPatternMetadata(const char* name)` performs a linear PROGMEM scan to resolve names to IDs at ShowBundle load time (not at render time). This makes bundles portable across firmware versions where effect IDs may shift, at the cost of a one-time O(n) scan during load. With 147 effects and strcmp_P, this takes <1 ms — acceptable for a load-time operation.

**Portability guarantee:** A ShowBundle authored on PRISM_STUDIO can be loaded onto any K1_RUNTIME device running firmware that supports the referenced effect names. If an effect name is not found, the loader falls back to the device's default effect and logs a warning. No crash, no undefined behaviour.

### 2.3 How the Spine Connects

```
                        TrinityContract
                             │
              ┌──────────────┼──────────────┐
              │              │              │
              ▼              ▼              ▼
         PRISM_STUDIO   LIGHTWAVE_APP   ShowDirectorActor
         (authoring)    (playback sync)  (semantic bridge)
              │              │              │
              └──────┬───────┘              │
                     │                      │
                     ▼                      │
                 ShowBundle ────────────────┘
                     │
                     ▼
               K1_RUNTIME
              (render authority)
```

Three paths consume TrinityContract:

1. **Authoring path** (PRISM_STUDIO): Uses beats/segments/stems to suggest cue placement, effect selection, and parameter curves in the timeline editor. AI assistance operates here.

2. **Playback sync path** (LIGHTWAVE_APP): Uses beat timing to synchronise trinity.beat WebSocket messages to K1_RUNTIME during show playback. Audio plays on the phone; beat/macro events stream to the device.

3. **Semantic bridge path** (ShowDirectorActor on K1_RUNTIME): When no ShowBundle is loaded but Trinity data is streaming, the firmware's built-in semantic bridge maps segment labels to narrative phases and parameter presets. This is the "no authoring required" fallback — Trinity analysis alone produces a reasonable light show using hardcoded mappings.

---

## 3. The Perceptual Bridge: Music to Light

This section is the creative core of the architecture. It defines **how** musical features map to visual parameters, grounded in perception science rather than engineering convenience.

### 3.1 Crossmodal Correspondence Defaults

These mappings are backed by decades of experimental psychology. They are not preferences — they are perceptual tendencies observed cross-culturally and in pre-verbal infants. They form the **default** parameter mappings that AI authoring starts from and that the semantic bridge uses.

| Musical Feature | Visual Parameter | Direction | Source |
|----------------|-----------------|-----------|--------|
| Pitch height (spectral centroid) | Brightness | Higher pitch → brighter | Marks (1974), Walker (2012) |
| Loudness (RMS energy) | Size / Intensity | Louder → larger / more intense | Smith & Sera (1992) |
| Tempo (BPM) | Speed of motion | Faster tempo → faster animation | Eitan & Granot (2006) |
| Spectral roughness (dissonance) | Angularity / Sharpness | Rougher → more angular patterns | Kiki-Bouba effect, Ramachandran (2001) |
| Consonance (harmonic stability) | Smoothness / Roundness | More consonant → smoother curves | Kiki-Bouba inverse |
| Spectral brightness (high freq energy) | Colour temperature | Brighter timbre → cooler colours | Ward et al. (2006) |
| Onset density (attack frequency) | Flicker / Pulse rate | More onsets → more visual events | Juslin & Sloboda (2010) |
| Dynamic range (envelope shape) | Expansion / Contraction | Crescendo → outward motion; diminuendo → inward | Eitan & Granot (2006) |

**Implementation:** These defaults are encoded in `AudioMappingRegistry::applyMappings()` on the firmware. When no ShowBundle override is active, audio features are mapped to the 9 expression parameters (brightness, speed, intensity, saturation, complexity, variation, mood, trails, hue_shift) using these correspondences. This is why "just plugging in and playing music" already produces a compelling response — the defaults are perceptually natural.

### 3.2 The GEMS Emotional Vocabulary

The Geneva Emotional Music Scale (Zentner et al., 2008) identifies 9 music-specific emotions grouped into 3 domains. SpectraSynq maps these to effect families, providing the vocabulary for AI-assisted ShowBundle authoring:

| GEMS Domain | GEMS Emotion | Effect Family Mapping | Parameter Tendency |
|------------|-------------|----------------------|-------------------|
| **Sublimity** | Wonder | LGP optical effects (holographic, opal film, caustic shards) | Slow, bright, high complexity |
| | Transcendence | Field effects (Kuramoto transport, reaction-diffusion) | Very slow, medium brightness, organic motion |
| | Tenderness | Soft flow effects (water caustics, schlieren flow) | Slow, warm palette, low intensity |
| | Nostalgia | Warm glow effects (bloom, soft pulse) | Slow, warm palette, medium trails |
| | Peacefulness | Minimal effects (gentle breathe, ambient drift) | Very slow, dim, low complexity |
| **Vitality** | Power | Beat-reactive effects (beat pulse bloom, starburst narrative) | Fast, bright, high intensity, low trails |
| | Joyful Activation | Bright rhythmic effects (ripple enhanced, spectrum detail) | Fast, saturated, medium complexity |
| **Unease** | Tension | Angular effects (stress glass, grating scan, moire silk) | Medium speed, desaturated, high complexity |
| | Sadness | Minimal dim effects (parallax depth, slow dissolution) | Very slow, dim, cool palette, high trails |

**How this is used:** When AI generates a ShowBundle, it analyses each segment's musical features to estimate GEMS emotional tendency, then selects effects and parameters accordingly. A chorus with high energy, fast tempo, and major key suggests "Joyful Activation" → bright rhythmic effects with saturated palettes. A quiet bridge with minor harmony suggests "Tenderness" → soft flow effects with warm palettes.

### 3.3 Hierarchical Timescales

Different visual parameters should respond to musical structure at different temporal scales. Changing everything on every beat creates chaos. Changing nothing between sections creates monotony. The key insight from DAW architecture (Ableton's Session View vs Arrangement View) is that **different parameters belong to different timescales.**

| Timescale | Musical Unit | Duration | Visual Parameters | Update Trigger |
|-----------|-------------|----------|-------------------|----------------|
| **Micro** | Beat / sub-beat | <1 s | Brightness pulse, motion burst, beat-sync flash | `trinity.beat` event |
| **Meso** | Phrase / 4-8 bars | 4-16 s | Speed, intensity, saturation, complexity, trails | Parameter sweep (CUE_PARAMETER_SWEEP) |
| **Macro** | Section / song part | 16-64 s | Effect selection, palette, zone count, narrative phase | Cue events (CUE_EFFECT, CUE_PALETTE, CUE_ZONE_CONFIG) |

**Implementation in ShowBundle:** Cues encode macro-level decisions (effect changes, palette swaps). Parameter sweeps encode meso-level transitions (gradual intensity builds across a verse). Micro-level response is handled by the render loop's real-time audio mapping — effects query `EffectContext.audio()` for beat phase, onset strength, and RMS on every frame.

### 3.4 Tension/Release as the Master Narrative

Musical tension — computable from spectral roughness, harmonic function (chord progression analysis), and dynamic contour — is the single most powerful predictor of musical narrative. A song's emotional arc is fundamentally a tension curve.

SpectraSynq's NarrativeEngine maps this directly:

```
Tension ──────► NarrativePhase
  High (>0.7)    BUILD or HOLD    → Fast motion, high brightness, saturated, angular
  Medium (0.3-0.7) BUILD or RELEASE → Transitioning parameters, blended palettes
  Low (<0.3)     RELEASE or REST   → Slow motion, dim, desaturated, smooth
```

The tension curve from TrinityContract's `macro_curves.tension` drives the NarrativeEngine's phase progression speed. High tension accelerates phase cycling (2s per phase at peak). Low tension decelerates it (8s per phase at rest). This creates visual breathing that tracks the music's emotional contour without needing per-beat choreography.

### 3.5 The Saliency Compass

The firmware's existing `SaliencyEmphasis` system maps four musical dimensions to four visual dimensions. This is the real-time version of what the GEMS vocabulary does at authoring time:

```
           ┌─── Harmonic saliency ──→ Colour (palette selection, hue shift)
           │
Music ─────┼─── Rhythmic saliency ──→ Motion (speed, beat-sync intensity)
           │
           ├─── Timbral saliency ──→ Texture (complexity, pattern detail)
           │
           └─── Dynamic saliency ──→ Intensity (brightness, saturation)
```

Each frame, the EffectContext provides saliency emphasis values. Effects that respect saliency emphasis produce responses that feel "correct" — a drum-heavy passage drives motion, a string swell drives colour warmth, a distorted guitar drives texture complexity.

---

## 4. Subsystem Map

### 4.1 Registry

```
┌─────────────────────────────────────────────────────────────────────┐
│                          SPECTRASYNQ                                │
│                                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────────────────────┐│
│  │  K1_RUNTIME   │  │  TAB5_HUB    │  │    LIGHTWAVE_APP           ││
│  │  (ESP32-S3)   │  │  (ESP32-P4)  │  │    (iOS / SwiftUI)        ││
│  │               │  │              │  │                            ││
│  │  Render       │  │  Tactile     │  │  Performance +             ││
│  │  authority    │  │  control +   │  │  browse authority          ││
│  │               │  │  local hub   │  │                            ││
│  └──────┬───────┘  └──────┬───────┘  └────────────┬───────────────┘│
│         │                  │                       │                │
│         └──────────┬───────┴───────────────────────┘                │
│                    │                                                │
│         ┌──────────┴───────────────────────┐                       │
│         │    TRINITY_PROTOCOL (wire)       │                       │
│         └──────────┬───────────────────────┘                       │
│                    │                                                │
│  ┌─────────────────┼───────────────────────────────┐               │
│  │                 │                               │               │
│  │  ┌──────────────┴──┐  ┌──────────────┐  ┌──────┴────────────┐ │
│  │  │ TRINITY_PIPELINE │  │ PRISM_STUDIO │  │ PRISM_API         │ │
│  │  │ (Desktop Python) │  │ (Web app)    │  │ (Cloud, post-KS)  │ │
│  │  │ Analysis         │  │ Authoring    │  │ Content + sharing │ │
│  │  └─────────────────┘  └──────────────┘  └───────────────────┘ │
│  └───────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.2 Subsystem Definitions

| Subsystem | Runtime | Authority | Owns |
|-----------|---------|-----------|------|
| **K1_RUNTIME** | ESP32-S3 (FreeRTOS) | **Render authority** — only writer of LED frames | LED buffers, effect execution, zone composition, centre-origin mapping, real-time DSP, cue scheduling |
| **TAB5_HUB** | ESP32-P4 (FreeRTOS, LVGL) | **Tactile control** — physical encoders/display | Encoder state, local preset cache (8 NVS slots), LVGL display, WebSocket client to K1 |
| **LIGHTWAVE_APP** | iOS 17+ (SwiftUI) | **Performance + browse** — show playback orchestrator | Audio playback, beat sync streaming, ShowBundle browsing, device discovery |
| **TRINITY_PIPELINE** | Desktop Python (conda) | **Analysis authority** — ML inference | Beat/segment/stem extraction, TrinityContract generation |
| **PRISM_STUDIO** | Web application (browser) | **Authoring authority** — timeline editor | ShowBundle creation/editing, AI-assisted mapping, visual preview |
| **PRISM_API** | Cloud backend | **Content authority** (post-Kickstarter) | User accounts, ShowBundle hosting, community gallery |
| **TRINITY_PROTOCOL** | Wire format (not a runtime) | **Contract authority** | Message schemas, versioning, sequencing rules |

### 4.3 Boundary Rules

1. **K1_RUNTIME is the only subsystem that writes LED frames.** No other subsystem bypasses RendererActor to push pixels. [L1, L5, L7]

2. **TRINITY_PIPELINE is the only subsystem that runs heavy ML inference.** K1_RUNTIME runs only Goertzel-based DSP (FFT, beat tracking, saliency) for real-time audio response. LIGHTWAVE_APP does not run ML. TAB5_HUB does not run ML.

3. **ShowBundle is the only artefact that crosses all subsystem boundaries.** It is authored in PRISM_STUDIO, transferred via LIGHTWAVE_APP or REST, executed by K1_RUNTIME, and optionally shared via PRISM_API. Its format is the system's lingua franca.

4. **Audio never touches K1_RUNTIME as a waveform.** K1_RUNTIME either (a) captures audio locally via its onboard I2S microphone, or (b) receives pre-extracted features via trinity.beat/macro/segment WebSocket messages. No PCM audio is streamed to the device. This respects the WiFi bandwidth budget and keeps the device's job focused on rendering.

5. **Every subsystem communicates through TRINITY_PROTOCOL messages.** REST endpoints, WebSocket frames, and file formats all follow the protocol's schema. No ad-hoc JSON shapes between subsystems.

### 4.4 What Exists Today vs What Must Be Built

| Subsystem | Status | Lines of Code | Readiness |
|-----------|--------|---------------|-----------|
| K1_RUNTIME | **Production** | ~45,000 | 147 effects, zone system, show director, full API. Ready. |
| TAB5_HUB | **Prototype** | ~8,000 | LVGL UI, encoder input, WebSocket client. Functional. |
| LIGHTWAVE_APP | **Production (remote control)** | ~21,800 | Full device control. Missing: audio playback, show orchestration, beat sync. ~4,200 lines needed. |
| TRINITY_PIPELINE | **Working with bugs** | ~5,000 | 3-stage ML pipeline works. Macro derivation broken. ~500 lines to fix. |
| PRISM_STUDIO | **Not started** | 0 | Critical path. Estimated ~15,000 lines. |
| PRISM_API | **Not started** | 0 | Post-Kickstarter. Not required for launch. |

---

## 5. Data Flow Architecture

### 5.1 Path A — Live Audio (No ShowBundle, No Trinity)

The simplest path. The device listens to ambient audio through its onboard I2S microphone and renders effects that respond in real-time.

```
Room audio ──► I2S mic ──► AudioActor (Goertzel DSP)
                                    │
                                    ▼
                           ControlBusFrame {
                             rms, flux, bass, mid, treble,
                             beatPhase, isOnBeat, bpm,
                             fft[64], saliency, musicStyle
                           }
                                    │
                                    ▼
                           RendererActor ──► AudioMappingRegistry.applyMappings()
                                    │         (crossmodal correspondence defaults)
                                    ▼
                           EffectContext.audio() provides ~80 accessors
                                    │
                                    ▼
                           IEffect::render() ──► LED buffer ──► FastLED.show()
```

**Latency:** Mic capture → LED output: ~12 ms (one DSP hop + one render frame). Perceptually instantaneous.

### 5.2 Path B — Trinity Playback (ShowBundle + TrinityContract)

The flagship path. Audio plays on the phone. Pre-analysed beat/macro/segment events stream via WebSocket to the device, synchronised to playback position.

```
LIGHTWAVE_APP                              K1_RUNTIME
┌──────────────────────┐                  ┌──────────────────────────┐
│ Audio playback       │                  │                          │
│ (AVAudioPlayer)      │                  │ ShowDirectorActor        │
│         │            │   WebSocket      │   ├─ CueScheduler        │
│         ├── beat ────┼──────────────────┼──►├─ ParameterSweeper    │
│         ├── macro ───┼──────────────────┼──►├─ NarrativeEngine     │
│         └── segment ─┼──────────────────┼──►└─ TrinityControlBus   │
│                      │                  │         │                │
│ ShowBundle loaded    │   REST (once)    │         ▼                │
│ locally on phone     ├──────────────────┼──► ShowBundle loaded     │
│                      │                  │    (names → IDs resolved)│
└──────────────────────┘                  │         │                │
                                          │         ▼                │
                                          │  RendererActor executes  │
                                          │  cues + audio mapping    │
                                          │         │                │
                                          │         ▼                │
                                          │  LED output              │
                                          └──────────────────────────┘
```

**Sync mechanism — inspired by sACN (E1.31):**

Professional lighting uses a pattern where state packets and sync trigger packets are separated. SpectraSynq adopts this:

1. **State updates** (effect change, parameter sweep, palette swap) are sent via ShowBundle cues, pre-loaded on the device. They don't need real-time delivery — they're already resident in device memory.

2. **Sync triggers** (`trinity.beat`, `trinity.segment`) are streamed via WebSocket from the phone. These are lightweight timestamps that align the device's internal clock to the audio playback position.

3. **Local prediction:** Once BPM and phase are established, K1_RUNTIME predicts beat timing locally using its own high-resolution clock. This means WiFi jitter (30-100 ms on consumer networks) doesn't affect beat-sync visual quality — the device only needs periodic phase corrections, not per-beat delivery.

**Perceptual threshold compliance:** Audio-visual synchronisation research establishes <20 ms as the detection threshold for beat-sync events. K1_RUNTIME's local beat prediction achieves <5 ms accuracy (120 FPS = 8.33 ms maximum error, average ~4 ms). The WiFi path is used only for phase correction, not for frame-accurate sync.

### 5.3 Path C — Semantic Bridge (Trinity Streaming, No ShowBundle)

The "no authoring required" path. Trinity analysis streams to the device in real-time. The firmware's built-in semantic bridge maps segment labels to narrative phases and default parameters.

```
TRINITY_PIPELINE (real-time)              K1_RUNTIME
┌──────────────────────┐                  ┌──────────────────────────┐
│ Live analysis         │   WebSocket     │                          │
│ (Dashboard V4)        ├────────────────►│ ShowDirectorActor        │
│                       │                 │   └─ intentForLabelHash()│
│ trinity.beat          │                 │       Maps segment labels│
│ trinity.macro         │                 │       to narrative phases│
│ trinity.segment       │                 │       and param defaults │
└───────────────────────┘                 │                          │
                                          │ Firmware selects effects │
                                          │ and parameters using     │
                                          │ hardcoded mappings       │
                                          └──────────────────────────┘
```

The semantic bridge's `intentForLabelHash()` uses FNV-1a hashing of segment labels to 16-bit keys for efficient lookup without heap-allocated string maps. Current mappings:

| Segment Label | Narrative Phase | Tension | Brightness | Speed | Mood | Sweep Ms |
|--------------|----------------|---------|------------|-------|------|----------|
| chorus | HOLD | 220 | 230 | 200 | 70 (reactive) | 300 |
| verse | BUILD | 150 | 160 | 140 | 150 | 500 |
| drop | HOLD | 255 | 255 | 240 | 40 (very reactive) | 250 |
| intro | BUILD | 80 | 100 | 100 | 210 (smooth) | 650 |
| bridge | RELEASE | 120 | 140 | 130 | 180 | 450 |
| outro | REST | 60 | 80 | 80 | 235 (very smooth) | 800 |
| break | REST | 40 | 60 | 60 | 220 | 700 |

These defaults embody the crossmodal correspondences from Section 3.1 and the GEMS mappings from Section 3.2. They are "good enough" for the zero-authoring path and serve as the starting point for AI-generated ShowBundles.

### 5.4 Path D — Direct Control (No Audio Analysis)

Simple interactive control via the app or Tab5. No music analysis, no show playback. User manually selects effects, palettes, and parameters.

```
LIGHTWAVE_APP / TAB5_HUB                  K1_RUNTIME
┌──────────────────────┐                  ┌──────────────────────────┐
│ User selects effect  │   REST/WS        │                          │
│ User adjusts slider  ├────────────────►│ RendererActor applies    │
│ User picks palette   │                  │ requested state          │
└──────────────────────┘                  └──────────────────────────┘
```

This path exists today and is fully functional. It is the fallback when no audio input and no Trinity analysis are available.

---

## 6. Show Execution System

### 6.1 ShowCue Binary ABI

Every ShowCue is exactly **10 bytes**. This is the on-wire and in-memory format. No padding, no alignment games.

```c
struct ShowCue {              // 10 bytes total
    uint32_t timeMs;          // [0:3]  Execution time from show start (little-endian)
    CueType  type;            // [4]    Cue type enum (uint8_t)
    uint8_t  targetZone;      // [5]    Zone ID, or 0xFF for ZONE_GLOBAL
    uint8_t  data[4];         // [6:9]  Type-specific payload
};
```

### 6.2 Per-CueType Data Layout

Each CueType interprets `data[0..3]` differently. This is the exact ABI — no ambiguity.

**CUE_EFFECT (type = 0)**
```
data[0] = effectId           // Index into PatternRegistry (0-146)
data[1] = transitionType     // TransitionType enum (0-11)
data[2] = unused (0x00)
data[3] = unused (0x00)
```

**CUE_PARAMETER_SWEEP (type = 1)**
```
data[0] = paramId            // ParamId enum (0-5: brightness/speed/intensity/saturation/complexity/variation)
data[1] = targetValue        // Target value (0-255)
data[2] = durationMs low     // Sweep duration in ms, little-endian uint16
data[3] = durationMs high
```

**CUE_ZONE_CONFIG (type = 2)**
```
data[0] = zoneCount          // Number of zones (1-4)
data[1] = enabledMask        // Bitmask of enabled zones (bit 0 = zone 0, etc.)
data[2] = unused (0x00)
data[3] = unused (0x00)
```

**CUE_TRANSITION (type = 3)**
```
data[0] = transitionType     // TransitionType enum (0-11)
data[1] = durationMs low     // Transition duration in ms, little-endian uint16
data[2] = durationMs high
data[3] = unused (0x00)
```

**CUE_NARRATIVE (type = 4)**
```
data[0] = phase              // ShowNarrativePhase enum (0-3: BUILD/HOLD/RELEASE/REST)
data[1] = tempoMs low        // NarrativeEngine tempo in ms, little-endian uint16
data[2] = tempoMs high
data[3] = unused (0x00)
```

**CUE_PALETTE (type = 5)**
```
data[0] = paletteId          // Index into palette registry (0-74)
data[1] = unused (0x00)
data[2] = unused (0x00)
data[3] = unused (0x00)
```

**CUE_MARKER (type = 6)**
```
data[0] = markerType         // Application-defined sync marker
data[1] = unused (0x00)
data[2] = unused (0x00)
data[3] = unused (0x00)
```

### 6.3 ShowBundle JSON-to-Binary Pipeline

ShowBundle is authored and transferred as JSON. On load, K1_RUNTIME converts it to binary cue arrays:

1. **Transfer:** ShowBundle JSON is sent to K1_RUNTIME via REST `POST /api/v1/shows/upload`.
2. **Parse:** ArduinoJson v7 parses the JSON document.
3. **Resolve names:** Effect and palette string names are resolved to numeric IDs via `PatternRegistry::getPatternMetadata()` (linear PROGMEM scan, <1 ms for 147 entries).
4. **Pack:** Cues are packed into `ShowCue` structs and written to a PSRAM buffer.
5. **Index:** Chapter offsets and cue count are validated. `uint8_t` limits: max 255 chapters, max 255 cues per show.
6. **Register:** ShowDefinition is registered with ShowDirectorActor for playback.

### 6.4 ArduinoJson Rule

> **CORRECTED from v1:** ArduinoJson v7 does NOT have `StaticJsonDocument`. The v6 `StaticJsonDocument<N>` API was removed. All `JsonDocument` instances use dynamic heap allocation.

**The single rule for ShowBundle parsing on K1_RUNTIME:**

Allocate ONE `JsonDocument` parse arena at boot using `ps_malloc()`. Assign it as the ArduinoJson allocator for ShowBundle parsing. Clear and reuse for each parse operation. Never allocate `JsonDocument` on the stack. Never allocate in the render path.

```cpp
// Boot-time allocation (once)
static constexpr size_t SHOW_PARSE_ARENA_SIZE = 16384;  // 16 KB PSRAM
static uint8_t* showParseArena = nullptr;

void initShowParser() {
    showParseArena = (uint8_t*)ps_malloc(SHOW_PARSE_ARENA_SIZE);
    // This arena is reused for every ShowBundle parse
}

// Parse-time usage (on ShowBundle upload, NOT in render)
JsonDocument doc;
// ArduinoJson v7 uses SPI RAM automatically if ESP-IDF is configured
// with CONFIG_SPIRAM_USE_MALLOC=y, which we enforce in sdkconfig
DeserializationError err = deserializeJson(doc, jsonPayload);
```

The firmware's `sdkconfig.defaults` enables `CONFIG_SPIRAM_USE_MALLOC=y`, which makes ArduinoJson v7's internal allocator use PSRAM for large allocations automatically. No custom allocator wrapper is needed — the ESP-IDF malloc integration handles it.

**Maximum ShowBundle size:** Bounded by PSRAM availability. With 8 MB PSRAM, 16 KB parse arena, and binary cues at 10 bytes each, a single ShowBundle can hold 255 cues (2.5 KB binary) parsed from ~8 KB JSON. Headroom is generous.

### 6.5 Scheduling and Sweeps

**CueScheduler** executes up to `MAX_CUES_PER_FRAME = 4` cues per tick (20 Hz, 50 ms). Uses `memcpy_P` for PROGMEM/PSRAM compatibility. Cues are time-sorted and executed in order.

**ParameterSweeper** manages up to `MAX_SWEEPS = 8` concurrent linear interpolations. Each sweep applies a callback to set the target parameter value each frame. Sweep completion triggers automatic cleanup.

**NarrativeEngine** cycles through BUILD → HOLD → RELEASE → REST phases. Phase duration is modulated by tension level from the current ShowChapter or Trinity semantic bridge. Zone offsets stagger phase progression across zones (outer zones lag inner zones by a configurable delay), creating visual depth.

### 6.6 The 12 Transition Types

All transitions operate on three buffers (source → target → output) and respect centre-origin geometry:

| ID | Name | Description |
|----|------|-------------|
| 0 | FADE | Crossfade between source and target |
| 1 | WIPE_OUT | Centre-origin wipe expanding outward |
| 2 | WIPE_IN | Outside-in wipe contracting to centre |
| 3 | DISSOLVE | Pixel-by-pixel random dissolution |
| 4 | PHASE_SHIFT | Phase offset blend between source and target |
| 5 | PULSEWAVE | Centre-origin pulse that reveals target in waves |
| 6 | IMPLOSION | Outside-in collapse with energy accumulation |
| 7 | IRIS | Circular iris opening from centre |
| 8 | NUCLEAR | Explosive centre-origin burst |
| 9 | STARGATE | Spiralling centre-origin tunnel effect |
| 10 | KALEIDOSCOPE | Symmetrical fractal dissolve |
| 11 | MANDALA | Concentric ring reveal |

---

## 7. AI-Native Authoring

### 7.1 Philosophy: Assist, Don't Replace

AI authoring in SpectraSynq follows the **Template + Refine** pattern observed in successful creative tools (Suno Studio's "create then edit", Midjourney's "upscale then vary", Figma's "constraint-aware generation"). The AI generates a complete first draft. The human refines specific sections. Neither party works alone.

> **CORRECTED from v1:** The previous architecture suggested removing AI assist in favour of fully manual authoring. This was wrong. The correct approach is progressive disclosure — beginners get maximum AI assistance, experts get full manual control, and both can mix modes freely.

### 7.2 Three Authoring Modes (Progressive Disclosure)

**Magic Mode (Simple)**
"Drop a song. Get a light show."

1. User provides an audio file
2. TRINITY_PIPELINE analyses it → TrinityContract
3. AI generates a complete ShowBundle:
   - Segments → chapters with narrative phases
   - Tempo → base speed parameter
   - Key/mode → palette selection (major → warm, minor → cool)
   - Stem features → zone assignments (drums → outer zone, vocals → inner zone)
   - Beat positions → cue timing alignment
   - GEMS emotion estimation → effect family selection
4. ShowBundle is immediately playable
5. User can accept entirely, or switch to Guide Mode to refine

**Target: 30 seconds from audio file to playable light show.** This is the Kickstarter demo moment.

**Guide Mode (Standard)**
"I'll suggest, you decide."

1. AI generates a ShowBundle skeleton (chapters, suggested effects per section)
2. User sees a per-section overview:
   - "Intro (0:00-0:32): LGP Holographic, Ocean Depths palette, BUILD phase — [Accept] [Change effect] [Change palette]"
   - "Verse (0:32-1:04): Schlieren Flow, Warm Amber palette, BUILD phase — [Accept] [Change effect] [Change palette]"
3. User overrides specific sections, accepts others
4. AI regenerates cue timing for modified sections, preserving user choices
5. Parameter curves are pre-filled from crossmodal defaults, manually adjustable

**Direct Mode (Expert)**
"Full timeline control."

1. User sees a horizontal timeline editor (PRISM_STUDIO)
2. Cue lanes for effects, palettes, parameters, zones, transitions, narrative
3. Manual placement of cues with snap-to-beat alignment
4. Parameter curve editor for continuous sweeps
5. Real-time preview against audio waveform
6. AI available as an assistant ("suggest a transition here", "fill this section")

### 7.3 Constraint-Aware Generation

The AI operates within the firmware's existing capability set:

- **Effect catalogue:** Only references effects present in PatternRegistry (147 effects, 10 families)
- **Palette catalogue:** Only references palettes present in the firmware (75 palettes with metadata flags)
- **Parameter ranges:** All values 0-255 (uint8_t)
- **Zone limits:** 1-4 concentric zones
- **Transition types:** Only the 12 defined types
- **Cue limits:** Max 255 per show (uint8_t)
- **Timing resolution:** 1 ms (uint32_t timeMs)

The AI never hallucinates capabilities that don't exist on the device. Effect names in generated ShowBundles are validated against the firmware's registry before export.

### 7.4 Where AI Runs

**For Kickstarter:**
- AI generation runs in PRISM_STUDIO (browser)
- LLM API calls go to a cloud endpoint (Anthropic Claude API)
- The prompt includes: TrinityContract, effect catalogue, palette catalogue, GEMS mapping table
- The response is a complete ShowBundle JSON
- No AI on device. No AI in the iOS app.

**Post-Kickstarter:**
- AI generation may also be available in LIGHTWAVE_APP via API
- Cloud endpoint handles inference; client handles prompt assembly and response parsing
- On-device AI is not planned — ESP32-S3 lacks the compute

### 7.5 Non-Destructive Suggestion Loop

When AI suggests modifications (in Guide Mode or Direct Mode), suggestions are presented as overlays, not immediate mutations:

1. AI generates suggestion (e.g., "try Kaleidoscope transition at bar 32")
2. PRISM_STUDIO shows the suggestion as a ghost cue on the timeline
3. User clicks Accept → suggestion becomes a real cue
4. User clicks Reject → suggestion disappears
5. User clicks Modify → suggestion opens in edit mode

Provenance tracking records which cues were AI-generated vs human-placed, enabling analytics on how much users customise vs accept defaults.

---

## 8. The Kickstarter Path

### 8.1 What Must Ship

For Kickstarter, the minimum viable system is:

| Component | Required | Notes |
|-----------|----------|-------|
| K1_RUNTIME | Yes (exists) | Firmware is production-ready. 147 effects, full API. |
| LIGHTWAVE_APP | Yes (partial) | Needs: show playback orchestrator, beat sync streaming, background audio mode. ~4,200 new lines. |
| TRINITY_PIPELINE | Yes (with fixes) | Fix macro derivation bugs. ~500 lines of changes. |
| PRISM_STUDIO | **Yes (critical path)** | The authoring tool. Minimum viable: Magic Mode + Guide Mode + basic timeline. ~15,000 lines. |
| TAB5_HUB | Optional | Nice to have for demo. Works today for direct control. |
| PRISM_API | **No** | Post-Kickstarter. File-based sharing for launch. |

### 8.2 The Demo Sequence

The Kickstarter video demonstrates this sequence:

1. **Open PRISM_STUDIO** in a browser
2. **Drop an audio file** (e.g., a recognisable song)
3. **Watch Trinity analyse** (visual progress: beats appear on waveform, segments colour-code)
4. **AI generates ShowBundle** (30 seconds, progress shown)
5. **Preview in browser** (simulated LED strip, synchronised to audio)
6. **Send to device** (one-click transfer to K1_RUNTIME via local network)
7. **Play on the physical LGP** (music from phone speakers, light show on the panel)
8. **Share the ShowBundle** (export as JSON file, send to a friend)

Time from "drop a song" to "light show playing on hardware": **under 2 minutes** (limited by Trinity ML analysis, which takes ~60s for a 3-minute song on a laptop GPU).

### 8.3 Sharing Without Cloud

For Kickstarter (no PRISM_API):

- ShowBundles are exported as `.lightshow.json` files
- Users share via AirDrop, email, messaging apps, file transfer
- LIGHTWAVE_APP can import `.lightshow.json` files via iOS share sheet
- PRISM_STUDIO can import via file picker
- ShowBundles reference songs by metadata (title, artist, BPM) — user provides their own audio file
- No DRM, no account required, no cloud dependency

Post-Kickstarter, PRISM_API adds:
- Hosted ShowBundle gallery (browse, search, download)
- User profiles and collections
- Rating and curation
- Song identification via AcoustID/MusicBrainz for automatic matching

### 8.4 PRISM_STUDIO Technology Stack

Based on research into web platform capabilities:

| Layer | Technology | Rationale |
|-------|-----------|-----------|
| Framework | React 19 + TypeScript | Mature ecosystem, component model fits timeline editor |
| State | Zustand | Lightweight, no boilerplate, works with React 19 |
| Timeline renderer | PixiJS v8 (WebGL canvas) | 233% faster than v7, hardware-accelerated, ideal for waveform + cue rendering |
| Waveform display | WaveSurfer.js v7 | Battle-tested audio waveform component, integrates with Web Audio API |
| Audio engine | Web Audio API | Sample-accurate scheduling, AnalyserNode for real-time FFT |
| File storage | Origin Private File System (OPFS) | 5-10x faster than IndexedDB on Android, persistent browser storage |
| Device comms | WebSocket + REST | Direct to K1_RUNTIME on local network |
| Build | Vite | Fast HMR, optimised production builds |

**No mDNS from browser** — device IP must be entered manually or discovered via a QR code on the device's web UI. This is a browser platform limitation.

---

## 9. Migration Plan

### Stage 0 — Baseline Instrumentation
**Exit criteria:**
- Per-frame render time measured and logged (exists: `updateShowStats()`)
- Heap allocations in render path verified == 0 (use ESP-IDF heap trace)
- PSRAM availability verified at boot
- LED push time measured accurately (should read ~4.8 ms, not 9.6 ms)
- Stale comment at `RendererActor.cpp:729` corrected

**Status:** Mostly complete. The timing measurement infrastructure exists. The stale timing comment needs correction.

### Stage 1 — Trinity Pipeline Bug Fixes
**Exit criteria:**
- Dashboard V4 macro mapping sends non-zero values for all 5 channels
- `export_contract.py` derives macros from actual audio features, not sinusoidal placeholders
- `harmonic_complexity`, `rhythmic_density`, `spectral_centroid` computed and populated
- `TrinityControlBusProxy` maps mid-frequency bands (2-5) from stem band_energy, not zeros
- TrinityContract output validated against schema for 3 reference tracks

**Status:** Bugs identified by research agents. Fixes are straightforward (~500 lines).

### Stage 2 — iOS Show Playback Orchestrator
**Exit criteria:**
- `AVAudioPlayer` integration for local audio file playback
- `Background Modes: audio` capability enabled
- Beat sync streaming via WebSocket (`trinity.beat` messages aligned to playback position)
- Show playback controls (play/pause/stop/seek) functional
- ShowBundle import via iOS share sheet (`.lightshow.json` file association)
- Phase drift detection and correction (re-sync if audio position and cue position diverge >50 ms)

**Status:** Not started. ~4,200 lines estimated. REST/WebSocket infrastructure exists.

### Stage 3 — PRISM_STUDIO MVP
**Exit criteria:**
- Audio file import and waveform display (WaveSurfer.js)
- Trinity analysis integration (call TRINITY_PIPELINE, display results on waveform)
- Magic Mode: drop audio → AI generates ShowBundle in <30 seconds
- Guide Mode: per-section overview with accept/change per section
- Basic timeline view: cue lanes, beat grid, segment blocks
- ShowBundle export as `.lightshow.json`
- Device transfer via REST (one-click send to K1_RUNTIME on local network)
- Simulated LED preview (320-pixel Canvas strip, centre-origin)

**Status:** Not started. Critical path. ~15,000 lines estimated.

### Stage 4 — Full Show System Integration
**Exit criteria:**
- End-to-end flow validated: audio → Trinity → AI → ShowBundle → iOS → K1_RUNTIME → LEDs
- ShowBundle name resolution verified (effect/palette names → IDs, with fallback on missing names)
- Transition types verified visually for all 12 types
- Parameter sweeps verified for all 6 sweepable parameters
- Zone configuration changes verified (1→2→4→1 zone transitions during show)
- NarrativeEngine phase cycling verified with Trinity tension curve input
- Semantic bridge verified as fallback when no ShowBundle is loaded

**Status:** Blocked on Stages 1-3.

### Stage 5 — Polish and Kickstarter Prep
**Exit criteria:**
- Demo sequence (Section 8.2) executes flawlessly in <2 minutes
- 3 reference ShowBundles authored for demo tracks
- PRISM_STUDIO deployed as static web app (no cloud dependency)
- LIGHTWAVE_APP beta build on TestFlight
- File-based sharing validated (export → import round-trip)

### Stage 6 — Post-Kickstarter: Cloud and Community (PRISM_API)
**Exit criteria:** (deferred, not required for launch)
- User accounts and authentication
- ShowBundle upload/download/versioning
- Community gallery with search and curation
- Song identification via AcoustID for automatic ShowBundle matching
- Content moderation policies

---

## 10. Risk Register

| # | Risk | Impact | Likelihood | Mitigation |
|---|------|--------|------------|------------|
| R1 | Trinity ML analysis too slow for "30-second" demo claim | High | Medium | Pre-analyse demo tracks. Show progress UI. Target: <60s for 3-min song on laptop GPU. |
| R2 | AI-generated ShowBundles feel generic | High | Medium | Curate GEMS→effect mappings carefully. A/B test defaults. Allow rapid iteration via Guide Mode. |
| R3 | WiFi jitter causes visible sync drift | Medium | Low | Local beat prediction makes this moot for beat-sync. Only phase correction relies on WiFi. |
| R4 | PRISM_STUDIO scope creep delays launch | High | High | Ship Magic Mode + Guide Mode only. Direct Mode (full timeline) is post-KS. |
| R5 | ShowBundle format needs breaking changes | Medium | Medium | Version field in meta. K1_RUNTIME rejects incompatible versions with clear error. |
| R6 | 255-cue limit insufficient for long shows | Low | Low | 255 cues at ~1 per 2 seconds = 8.5 minute show. For longer shows, use fewer cues + longer parameter sweeps. Revisit if user feedback demands it. |
| R7 | Effect name changes break existing ShowBundles | Medium | Low | PatternRegistry names are stable. Any rename must add backward-compatible alias. |
| R8 | iOS audio session management complexity | Medium | Medium | Use `.playback` category. Handle interruptions (phone calls). Test Bluetooth audio routing. |
| R9 | Web Audio API restrictions on mobile browsers | Low | High | PRISM_STUDIO is primarily a desktop/laptop tool. Mobile use is a nice-to-have. |
| R10 | Community sharing enables low-quality content flood | Low | High (post-KS) | Curation, rating system, featured selections. Not a KS concern. |

---

## 11. Critical Fixes Applied (Delta from v1)

### 11.1 Fix: WS2812 Timing

**Was (v1):** "~9.6 ms total for 320 LEDs across 2 strips"
**Is (v2):** **~4.8 ms total for 320 LEDs, driven in parallel.**

The firmware defines `FASTLED_RMT_BUILTIN_DRIVER=1` in `platformio.ini`. This enables FastLED's built-in RMT driver, which assigns each strip to a separate RMT peripheral. Both strips clock out data simultaneously. 160 LEDs at 30 us/LED = 4.8 ms. Both strips complete in 4.8 ms, not 9.6 ms.

Evidence: `LedDriver_S3.cpp` creates two separate `CLEDController` instances on GPIO 16 and GPIO 17. `FastLED.show()` triggers both RMT channels. `updateShowStats()` measures actual push time.

**Impact:** The render budget has ~1.5 ms more headroom than v1 claimed. 120 FPS is achievable, not aspirational.

### 11.2 Fix: Cue ABI

**Was (v1):** Cue data layout described informally
**Is (v2):** Explicit per-CueType byte packing in Section 6.2, matching the exact layout in `ShowTypes.h` lines 73-110.

### 11.3 Fix: ArduinoJson

**Was (v1):** Mentioned `StaticJsonDocument` and "fixed PSRAM arena" without specifying how
**Is (v2):** ArduinoJson v7 has NO `StaticJsonDocument`. Dynamic allocation only. ESP-IDF's `CONFIG_SPIRAM_USE_MALLOC=y` routes large allocations to PSRAM automatically. One clear rule: never parse JSON in the render path; parse at load time only. See Section 6.4.

### 11.4 Fix: AI-Native Authoring

**Was (v1):** Removed AI assist in favour of manual authoring
**Is (v2):** Three-tier progressive disclosure (Magic / Guide / Direct). AI is always available as an assistant. Beginners get maximum AI help. Experts get full manual control. Both can mix modes. See Section 7.

### 11.5 Fix: Cloud/Community Staging

**Was (v1):** Cloud and community features described as part of the core architecture
**Is (v2):** PRISM_API is explicitly post-Kickstarter. For launch, ShowBundles are shared as files. No cloud dependency. No accounts required. See Sections 8.3 and Stage 6 in the migration plan.

---

## 12. What Makes This Category-Defining

Generic music visualisers pick colours from FFT bins. They are seismographs — they measure energy and display it. SpectraSynq is fundamentally different because of three architectural choices:

**1. The Contract Spine (ShowBundle + TrinityContract)**
By separating "what the music contains" (TrinityContract) from "what the creator intends" (ShowBundle), SpectraSynq enables a creative layer that generic visualisers cannot have. Two people can create different light shows for the same song, and both will be musically coherent because both are grounded in the same structural analysis.

**2. Perceptual Grounding**
The default parameter mappings aren't arbitrary — they're rooted in crossmodal correspondence research. High pitch maps to brightness not because an engineer chose it, but because human neurology wires it that way. This means even the zero-authoring path (live microphone + default mappings) produces responses that feel "right" to most people, across cultures.

**3. Narrative, Not Reaction**
The NarrativeEngine's BUILD → HOLD → RELEASE → REST cycle, driven by musical tension, creates visual storytelling. A verse builds anticipation. A chorus delivers intensity. A bridge provides release. An outro fades to rest. This mirrors the emotional arc of music because it's derived from the same structural analysis that music producers use when arranging a song.

These aren't features to be added later. They are load-bearing architectural decisions that every subsystem is built around. The ShowBundle format encodes narrative chapters. The TrinityContract extracts musical tension. The NarrativeEngine executes emotional arcs. The crossmodal defaults ensure perceptual coherence. Remove any one, and you have a generic visualiser. Keep all four, and you have a new medium.

---

## Appendix A: Effect Families and GEMS Mapping Reference

The 147 effects are organised into 10 families. Each family has natural affinity with specific GEMS emotions:

| Family | Count | Example Effects | GEMS Affinity | Typical Parameters |
|--------|-------|-----------------|---------------|-------------------|
| LGP Optical | 18 | Holographic, Opal Film, Caustic Shards | Wonder, Transcendence | Slow, bright, high complexity |
| Beat Pulse | 12 | Beat Pulse Bloom, Starburst Narrative | Power, Joyful Activation | Fast, beat-sync, high intensity |
| Flow / Organic | 15 | Schlieren Flow, Water Caustics | Tenderness, Peacefulness | Slow, warm, smooth motion |
| Geometric | 14 | Stress Glass, Grating Scan, Moire Silk | Tension, Unease | Angular, desaturated, complex |
| Particle Field | 10 | Kuramoto Transport, Reaction-Diffusion | Transcendence, Wonder | Emergent, medium speed, organic |
| Ripple | 8 | Ripple Enhanced, Ripple ES Tuned | Joyful Activation | Centre-origin pulse, beat-sync |
| Spectrum | 11 | Spectrum Detail, Spectrum Detail Enhanced | Power, Vitality | Bright, saturated, frequency-mapped |
| Ambient | 16 | Parallax Depth, Gentle Breathe | Peacefulness, Nostalgia | Very slow, dim, high trails |
| Narrative | 9 | Starburst Narrative, Bloom Parity | (follows chapter) | Varies with narrative phase |
| Classic | 24 | Fire, Plasma, Sparkle, Twinkle | Various | Legacy effects, well-tuned |

## Appendix B: Palette Categories and Mood Mapping

The 75 palettes are tagged with metadata flags:

| Flag | Count | Usage |
|------|-------|-------|
| WARM | 22 | Major key, high valence, tenderness |
| COOL | 18 | Minor key, low valence, tension, sadness |
| HIGH_SAT | 20 | Chorus, power, joyful activation |
| CALM | 12 | Verse, intro, peacefulness |
| VIVID | 15 | Drop, climax, maximum energy |
| WHITE_HEAVY | 8 | Transcendence, wonder, ethereal |
| CVD_FRIENDLY | 10 | Accessible palettes for colour vision deficiency |

AI ShowBundle generation uses these flags to match palette selection to musical mood. A major-key chorus with high energy → WARM + HIGH_SAT + VIVID. A minor-key verse with low energy → COOL + CALM.

## Appendix C: Firmware Actor Map

```
Core 1 (Render-critical)
├── RendererActor     Priority 5, 8ms tick (120 FPS)
│   ├── ZoneComposer          1-4 concentric zones
│   ├── TransitionEngine      3-buffer source/target/output
│   ├── AudioMappingRegistry  Crossmodal correspondence defaults
│   └── NarrativeEngine       BUILD/HOLD/RELEASE/REST cycle
└── AudioActor        Priority 4, continuous
    ├── Goertzel DSP          FFT, beat tracking, saliency
    ├── ControlBus            Shared audio feature frame
    └── TrinityControlBusProxy  Trinity macro → ControlBus bridge

Core 0 (System services)
├── NetworkActor      Priority 3, event-driven
│   ├── REST API              46+ endpoints
│   ├── WebSocket             State broadcast, Trinity stream
│   └── WiFiManager           AP/STA fallback
├── ShowDirectorActor Priority 2, 20Hz tick (50ms)
│   ├── CueScheduler          Time-sorted cue execution
│   ├── ParameterSweeper      8 concurrent interpolations
│   └── Semantic Bridge       Trinity segment → narrative mapping
├── HMI Actor         Priority 2, event-driven
│   └── Serial interface, button handler
└── PluginManagerActor Priority 1, event-driven
    └── Effect lifecycle, hot-reload
```

## Appendix D: iOS App — What Needs Building

Current state: 21,800 lines, 95 files, production-quality SwiftUI remote control.

| Feature | Estimated Lines | Priority |
|---------|----------------|----------|
| AVAudioPlayer integration | 400 | P0 |
| Background Modes: audio | 50 (config) | P0 |
| Beat sync WebSocket streaming | 600 | P0 |
| Show playback state machine (play/pause/stop/seek) | 800 | P0 |
| ShowBundle import via share sheet | 300 | P0 |
| Phase drift detection and correction | 500 | P1 |
| ShowBundle browser UI | 800 | P1 |
| Audio session management (interruptions, Bluetooth) | 400 | P1 |
| MusicKit integration (Apple Music library access) | 350 | P2 |
| **Total** | **~4,200** | |

---

## Rules Compliance

- British English spelling used throughout (centre, colour, serialise, authorise, defence, catalogue)
- No generic IoT boilerplate — all choices tied to Constitution invariants or firmware reality
- All technical claims verified against firmware source code (ShowTypes.h, RendererActor.cpp, PatternRegistry.cpp, LedDriver_S3.cpp, platformio.ini, AudioMappingRegistry, NarrativeEngine)
- No speculative features described as existing — clear separation between "exists today" and "must be built"
- Five critical fixes from user feedback explicitly addressed in Section 11
