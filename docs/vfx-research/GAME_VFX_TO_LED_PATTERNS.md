# Game VFX & Shader Techniques for LED Strip Patterns

## Overview

This document synthesizes visual effects techniques from game development, VFX, and procedural animation that translate to 1D LED arrays (320 WS2812 LEDs). These techniques leverage principles from particle systems, shaders, noise functions, and state communication design used in modern game engines.

---

## 1. PARTICLE SYSTEMS: Adapting for 1D LED Arrays

### Core Principles

**What Makes Particle Systems Work:**
- Emitters spawn particles with initial properties (position, velocity, lifetime, colour)
- Each frame: update positions, age particles, remove expired particles, render
- Efficient data structures enable scales from thousands to millions of particles on GPU

**Why This Matters for LEDs:**
On a 320-pixel strip, "particles" become discrete segments or points with properties. The efficiency principles still apply: pre-allocate buffers, avoid allocations in render loops, use data-oriented design.

### Techniques Adapted for 1D LED Arrays

#### 1.1 Ring Buffer Particle Pool

```cpp
struct Particle {
    float position;      // 0.0 - 319.0 along LED strip
    float velocity;      // units per frame
    float lifetime;      // 0.0 - 1.0, where 1.0 = fully expired
    uint32_t colour;     // ARGB32
    float scale;         // brightness/size multiplier
};

class ParticleEmitter {
    static const int MAX_PARTICLES = 64;
    Particle particles[MAX_PARTICLES];
    int writeHead = 0;
    int liveCount = 0;

    void emit(float pos, float vel, uint32_t col) {
        particles[writeHead] = {pos, vel, 0.0f, col, 1.0f};
        writeHead = (writeHead + 1) % MAX_PARTICLES;
        if (liveCount < MAX_PARTICLES) liveCount++;
    }

    void update(float dt) {
        for (int i = 0; i < liveCount; i++) {
            particles[i].position += particles[i].velocity * dt;
            particles[i].lifetime += dt / lifespan;

            // Fade out in final 30% of lifetime
            if (particles[i].lifetime > 0.7f) {
                float fade = 1.0f - (particles[i].lifetime - 0.7f) / 0.3f;
                particles[i].scale = fade;
            }
        }
        // Remove expired particles (compact array)
        int writeIdx = 0;
        for (int i = 0; i < liveCount; i++) {
            if (particles[i].lifetime < 1.0f) {
                particles[writeIdx++] = particles[i];
            }
        }
        liveCount = writeIdx;
    }
};
```

**Key Constraints:**
- MAX_PARTICLES typically 32-64 on embedded systems (keeps per-frame overhead < 0.5ms)
- No dynamic allocation in render path
- Use integer arithmetic where possible (velocity in fixed-point)

#### 1.2 Emission Patterns

**Point Emitter** (from LED position outward):
```cpp
// Emit from centre outward (radially symmetric on 1D = both directions)
void emitCentre(float time) {
    float angle = sin(time * 3.0f);  // oscillate emission angle
    float speed = 50.0f;  // pixels/sec

    // Left direction
    emitter.emit(160.0f, -speed, colourA);
    // Right direction
    emitter.emit(160.0f, +speed, colourA);
}
```

**Ring Emitter** (particles born from all LEDs):
```cpp
// Emit from entire strip, particles converge to centre
void emitRing(float time) {
    int numEmit = 8;
    for (int i = 0; i < numEmit; i++) {
        float t = (float)i / numEmit;
        float pos = t * 320.0f;
        float dirToCenter = (160.0f - pos) > 0 ? 1.0f : -1.0f;
        float speed = 30.0f;

        emitter.emit(pos, dirToCenter * speed, colourA);
    }
}
```

### Application: "Shock Wave" Impact Effect

Particles radiate outward from impact point, fade as they travel. Used for explosions, hits, ability triggers.

```cpp
class ShockwaveEffect {
    ParticleEmitter particles;
    float impactTime = -999.0f;

    void trigger(float centrePos, uint32_t colour) {
        impactTime = getTime();
        // Burst emit 16 particles
        for (int i = 0; i < 16; i++) {
            float angle = (float)i / 16.0f * 2.0f * M_PI;
            float speed = 100.0f + randomFloat(-20.0f, 20.0f);
            float vel = (angle < M_PI) ? speed : -speed;
            particles.emit(centrePos, vel, colour);
        }
    }

    void render(uint32_t* leds, int ledCount) {
        particles.update(1.0f / 120.0f);

        for (int i = 0; i < ledCount; i++) leds[i] = 0;

        for (int p = 0; p < particles.liveCount; p++) {
            int ledIdx = (int)particles.particles[p].position;
            if (ledIdx >= 0 && ledIdx < ledCount) {
                uint32_t col = particles.particles[p].colour;
                uint8_t brightness = (uint8_t)(255.0f * particles.particles[p].scale);
                leds[ledIdx] = rgbWithBrightness(col, brightness);
            }
        }
    }
};
```

### Performance Budgets for LED Strips

- **Emitter logic:** < 0.5ms per 120 FPS frame
- **Particle update loop:** 64 particles at 120 FPS ≈ 0.1ms
- **Rendering (blending):** 320 LEDs with additive blend ≈ 0.2ms
- **Total particle system budget:** ~1ms per frame

---

