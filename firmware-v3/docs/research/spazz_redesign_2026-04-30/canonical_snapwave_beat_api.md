---
abstract: "Distillation of the canonical Snapwave + Beat-Detection API design from K1-team historical docs (SNAPWAVE_BEAT_DETECTION_API.md, SNAPWAVE_INTEGRATION_EXAMPLES.md). Captures the prescribed motion model (BeatDetectionAPI::getBeatOscillation → tanh snap → tanh-clipped position), the 64-byte BeatState surface (oscillation_value/velocity, beat_phase, beat_strength, frequency-band energies, harmonic_blend[4]), integration patterns (zero-copy cached read, velocity-based smoothing, silence-aware fade), and the explicit anti-patterns. Final section documents the concrete gap between this canonical design and K1's current SnapwaveLinearEffect.cpp (which has NO beat-detection consumption — it builds oscillation from raw chroma × sin(t × multi_freq) and gates on RMS only) and ranks the patterns that MUST be adopted to fix the spazz/jerk."
---

# Canonical Snapwave + Beat-Detection API — Doctrine Distilled

**Status:** GROUNDED — every claim is cited by file:line range against the two source documents the Captain surfaced (`/Users/spectrasynq/Workspace_Management/Software/LightwaveOS_Official/docs/SNAPWAVE_BEAT_DETECTION_API.md`, `SNAPWAVE_INTEGRATION_EXAMPLES.md`) and against `firmware-v3/src/effects/ieffect/SnapwaveLinearEffect.cpp` for gap analysis.

**Scope:** READ-ONLY synthesis. No source-code edits.

## 1. Document overview

### Purpose

The two source documents are the K1-team's historical canonical specification for how Snapwave consumes beat-detection data. `SNAPWAVE_BEAT_DETECTION_API.md` defines the API surface, data structures, configuration, and performance budgets (BEAT_DETECTION_API.md:1-200, 202-316). `SNAPWAVE_INTEGRATION_EXAMPLES.md` provides reference Snapwave implementations at three escalating sophistication levels — basic, enhanced, multi-harmonic — plus integration glue, performance monitoring, error handling, and an anti-pattern catalogue (INTEGRATION_EXAMPLES.md:1-12, 14-248, 252-492, 875-1027).

### Scope

The doctrine is built around a single primary visualisation function (`light_mode_snapwave`) consuming a single canonical data source (`SensoryBridge::BeatDetection::BeatDetectionAPI`). The API is a `static`-method singleton with a 64-byte `BeatState` struct, accessed zero-copy via `getBeatState()` (BEAT_DETECTION_API.md:42-90, 216-251). Performance targets are explicit: <200 µs typical processing, 108+ FPS, single-core ESP32-S3, statically-allocated state (BEAT_DETECTION_API.md:13-17, 654-682).

### Intended audience

Effect authors writing audio-reactive visualisations and the audio-thread engineer integrating beat detection into the FFT pipeline. The boundary is hard: audio thread *writes* `BeatState`, render thread *reads* it (BEAT_DETECTION_API.md:341-357).

---

## 2. Canonical Snapwave motion model

### 2.1 The single-line core

Both documents prescribe an identical core motion equation; this is the canonical "Snapwave" in 4 lines:

```cpp
// Get oscillation value optimized for Snapwave motion
float oscillation = BeatDetectionAPI::getBeatOscillation();

// Apply hyperbolic tangent normalization for "snap" effect
// This creates the characteristic Snapwave motion pattern
float position = tanh(oscillation * 2.0f) * 0.7f;
```
(INTEGRATION_EXAMPLES.md:30-34, also BEAT_DETECTION_API.md:368-371)

The contract is: **the oscillation source is a pre-computed, beat-synchronised, smoothed scalar in `[-1, +1]` produced by the audio-side BeatDetection module**, NOT an ad-hoc sum of chroma × sin(t) computed inside the effect itself. The effect's job is the snap transform and the LED mapping; the audio-derived motion is finished before the effect ever sees it.

`getBeatOscillation()` is documented as: *"Get beat-synchronized oscillation value for Snapwave / Range: -1.0 to +1.0 with hyperbolic tangent normalization / CRITICAL: This is the primary value Snapwave uses for motion"* (BEAT_DETECTION_API.md:103-107). The internal calculation is named `calculateOscillation()` and lives inside the API (BEAT_DETECTION_API.md:193).

### 2.2 Beat-strength amplitude modulation (additive, capped)

Position is then modulated — not replaced — by a **bounded** beat-strength term:

