# Kuramoto Coupled Oscillator Model Research
## LED Effect Implementation Analysis

**Date**: 2026-02-08
**Purpose**: Research foundation for implementing Kuramoto-style coupled oscillator effect
**Target System**: LightwaveOS v2 (ESP32-S3, 320 WS2812 LEDs, 120 FPS, PSRAM)

---

## Executive Summary

The Kuramoto model is a canonical system from nonlinear dynamics that produces **emergent synchronization** in populations of coupled oscillators. Unlike beat-pulse effects that directly map audio to visual parameters, Kuramoto creates a **living dynamical system** where audio acts as environmental steering signals. The system exhibits qualitatively distinct regimes:

- **Travelling waves** (interference bands moving around the ring)
- **Sync clusters** (groups of oscillators locking phase while others drift)
- **Phase slips** (sudden tears and re-locks during perturbations)
- **Partial synchronization** (chimera states with coexisting order/disorder)

This matches the design brief requirement: **stateful effects with memory** where motion remains interesting even without audio input.

---

## 1. Core Mathematics

### 1.1 The Kuramoto Equation

The fundamental equation governing oscillator *i*:

```
dθ_i/dt = ω_i + (K/N) Σ_{j=1}^{N} sin(θ_j - θ_i)
```

**Parameters**:
- `θ_i(t)`: Phase of oscillator *i* (radians, typically 0 to 2π)
- `ω_i`: Natural (intrinsic) frequency of oscillator *i* (rad/s)
- `K`: Coupling strength (global interaction gain)
- `N`: Total number of oscillators

**Physics**:
- **First term** (ω_i): Each oscillator tries to rotate at its own natural frequency
- **Second term**: Coupling forces oscillator *i* toward the average phase of all others
- **sin(θ_j - θ_i)**: Sinusoidal coupling creates phase attraction (positive when *j* leads *i*)

### 1.2 Order Parameter (Global Coherence)

The **Kuramoto order parameter** quantifies global synchronization:

```
z = r·e^(iψ) = (1/N) Σ_{j=1}^{N} e^(iθ_j)
```

Where:
- `r = |z|`: **Phase coherence** (0 = incoherent, 1 = fully synchronized)
- `ψ = arg(z)`: **Mean phase** (average phase of synchronized oscillators)

This can be computed efficiently:

```c++
float sum_cos = 0.0f, sum_sin = 0.0f;
for (int i = 0; i < N; i++) {
    sum_cos += cosf(theta[i]);
    sum_sin += sinf(theta[i]);
}
float r = sqrtf(sum_cos*sum_cos + sum_sin*sum_sin) / N;
float psi = atan2f(sum_sin, sum_cos);
```

### 1.3 Synchronization Regimes

The system exhibits a **phase transition** as coupling strength increases:

| Coupling K | Regime | Visual Appearance |
|------------|--------|-------------------|
| K < K_c | **Incoherent** (r ≈ 0) | Random flickering, no patterns |
| K ≈ K_c | **Partial sync** (0 < r < 1) | Drifting clusters, breathing sync |
| K > K_c | **Full sync** (r → 1) | Solid color rotation, travelling waves |

**Critical coupling** K_c depends on the distribution of natural frequencies:
- Narrow frequency spread → lower K_c (easy to sync)
- Wide frequency spread → higher K_c (hard to sync)

For Lorentzian distribution with width γ:
```
K_c = 2γ / π
```

---

## 2. Computational Efficiency

### 2.1 Problem: N² Complexity

The naive all-to-all coupling requires **O(N²)** operations per timestep:

```c++
// BAD: 160² = 25,600 operations per frame at 120 FPS = 3,072,000 ops/sec
for (int i = 0; i < 160; i++) {
    float coupling = 0.0f;
    for (int j = 0; j < 160; j++) {
        coupling += sin(theta[j] - theta[i]);
    }
    dtheta[i] = omega[i] + (K/160.0f) * coupling;
}
```

At 120 FPS with 2 ms render budget, this is **borderline** but wastes cycles.

### 2.2 Solution: Local Coupling

**Strategy**: Couple each oscillator only to **nearest neighbors** on the ring topology.

```
dθ_i/dt = ω_i + (K/2) [sin(θ_{i+1} - θ_i) + sin(θ_{i-1} - θ_i)]
```

**Complexity**: O(N) — 320 operations per frame (160 oscillators × 2 neighbors)