## 2. SHADER TECHNIQUES: Approximating on Discrete LEDs

### 2.1 Bloom Effect (Glow Propagation)

Bloom creates haloes around bright pixels. On LEDs, this becomes "glow propagation"—brightness bleeds to neighbouring LEDs.

#### Gaussian Bloom on 320-LED Strip

**3-Pass Approach (mimics Gaussian blur):**

```cpp
class BloomEffect {
    uint32_t stripBuffer[320];
    uint32_t bloomBuffer[320];

    void applyBloom(uint32_t* leds, float bloomStrength = 0.5f) {
        // Pass 1: Downsampling (every 4th LED)
        uint32_t downsampled[80];
        for (int i = 0; i < 80; i++) {
            downsampled[i] = leds[i * 4];
        }

        // Pass 2: Horizontal Gaussian blur on downsampled
        uint32_t blurred[80];
        for (int i = 1; i < 79; i++) {
            // Weights: [0.25, 0.5, 0.25]
            blurred[i] = blend(
                downsampled[i-1], 0.25f,
                downsampled[i], 0.5f,
                downsampled[i+1], 0.25f
            );
        }

        // Pass 3: Upsample back to full resolution with additive blend
        memcpy(bloomBuffer, leds, sizeof(leds[0]) * 320);
        for (int i = 0; i < 80; i++) {
            // Each downsampled pixel contributes to 4 upsampled pixels
            for (int j = 0; j < 4; j++) {
                int ledIdx = i * 4 + j;
                bloomBuffer[ledIdx] = additiveBlend(
                    bloomBuffer[ledIdx],
                    blurred[i],
                    bloomStrength
                );
            }
        }
        memcpy(leds, bloomBuffer, sizeof(leds[0]) * 320);
    }
};
```

**Performance:**
- Downsampling: 80 reads
- Blur pass: 80 × 3 reads = 240 reads
- Upsample: 320 writes with blending
- **Total:** < 0.1ms at 120 FPS

#### Local Bloom (Brighter Threshold)

For a more immediate effect without downsampling:

```cpp
void applyLocalBloom(uint32_t* leds, float threshold = 200.0f) {
    uint32_t temp[320];
    memcpy(temp, leds, sizeof(temp));

    for (int i = 1; i < 319; i++) {
        // Get brightness of current LED
        float brightness = getLuminance(temp[i]);

        if (brightness > threshold) {
            // Spread to neighbours
            float overflow = (brightness - threshold) / 255.0f;
            uint32_t bloom = scaleColour(temp[i], overflow * 0.5f);

            leds[i-1] = additiveBlend(temp[i-1], bloom, 1.0f);
            leds[i+1] = additiveBlend(temp[i+1], bloom, 1.0f);
        }
    }
}
```

### 2.2 Chromatic Aberration (Colour Shift Separation)

In game shaders, chromatic aberration offsets RGB channels. On LEDs, this becomes colour-based position offset.

```cpp
void applyChromaticAberration(uint32_t* leds, float time) {
    uint32_t temp[320];
    memcpy(temp, leds, sizeof(temp));
    memset(leds, 0, sizeof(leds[0]) * 320);

    // Offset each colour channel differently
    float offsetR = 5.0f * sin(time * 2.0f);  // Red shifts left/right
    float offsetG = 0.0f;                      // Green stays centre
    float offsetB = -5.0f * sin(time * 2.0f); // Blue opposite to red

    for (int i = 0; i < 320; i++) {
        uint32_t col = temp[i];
        uint8_t r = (col >> 16) & 0xFF;
        uint8_t g = (col >> 8) & 0xFF;
        uint8_t b = col & 0xFF;

        // Place each channel at offset position
        int posR = clamp(0, 319, (int)(i + offsetR));
        int posG = i;
        int posB = clamp(0, 319, (int)(i + offsetB));

        leds[posR] = additiveBlend(leds[posR], (r << 16), 1.0f);
        leds[posG] = additiveBlend(leds[posG], (g << 8), 1.0f);
        leds[posB] = additiveBlend(leds[posB], b, 1.0f);
    }
}
```

**Use Case:** Disorientation effect, energy overload, teleportation.

### 2.3 Post-Processing Pipeline

```cpp
class PostProcessPipeline {
    void render(uint32_t* leds, float time) {
        // 1. Render base effect to leds[]
        baseEffect.render(leds);

        // 2. Apply glow
        applyLocalBloom(leds, 180.0f);

        // 3. Apply colour shifts
        applyChromaticAberration(leds, time);

        // 4. Clamp to [0, 255] per channel
        for (int i = 0; i < 320; i++) {
            leds[i] = clampRGB888(leds[i]);
        }
    }
};
```

---

## 3. PROCEDURAL ANIMATION: Noise Functions & Waves

### 3.1 Perlin Noise for Organic Motion

Perlin noise is the foundation for natural-looking, non-repeating animations: fire, water, organic shapes, flowing textures.

#### 1D Perlin Noise (Arduino-Optimized)

**Key Optimization for Embedded Systems:**
- Use **lookup tables** stored in program memory (PROGMEM on ESP32)
- Precompute permutation arrays and fade curves
- Avoid trigonometric functions (use lookup tables instead)