```cpp
// Modulate position with beat strength for impact effects
float beat_strength = BeatDetectionAPI::getBeatStrength();
position *= (1.0f + beat_strength * 0.3f);
```
(INTEGRATION_EXAMPLES.md:36-38)

The basic Snapwave example caps the amplitude bonus at +30 % (factor 1.0–1.3). The enhanced variant caps it at +50 % via `beat_multiplier = 1.0f + (beat_state.beat_strength * 0.5f)` (BEAT_DETECTION_API.md:373-374). In all cases the rate (oscillation) and the amplitude (beat_strength) are decoupled — beats make the dot *travel further*, they do not *change the oscillation frequency on the same frame*.

### 2.3 Velocity-based smoothing (enhanced model)

The enhanced reference Snapwave smooths the oscillation using the API-provided `oscillation_velocity` field, not a fixed EMA:

```cpp
// Primary oscillation with velocity smoothing
float oscillation = beat_state.oscillation_primary;
float velocity = beat_state.oscillation_velocity;

// Smooth oscillation using velocity for interpolation
static float smoothed_oscillation = 0.0f;
smoothed_oscillation += velocity * 0.016f; // Assume ~60 FPS for smoothing
smoothed_oscillation = smoothed_oscillation * 0.9f + oscillation * 0.1f;

// Apply snap transformation with beat modulation
float snap_factor = 2.0f + (beat_state.beat_strength * 1.0f);
float position = tanh(smoothed_oscillation * snap_factor) * 0.75f;
```
(INTEGRATION_EXAMPLES.md:88-98)

Note three properties of this smoothing pattern:
1. The velocity is consumed as a derivative — `smoothed += velocity * dt`, an integration step (INTEGRATION_EXAMPLES.md:93).
2. The smoothed value is then mixed with the current oscillation at a 0.9 / 0.1 ratio (INTEGRATION_EXAMPLES.md:94). This is a damped follower, not a free integrator.
3. The snap factor itself becomes beat-strength dependent — strong beats sharpen the tanh (INTEGRATION_EXAMPLES.md:97-98).

The advanced multi-harmonic variant goes further with explicit inertia:

```cpp
// Velocity-based smoothing for organic feel
static float motion_velocity = 0.0f;
static float last_motion = 0.0f;

float target_motion = primary_motion + phase_influence;
float motion_change = target_motion - last_motion;

// Apply inertia
motion_velocity = motion_velocity * 0.85f + motion_change * 0.15f;
float smoothed_motion = last_motion + motion_velocity;

last_motion = smoothed_motion;
```
(INTEGRATION_EXAMPLES.md:325-337)

This is a critically-damped second-order follower (a velocity-Verlet-style integrator with 15 % drag per frame). It produces the "organic" rather than "twitchy" motion the doctrine emphasises.

### 2.4 Phase prediction (tempo-confidence-weighted)

When tempo confidence is high, the canonical model adds a bounded predictive offset derived from beat phase:

```cpp
// Beat phase synchronization for predictive motion
if (beat_state.tempo_confidence > 0.5f) {
    float phase_offset = sin(beat_state.beat_phase * 2.0f * PI) * 0.1f;
    position += phase_offset * beat_state.tempo_confidence;
}
```
(INTEGRATION_EXAMPLES.md:101-104; analogous block at BEAT_DETECTION_API.md:378-380, 508-510)

The phase term is gated by `tempo_confidence > 0.5f` and scaled by `tempo_confidence` again — when the BPM estimate is uncertain, predictive phase contributes nothing.

### 2.5 LED mapping and trail fade

The position-to-LED mapping is a single line — `center + position × center` — which assumes a **linear strip with centre origin** (INTEGRATION_EXAMPLES.md:41-43, BEAT_DETECTION_API.md:382-384). Trail fade is *energy-driven*, not constant: more total audio energy = longer trails:

```cpp
// Dynamic trail fading based on audio energy
float total_energy = BeatDetectionAPI::getTotalEnergy();
float fade_rate = 0.92f + (total_energy * 0.06f); // More energy = longer trails
fade_leds_toward_black(leds_16, fade_rate);
```
(INTEGRATION_EXAMPLES.md:58-60). The enhanced variant also injects beat-strength influence (`0.88f + beat_strength * 0.10f`, INTEGRATION_EXAMPLES.md:115).

Critically, the silence path *increases* fade persistence and *halves* oscillation amplitude:

```cpp
// Silence detection for gentle mode
if (beat_state.in_silence) {
    // Gentle fading during silence
    trail_persistence = 0.96f;
    // Reduce oscillation amplitude
    position *= 0.5f;
    led_pos = center + (position * center);
    led_pos = constrain(led_pos, 0, NATIVE_RESOLUTION - 1);
}
```
(INTEGRATION_EXAMPLES.md:118-125)

The doctrine never zeros position outright in silence — it scales it down so the dot drifts gently.

---

## 3. Beat-detection API surface

### 3.1 The 64-byte BeatState (verbatim layout)

```cpp
struct BeatState {
    // === CORE BEAT DETECTION ===
    bool     beat_detected;        // True when beat detected this frame
    float    confidence;           // Beat detection confidence (0.0-1.0)
    float    beat_strength;        // Current beat amplitude (0.0-1.0)
    float    beat_phase;           // Position in beat cycle (0.0-1.0)

    // === TEMPO ANALYSIS ===
    float    estimated_bpm;        // Current BPM estimate
    uint32_t last_beat_time;       // Timestamp of last beat (ms)
    uint32_t next_predicted_beat;  // Predicted next beat time (ms)
    float    tempo_confidence;     // Reliability of BPM estimate (0.0-1.0)

    // === FREQUENCY ANALYSIS ===
    float    bass_energy;          // Bass frequency energy (0.0-1.0)
    float    mid_energy;           // Mid frequency energy (0.0-1.0)
    float    high_energy;          // High frequency energy (0.0-1.0)
    float    total_energy;         // Combined frequency energy

    // === SNAPWAVE OSCILLATION DATA ===
    float    oscillation_value;    // Primary oscillation (-1.0 to +1.0)
    float    oscillation_velocity; // Rate of change for smooth motion
    float    harmonic_blend[4];    // Multi-harmonic components

    // === ONSET DETECTION ===
    float    onset_threshold;      // Current adaptive threshold
    float    onset_energy;         // Current onset detection energy
    uint32_t last_onset_time;      // Time of last onset detection
    bool     in_silence;           // Currently in silent period
    ...
};
```
(BEAT_DETECTION_API.md:216-249)

### 3.2 Field-by-field semantics for Snapwave

| Field | Update cadence | Range / units | Latching | Snapwave role |
|---|---|---|---|---|
| `beat_detected` | Per audio frame (~86.6 Hz, BEAT_DETECTION_API.md:215) | bool | One-frame edge-trigger; combined with `current_beat && !last_beat_state` for event-style logging (INTEGRATION_EXAMPLES.md:686-697) | Gate burst/special-effect rendering only; not a motion driver |
| `confidence` | Per audio frame | 0.0–1.0 | Continuous | Filter false positives (`if (confidence > X)`) |
| `beat_strength` | Per audio frame, decays naturally between beats (BEAT_DETECTION_API.md:118-122) | 0.0–1.0 | Self-decaying envelope | **Amplitude modulator** — scales `position` by 1.0 + strength × {0.3, 0.5} |
| `beat_phase` | Per audio frame | 0.0–1.0 within current beat cycle (BEAT_DETECTION_API.md:110-114) | Cyclic | Predictive sin(phase × 2π) offset, gated on tempo_confidence |
| `estimated_bpm` | Smoothed by `config.tempo_smoothing` default 0.1 (BEAT_DETECTION_API.md:277) | float BPM | Heavily smoothed | Auto-config / display only; not consumed by motion |
| `tempo_confidence` | Per audio frame | 0.0–1.0 | Continuous | Gate (and weight) the phase-prediction term (INTEGRATION_EXAMPLES.md:101-104) |
| `bass_energy` / `mid_energy` / `high_energy` | Per audio frame, derived from spectrogram bins 0-19 / 20-55 / 56-95 (BEAT_DETECTION_API.md:128-149) | 0.0–1.0 | Continuous | Frequency-band colour blend; secondary position influence in advanced model (INTEGRATION_EXAMPLES.md:281-285) |
| `total_energy` | Combined | 0.0–1.0 | Continuous | Trail-fade modulation (`fade_rate = 0.92f + total_energy * 0.06f`) |
| `oscillation_value` | Per audio frame | -1.0–+1.0, tanh-normalised (BEAT_DETECTION_API.md:236) | Continuous | **Primary motion source** — fed straight into `tanh(x * snap_factor)` |
| `oscillation_velocity` | Per audio frame | float, signed rate-of-change | Continuous | Velocity-Verlet smoothing input (INTEGRATION_EXAMPLES.md:91-94) |
| `harmonic_blend[4]` | Per audio frame | 4-element 0.0–1.0 | Continuous | Optional richness term in advanced model — weighted 1/1, 1/2, 1/3, 1/4 sum (INTEGRATION_EXAMPLES.md:270-275) |
| `in_silence` | Per audio frame, set after `silence_timeout` (default 3000 ms, BEAT_DETECTION_API.md:272) | bool | Latched until audio resumes | **Mode switch** — when true, increase trail persistence and halve oscillation amplitude (INTEGRATION_EXAMPLES.md:118-125) |
| `onset_threshold` / `onset_energy` / `last_onset_time` | Per audio frame | floats / ms timestamp | Adaptive (BEAT_DETECTION_API.md:240-243) | Diagnostics / advanced effects only |

