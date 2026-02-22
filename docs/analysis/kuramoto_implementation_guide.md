# Kuramoto Effect Implementation Guide
## Concrete Code Patterns for LightwaveOS v2

**Companion to**: `kuramoto_research.md`
**Target**: ESP32-S3 firmware with IEffect API
**Date**: 2026-02-08

---

## 1. File Structure

```
firmware/v2/src/effects/ieffect/
├── KuramotoEffect.h         # Class declaration
├── KuramotoEffect.cpp       # Implementation
└── CMakeLists.txt           # Build registration (if using CMake)

firmware/v2/src/effects/
└── EffectRegistry.cpp       # Effect ID registration
```

---

## 2. Header File Template

```c++
/**
 * @file KuramotoEffect.h
 * @brief Kuramoto Coupled Oscillators - Emergent synchronization patterns
 *
 * Implements the Kuramoto model of phase-coupled oscillators on a 1D ring.
 * Audio acts as steering signals (tempo, phase gradient, perturbations)
 * rather than direct visual mapping, creating stateful emergent dynamics.
 *
 * Effect ID: 122 (TBD - verify MAX_EFFECTS and registry)
 *
 * Visual Regimes:
 * - Travelling interference bands (partial sync, moderate coupling)
 * - Breathing sync clusters (high coupling, uniform frequencies)
 * - Phase slips and tears (flux perturbations)
 * - Chaotic flicker (low coupling, high frequency spread)
 *
 * Design Brief: Prototype #1 for stateful dynamical effects
 * Acceptance: A1-A4 (motion without audio, fps-independent, regime diversity, no white wash)
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos::effects::ieffect {

class KuramotoEffect final : public plugins::IEffect {
public:
    KuramotoEffect();
    ~KuramotoEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    // ========================================================================
    // Oscillator System
    // ========================================================================

    static constexpr int NUM_OSC = 80;  ///< One oscillator per radial position

    // Dynamic state (PSRAM allocation: 320 bytes)
    float m_theta[NUM_OSC];    ///< Current phase [0, 2π) radians
    float m_omega[NUM_OSC];    ///< Natural frequencies (rad/s, relative to base)

    // Parameters (tunable via setParameter)
    float m_coupling;          ///< K: Coupling strength (0.0 - 2.0, typical 0.8-1.2)
    float m_freqSpread;        ///< σ: Natural frequency std dev (0.0 - 0.5, typical 0.1)
    float m_kickStrength;      ///< Perturbation gain for flux events (0.0 - 1.0)
    float m_paletteSpeed;      ///< Palette rotation speed (cycles/second, 0.0 - 1.0)

    // Slow state
    float m_palettePhase;      ///< Palette rotation accumulator [0, 1)

    // Coherence cache (avoid recomputation)
    float m_localCoherence[NUM_OSC];  ///< Per-oscillator sync measure

    // Metadata
    plugins::EffectMetadata m_meta;

    // ========================================================================
    // Internal Helpers
    // ========================================================================

    /**
     * @brief Compute local phase coherence in neighborhood radius
     * @param i Oscillator index
     * @param radius Neighborhood half-width (e.g., 3 → 7 oscillators total)
     * @return r_local ∈ [0, 1] where 1 = fully synchronized
     */
    float computeLocalCoherence(int i, int radius);

    /**
     * @brief Map oscillator phase to LED color with coherence modulation
     * @param theta Phase angle [0, 2π)
     * @param r_local Local coherence [0, 1]
     * @param palette Palette reference
     * @param palette_offset Global palette rotation offset [0, 1)
     * @return CRGB color with brightness scaled by coherence
     */
    CRGB phaseToColor(float theta, float r_local,
                      plugins::PaletteRef& palette, float palette_offset);

    /**
     * @brief Initialize natural frequencies with Gaussian distribution
     * @param mean Mean frequency (typically 1.0, will be scaled by BPM)
     * @param sigma Standard deviation (m_freqSpread)
     */
    void initializeFrequencies(float mean, float sigma);

    /**
     * @brief Apply random phase kicks to subset of oscillators
     * @param strength Kick magnitude (radians)
     * @param probability Probability [0, 1] each oscillator gets kicked
     */
    void applyPerturbations(float strength, float probability);
};

} // namespace lightwaveos::effects::ieffect
```

---

## 3. Key Implementation Snippets