```cpp
// Based on Ken Perlin's Improved Noise (2002)
class PerlinNoise1D {
    static const uint8_t permutation[256];  // PROGMEM
    static const uint8_t p[512];            // Doubled for wraparound

    float fade(float t) {
        // Smoothstep: 6t⁵ - 15t⁴ + 10t³
        // Optimized: ((6t - 15)t + 10)t³
        return t * t * t * (t * (t * 6 - 15) + 10);
    }

    float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    float grad(uint8_t hash, float x) {
        // Gradient direction based on hash
        uint8_t h = hash & 0x0F;
        float u = (h & 8) == 0 ? x : -x;
        return u;
    }

public:
    // Returns 0.0 to 1.0
    float noise(float x) {
        x = x * 0.1f;  // Scale input for reasonable wavelength

        int xi = (int)x & 255;
        float xf = x - (int)x;

        float u = fade(xf);

        uint8_t p0 = p[xi];
        uint8_t p1 = p[xi + 1];

        float g0 = grad(p0, xf);
        float g1 = grad(p1, xf - 1.0f);

        return 0.5f + 0.5f * lerp(g0, g1, u);
    }
};
```

**Performance:** ~20μs per call on ESP32 (suitable for per-LED calculation).

#### Application: Flowing Wave Effect

```cpp
class FlowingWaveEffect {
    PerlinNoise1D perlin;
    float time = 0.0f;

    void render(uint32_t* leds, int ledCount) {
        time += 0.016f;  // ~60ms increment per frame at 120 FPS

        for (int i = 0; i < ledCount; i++) {
            // Two octaves of noise for richer variation
            float n1 = perlin.noise((float)i + time * 20.0f);
            float n2 = perlin.noise((float)i * 0.5f + time * 50.0f);

            // Blend octaves (persistence = 0.5)
            float combined = n1 * 0.7f + n2 * 0.3f;

            // Map noise to colour
            uint8_t brightness = (uint8_t)(combined * 255.0f);
            uint8_t hue = (uint8_t)(combined * 255.0f);

            leds[i] = hsv2rgb(hue, 255, brightness);
        }
    }
};
```

### 3.2 Simplex Noise (Improved Performance)

**Advantage over Perlin Noise:**
- Lower computational cost (fewer dot products, simplex structure instead of grid)
- Better visual quality in 4D+
- Faster for embedded systems

```cpp
// Simplex noise is more efficient but requires different permutation structure
// For ESP32 with PSRAM, consider FastLED's built-in noise functions
// They're already optimized for microcontrollers
```

### 3.3 Wave Interference Patterns

Superposition of multiple wave sources creates standing waves and interference patterns—visually striking and meaningful for music-reactive effects.

```cpp
class WaveInterferenceEffect {
    float time = 0.0f;
    float freq1 = 2.0f;   // Hz
    float freq2 = 3.0f;   // Hz
    float phase1 = 0.0f;
    float phase2 = M_PI / 4.0f;

    void render(uint32_t* leds, int ledCount) {
        time += 0.016f;

        for (int i = 0; i < ledCount; i++) {
            float x = (float)i / ledCount * 2.0f * M_PI;  // 0 to 2π across strip

            // Two waves
            float wave1 = sin(x * freq1 + phase1 + time);
            float wave2 = sin(x * freq2 + phase2 + time);

            // Superposition (interference)
            float combined = (wave1 + wave2) * 0.5f;  // Average to normalize

            // Create node (zero-point) at interference nulls
            float brightness = fabs(combined);  // 0 at nodes, 1 at anti-nodes

            uint8_t b = (uint8_t)(brightness * 200.0f);  // Scale to LED range
            leds[i] = RGB(0, b, 0);  // Green for interference pattern
        }
    }
};
```

**Visual Result:** Creates standing wave patterns that shift as frequencies drift.

### 3.4 Cellular Automata (Conway's Game of Life Adapted)

On a 1D strip, cellular automata generates organic, evolving patterns based on simple neighbor rules.

```cpp
class CellularAutomataEffect {
    uint8_t state[320];
    uint8_t newState[320];
    uint8_t genCount = 0;

    void init() {
        // Random initial state
        for (int i = 0; i < 320; i++) {
            state[i] = random(0, 2);
        }
    }

    void step() {
        // 1D Rule 110 (simple, produces interesting patterns)
        // Each cell looks at left, self, right
        for (int i = 1; i < 319; i++) {
            uint8_t left = state[i-1];
            uint8_t center = state[i];
            uint8_t right = state[i+1];

            uint8_t neighborhood = (left << 2) | (center << 1) | right;

            // Rule 110 lookup table (from binary 01101110 = 110)
            // neighborhood 0-7 maps to output bit
            const uint8_t rule110 = 0b01101110;
            newState[i] = (rule110 >> neighborhood) & 1;
        }

        memcpy(state, newState, sizeof(state));
        genCount++;
    }

    void render(uint32_t* leds) {
        // Alive cells = bright, dead cells = dark
        for (int i = 0; i < 320; i++) {
            if (state[i]) {
                leds[i] = RGB(255, 255, 255);  // White for alive
            } else {
                leds[i] = RGB(0, 0, 0);        // Dark for dead
            }
        }

        // Evolve state every 5 frames (slower evolution for visibility)
        if (genCount % 5 == 0) {
            step();
        }
    }
};
```