### 3.3 Inline accessor surface

The API exposes inline accessors for every hot-path read so callers can hit a single field-load:

```cpp
static inline bool isBeatDetected();             // BEAT_DETECTION_API.md:80-82
static inline float getBeatConfidence();         // BEAT_DETECTION_API.md:88-90
static inline float getEstimatedBPM();           // BEAT_DETECTION_API.md:96-98
static float getBeatOscillation();               // BEAT_DETECTION_API.md:107 (NOT inline — internally calls calculateOscillation)
static inline float getBeatPhase();              // BEAT_DETECTION_API.md:113-115
static inline float getBeatStrength();           // BEAT_DETECTION_API.md:121-123
static inline float getBassEnergy();             // BEAT_DETECTION_API.md:131-133
static inline float getMidEnergy();              // BEAT_DETECTION_API.md:139-141
static inline float getHighEnergy();             // BEAT_DETECTION_API.md:147-149
```

### 3.4 Configuration surface (BeatConfig, 32 bytes)

```cpp
struct BeatConfig {
    float    onset_sensitivity;     // 0.5-2.0, default 1.0
    float    bass_weight;           // default 2.0
    float    mid_weight;            // default 1.5
    float    high_weight;           // default 0.5
    uint32_t min_beat_interval;     // default 300 ms (200 BPM ceiling)
    uint32_t max_beat_interval;     // default 2000 ms
    uint32_t silence_timeout;       // default 3000 ms
    bool     adaptive_threshold;    // enable adaptation
    float    threshold_decay;       // default 0.95
    float    tempo_smoothing;       // default 0.1
    float    oscillation_damping;   // default 0.92  -- damps the BeatState.oscillation_value internally
    uint8_t  harmonic_count;        // default 4
};
```
(BEAT_DETECTION_API.md:262-283)

`oscillation_damping = 0.92` is the default damping factor applied **inside** the API to its internal oscillator state. The effect side does NOT need to add a second EMA on top — that would double-smooth.

### 3.5 Performance budget

| Metric | Target | Maximum | Critical |
|---|---|---|---|
| Processing time | <200 µs | <500 µs | <1000 µs |
| Memory access | Zero-copy | Direct | Pointer-based |
| Beat latency | <12 ms | <20 ms | <50 ms |
| False positive rate | <5 % | <10 % | <20 % |
| CPU usage | <2 % | <5 % | <10 % |

(BEAT_DETECTION_API.md:666-673). Total API memory footprint is **136 bytes** (BEAT_DETECTION_API.md:658-663).

---

## 4. Integration patterns

### 4.1 Cache the BeatState reference at frame start, then read fields locally

```cpp
// ✅ DO: Cache frequently accessed values
void optimized_snapwave_integration() {
    // Cache beat state at start of frame
    const auto& beat_state = BeatDetectionAPI::getBeatState();
    float oscillation = beat_state.oscillation_primary;
    float beat_strength = beat_state.beat_strength;

    // Use cached values throughout frame processing
    // ...
}
```
(INTEGRATION_EXAMPLES.md:884-893)

### 4.2 Audio-thread integration (single producer)

```cpp
// In audio processing loop (i2s_audio.h)
void process_audio_frame() {
    // ... existing FFT processing ...

    // Update beat detection with new audio data
    SensoryBridge::BeatDetection::BeatDetectionAPI::processAudioFrame(
        spectrogram,        // FFT magnitude bins
        chromagram_smooth,  // Note chromagram data
        millis()           // Current timestamp
    );

    // ... continue with other audio processing ...
}
```
(BEAT_DETECTION_API.md:344-356)

The API explicitly takes `spectrogram_data` (96-bin FFT magnitudes) and `chromagram_data` (12-note chromagram) — it does NOT consume only chroma. Snapwave's motion is driven by spectrogram-derived bands and onset/tempo logic, with chroma feeding the chromatic colour path only.