**Benefits**:
- **10× faster** than all-to-all
- Still produces **travelling waves** (proven in literature)
- **Spatially localized** synchronization creates richer patterns

**Research evidence**:
- Synchronized oscillations on ring topologies exhibit travelling wave solutions with fixed winding number *m*
- Phase waves propagate around the ring even with nearest-neighbor coupling
- Multiple stable states (different winding numbers) can coexist

### 2.3 Radial Symmetry Optimization

Since LEDs are mirrored (0↔159, 1↔158, ..., 79↔80), use **80 oscillators** for 160 LEDs:

```c++
const int NUM_OSC = 80;  // One per radial position
float theta[NUM_OSC];    // Phase state

void render(EffectContext& ctx) {
    // Update oscillators (80 iterations)
    for (int i = 0; i < NUM_OSC; i++) { ... }

    // Map to LEDs with mirroring (160 iterations)
    for (int i = 0; i < NUM_OSC; i++) {
        CRGB color = phaseToColor(theta[i], ...);
        ctx.leds[79 - i] = color;  // Left side
        ctx.leds[80 + i] = color;  // Right side
    }
}
```

**Cost**: 80 oscillator updates + 80 LED writes (2× for mirroring) = **160 ops vs 25,600**

### 2.4 Exponential Distance Coupling (Future Enhancement)

For richer dynamics without full N²:

```c++
float coupling = 0.0f;
for (int j = i-R; j <= i+R; j++) {
    int jj = (j + NUM_OSC) % NUM_OSC;  // Wrap around ring
    int dist = abs(jj - i);
    float weight = expf(-dist / decay_length);
    coupling += weight * sin(theta[jj] - theta[i]);
}
```

**Parameters**:
- `R`: Coupling radius (e.g., 5-10 neighbors)
- `decay_length`: Exponential falloff (e.g., 3.0)

**Cost**: O(N·R) — still linear if R is constant

---

## 3. Audio Control Mapping

### 3.1 Design Philosophy

Audio does **NOT** directly set LED colors. Instead, it **steers the dynamical system**:

| Audio Feature | System Parameter | Effect on Dynamics |
|---------------|------------------|-------------------|
| **Beat Phase** | Global rotation rate | Sets the "tempo" of phase rotation |
| **Spectral Centroid** | Phase gradient bias | Creates directional "wind" |
| **Spectral Flux** | Perturbation strength | Triggers phase kicks (slips) |
| **RMS Energy** | Coupling strength | Modulates sync ↔ chaos transition |

### 3.2 Beat Phase → Global Rotation Rate

Map tempo to a **base rotation speed**:

```c++
float bpm = ctx.audio.musicalGrid.bpm;
float base_omega = (bpm / 60.0f) * TWO_PI;  // rad/s

// Set all oscillators to rotate at beat tempo
for (int i = 0; i < NUM_OSC; i++) {
    omega[i] = base_omega + frequency_spread[i];
}
```

**Effect**: The synchronized phase rotates at the song tempo, creating **beat-locked motion** without direct beat pulses.

### 3.3 Spectral Centroid → Phase Gradient

The **spectral centroid** measures timbral brightness (weighted average frequency). Use it to create a **spatial phase bias**:

```c++
float centroid = ctx.audio.controlBus.spectral_centroid;  // 0.0 (dark) to 1.0 (bright)
float wind_strength = 2.0f * (centroid - 0.5f);  // -1.0 to +1.0

for (int i = 0; i < NUM_OSC; i++) {
    float spatial_pos = (float)i / NUM_OSC;  // 0.0 to 1.0
    omega[i] = base_omega + wind_strength * spatial_pos;
}
```

**Effect**:
- **centroid > 0.5** (bright timbre): Phase gradient flows **right** (creates travelling wave)
- **centroid < 0.5** (dark timbre): Phase gradient flows **left** (reverse wave)
- **centroid ≈ 0.5** (neutral): Uniform rotation (breathing sync)

### 3.4 Spectral Flux → Perturbations

**Spectral flux** detects sudden timbral changes (onsets, transients). Use it to **kick oscillators**:

```c++
float flux = ctx.audio.fastFlux();  // High-frequency onset signal

if (flux > flux_threshold) {
    // Apply phase kick to random subset of oscillators
    for (int i = 0; i < NUM_OSC; i++) {
        if (random8() < kick_probability) {
            theta[i] += flux * kick_strength;  // Phase jump
        }
    }
}
```