**Rules for 1D Cellular Automata:**
- **Rule 30:** Chaotic, random-looking
- **Rule 110:** Complex, self-organizing
- **Rule 184:** Simple patterns, good for "particle flow"

---

## 4. HIT / IMPACT EFFECTS: Making Them Feel Punchy

Game VFX artists use several layers to create impactful hit feedback:

### 4.1 Layered Impact System

```cpp
class ImpactEffect {
    enum Layer {
        INITIAL_BURST,      // Immediate bright flash
        SHOCKWAVE,          // Expanding waves
        AFTERGLOW,          // Trailing fade
    };

    struct Impact {
        float centrePos;
        float time;         // Time since impact
        uint32_t colour;
        float intensity;    // 0.0 to 1.0
    };

    Impact impacts[8];  // Multiple simultaneous impacts
    int impactCount = 0;

    void trigger(float pos, uint32_t colour) {
        if (impactCount < 8) {
            impacts[impactCount++] = {pos, 0.0f, colour, 1.0f};
        }
    }

    void render(uint32_t* leds) {
        memset(leds, 0, sizeof(uint32_t) * 320);

        for (int imp = 0; imp < impactCount; imp++) {
            Impact& impact = impacts[imp];
            float age = impact.time;

            // Layer 1: Initial burst (0-0.05 sec)
            if (age < 0.05f) {
                float burstIntensity = 1.0f - (age / 0.05f);
                int ledIdx = (int)impact.centrePos;
                if (ledIdx >= 0 && ledIdx < 320) {
                    uint32_t col = scaleColour(impact.colour, burstIntensity);
                    leds[ledIdx] = additiveBlend(leds[ledIdx], col, 1.0f);
                }
            }

            // Layer 2: Shockwave (0.05-0.3 sec)
            if (age < 0.3f) {
                float shockAge = age - 0.05f;
                float shockProgress = shockAge / 0.25f;  // 0 to 1
                float shockRadius = shockProgress * 160.0f;  // Expands to half strip

                // Decreasing intensity over distance
                for (int i = 0; i < 320; i++) {
                    float dist = fabs((float)i - impact.centrePos);
                    if (dist < shockRadius && dist > shockRadius - 5.0f) {
                        // Narrow band of brightness at shock front
                        float intensity = 1.0f - (dist - (shockRadius - 5.0f)) / 5.0f;
                        intensity *= (1.0f - shockProgress);  // Fade out

                        uint32_t col = scaleColour(impact.colour, intensity * 0.8f);
                        leds[i] = additiveBlend(leds[i], col, 1.0f);
                    }
                }
            }

            // Layer 3: Afterglow (0.3-0.8 sec)
            if (age >= 0.3f && age < 0.8f) {
                float glowAge = age - 0.3f;
                float glowFade = 1.0f - (glowAge / 0.5f);
                float glowRadius = 40.0f;

                for (int i = 0; i < 320; i++) {
                    float dist = fabs((float)i - impact.centrePos);
                    if (dist < glowRadius) {
                        float attenuation = 1.0f - (dist / glowRadius);
                        uint32_t col = scaleColour(impact.colour,
                            attenuation * glowFade * 0.5f);
                        leds[i] = additiveBlend(leds[i], col, 1.0f);
                    }
                }
            }

            impact.time += 1.0f / 120.0f;
        }

        // Compact impacts array (remove expired)
        int writeIdx = 0;
        for (int i = 0; i < impactCount; i++) {
            if (impacts[i].time < 0.8f) {
                impacts[writeIdx++] = impacts[i];
            }
        }
        impactCount = writeIdx;
    }
};
```

### 4.2 "Feel" Parameters for Tuning

The visual "punch" comes from:

| Parameter | Effect | Range |
|-----------|--------|-------|
| Burst intensity | Initial flash brightness | 0.5 - 1.0 |
| Burst duration | How long initial flash lasts | 30-100ms |
| Shockwave speed | Expansion velocity | 200-1000 px/sec |
| Shockwave width | Thickness of expanding band | 2-10 LEDs |
| Afterglow radius | How far glow spreads | 20-100 LEDs |
| Afterglow duration | Total effect length | 0.5-1.2 sec |

**Tuning Strategy:**
- Short, sharp burst (50ms) + fast shockwave = explosive feel
- Longer burst (100ms) + slow shockwave = heavy, forceful feel
- Large afterglow + bloom = magical/energy feel

---

## 5. ENERGY / POWER EFFECTS: Charging & Releasing

### 5.1 Charging Pulse (Building Tension)