### 4.3 Frame-rate-independent smoothing

```cpp
// ✅ DO: Use frame-rate aware smoothing
void frame_rate_aware_smoothing() {
    static uint32_t last_frame_time = 0;
    uint32_t current_time = millis();

    float delta_time = (current_time - last_frame_time) / 1000.0f; // Seconds
    float smoothing_rate = 1.0f - exp(-delta_time * 10.0f); // 10Hz time constant

    // Apply frame-rate independent smoothing
    static float smoothed_value = 0.0f;
    float target_value = BeatDetectionAPI::getBeatOscillation();
    smoothed_value += (target_value - smoothed_value) * smoothing_rate;

    last_frame_time = current_time;
}
```
(INTEGRATION_EXAMPLES.md:1013-1026). The smoothing constant is `1 - exp(-dt × ω)` with ω = 10 Hz — correct for any framerate, not assuming 60 / 86 / 120 FPS.

### 4.4 Silence handling

The doctrine treats silence as a distinct mode, not as "RMS below threshold = output zero". The fallback (when the API itself is unavailable) is a slow time-based oscillation:

```cpp
// Detect and handle silence periods
const auto& beat_state = BeatDetectionAPI::getBeatState();
if (beat_state.in_silence) {
    // Use slower, gentler motion during silence
    float gentle_osc = sin(millis() * 0.0005f) * 0.3f;
    // ... apply gentle motion ...
} else {
    // Normal beat-driven motion
    float beat_osc = SensoryBridge::BeatDetection::BeatDetectionAPI::getBeatOscillation();
    // ... apply beat motion ...
}
```
(BEAT_DETECTION_API.md:424-433; full fallback example INTEGRATION_EXAMPLES.md:850-870)

`in_silence` is *latched* — it does not flicker on every transient quiet frame. It only sets after `silence_timeout` (default 3 seconds) has elapsed (BEAT_DETECTION_API.md:272).

### 4.5 Beat-edge detection (event triggering)

```cpp
static bool last_beat_state = false;
bool current_beat = BeatDetectionAPI::isBeatDetected();

// Log beat detection events
if (current_beat && !last_beat_state) {
    // ... fire event ...
}
last_beat_state = current_beat;
```
(INTEGRATION_EXAMPLES.md:686-708)

The pattern is rising-edge detection. Burst effects fire on `beat_detected && beat_strength > 0.6f` (INTEGRATION_EXAMPLES.md:130-132) — never on raw RMS.

---

## 5. Named anti-patterns (verbatim)

The integration document explicitly enumerates anti-patterns. These are not advisories — they are stated as "DO NOT" rules:

### 5.1 Polling the API inside the per-LED loop

```cpp
// ❌ DON'T: Call API functions repeatedly in loops
void unoptimized_snapwave_integration() {
    for (int i = 0; i < NATIVE_RESOLUTION; i++) {
        // This calls the API function 144 times per frame!
        float oscillation = BeatDetectionAPI::getBeatOscillation(); // BAD
        // ...
    }
}
```
(INTEGRATION_EXAMPLES.md:895-902)

### 5.2 Heap allocation in render

```cpp
// ✅ DO: Minimize memory allocations
void memory_efficient_snapwave() {
    // Use stack variables for temporary calculations
    float position = getSnapwavePosition();
    CRGB16 color = calculateBeatColor(); // Pass by value for small structs

    // Avoid dynamic allocation
    // float* temp_array = new float[100]; // DON'T DO THIS
}
```
(INTEGRATION_EXAMPLES.md:909-917)

### 5.3 Large stack arrays / dynamic allocation in DSP path

```cpp
// ❌ DON'T: Use large stack arrays or dynamic allocation
void bad_memory_management() {
    float large_array[1000]; // Too much stack usage
    float* dynamic_array = (float*)malloc(1000 * sizeof(float)); // No malloc!

    // This will fragment heap and cause crashes
    free(dynamic_array);
}
```
(INTEGRATION_EXAMPLES.md:952-959)

### 5.4 Implicit (unstated but enforced by design)

By the structure of the canonical examples, several additional "do nots" are clear:

- **Do not compute oscillation inside the effect from raw audio buffers.** Every example consumes `getBeatOscillation()` or `beat_state.oscillation_value` / `oscillation_primary`. None of them reach into the FFT, chromagram, or RMS to *build* an oscillator — they consume one. This is the architectural separation the API enforces.
- **Do not use raw `chroma[i] × sin(t)` summation as a motion source.** The chromagram is consumed only by colour code (`calculateFrequencyColor`, `calculateHarmonicColor` — INTEGRATION_EXAMPLES.md:147-185, 346-382). It never drives position.
- **Do not use a fixed-amplitude EMA followed by `tanh(x × constant)`.** The smoothing must be either velocity-integration (INTEGRATION_EXAMPLES.md:91-94) or `1 - exp(-dt × ω)` (INTEGRATION_EXAMPLES.md:1013-1026), and the snap factor must scale with beat strength (INTEGRATION_EXAMPLES.md:97).
- **Do not couple oscillation rate to chord identity.** The phase-spread term `(1.0 + i * 0.5)` per chroma index — which is exactly what K1 currently does — is nowhere in the canonical model. Chord identity drives colour, not motion frequency.

---

## 6. K1 implementation gap

K1's `firmware-v3/src/effects/ieffect/SnapwaveLinearEffect.cpp` does not implement the canonical model. The differences are structural, not parametric.

### 6.1 Motion source (ROOT CAUSE of spazz)

**Canonical:**
```cpp
float oscillation = BeatDetectionAPI::getBeatOscillation();
```
A single read of a pre-smoothed, beat-synchronised, tempo-coherent scalar. (BEAT_DETECTION_API.md:368, INTEGRATION_EXAMPLES.md:30)

**K1 current (`SnapwaveLinearEffect.cpp:107-143`):**
```cpp
float SnapwaveLinearEffect::computeOscillation(const plugins::EffectContext& ctx) {
    float oscillation = 0.0f;
    if (ctx.audio.available) {
        float rms = ctx.audio.rms();
        if (rms < ENERGY_GATE_THRESHOLD) return 0.0f;
        float timeMs = static_cast<float>(ctx.rawTotalTimeMs);
        for (uint8_t i = 0; i < 12; ++i) {
            float chromaVal = ctx.audio.getChroma(i);
            if (chromaVal > NOTE_THRESHOLD) {
                float freqMult = 1.0f + PHASE_SPREAD * i;          // 1.0, 1.5, 2.0, ..., 6.5
                oscillation += chromaVal * sinf(timeMs * BASE_FREQUENCY * freqMult);
            }
        }
    }
    oscillation = tanhf(oscillation * TANH_SCALE);                  // TANH_SCALE = 3.0
    return oscillation;
}
```
(SnapwaveLinearEffect.cpp:107-143; constants in SnapwaveLinearEffect.h:69-78)

This is a **12-frequency chord-driven sin wave summer with no beat synchronisation, no smoothing, and a sharp tanh × 3.0 saturation**. Three failure modes follow directly:

1. **Chord changes alter motion frequency.** When a new note enters the chromagram, a new sin term at a new freq_mult activates. Step changes in active-note set produce instantaneous step changes in the resulting waveform — a dot that was tracking sin(t × 0.001 × 1.0) suddenly jumps to a 12-term sum spanning sin(t × 0.001 × 1.0) through sin(t × 0.001 × 6.5). This is the textbook source of "spazz".
2. **No phase coherence.** The 12 sinusoids run at frequencies 0.001, 0.0015, 0.002, …, 0.0065 Hz × 1000 ms units, all sharing `t = millis()`. They constructively interfere unpredictably, producing a non-stationary, non-beat-locked waveform. Tanh saturates the chaotic sum to ±1, *concealing* the chaos at peak amplitude but not removing it from the velocity.
3. **NOTE_THRESHOLD = 0.1 is a hard gate.** Notes flicker in and out of the sum as their chroma values cross 0.1 — each crossing is a discontinuity in the oscillator term count, producing a snap in the oscillation derivative even when the tanh-saturated value is stable.

**The K1 effect has zero `BeatDetectionAPI` consumption.** No `getBeatOscillation`, no `getBeatStrength`, no `oscillation_velocity`, no `beat_phase`, no `tempo_confidence`, no `in_silence`. The audio surface used is `ctx.audio.rms()` and `ctx.audio.getChroma(i)` only.

### 6.2 Smoothing strategy

**Canonical:** Velocity-Verlet integration (`smoothed += velocity × dt`, then 0.9/0.1 mix-back; INTEGRATION_EXAMPLES.md:91-94) OR frame-rate-independent EMA (`smoothing_rate = 1 - exp(-dt × 10)`; INTEGRATION_EXAMPLES.md:1018). Snap factor scales with beat strength (`2.0 + beat_strength × 1.0`; INTEGRATION_EXAMPLES.md:97).