### 3.1 Initialization (Random Phases + Gaussian Frequencies)

```c++
bool KuramotoEffect::init(EffectContext& ctx) {
    // Random initial phases (breaks symmetry, triggers transient dynamics)
    for (int i = 0; i < NUM_OSC; i++) {
        m_theta[i] = random16() * (TWO_PI / 65536.0f);
    }

    // Initialize natural frequencies with Gaussian spread
    initializeFrequencies(1.0f, m_freqSpread);

    // Zero slow state
    m_palettePhase = 0.0f;

    return true;
}

void KuramotoEffect::initializeFrequencies(float mean, float sigma) {
    // Box-Muller transform for Gaussian random numbers
    for (int i = 0; i < NUM_OSC; i += 2) {
        // Generate two independent Gaussian samples
        float u1 = random16() / 65536.0f;
        float u2 = random16() / 65536.0f;

        float r = sqrtf(-2.0f * logf(u1 + 1e-8f));  // Avoid log(0)
        float theta = TWO_PI * u2;

        m_omega[i] = mean + sigma * r * cosf(theta);
        if (i + 1 < NUM_OSC) {
            m_omega[i + 1] = mean + sigma * r * sinf(theta);
        }
    }
}
```

**Why Gaussian?** Natural systems have bell-curve frequency distributions. This creates realistic critical coupling behavior.

### 3.2 Core Dynamics Update (Euler Integration)

```c++
void KuramotoEffect::render(EffectContext& ctx) {
    float dt = ctx.getSafeDeltaSeconds();  // Clamped [0.0001s, 0.05s]

    // ========================================================================
    // Audio Steering (Lightweight - no rendering yet)
    // ========================================================================

#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // Base rotation rate from BPM (convert to rad/s)
        float bpm = ctx.audio.musicalGrid.bpm;
        float base_omega = (bpm / 60.0f) * TWO_PI;  // ~6.28 rad/s at 60 BPM

        // Phase gradient from spectral centroid
        float centroid = ctx.audio.controlBus.spectral_centroid;
        float wind_strength = 2.0f * (centroid - 0.5f);  // [-1, +1]

        // Coupling modulation from RMS (quiet = weak coupling, loud = strong)
        float rms = ctx.audio.rms();
        float K_current = m_coupling * (0.5f + 0.5f * rms);

        // ====================================================================
        // Kuramoto Dynamics (Nearest-Neighbor Coupling)
        // ====================================================================

        float dtheta[NUM_OSC];

        for (int i = 0; i < NUM_OSC; i++) {
            // Spatial position for gradient bias
            float spatial_pos = (float)i / NUM_OSC;  // 0.0 to 1.0

            // Natural frequency with audio steering
            float omega_i = base_omega * m_omega[i]  // Scale by relative frequency
                          + wind_strength * spatial_pos;  // Add phase gradient

            // Nearest-neighbor coupling (ring topology)
            int i_prev = (i - 1 + NUM_OSC) % NUM_OSC;
            int i_next = (i + 1) % NUM_OSC;

            float coupling_force = sinf(m_theta[i_next] - m_theta[i])
                                 + sinf(m_theta[i_prev] - m_theta[i]);

            // Kuramoto equation: dθ/dt = ω + (K/2) Σ sin(θ_j - θ_i)
            dtheta[i] = omega_i + 0.5f * K_current * coupling_force;
        }

        // Forward Euler integration
        for (int i = 0; i < NUM_OSC; i++) {
            m_theta[i] += dtheta[i] * dt;

            // Wrap phase to [0, 2π)
            if (m_theta[i] < 0.0f) m_theta[i] += TWO_PI;
            if (m_theta[i] >= TWO_PI) m_theta[i] -= TWO_PI;
        }

        // ====================================================================
        // Perturbations (Flux Kicks)
        // ====================================================================

        float flux = ctx.audio.fastFlux();
        if (flux > 0.7f) {  // Threshold for significant onset
            float kick_prob = 0.25f;  // 25% of oscillators get kicked
            applyPerturbations(m_kickStrength * flux, kick_prob);
        }
    } else {
#endif
        // ====================================================================
        // No Audio Mode (Autonomous Dynamics)
        // ====================================================================

        // Fixed base rotation (1 Hz = 1 cycle/second)
        float base_omega = TWO_PI;

        float dtheta[NUM_OSC];
        for (int i = 0; i < NUM_OSC; i++) {
            float omega_i = base_omega * m_omega[i];

            int i_prev = (i - 1 + NUM_OSC) % NUM_OSC;
            int i_next = (i + 1) % NUM_OSC;

            float coupling_force = sinf(m_theta[i_next] - m_theta[i])
                                 + sinf(m_theta[i_prev] - m_theta[i]);

            dtheta[i] = omega_i + 0.5f * m_coupling * coupling_force;
        }

        for (int i = 0; i < NUM_OSC; i++) {
            m_theta[i] += dtheta[i] * dt;
            if (m_theta[i] < 0.0f) m_theta[i] += TWO_PI;
            if (m_theta[i] >= TWO_PI) m_theta[i] -= TWO_PI;
        }

#if FEATURE_AUDIO_SYNC
    }
#endif

    // ========================================================================
    // Coherence Computation (for brightness modulation)
    // ========================================================================

    int coherence_radius = 3;  // 7 neighbors total (3 left, self, 3 right)
    for (int i = 0; i < NUM_OSC; i++) {
        m_localCoherence[i] = computeLocalCoherence(i, coherence_radius);
    }

    // ========================================================================
    // Palette Rotation (Slow Timescale)
    // ========================================================================

    m_palettePhase += m_paletteSpeed * dt;
    if (m_palettePhase >= 1.0f) m_palettePhase -= 1.0f;

    // ========================================================================
    // Rendering (Phase → Color with Coherence Brightness)
    // ========================================================================

    for (int i = 0; i < NUM_OSC; i++) {
        CRGB color = phaseToColor(m_theta[i], m_localCoherence[i],
                                  ctx.palette, m_palettePhase);

        // Mirror to both sides (centre origin: 79/80 split)
        ctx.leds[79 - i] = color;
        ctx.leds[80 + i] = color;
    }
}
```