```cpp
class ChargingEffect {
    float chargeLevel = 0.0f;  // 0.0 to 1.0
    float targetCharge = 1.0f;
    float chargeSpeed = 0.5f;  // Seconds to full charge

    void render(uint32_t* leds) {
        // Update charge level
        chargeLevel += (1.0f / 120.0f) / chargeSpeed;
        if (chargeLevel > targetCharge) chargeLevel = targetCharge;

        // Charging pattern: fill from edges to centre
        int fillLeft = (int)(160.0f * chargeLevel);
        int fillRight = 160 + fillLeft;

        for (int i = 0; i < 320; i++) {
            uint32_t col = 0;

            // Filled region (solid colour)
            if (i < 160 - fillLeft || i >= fillRight) {
                col = RGB(0, 100, 200);  // Blue core
            }
            // Charging edge (pulsing halo)
            else if (i == 160 - fillLeft || i == fillRight - 1) {
                float pulse = sin(getTime() * 6.28f * 2.0f) * 0.5f + 0.5f;
                uint8_t b = (uint8_t)(pulse * 255.0f);
                col = RGB(0, b, 255);  // Bright blue edge
            }

            leds[i] = col;
        }

        // Optional: Perlin noise field around charge front for organic feel
        applyChargeFieldNoise(leds);
    }

    void applyChargeFieldNoise(uint32_t* leds) {
        PerlinNoise1D perlin;
        float time = getTime();

        int fillLeft = (int)(160.0f * chargeLevel);
        int fillRight = 160 + fillLeft;
        int edgeWidth = 3;

        for (int i = 160 - fillLeft - edgeWidth; i < 160 - fillLeft + edgeWidth; i++) {
            if (i >= 0 && i < 320) {
                float noise = perlin.noise((float)i + time * 20.0f);
                float waviness = noise * edgeWidth * 0.5f;

                int actualEdge = 160 - fillLeft + (int)waviness;
                if (i < actualEdge) {
                    leds[i] = additiveBlend(leds[i], RGB(0, 0, 100), 0.5f);
                }
            }
        }
    }
};
```

**Perception:**
- Pulsing edge = energy building
- Fill direction = progress toward goal
- Noise at edge = instability, power leaking

### 5.2 Release / Burst

```cpp
class EnergyReleaseEffect {
    enum State {
        IDLE,
        BUILDING,
        RELEASING,
    };

    State state = IDLE;
    float time = 0.0f;

    void release() {
        state = RELEASING;
        time = 0.0f;
    }

    void render(uint32_t* leds) {
        switch (state) {
            case IDLE:
                // Dim baseline
                for (int i = 0; i < 320; i++) {
                    leds[i] = RGB(10, 10, 10);
                }
                break;

            case RELEASING: {
                // Phase 1: Bright flash (0-0.1 sec)
                if (time < 0.1f) {
                    float flashIntensity = 1.0f - (time / 0.1f);
                    for (int i = 0; i < 320; i++) {
                        uint8_t b = (uint8_t)(flashIntensity * 255.0f);
                        leds[i] = RGB(b, b, b);  // White flash
                    }
                }

                // Phase 2: Energy discharge wave (0.1-0.6 sec)
                else if (time < 0.6f) {
                    float waveAge = time - 0.1f;
                    float wavePos = (waveAge / 0.5f) * 320.0f;

                    for (int i = 0; i < 320; i++) {
                        float dist = fabs((float)i - wavePos);
                        float intensity = exp(-dist * dist / (40.0f * 40.0f));
                        intensity *= (1.0f - waveAge / 0.5f);

                        uint8_t b = (uint8_t)(intensity * 200.0f);
                        leds[i] = RGB(b, 0, 0);  // Red discharge
                    }
                }

                time += 1.0f / 120.0f;
                if (time > 0.6f) {
                    state = IDLE;
                }
                break;
            }
        }
    }
};
```

---

## 6. TRAIL EFFECTS: Motion Trails & History Buffers

### 6.1 Simple Velocity-Based Trail

```cpp
class VelocityTrailEffect {
    float position = 160.0f;
    float velocity = 0.0f;
    uint32_t colour = RGB(255, 100, 0);

    // History buffer (last N frames)
    static const int TRAIL_LENGTH = 30;
    float positionHistory[TRAIL_LENGTH];
    float ageHistory[TRAIL_LENGTH];

    void update() {
        // Shift history
        for (int i = TRAIL_LENGTH - 1; i > 0; i--) {
            positionHistory[i] = positionHistory[i-1];
            ageHistory[i] = ageHistory[i-1] + (1.0f / 120.0f);
        }

        // Add current position
        positionHistory[0] = position;
        ageHistory[0] = 0.0f;

        // Update position (simple oscillation for demo)
        velocity = sin(getTime() * 3.0f) * 100.0f;
        position += velocity * (1.0f / 120.0f);
        position = clamp(0.0f, 320.0f, position);
    }

    void render(uint32_t* leds) {
        update();

        memset(leds, 0, sizeof(uint32_t) * 320);

        // Render trail
        for (int i = 0; i < TRAIL_LENGTH; i++) {
            int ledIdx = (int)positionHistory[i];
            if (ledIdx >= 0 && ledIdx < 320) {
                // Fade older trail points
                float fade = 1.0f - (ageHistory[i] / 0.25f);
                if (fade > 0.0f) {
                    fade = fade > 1.0f ? 1.0f : fade;
                    uint32_t col = scaleColour(colour, fade);
                    leds[ledIdx] = additiveBlend(leds[ledIdx], col, 1.0f);
                }
            }
        }
    }
};
```

**Memory Cost:**
- 30-frame history = 30 floats × 4 bytes = 120 bytes (negligible)
- Scales to ~10 simultaneous trails on ESP32

### 6.2 Velocity-Based Stretching

Objects moving fast become "stretched" along their motion direction:

```cpp
void renderStretchedObject(uint32_t* leds,
                          float position,
                          float velocity,
                          uint32_t colour) {
    // Stretch calculation (inspired by screen-space velocity in shaders)
    float stretchMagnitude = fabs(velocity) * 0.05f;  // Tune scaling
    stretchMagnitude = pow(stretchMagnitude, 2.0f);   // Nonlinear falloff
    stretchMagnitude = clamp(0.0f, 6.0f, stretchMagnitude);

    int ledIdx = (int)position;
    float subpixel = position - ledIdx;

    // Draw "core" at position
    if (ledIdx >= 0 && ledIdx < 320) {
        leds[ledIdx] = additiveBlend(leds[ledIdx], colour, 1.0f);
    }

    // Draw stretch in velocity direction
    int stretchDir = velocity > 0.0f ? 1 : -1;
    for (float i = 1.0f; i <= stretchMagnitude; i += 1.0f) {
        int stretchIdx = ledIdx + (int)(i * stretchDir);
        if (stretchIdx >= 0 && stretchIdx < 320) {
            // Exponential falloff along stretch
            float falloff = exp(-i / (stretchMagnitude * 0.5f));
            uint32_t stretchCol = scaleColour(colour, falloff);
            leds[stretchIdx] = additiveBlend(leds[stretchIdx], stretchCol, 1.0f);
        }
    }
}
```

---

## 7. STATE COMMUNICATION: Visual Feedback Design

### 7.1 Health/Power Status Indicator

Different patterns communicate different states without needing explicit UI:

```cpp
class StateIndicatorEffect {
    enum SystemState {
        HEALTHY,      // Full brightness, steady colour
        CAUTION,      // Pulsing yellow, centre region bright
        WARNING,      // Flashing red, reduced brightness
        CRITICAL,     // Strobing red, only small region bright
    };

    SystemState state = HEALTHY;
    float stateValue = 1.0f;  // 0.0 to 1.0 (health/energy)

    void render(uint32_t* leds) {
        // Determine state from value
        if (stateValue > 0.7f) state = HEALTHY;
        else if (stateValue > 0.4f) state = CAUTION;
        else if (stateValue > 0.15f) state = WARNING;
        else state = CRITICAL;

        memset(leds, 0, sizeof(uint32_t) * 320);

        float pulse = sin(getTime() * 6.28f) * 0.5f + 0.5f;

        switch (state) {
            case HEALTHY:
                // Full strip, steady green
                for (int i = 0; i < 320; i++) {
                    leds[i] = RGB(0, (uint8_t)(stateValue * 255.0f), 0);
                }
                break;

            case CAUTION: {
                // Centre region bright, pulse yellow
                int width = (int)(160.0f * stateValue);
                for (int i = 80; i < 240; i++) {
                    float dist = fabs((float)i - 160.0f);
                    float intensity = 1.0f - (dist / width);
                    intensity *= pulse;
                    leds[i] = RGB((uint8_t)(intensity * 255.0f),
                                 (uint8_t)(intensity * 200.0f), 0);
                }
                break;
            }

            case WARNING: {
                // Shrinking region, flashing red
                int width = (int)(80.0f * stateValue);
                float flash = fmod(getTime(), 0.2f) < 0.1f ? 1.0f : 0.3f;
                for (int i = 110; i < 210; i++) {
                    float dist = fabs((float)i - 160.0f);
                    if (dist < width) {
                        leds[i] = RGB((uint8_t)(flash * 255.0f), 0, 0);
                    }
                }
                break;
            }

            case CRITICAL: {
                // Only centre 2 LEDs, intense strobe
                float strobe = fmod(getTime(), 0.1f) < 0.05f ? 1.0f : 0.0f;
                leds[159] = RGB((uint8_t)(strobe * 255.0f), 0, 0);
                leds[160] = RGB((uint8_t)(strobe * 255.0f), 0, 0);
                break;
            }
        }
    }
};
```

### 7.2 Action Confirmation / Feedback

Instant visual response to user input/triggers:

```cpp
class ConfirmationFeedback {
    enum FeedbackType {
        NONE,
        SUCCESS,      // Green pulse
        FAILURE,      // Red burst
        NEUTRAL,      // Blue flash
    };

    FeedbackType lastFeedback = NONE;
    float feedbackTime = 0.0f;
    static const float FEEDBACK_DURATION = 0.3f;

    void trigger(FeedbackType type) {
        lastFeedback = type;
        feedbackTime = 0.0f;
    }

    void render(uint32_t* leds) {
        if (feedbackTime > FEEDBACK_DURATION) {
            return;  // No active feedback
        }

        float progress = feedbackTime / FEEDBACK_DURATION;
        float intensity = 1.0f - progress;

        uint32_t colour;
        switch (lastFeedback) {
            case SUCCESS:
                colour = RGB(0, (uint8_t)(intensity * 255.0f), 0);  // Green
                break;
            case FAILURE:
                colour = RGB((uint8_t)(intensity * 255.0f), 0, 0);  // Red
                break;
            case NEUTRAL:
                colour = RGB(0, 0, (uint8_t)(intensity * 255.0f));  // Blue
                break;
            default:
                return;
        }

        // Flash entire strip
        for (int i = 0; i < 320; i++) {
            leds[i] = colour;
        }

        feedbackTime += 1.0f / 120.0f;
    }
};
```

---

## 8. INTEGRATION PATTERNS: Architecture for Complex Effects

### 8.1 Effect Composition (Layering Multiple Effects)