**Effect**: Creates **phase slips** (synchronized group suddenly tears apart and re-locks), adding visual drama during timbral onsets.

### 3.5 RMS Energy → Coupling Strength

Modulate **K** based on energy to shift between sync/chaos:

```c++
float rms = ctx.audio.rms();
float K = K_min + (K_max - K_min) * rms;

// Loud passages (high RMS) → strong coupling → full sync
// Quiet passages (low RMS) → weak coupling → partial sync/chaos
```

**Effect**: Song dynamics control the **degree of order**, making loud sections more synchronized and quiet sections more chaotic.

---

## 4. Visual Rendering

### 4.1 Phase → Color Mapping

The core rendering function maps oscillator phase to LED color:

```c++
CRGB phaseToColor(float theta, float local_coherence, PaletteRef palette, float palette_pos) {
    // Map phase to palette position (0-255)
    uint8_t palette_index = (uint8_t)((theta / TWO_PI) * 255.0f);

    // Sample palette
    CRGB color = palette.getColor(palette_index + (uint8_t)(palette_pos * 255.0f));

    // Modulate brightness by local coherence
    // High coherence (r_i ≈ 1) → bright, Low coherence (r_i ≈ 0) → dim
    color.nscale8(64 + (uint8_t)(191.0f * local_coherence));

    return color;
}
```

**Visual effect**:
- **Phase (θ)**: Determines hue (oscillators with similar phase → similar color)
- **Local coherence (r_i)**: Brightness (synchronized regions glow, chaotic regions dim)
- **Palette rotation**: Slow drift over seconds (secondary timescale)

### 4.2 Local Coherence Computation

Compute **per-oscillator coherence** to detect local sync clusters:

```c++
float computeLocalCoherence(int i, float theta[], int radius) {
    float sum_cos = cosf(theta[i]);
    float sum_sin = sinf(theta[i]);
    int count = 1;

    // Average over local neighborhood
    for (int j = i - radius; j <= i + radius; j++) {
        if (j == i) continue;
        int jj = (j + NUM_OSC) % NUM_OSC;
        sum_cos += cosf(theta[jj]);
        sum_sin += sinf(theta[jj]);
        count++;
    }

    float r_local = sqrtf(sum_cos*sum_cos + sum_sin*sum_sin) / count;
    return r_local;
}
```

**Cost**: O(N·radius) per frame — with radius=5, that's 80×5 = 400 operations

### 4.3 Travelling Wave vs Sync Cluster Visualization

Different audio conditions create different **visual regimes**:

| System State | r (global) | Visual Appearance |
|--------------|------------|-------------------|
| **Incoherent** | r < 0.3 | Flickering chaos, dim overall |
| **Travelling wave** | 0.3 < r < 0.7 | Interference bands moving around ring |
| **Sync clusters** | r > 0.7 | Solid regions breathing together |
| **Phase slip** | r drops suddenly | Bright tear propagating through strip |

**Color mapping strategy**:
- Use **cyclic palettes** (e.g., Fire, Ocean, Rainbow) so phase wrapping (0 ≡ 2π) is continuous
- Avoid sudden hue jumps at palette boundaries
- Optional: Add **hue modulation** by local coherence gradient (edges of sync clusters get different color)

---

## 5. Implementation Pseudo-Code

### 5.1 Effect State (Minimal)

```c++
class KuramotoEffect {
private:
    static constexpr int NUM_OSC = 80;

    // Oscillator state (320 bytes in PSRAM)
    float theta[NUM_OSC];    // Current phase (radians)
    float omega[NUM_OSC];    // Natural frequencies (rad/s)

    // Parameters (tunable via API)
    float K = 1.0f;          // Coupling strength
    float freq_spread = 0.1f; // Natural frequency variation

    // Slow state
    float palette_phase = 0.0f;  // Slow palette rotation (0-1)
};
```

**Memory**: 80 floats × 2 = 320 bytes (negligible on ESP32-S3 with 8MB PSRAM)

### 5.2 Initialization

```c++
bool KuramotoEffect::init(EffectContext& ctx) {
    // Initialize phases randomly
    for (int i = 0; i < NUM_OSC; i++) {
        theta[i] = random16() * (TWO_PI / 65536.0f);
    }

    // Initialize natural frequencies with Gaussian spread
    float base_freq = 1.0f;  // 1 Hz default (will be modulated by BPM)
    for (int i = 0; i < NUM_OSC; i++) {
        omega[i] = base_freq + freq_spread * random_gaussian();
    }

    return true;
}
```