### 3.3 Local Coherence Computation

```c++
float KuramotoEffect::computeLocalCoherence(int i, int radius) {
    // Compute local order parameter: z = (1/N) Σ e^(iθ_j)
    float sum_cos = cosf(m_theta[i]);
    float sum_sin = sinf(m_theta[i]);
    int count = 1;

    for (int offset = 1; offset <= radius; offset++) {
        // Left neighbor
        int j_left = (i - offset + NUM_OSC) % NUM_OSC;
        sum_cos += cosf(m_theta[j_left]);
        sum_sin += sinf(m_theta[j_left]);
        count++;

        // Right neighbor
        int j_right = (i + offset) % NUM_OSC;
        sum_cos += cosf(m_theta[j_right]);
        sum_sin += sinf(m_theta[j_right]);
        count++;
    }

    // Magnitude of complex average: r = |z|
    float r_local = sqrtf(sum_cos * sum_cos + sum_sin * sum_sin) / count;

    return r_local;
}
```

### 3.4 Phase-to-Color Rendering

```c++
CRGB KuramotoEffect::phaseToColor(float theta, float r_local,
                                   plugins::PaletteRef& palette,
                                   float palette_offset) {
    // Map phase [0, 2π) to palette index [0, 255]
    uint8_t palette_index = (uint8_t)((theta / TWO_PI) * 255.0f);

    // Add slow palette rotation
    uint8_t offset_index = (uint8_t)(palette_offset * 255.0f);
    palette_index += offset_index;  // Wraps automatically (uint8_t overflow)

    // Sample palette
    CRGB color = palette.getColor(palette_index);

    // Modulate brightness by local coherence
    // Low coherence (chaos) → dim (25% brightness)
    // High coherence (sync) → bright (100% brightness)
    uint8_t brightness_min = 64;   // 25% floor
    uint8_t brightness_max = 255;  // 100% ceiling

    uint8_t brightness = brightness_min +
                         (uint8_t)((brightness_max - brightness_min) * r_local);

    color.nscale8(brightness);

    return color;
}
```

### 3.5 Perturbations (Phase Kicks)

```c++
void KuramotoEffect::applyPerturbations(float strength, float probability) {
    uint8_t prob_threshold = (uint8_t)(probability * 255.0f);

    for (int i = 0; i < NUM_OSC; i++) {
        if (random8() < prob_threshold) {
            // Random kick direction
            float kick_sign = (random8() < 128) ? 1.0f : -1.0f;
            m_theta[i] += kick_sign * strength * (random8() / 255.0f);

            // Keep phase in [0, 2π)
            if (m_theta[i] < 0.0f) m_theta[i] += TWO_PI;
            if (m_theta[i] >= TWO_PI) m_theta[i] -= TWO_PI;
        }
    }
}
```