```cpp
class EffectComposer {
    // Base layer (background pattern)
    BaseEffect baseEffect;

    // Overlay layers (particles, impacts, etc.)
    ParticleEmitter particles;
    ImpactEffect impacts;
    TrailEffect trails;

    // Post-processing
    PostProcessPipeline postFX;

    uint32_t renderBuffer[320];
    uint32_t finalBuffer[320];

    void render() {
        // 1. Render base
        baseEffect.render(renderBuffer);

        // 2. Blend overlays (additive)
        particles.render(renderBuffer);
        impacts.render(renderBuffer);
        trails.render(renderBuffer);

        // 3. Post-processing on combined result
        memcpy(finalBuffer, renderBuffer, sizeof(renderBuffer));
        postFX.render(finalBuffer);

        // 4. Write to strip
        writeLEDs(finalBuffer);
    }
};
```

### 8.2 Effect Choreography (Synchronized Multi-Effect Timing)

```cpp
class EffectSequencer {
    enum SequencePhase {
        PHASE_CHARGE,
        PHASE_PEAK,
        PHASE_RELEASE,
        PHASE_COOLDOWN,
    };

    SequencePhase phase = PHASE_COOLDOWN;
    float phaseTimer = 0.0f;
    float phaseTimings[4] = {1.0f, 0.5f, 0.8f, 2.0f};  // Duration of each phase

    void update() {
        phaseTimer += 1.0f / 120.0f;

        float currentDuration = phaseTimings[(int)phase];
        if (phaseTimer > currentDuration) {
            phaseTimer = 0.0f;
            phase = (SequencePhase)((phase + 1) % 4);
        }
    }

    void render(uint32_t* leds) {
        update();

        switch (phase) {
            case PHASE_CHARGE:
                renderCharging(leds);
                break;
            case PHASE_PEAK:
                renderPeak(leds);
                break;
            case PHASE_RELEASE:
                renderRelease(leds);
                break;
            case PHASE_COOLDOWN:
                renderCooldown(leds);
                break;
        }
    }
};
```

---

## 9. PERFORMANCE BUDGETS & OPTIMIZATION TIPS

### 9.1 Frame Budget at 120 FPS

**Total per-frame time: ~8.3ms**

| Component | Time | Notes |
|-----------|------|-------|
| **Effect render** | 2-3ms | Particle sims, noise functions |
| **Noise functions** | 0.5-1.0ms | Perlin per 320 LEDs |
| **Post-processing** | 0.5-1.0ms | Bloom, chromatic aberration |
| **LED write/DMA** | 1-2ms | 320 LEDs @ 800kHz WS2812 protocol |
| **Audio sync** | 0.2-0.5ms | Beat detection, analysis |
| **Network/serial** | 0.5-1.0ms | Commands, status |
| **Safety margin** | 2ms | Headroom for spikes |

### 9.2 Memory-Efficient Techniques

**Use these to reduce SRAM footprint:**

1. **Ring Buffers:** Allocate fixed size, reuse memory
2. **Lookup Tables (PROGMEM):** Move permutation tables, sine tables to flash
3. **Fixed-Point Math:** Use int16_t/int32_t instead of float where possible
4. **Bitpacking:** Store multiple boolean flags in single byte
5. **PSRAM for Buffers:** Large history/effect buffers in PSRAM, not SRAM

### 9.3 Code Patterns to Avoid

- **No malloc/new in render loops** (causes heap fragmentation)
- **No String concatenation in render** (memory thrashing)
- **No floating-point division in inner loops** (use precomputed reciprocals)
- **No modulo on non-power-of-2 numbers** (use bitwise AND for 256)
- **No function pointers in tight loops** (inline or unroll)

### 9.4 Optimization Checklist

```cpp
// BEFORE (inefficient)
for (int i = 0; i < 320; i++) {
    float normalized = (float)i / 320.0f;  // Division in loop!
    float noise = perlin.noise(normalized);
    // ...
}

// AFTER (optimized)
const float inv320 = 1.0f / 320.0f;
for (int i = 0; i < 320; i++) {
    float noise = perlin.noise(i * inv320);  // Precomputed reciprocal
    // ...
}
```

---

## 10. PRACTICAL EXAMPLES: Complete Standalone Effects

### 10.1 "Electric Storm" (Combines Multiple Techniques)

```cpp
class ElectricStormEffect {
    ParticleEmitter sparks;
    PerlinNoise1D noise;
    float time = 0.0f;

    void render(uint32_t* leds) {
        time += 1.0f / 120.0f;

        // Base: Perlin noise background
        for (int i = 0; i < 320; i++) {
            float n = noise.noise((float)i * 0.02f + time * 2.0f);
            uint8_t b = (uint8_t)(n * 100.0f);
            leds[i] = RGB(0, b, 200);  // Dark blue base
        }

        // Emit sparks at random positions
        if (fmod(time, 0.05f) < 0.016f) {
            for (int j = 0; j < 3; j++) {
                float pos = random(0.0f, 320.0f);
                float vel = random(-50.0f, 50.0f);
                sparks.emit(pos, vel, RGB(255, 200, 100));
            }
        }

        // Render and update sparks
        sparks.update(1.0f / 120.0f);
        for (int p = 0; p < sparks.liveCount; p++) {
            int ledIdx = (int)sparks.particles[p].position;
            if (ledIdx >= 0 && ledIdx < 320) {
                uint32_t col = sparks.particles[p].colour;
                uint8_t brightness = (uint8_t)
                    (255.0f * (1.0f - sparks.particles[p].lifetime));
                leds[ledIdx] = RGB(
                    ((col >> 16) & 0xFF),
                    ((col >> 8) & 0xFF) * brightness / 255,
                    (col & 0xFF) * brightness / 255
                );
            }
        }

        // Post-processing: Local bloom on bright pixels
        applyLocalBloom(leds, 150.0f);
    }
};
```