### 5.3 Update Loop (Euler Integration)

```c++
void KuramotoEffect::render(EffectContext& ctx) {
    float dt = ctx.getSafeDeltaSeconds();  // Clamped [0.0001s, 0.05s]

    // === AUDIO STEERING ===

    // Base rotation from beat tempo
    float bpm = ctx.audio.musicalGrid.bpm;
    float base_omega = (bpm / 60.0f) * TWO_PI;

    // Phase gradient from spectral centroid
    float centroid = ctx.audio.controlBus.spectral_centroid;
    float wind = 2.0f * (centroid - 0.5f);

    // Coupling modulation from RMS
    float rms = ctx.audio.rms();
    float K_current = K * (0.5f + 0.5f * rms);  // 0.5K to K

    // === DYNAMICS UPDATE ===

    float dtheta[NUM_OSC];

    for (int i = 0; i < NUM_OSC; i++) {
        // Natural frequency + audio steering
        float spatial_pos = (float)i / NUM_OSC;
        float omega_i = base_omega * (1.0f + freq_spread * (omega[i] - 1.0f))
                       + wind * spatial_pos;

        // Nearest-neighbor coupling
        int i_prev = (i - 1 + NUM_OSC) % NUM_OSC;
        int i_next = (i + 1) % NUM_OSC;

        float coupling = sinf(theta[i_next] - theta[i])
                       + sinf(theta[i_prev] - theta[i]);

        dtheta[i] = omega_i + 0.5f * K_current * coupling;
    }

    // Euler integration (forward step)
    for (int i = 0; i < NUM_OSC; i++) {
        theta[i] += dtheta[i] * dt;
        theta[i] = fmodf(theta[i], TWO_PI);  // Wrap to [0, 2π)
    }

    // === PERTURBATIONS (Flux kicks) ===

    float flux = ctx.audio.fastFlux();
    if (flux > 0.7f) {
        for (int i = 0; i < NUM_OSC; i++) {
            if (random8() < 64) {  // 25% probability
                theta[i] += flux * 0.5f;
            }
        }
    }

    // === RENDERING ===

    // Slow palette drift (2-second period)
    palette_phase += dt / 2.0f;
    if (palette_phase > 1.0f) palette_phase -= 1.0f;

    for (int i = 0; i < NUM_OSC; i++) {
        // Compute local coherence (radius=3)
        float r_local = computeLocalCoherence(i, theta, 3);

        // Map phase to color
        CRGB color = phaseToColor(theta[i], r_local, ctx.palette, palette_phase);

        // Mirror to both sides
        ctx.leds[79 - i] = color;
        ctx.leds[80 + i] = color;
    }
}
```

### 5.4 Performance Estimate

**Operations per frame** (120 FPS, 8.33 ms budget):

| Operation | Count | Cost (μs) | Notes |
|-----------|-------|-----------|-------|
| Audio setup | 1 | ~10 | BPM, centroid, RMS reads |
| Oscillator update | 80 | ~200 | 2 sin() per oscillator @ 2.5 μs each |
| Integration | 80 | ~20 | Simple float math |
| Local coherence | 80 | ~400 | 7 oscillators × cos/sin per LED |
| Palette sampling | 160 | ~160 | FastLED getColor() |
| LED writes | 160 | ~50 | Memory copies |
| **Total** | | **~840 μs** | **10% of frame budget** |

**Conclusion**: Easily fits in 2 ms render budget with 60% headroom.

---

## 6. Design Validation Checklist

Against design brief acceptance criteria:

### A1: Motion Interesting Without Audio
✅ **YES** — With fixed ω_i and K, oscillators still evolve:
- Random initial conditions create transient patterns
- Coupling drives phase locking over ~5-10 seconds
- Result: "Breathing" sync clusters even in silence

### A2: Frame-Rate Independence
✅ **YES** — Uses `ctx.getSafeDeltaSeconds()`:
- Euler integration: `θ += dθ/dt × dt`
- Same trajectory at 60 FPS, 90 FPS, or 120 FPS
- Visual *feel* identical (no jitter)

### A3: Different Songs → Different Regimes
✅ **YES** — Audio steering creates distinct modes:
- **Electronic/EDM** (high flux, high centroid): Fast travelling waves with frequent slips
- **Ambient/drone** (low flux, low centroid): Slow breathing sync, minimal movement
- **Jazz/complex** (variable centroid): Shifting wind patterns, chaotic clusters