---

## 4. Parameter Configuration

### 4.1 Effect Parameters (Tunable via Web UI)

```c++
uint8_t KuramotoEffect::getParameterCount() const {
    return 4;
}

const plugins::EffectParameter* KuramotoEffect::getParameter(uint8_t index) const {
    static const plugins::EffectParameter params[] = {
        {"coupling",      0.0f, 2.0f, 1.0f, "Coupling Strength (K)"},
        {"freq_spread",   0.0f, 0.5f, 0.1f, "Frequency Spread (σ)"},
        {"kick_strength", 0.0f, 1.0f, 0.5f, "Perturbation Gain"},
        {"palette_speed", 0.0f, 1.0f, 0.5f, "Palette Rotation Speed"},
    };

    return (index < 4) ? &params[index] : nullptr;
}

bool KuramotoEffect::setParameter(const char* name, float value) {
    if (strcmp(name, "coupling") == 0) {
        m_coupling = constrain(value, 0.0f, 2.0f);
        return true;
    } else if (strcmp(name, "freq_spread") == 0) {
        m_freqSpread = constrain(value, 0.0f, 0.5f);
        // Re-initialize frequencies if spread changes
        initializeFrequencies(1.0f, m_freqSpread);
        return true;
    } else if (strcmp(name, "kick_strength") == 0) {
        m_kickStrength = constrain(value, 0.0f, 1.0f);
        return true;
    } else if (strcmp(name, "palette_speed") == 0) {
        m_paletteSpeed = constrain(value, 0.0f, 1.0f);
        return true;
    }
    return false;
}

float KuramotoEffect::getParameter(const char* name) const {
    if (strcmp(name, "coupling") == 0) return m_coupling;
    if (strcmp(name, "freq_spread") == 0) return m_freqSpread;
    if (strcmp(name, "kick_strength") == 0) return m_kickStrength;
    if (strcmp(name, "palette_speed") == 0) return m_paletteSpeed;
    return 0.0f;
}
```

### 4.2 Parameter Recommendations

| Parameter | Low | Medium | High | Effect on Visuals |
|-----------|-----|--------|------|-------------------|
| **coupling** | 0.3 | 1.0 | 1.5 | Low = chaos, High = full sync |
| **freq_spread** | 0.0 | 0.1 | 0.3 | Low = uniform, High = diverse |
| **kick_strength** | 0.1 | 0.5 | 1.0 | Low = gentle, High = violent tears |
| **palette_speed** | 0.1 | 0.5 | 0.9 | Low = slow drift, High = fast churn |

**Sweet spot for first demo**: coupling=1.0, freq_spread=0.1, kick_strength=0.5, palette_speed=0.5

---

## 5. Effect Metadata Registration

```c++
KuramotoEffect::KuramotoEffect() {
    m_meta.id = 122;  // TBD: Verify next available ID in EffectRegistry
    m_meta.name = "Kuramoto Sync";
    m_meta.description = "Coupled oscillator synchronization with emergent patterns";
    m_meta.category = plugins::EffectCategory::AUDIO_REACTIVE;
    m_meta.flags = plugins::EffectFlags::SUPPORTS_AUDIO
                 | plugins::EffectFlags::CENTER_ORIGIN;

    // Default parameters
    m_coupling = 1.0f;
    m_freqSpread = 0.1f;
    m_kickStrength = 0.5f;
    m_paletteSpeed = 0.5f;
    m_palettePhase = 0.0f;
}

const plugins::EffectMetadata& KuramotoEffect::getMetadata() const {
    return m_meta;
}
```

### 5.1 Registry Entry (in `EffectRegistry.cpp`)

```c++
// Add to effect registration list
#include "effects/ieffect/KuramotoEffect.h"

void EffectRegistry::registerBuiltinEffects() {
    // ... existing effects ...

    registerEffect(std::make_unique<effects::ieffect::KuramotoEffect>());

    // ... more effects ...
}
```

---

## 6. Memory Budget