### 10.2 "Resonance" (Wave Interference + Beat Sync)

```cpp
class ResonanceEffect {
    float baseFreq1 = 2.0f;
    float baseFreq2 = 3.0f;
    float time = 0.0f;
    float beatPhase = 0.0f;

    // Called when beat is detected
    void onBeat(float beatIntensity) {
        // Frequency shifts on beat for "locked" feel
        baseFreq1 += beatIntensity * 0.5f;
        baseFreq2 += beatIntensity * 0.3f;
    }

    void render(uint32_t* leds) {
        time += 1.0f / 120.0f;

        for (int i = 0; i < 320; i++) {
            float x = (float)i / 320.0f * 2.0f * M_PI;

            // Frequency modulation on beat
            float freq1 = baseFreq1 * (1.0f + sin(beatPhase) * 0.2f);
            float freq2 = baseFreq2 * (1.0f + cos(beatPhase) * 0.2f);

            float wave1 = sin(x * freq1 + time);
            float wave2 = sin(x * freq2 + time + M_PI / 2.0f);

            float combined = (wave1 + wave2) * 0.5f;

            // Map to colour
            float brightness = fabs(combined);
            uint8_t h = (uint8_t)((combined + 1.0f) * 127.5f);  // Hue shift
            uint8_t v = (uint8_t)(brightness * 255.0f);

            leds[i] = hsv2rgb(h, 200, v);
        }
    }
};
```

---

## References & Further Reading

### Particle Systems
- [Building a Million-Particle System](https://www.gamedeveloper.com/programming/building-a-million-particle-system)
- [GPU-based particle simulation – Wicked Engine](https://wickedengine.net/2017/11/gpu-based-particle-simulation/)
- [Bevy Hanabi (Bevy game engine)](https://deepwiki.com/djeedai/bevy_hanabi)

### Shader Techniques
- [LearnOpenGL - Bloom](https://learnopengl.com/Advanced-Lighting/Bloom)
- [LearnOpenGL - Phys. Based Bloom](https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom)
- [Chromatic Aberration techniques](https://halisavakis.com/my-take-on-shaders-chromatic-aberration-introduction-to-image-effects-part-iv/)

### Procedural Animation
- [Perlin Noise Implementation Guide](https://garagefarm.net/blog/perlin-noise-implementation-procedural-generation-and-simplex-noise)
- [Perlin Noise on Arduino](https://www.kasperkamperman.com/blog/arduino/perlin-noise-improved-noise-on-arduino/)
- [The Book of Shaders: Noise](https://thebookofshaders.com/11/)
- [Wave Function Collapse for procedural generation](https://www.gridbugs.org/wave-function-collapse/)

### Game VFX Design
- [VFX in Gaming: The Ultimate Guide | Boris FX](https://borisfx.com/blog/vfx-in-gaming-ultimate-guide-visual-effects-games/)
- [Game VFX Design Principles](https://magicmedia.studio/news-insights/guide-to-vfx-for-gaming/)

### LED Strip Techniques
- [FastLED Library](https://fastled.io/)
- [WS2812B Guide](https://randomnerdtutorials.com/guide-for-ws2812b-addressable-rgb-led-strip-with-arduino/)
- [LED Strip Animation Techniques](https://sunlite-led.com/programming-led-strip-effects-animations/)

### Beat Detection & Audio Sync
- [Beat Detection Synchronization](https://www.sciopen.com/article/10.1007/s41095-018-0115-y)
- [ESP32 Music Beat Sync Example](https://github.com/blaz-r/ESP32-music-beat-sync)

---

## Summary Table: Technique Selection Guide

| Visual Goal | Best Technique | Complexity | SRAM Cost | Notes |
|------------|-----------------|-----------|-----------|-------|
| Organic flowing patterns | Perlin noise | Low | ~200 bytes | Smooth, continuous, non-repeating |
| Explosive impact | Layered burst + shockwave | Medium | ~500 bytes | Multiple phases for "punch" |
| Music-reactive | Wave interference + beat sync | Medium | ~300 bytes | Creates standing patterns |
| Energy charging | Perlin edge + pulse | Low-Medium | ~400 bytes | Build-up visualization |
| Particle effects | Ring buffer pool | Medium | ~1KB per 64 particles | Flexible, reusable |
| Glowing highlights | Local bloom + threshold | Low | ~0 (in-place) | Fast post-process |
| Motion trails | History buffer | Low-Medium | 120-500 bytes | Velocity-based stretching optional |
| State communication | Simple patterns + pulse | Low | ~100 bytes | Not computationally heavy |
| Cellular automata | Rule-based stepping | Medium | ~320 bytes | Mesmerizing, evolving patterns |
| Colour shifts | Chromatic aberration | Low | ~0 (in-place) | Great for overlays |