### A4: No Full-Strip White Wash
✅ **YES** — Brightness modulation by local coherence:
- Even at full sync (r=1), palette rotation ensures **hue variation**
- Low-coherence regions stay dim (64/255 minimum brightness)
- No all-white frames possible

---

## 7. Alternative Prototype Families (Future Work)

Per design brief, other stateful dynamical systems to explore:

### 7.1 Reaction-Diffusion (Gray-Scott Model)

**Equations**:
```
∂u/∂t = D_u ∇²u - uv² + F(1-u)
∂v/∂t = D_v ∇²v + uv² - (F+k)v
```

**Audio mapping**:
- Feed rate F ← bass energy (more bass = more "food")
- Kill rate k ← treble energy (more treble = faster decay)
- Produces **spots, stripes, travelling fronts**

**Complexity**: O(N) with 1D Laplacian

### 7.2 Excitable Medium (Cellular Automaton)

**Concept**: Each LED is a cell with states {REST, EXCITED, REFRACTORY}
- **Excited** cells trigger neighbors (wavefront propagation)
- **Refractory** period prevents immediate re-excitation

**Audio mapping**:
- Spectral flux → seed new wavefronts at random locations
- Beat phase → modulate refractory period (faster beats = faster waves)

**Complexity**: O(N) per frame (simple state machine)

### 7.3 Particle/Boid Flow Field

**Concept**: 80 particles with position + velocity, steered by vector field
- Flow field generated by Perlin noise + audio modulation
- Particles leave trails (advection blur)

**Audio mapping**:
- Bass → flow field curl (vorticity)
- Mid → flow field divergence (expansion/contraction)
- Treble → particle birth rate

**Complexity**: O(N) for particles, O(1) for flow sampling

---

## 8. Implementation Roadmap

### Phase 1: Minimal Prototype (4 hours)
- [ ] Create `KuramotoEffect.h/cpp` in `src/effects/ieffect/`
- [ ] Implement 80-oscillator nearest-neighbor coupling
- [ ] Fixed parameters (K=1.0, no audio, random ω_i)
- [ ] Simple phase-to-hue rendering (no coherence modulation)
- [ ] **Goal**: Verify travelling waves emerge on hardware

### Phase 2: Audio Integration (2 hours)
- [ ] Add BPM → base_omega mapping
- [ ] Add spectral centroid → phase gradient
- [ ] Add flux → perturbation kicks
- [ ] **Goal**: Prove audio steers dynamics (not renders directly)

### Phase 3: Visual Polish (2 hours)
- [ ] Implement local coherence computation
- [ ] Add brightness modulation (r_local → LED value)
- [ ] Add slow palette rotation (separate timescale)
- [ ] **Goal**: Match quality of existing holographic effect

### Phase 4: Parameterization (1 hour)
- [ ] Expose K (coupling strength) via effect parameter API
- [ ] Expose freq_spread (natural frequency variation)
- [ ] Expose kick_strength (perturbation gain)
- [ ] **Goal**: Allow user tuning via web UI

**Total estimated time**: ~9 hours (1-2 development sessions)

---

## 9. Technical References