**K1 current (`SnapwaveLinearEffect.cpp:230-246, 256`):**
```cpp
float currentPeak = ctx.audio.rms();
float peakSmoothed = m_peakFollower[z].update(currentPeak, dt);  // 20ms attack, 200ms release
float oscillation = computeOscillation(ctx);
float amp = oscillation * peakSmoothed * AMPLITUDE_MIX;          // AMPLITUDE_MIX = 0.7
```
The peak (RMS) is asymmetric-followed, but the oscillation itself is *not* smoothed — it is computed fresh every frame from the raw chord-summed sin and tanh-saturated. The asymmetric follower applies only to the amplitude scalar. The oscillation derivative is therefore as discontinuous as the chord changes themselves. There is no velocity term, no inertia, no `1 - exp(-dt × ω)` form.

### 6.3 Beat-driven amplitude modulation

**Canonical:** `position *= (1.0f + beat_strength × 0.3f)` for basic; `1.0f + beat_strength × 0.5f` for enhanced; bursts on `beat_detected && beat_strength > 0.6f`. (INTEGRATION_EXAMPLES.md:36-38, BEAT_DETECTION_API.md:373-374, INTEGRATION_EXAMPLES.md:130)

**K1 current:** Absent. There is no `beat_detected` consumption, no burst rendering, no rising-edge logic. Strong beats produce no special visual response; weak beats produce no different response from background motion.

### 6.4 Phase prediction

**Canonical:** `position += sin(beat_phase × 2π) × 0.1f × tempo_confidence` gated on `tempo_confidence > 0.5`. (INTEGRATION_EXAMPLES.md:101-104)

**K1 current:** Absent. No tempo, no phase, no prediction.

### 6.5 Silence handling

**Canonical:** Latched `in_silence` flag (3-second timeout) → halve position amplitude AND raise trail persistence to 0.96. (INTEGRATION_EXAMPLES.md:118-125, BEAT_DETECTION_API.md:272)

**K1 current (`SnapwaveLinearEffect.cpp:114-117`):**
```cpp
float rms = ctx.audio.rms();
if (rms < ENERGY_GATE_THRESHOLD) {
    return 0.0f;  // Silence = stillness
}
```
A bare per-frame RMS threshold with no latching. RMS dips below 0.05 even during music (between transients, in mid-bar rests) — this gate flickers, *adding* discontinuity rather than smoothing the silence transition. Trail fade has separate dynamic logic (`fadeAmount = 20 + 40 × (1.0 - smoothRms)`, line 226) that is similarly RMS-driven, not silence-flag-driven.

### 6.6 Trail-fade modulation

**Canonical:** `fade_rate = 0.92 + total_energy × 0.06` (basic) or `0.88 + beat_strength × 0.10` (enhanced). Trail strengthens with energy. (INTEGRATION_EXAMPLES.md:59, 115)

**K1 current:** `fadeAmount = 20 + 40 × (1.0 - smoothRms)` (line 226). Inverted logic — *louder* audio means *less* fade per frame, which *weakens* the trail when energy is high. This is the opposite of the canonical relationship.

### 6.7 Frequency-band consumption

**Canonical:** `bass_energy`, `mid_energy`, `high_energy`, `total_energy` drive colour and trail fade; advanced model uses them to modulate position (INTEGRATION_EXAMPLES.md:281-285).

**K1 current:** Not consumed. The K1 audio context likely surfaces these via `bands[0..7]` (per project memory), but the effect ignores them entirely.

### 6.8 Harmonic blend

**Canonical:** `harmonic_blend[4]`, weighted 1, 1/2, 1/3, 1/4 sum, optional richness for advanced model (INTEGRATION_EXAMPLES.md:270-275).

**K1 current:** Absent.

### 6.9 Summary table

| Canonical pattern | K1 status | Spazz contribution |
|---|---|---|
| Single API-derived `oscillation_value` | ❌ Absent — replaced by 12-sin chord summer | **Primary cause** |
| `oscillation_velocity` smoothing | ❌ Absent | High |
| Tempo-confidence-gated phase prediction | ❌ Absent | Low (would help, not required) |
| Latched `in_silence` (3 s) mode switch | ❌ Per-frame RMS gate flickers | Medium |
| Beat-strength amplitude modulation (capped) | ❌ Absent | Medium |
| Rising-edge `beat_detected` burst rendering | ❌ Absent | Low (visual richness) |
| Frame-rate-independent smoothing (`1 − exp(−dt × ω)`) | ❌ AsymmetricFollower applied to RMS only, not oscillation | High |
| Energy-positive trail-fade relation | ❌ Inverted — louder = less trail | Visual quality |
| Frequency-band colour blend | ❌ Uses chroma colour blend instead | Visual fidelity (not spazz) |
| Centre-origin LED mapping | ✅ Present (lines 96-103) | n/a |
| Zero-heap render path | ✅ Present (PSRAM struct in init only) | n/a |