| Allocation | Size | Location |
|------------|------|----------|
| `m_theta[80]` | 320 bytes | Stack/PSRAM |
| `m_omega[80]` | 320 bytes | Stack/PSRAM |
| `m_localCoherence[80]` | 320 bytes | Stack/PSRAM |
| Parameters | 16 bytes | Stack |
| Metadata | ~64 bytes | Stack |
| **Total** | **~1040 bytes** | Well within budget |

**ESP32-S3 PSRAM**: 8 MB available, current firmware uses ~2 MB → **0.01% impact**

---

## 7. Testing Strategy

### 7.1 Unit Tests (No Hardware)

```c++
TEST(KuramotoEffect, InitializesRandomPhases) {
    EffectContext ctx = createMockContext();
    KuramotoEffect effect;

    effect.init(ctx);

    // Verify phases are in [0, 2π) and not all identical
    bool all_same = true;
    float first_phase = /* access m_theta[0] via friend or getter */;
    for (int i = 1; i < 80; i++) {
        float phase = /* m_theta[i] */;
        EXPECT_GE(phase, 0.0f);
        EXPECT_LT(phase, TWO_PI);
        if (phase != first_phase) all_same = false;
    }
    EXPECT_FALSE(all_same);  // Should be randomized
}

TEST(KuramotoEffect, ConvergesToSyncWithHighCoupling) {
    EffectContext ctx = createMockContext();
    KuramotoEffect effect;

    effect.init(ctx);
    effect.setParameter("coupling", 2.0f);  // Very high coupling

    // Simulate 10 seconds at 120 FPS
    for (int frame = 0; frame < 1200; frame++) {
        ctx.deltaTimeSeconds = 1.0f / 120.0f;
        effect.render(ctx);
    }

    // Compute global coherence
    float r = computeGlobalCoherence(/* m_theta */);
    EXPECT_GT(r, 0.9f);  // Should be highly synchronized
}
```

### 7.2 Hardware Validation Checklist

- [ ] **Visual**: Patterns emerge within 3-5 seconds of init
- [ ] **Audio**: BPM change (60→120) doubles rotation speed
- [ ] **Audio**: Bass-heavy songs create inward gradient, treble-heavy outward
- [ ] **Audio**: Flux peaks cause visible "tears" in pattern
- [ ] **Params**: coupling=0.5 → chaotic, coupling=1.5 → synchronized
- [ ] **Performance**: Sustained 120 FPS with <2ms render time
- [ ] **Memory**: No heap fragmentation after 1 hour runtime

---

## 8. Debugging Tools

### 8.1 Serial Debug Commands

Add to effect's command handler (if applicable):

```c++
void KuramotoEffect::handleDebugCommand(const char* cmd) {
    if (strcmp(cmd, "k_dump") == 0) {
        // Dump oscillator state
        Serial.println("Kuramoto State Dump:");
        for (int i = 0; i < NUM_OSC; i += 10) {  // Sample every 10th
            Serial.printf("  osc[%d]: theta=%.2f, omega=%.2f, r_local=%.2f\n",
                          i, m_theta[i], m_omega[i], m_localCoherence[i]);
        }

        // Global coherence
        float r_global = computeGlobalCoherence();
        Serial.printf("  Global coherence: %.3f\n", r_global);
    } else if (strcmp(cmd, "k_kick") == 0) {
        // Manual perturbation
        applyPerturbations(1.0f, 0.5f);
        Serial.println("Applied manual perturbation");
    } else if (strcmp(cmd, "k_reset") == 0) {
        // Re-randomize phases
        init(/* ctx */);
        Serial.println("Reset oscillator phases");
    }
}
```

### 8.2 LED Diagnostics

Temporary rendering override for debugging:

```c++
// In render(), replace normal rendering with:
#ifdef KURAMOTO_DEBUG_RENDER
    for (int i = 0; i < NUM_OSC; i++) {
        // Phase → hue (red=0, green=120, blue=240)
        uint8_t hue = (uint8_t)((m_theta[i] / TWO_PI) * 255.0f);

        // Coherence → saturation (low sat = chaos, high sat = sync)
        uint8_t sat = (uint8_t)(m_localCoherence[i] * 255.0f);

        // Fixed brightness
        CRGB color = CHSV(hue, sat, 255);

        ctx.leds[79 - i] = color;
        ctx.leds[80 + i] = color;
    }
#endif
```