### Academic Papers
- Kuramoto, Y. (1984). "Chemical Oscillations, Waves, and Turbulence" — Original model
- Strogatz, S. (2000). ["From Kuramoto to Crawford"](https://www.sciencedirect.com/science/article/abs/pii/S0167278900001944) — Analytical theory
- Acebrón et al. (2005). ["The Kuramoto model: A simple paradigm for synchronization phenomena"](https://scala.uc3m.es/publications_MANS/PDF/finalKura.pdf) — Comprehensive review

### Ring Topology Studies
- Zheng et al. (2019). ["Pattern selection in a ring of Kuramoto oscillators"](https://www.sciencedirect.com/science/article/abs/pii/S1007570419301881) — Travelling waves on rings
- Kanamaru & Aihara (2010). ["Synchronized oscillations on a Kuramoto ring and their entrainment under periodic driving"](https://www.sciencedirect.com/science/article/abs/pii/S0960077912000768) — External forcing
- Hong & Strogatz (2011). ["Transition to complete synchronization in phase-coupled oscillators with nearest neighbor coupling"](https://www.researchgate.net/publication/24247038_Transition_to_complete_synchronization_in_phase-coupled_oscillators_with_nearest_neighbor_coupling) — Critical coupling

### Visualization & Computation
- [Math Insight: Kuramoto Order Parameters](https://mathinsight.org/applet/kuramoto_order_parameters) — Interactive applet
- [PyRates Documentation](https://pyrates.readthedocs.io/en/latest/auto_introductions/kuramoto.html) — Python implementation
- [GitHub: kuramoto (Python)](https://github.com/fabridamicelli/kuramoto) — Reference implementation

### Phase Oscillator Theory
- [Math Insight: Synchrony of Phase Oscillators](https://mathinsight.org/synchrony_phase_oscillators_idea) — Conceptual overview
- [Wikipedia: Kuramoto Model](https://en.wikipedia.org/wiki/Kuramoto_model) — General reference

---

## 10. Key Insights for LightwaveOS

### 10.1 Why This Works for LEDs

**Spatial mapping**: LED strip naturally maps to 1D ring topology
- 160 LEDs → 80 oscillators (radial symmetry)
- Nearest-neighbor coupling = physically adjacent LEDs
- Travelling waves = intuitive visual motion

**Temporal scales**: Two natural timescales
- **Fast** (beat-locked): Oscillator dynamics at BPM rate (~1-2 Hz)
- **Slow** (palette drift): Seconds-scale color evolution

**Audio semantics**: Kuramoto parameters have **physical meaning**
- Coupling K = "social attraction" (how much oscillators want to sync)
- Natural frequency ω_i = "personality" (preferred tempo)
- Perturbations = "surprise events" (phase kicks)

### 10.2 Differences from Existing Effects

Existing beat-pulse effects (BeatPulseBloomEffect, LGPHolographicEffect):
- **Stateless**: Each frame computed from current audio + time
- **Direct mapping**: RMS → brightness, beat → radius, spectrum → hue
- **Predictable**: Same audio pattern → same visual pattern

Kuramoto effect:
- **Stateful**: Current frame depends on **history** (phase memory)
- **Indirect mapping**: Audio → steering signals → emergent dynamics
- **Unpredictable**: Same audio → different visuals (depends on initial conditions, perturbation timing)

**User-facing difference**: "Feels alive" vs "feels like an equalizer"

### 10.3 Failure Modes to Avoid

❌ **Over-coupling** (K too high): All oscillators lock to mean phase → boring solid rotation
- **Fix**: Keep K near critical point (0.8 to 1.2 range)

❌ **Under-coupling** (K too low): Incoherent chaos → flickering noise
- **Fix**: Minimum K = 0.3, modulate upward with RMS

❌ **Too-fast rotation** (ω too high): Motion blur, hard to see patterns
- **Fix**: Base omega = BPM/60, not raw frequency (1-3 Hz typical)

❌ **No slow timescale**: All motion at beat rate → monotonous
- **Fix**: Separate palette rotation at ~0.5 Hz (2-second period)

---

## Sources

- [Kuramoto model - Wikipedia](https://en.wikipedia.org/wiki/Kuramoto_model)
- [Kuramoto Model of Synchronized Oscillators - MathWorks](https://blogs.mathworks.com/cleve/2019/08/26/kuramoto-model-of-synchronized-oscillators/)
- [The Kuramoto model: A simple paradigm for synchronization phenomena](https://scala.uc3m.es/publications_MANS/PDF/finalKura.pdf)
- [Synchronization of Coupled Oscillators in a Local One-Dimensional Kuramoto Model](https://www.researchgate.net/publication/45869992_Synchronization_of_Coupled_Oscillators_in_a_Local_One-Dimensional_Kuramoto_Model)
- [GitHub - adrianfessel/kuramoto](https://github.com/adrianfessel/kuramoto)
- [Math Insight: Kuramoto Order Parameters](https://mathinsight.org/applet/kuramoto_order_parameters)
- [Pattern selection in a ring of Kuramoto oscillators](https://www.sciencedirect.com/science/article/abs/pii/S1007570419301881)
- [Synchronized oscillations on a Kuramoto ring](https://www.sciencedirect.com/science/article/abs/pii/S0960077912000768)
- [PyRates Documentation - Kuramoto](https://pyrates.readthedocs.io/en/latest/auto_introductions/kuramoto.html)
- [Math Insight: Synchrony of Phase Oscillators](https://mathinsight.org/synchrony_phase_oscillators_idea)

---

**END OF RESEARCH DOCUMENT**