---

## 7. Recommendation — what MUST be adopted to fix the spazz

The spazz is structural, not parametric. Tuning `TANH_SCALE`, `NOTE_THRESHOLD`, `PHASE_SPREAD`, or `BASE_FREQUENCY` will not fix it because the oscillation source is the wrong shape. Two adoption tiers follow.

### Tier 1 — MUST adopt (fixes spazz directly)

1. **Replace `computeOscillation` with a beat-detection-derived oscillator.** The K1 audio chain already has tempo / beat / onset infrastructure (per project memory: `ControlBus.beat`, `onset`, tempo fields). A `BeatState`-equivalent `oscillation_value` must be produced *upstream of the effect* — either by adding a Snapwave-spec `getBeatOscillation()` to ControlBus, or by deriving it inline from existing tempo + onset signals — and consumed as a single scalar. The 12-sin chord summer must go.
2. **Apply velocity-integration or `1 − exp(−dt × ω)` smoothing to the consumed oscillation, with ω ≈ 10 Hz.** This decouples motion smoothness from frame rate and eliminates the chord-step discontinuities. Reference equation: `smoothed += (target − smoothed) × (1 − exp(−dt × 10.0f))` (INTEGRATION_EXAMPLES.md:1013-1026).
3. **Latch silence with a 3-second timeout, not per-frame RMS.** Replace the `if (rms < 0.05) return 0` gate with `in_silence`-style state: enter silence after `rms < threshold` for `silence_timeout` ms, exit immediately on RMS rise. In silence, halve oscillation amplitude (do not zero it) and raise trail persistence.
4. **Invert the trail-fade-vs-energy relationship.** Energy *up* → trail *longer* (lower per-frame fade). Replace `fadeAmount = 20 + 40 × (1.0 − smoothRms)` with `fadeAmount = max(8, 60 × (1.0 − energyTerm))` or the canonical `fade_rate = 0.92 + energy × 0.06` mapped to the K1 fade scale.

### Tier 2 — SHOULD adopt (visual richness, not spazz fix)

5. **Beat-strength amplitude bonus, capped:** `amp *= (1.0f + beat_strength × 0.3f)` once a beat-strength scalar is available.
6. **Rising-edge burst rendering on `beat_detected && beat_strength > 0.6`** — strong beats trigger a wider radius for one or two frames.
7. **Tempo-confidence-gated phase prediction** — once tempo confidence and beat phase are available, add `position += sin(phase × 2π) × 0.1 × confidence` when `confidence > 0.5`.

### Tier 3 — NICE-TO-HAVE

8. **Frequency-band colour blend** (bass / mid / high → R / G / B) instead of (or alongside) the chromagram colour path.
9. **`harmonic_blend[4]` richness term** for the advanced multi-harmonic motion if the K1 audio chain can supply it.

### Decoupling principle (Captain decision-relevant fact)

The single architectural fact the canonical doctrine enforces — and that K1 violates — is: **rate (oscillation frequency) and amplitude (RMS / beat strength) are produced by separate code paths and combined multiplicatively, never additively, never with rate dependent on chord identity.** The canonical Snapwave's "rate" comes from a tempo-locked oscillator inside the API; its "amplitude" comes from `beat_strength`. K1 currently couples rate to chord identity via the per-note `freqMult = 1.0 + 0.5 × i` term, so chord changes *change the speed of the dot*. This is the doctrinal violation underneath the surface symptom of "spazzing".

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:claude-opus-4-7 | Created. Distilled SNAPWAVE_BEAT_DETECTION_API.md + SNAPWAVE_INTEGRATION_EXAMPLES.md into canonical Snapwave + Beat-API doctrine. Performed gap analysis against firmware-v3/src/effects/ieffect/SnapwaveLinearEffect.cpp. Identified rate/amplitude coupling via chord-summed sin oscillator as the structural root cause of spazz/jerk. Tiered adoption recommendations issued (Tier 1 fixes spazz, Tier 2 adds visual richness, Tier 3 nice-to-have). |