---

## 9. Optimization Opportunities

### 9.1 SIMD Vectorization (Future)

ESP32-S3 lacks hardware SIMD, but manual unrolling can help:

```c++
// Unroll loop by 4 for better pipelining
for (int i = 0; i < NUM_OSC; i += 4) {
    float theta0 = m_theta[i];
    float theta1 = m_theta[i+1];
    float theta2 = m_theta[i+2];
    float theta3 = m_theta[i+3];

    // Compute all 4 coupling forces in parallel registers
    // (compiler may auto-vectorize)
    dtheta[i]   = omega[i]   + coupling_term(theta0, ...);
    dtheta[i+1] = omega[i+1] + coupling_term(theta1, ...);
    dtheta[i+2] = omega[i+2] + coupling_term(theta2, ...);
    dtheta[i+3] = omega[i+3] + coupling_term(theta3, ...);
}
```

**Gain**: ~10-15% speedup from reduced loop overhead

### 9.2 Lookup Table for sin()

FastLED provides `sin16()` and `cos16()` with 16-bit precision:

```c++
// Replace: float coupling = sinf(theta_diff);
// With:    float coupling = (sin16(theta_diff * 10430) / 32768.0f);
//          (10430 ≈ 32768 / π for scaling)

int16_t sin16_kuramoto(float theta) {
    int16_t angle = (int16_t)(theta * 10430.2f);  // rad → sin16 input
    return sin16(angle);
}
```

**Gain**: ~2× faster than `sinf()` (LUT vs FPU)

### 9.3 Reduce Coherence Radius

If performance is tight:
- radius=3 → 7 oscillators (current)
- radius=2 → 5 oscillators (~30% faster)
- radius=1 → 3 oscillators (~60% faster)

Trade-off: Smaller radius = less smooth brightness transitions

---

## 10. Next Steps After Implementation

### 10.1 Validation Against Design Brief

Run acceptance tests (from research doc Section 6):

1. **A1: Motion Without Audio**
   - Disable audio input, run for 30 seconds
   - Verify travelling waves or sync clusters emerge

2. **A2: FPS Independence**
   - Lock to 60 FPS, record video
   - Lock to 120 FPS, record video
   - Compare: motion should look identical (no speed change)

3. **A3: Regime Diversity**
   - Test with 3 songs: EDM, ambient, jazz
   - Verify qualitatively different patterns emerge

4. **A4: No White Wash**
   - Run at full RMS (sine wave input)
   - Verify no all-white frames (palette rotation prevents)

### 10.2 User Testing

Add to web UI effect picker with description:
> "**Kuramoto Sync** — Living synchronization patterns. Watch oscillators dance together and apart, steered by the music's energy and timbre."

Collect feedback on:
- Perceived "aliveness" vs existing effects
- Preferred parameter ranges for different music genres
- Any unexpected visual artifacts

### 10.3 Documentation

Update project docs:
- Add to `docs/agent/EFFECTS.md` with implementation notes
- Add parameter guide to user manual
- Cross-reference design brief (#28113 observation)

---

## 11. Common Pitfalls

### ❌ Pitfall 1: Phase Explosion

**Symptom**: Phases grow unbounded (theta >> 2π)

**Cause**: Forgot to wrap phase after integration

**Fix**:
```c++
m_theta[i] += dtheta[i] * dt;
m_theta[i] = fmodf(m_theta[i], TWO_PI);  // CRITICAL
if (m_theta[i] < 0.0f) m_theta[i] += TWO_PI;
```

### ❌ Pitfall 2: Timestep Instability

**Symptom**: Visual "judder" at high BPM or during frame drops

**Cause**: Euler integration with large `dt`

**Fix**: Already handled by `ctx.getSafeDeltaSeconds()` clamping to 50ms max

### ❌ Pitfall 3: All-to-All Coupling by Mistake

**Symptom**: Render time >5ms, FPS drops to 60

**Cause**: Nested loop over all oscillators

**Fix**: Verify only nearest neighbors (i-1, i+1) in coupling sum

### ❌ Pitfall 4: Audio Age Drift

**Symptom**: Patterns lag behind beat by ~100ms

**Cause**: Using stale audio data

**Fix**: Already handled by `ctx.audio.available` check and extrapolated beat phase

---

**END OF IMPLEMENTATION GUIDE**
