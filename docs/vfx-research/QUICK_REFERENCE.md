# LED Effect VFX Techniques - Quick Reference

## One-Page Lookup for Common Visual Goals

### Visual Effect → Recommended Techniques

| Visual Goal | Technique Stack | Complexity | Memory | Example |
|------------|----------------|-----------|--------|---------|
| **Flowing water/liquid** | Perlin noise + post-bloom | Low | 1KB | Base: noise(pos + time), Post: bloom(leds, 150) |
| **Explosion** | Burst particles + shockwave + afterglow | High | 1.5KB | emit(particles), renderShock(), renderGlow() |
| **Pulsing energy** | Sine wave + Perlin edge + noise | Low-Med | 800B | Sine for brightness, Perlin for waviness |
| **Charging effect** | Linear fill + edge pulse + noise | Low | 600B | Fill progress from edges, pulse edge, add noise |
| **Impact/hit** | Flash + shockwave ring + bloom | Medium | 1KB | Bright flash (100ms), expanding ring, glow |
| **Music sync beat** | Wave interference + frequency shift | Medium | 500B | 2 sin waves @ beat freq, multiply by beat intensity |
| **Rainbows** | Illegal | N/A | N/A | Don't do this. Use colour gradients instead. |
| **Flowing particles** | Velocity-based trails + additive blend | Low-Med | 120B | History buffer, render with fade |
| **State indicator** | Colour + pulse pattern | Very Low | 0B | Healthy=steady green, Warning=pulse yellow, Critical=strobe red |
| **Magical effect** | Chromatic aberration + bloom + wave | Medium | 1.5KB | RGB offset + bloom + sin wave modulation |
| **Scanning beam** | Linear motion + Gaussian blur | Low | 0B | pos += vel*dt, apply local bloom around pos |
| **Cellular pattern** | Rule 110 CA + history fade | Low | 320B | Apply rule each frame, fade old cells |
| **Interference pattern** | Multiple sin waves, magnitude at each pixel | Medium | 500B | For each i: sum(sin(i*freq + time)), show fabs() |
| **Trail effect** | Position history buffer + exponential fade | Low | 200B | Keep last 30 positions, render oldest→newest with fade |

---

## Quick Algorithm Reference

### Input: Effect Name | Output: Code Snippet

#### Perlin Noise Effect (Smooth Organic)
```cpp
PerlinNoise1D perlin;
for (int i = 0; i < 320; i++) {
    float n = perlin.noise((float)i * 0.01f + time * 3.0f);
    leds[i] = RGB(0, (uint8_t)(n * 255), 0);
}
```

#### Pulsing LED
```cpp
float pulse = sin(time * 6.28f) * 0.5f + 0.5f;  // 0.0 to 1.0
for (int i = 0; i < 320; i++) {
    leds[i] = RGB((uint8_t)(pulse * 255), 0, 0);
}
```

#### Expanding Shockwave
```cpp
for (int i = 0; i < 320; i++) {
    float dist = fabs((float)i - centre);
    float radius = shockTime * 100.0f;
    if (dist < radius && dist > radius - 5.0f) {
        float intensity = 1.0f - (dist - (radius - 5.0f)) / 5.0f;
        leds[i] = RGB((uint8_t)(intensity * 255), 0, 0);
    }
}
```

#### Wave Interference (Standing Waves)
```cpp
for (int i = 0; i < 320; i++) {
    float x = (float)i / 320.0f * 6.28f;
    float w1 = sin(x * 2.0f + time);
    float w2 = sin(x * 3.0f + time + M_PI/2);
    float brightness = fabs((w1 + w2) * 0.5f);
    leds[i] = RGB(0, (uint8_t)(brightness * 200), 0);
}
```

#### Bloom Effect
```cpp
BloomEffect bloom;
bloom.apply(leds, 180.0f, 0.4f);  // threshold, strength
```

#### Simple Bloom (Local, No Downsampling)
```cpp
for (int i = 0; i < 320; i++) {
    float lum = getLuminance(leds[i]);
    if (lum > 180.0f) {
        float overflow = (lum - 180) / 255;
        if (i > 0) leds[i-1] = additiveBlend(leds[i-1], leds[i], overflow * 0.3f);
        if (i < 319) leds[i+1] = additiveBlend(leds[i+1], leds[i], overflow * 0.3f);
    }
}
```

#### Particle Burst
```cpp
ParticlePool particles;
for (int i = 0; i < 16; i++) {
    float vel = (i < 8) ? 100 : -100;  // Left/right burst
    particles.emit(160, vel, RGB(255, 100, 0));
}
particles.update(dt);
particles.render(leds);
```

#### Motion Trail
```cpp
// Maintain float positionHistory[30] and ageHistory[30]
// Shift and add current position each frame
for (int h = 0; h < 30; h++) {
    int ledIdx = (int)positionHistory[h];
    float age = ageHistory[h];
    float fade = 1.0f - (age / 0.25f);
    if (fade > 0) {
        uint32_t col = scaleColour(colour, fade);
        leds[ledIdx] = additiveBlend(leds[ledIdx], col, 1.0f);
    }
}
```

#### Cellular Automata
```cpp
CellularAutomataEffect ca(110);  // Rule 110
// In render loop:
ca.update();
ca.render(leds);
```

#### Chromatic Aberration
```cpp
float offset = 5.0f * sin(time * 2.0f);
// Shift R channel left, B channel right
// Result: colour separation at motion
```

#### Linear Fill (Charging)
```cpp
int fillAmount = (int)(charge * 160);  // 0 to 160
for (int i = 0; i < 320; i++) {
    if (i < 160 - fillAmount || i >= 160 + fillAmount) {
        leds[i] = RGB(0, 100, 200);  // Filled
    } else {
        // Edge pulse
        float pulse = sin(time * 6.28f * 2.0f) * 0.5f + 0.5f;
        leds[i] = RGB(0, (uint8_t)(pulse * 255), 255);
    }
}
```

---

## Common Shader Approximations for LEDs

| Shader Concept | LED Approximation | Code Example |
|---|---|---|
| Bloom | Downsample → Blur → Upsample additive | See BloomEffect |
| Glow | Local additive spread to neighbors | `applyLocalBloom()` |
| Chromatic aberration | Offset RGB channels | Shift position by offset per channel |
| Motion blur | History buffer with fade | Keep position history, render faded |
| Noise texture | Perlin/Simplex at (pos, time) | `perlin.noise(pos * scale + time * speed)` |
| Fresnel | Angle-dependent fade (1D = edge effect) | Fade at strip edges based on angle |
| Parallax | Multiple noise octaves at different speeds | Render overlapping noise patterns |

---

## Performance Budget Checklist (120 FPS = 8.3ms per frame)

- **Perlin noise:** ~4.8ms for 320 LEDs (1 octave) → OK
- **Perlin noise (3 octave):** ~14.4ms → TOO SLOW
- **Bloom (3-pass):** ~1.5ms → OK
- **Bloom (local):** ~0.5ms → OK
- **Particle update (64 particles):** ~0.1ms → OK
- **Wave interference:** ~2-5ms → OK
- **Cellular automata step:** ~1-2ms per generation → OK
- **LED write (DMA):** ~1-2ms → OK
- **Network/serial:** Varies, budget 0.5-1ms
- **Safety margin:** ~2ms for spikes

**Typical Full Effect:**
- Base effect: 2-4ms
- Post-processing: 0.5-1ms
- Overlays (particles, trails): 1-2ms
- LED write: 1-2ms
- Total: 5-9ms (safe)

---

## Memory Budget Checklist (ESP32 Internal SRAM = ~320KB, PSRAM = 8MB)

**Per-Effect SRAM Cost:**

| Component | Bytes | Notes |
|-----------|-------|-------|
| Perlin Noise | 1,024 | Permutation table (fixed) |
| Bloom buffers | 2,048 | Downsampled + blurred |
| Particle pool (64) | 1,536 | 24 bytes per particle |
| History buffer (30) | 240 | For motion trails |
| CA state | 320 | Current generation |
| Temporary buffers | 960 | RGB extraction, blend |
| **Total per effect** | **~6-8KB** | Multiple effects can coexist |

**Recommendation:**
- Keep individual effect SRAM < 4KB
- Use PSRAM for large buffers (audio FFT, HD precalculated data)
- Reuse buffers across effects (use object pool pattern)

---

## Colour Theory for LEDs

### Emotional Impact by Colour
| Colour | Feeling | Use Case |
|--------|---------|----------|
| **Red** | Danger, intensity, power | Impacts, hits, charging peak |
| **Blue** | Calm, energy, technology | Charging, baseline, scan effects |
| **Green** | Health, growth, safe | Status OK, healing, flow |
| **Yellow** | Caution, energy | Warning state, excitement |
| **Cyan** | Cool, futuristic | Sci-fi effects, magic |
| **Magenta** | Intensity, otherworldly | Explosions, special effects |
| **White** | Brightness, flash | Initial burst, full intensity |

### Colour Shift Techniques
- **Hue rotation:** Cycle through colour wheel over time (avoid full rainbow)
- **Saturation fade:** Desaturate as brightness increases (more natural glow)
- **Value modulation:** Brightness drives intensity independently
- **HSV space:** Easier for procedural colour than RGB

---

## Beat Sync Integration

### Simple Beat Detection
```cpp
float currentMagnitude = getAudioMagnitude();
float avgMagnitude = movingAverage(currentMagnitude, 20);

if (currentMagnitude > avgMagnitude * 1.2f) {  // Threshold
    triggerBeatEffect();
}
```

### Frequency-Locked Effects
```cpp
// Update frequency based on beat detection
if (isBeat) {
    frequency *= beatIntensity * 0.5f;  // Shift frequency by beat
}

for (int i = 0; i < 320; i++) {
    float wave = sin((float)i * frequency + time);
    leds[i] = RGB(0, (uint8_t)(fabs(wave) * 255), 0);
}
```

### Time Sync (Beats per Minute)
```cpp
float bpm = 120.0f;
float beatTime = fmod(time, 60.0f / bpm);  // Time within beat
float beatPhase = beatTime / (60.0f / bpm);  // 0.0 to 1.0

// Use beatPhase for animations
float intensity = sin(beatPhase * M_PI);  // 0 to 1 to 0 per beat
```

---

## Debugging Tips

### Verify Perlin Noise
```cpp
// Should see smooth transitions without repeating patterns
PerlinNoise1D p;
for (float x = 0; x < 100; x += 0.1f) {
    Serial.println(p.noise(x), 4);  // Print 4 decimal places
}
```

### Check Bloom Threshold
```cpp
// Render with threshold visualization
for (int i = 0; i < 320; i++) {
    float lum = getLuminance(leds[i]);
    leds[i] = (lum > threshold) ? RGB(255, 0, 0) : RGB(0, 100, 0);
}
// Red = will bloom, Green = won't bloom
```

### Profile Effect
```cpp
PROFILE_START(effect);
renderEffect();
PROFILE_END(profiler, effect);
profiler.printReport();  // See microseconds
```

### Memory Leak Check
```cpp
uint32_t freeHeap = ESP.getFreeHeap();
Serial.printf("Free heap: %d bytes\n", freeHeap);
// Run effect 100 times
// Check if freeHeap decreases → memory leak
```

---

## Common Pitfalls & Fixes

| Pitfall | Symptom | Fix |
|---------|---------|-----|
| malloc/new in render | Frame stuttering, crashes | Use static buffers, pre-allocate |
| No clipping on additive blend | Colours overflow, look washed | Clamp RGB to [0, 255] |
| Floating-point division in loop | Frame drops | Precompute reciprocals |
| Using modulo (%) on non-power-of-2 | Slow | Use bitwise AND if possible: `& 255` |
| Full rainbow sweep | Violates constraints | Use colour gradients, HSV space |
| Allocating temp buffers each frame | Memory fragmentation | Declare static or PSRAM |
| No boundary checking on array access | Crashes, memory corruption | Always check `i >= 0 && i < 320` |
| Rendering to same buffer while reading | Visual glitches | Use double-buffer or temp array |
| No post-processing clamp | Colour overflow | Clamp each RGB channel after blend |

---

## Quick Wins: Easy Effects with Big Impact

### 1. Sine Wave Pulse (5 lines)
```cpp
float pulse = sin(time * 6.28f) * 0.5f + 0.5f;
for (int i = 0; i < 320; i++) {
    leds[i] = RGB(0, (uint8_t)(pulse * 255), 0);
}
```
**Time:** <0.1ms | **Memory:** 0B

### 2. Moving Dot with Trail (10 lines)
```cpp
float pos = 160 + 100 * sin(time * 2);
for (int i = 0; i < 320; i++) {
    float dist = fabs(i - pos);
    float brightness = exp(-dist * dist / 100);
    leds[i] = RGB((uint8_t)(brightness * 255), 0, 0);
}
```
**Time:** ~0.5ms | **Memory:** 0B

### 3. Perlin Flow (3 lines)
```cpp
PerlinNoise1D p;
for (int i = 0; i < 320; i++) {
    leds[i] = RGB(0, (uint8_t)(p.noise(i*0.01f + time*2) * 255), 0);
}
```
**Time:** ~5ms | **Memory:** 1KB

### 4. Impact Flash (1 line)
```cpp
memset(leds, (uint8_t)(intensity * 255), sizeof(leds[0]) * 320);  // White flash
```
**Time:** <0.1ms | **Memory:** 0B

### 5. Health Indicator (5 lines)
```cpp
int healthWidth = (int)(160 * health);
for (int i = 80; i < 240; i++) {
    leds[i] = (i < 80 + healthWidth) ? RGB(0, 255, 0) : RGB(50, 50, 50);
}
```
**Time:** <0.1ms | **Memory:** 0B

---

## References

**Implemented in firmware:**
- `/firmware/v2/src/effects/noise/PerlinNoise1D.h`
- `/firmware/v2/src/effects/postfx/BloomEffect.h`
- `/firmware/v2/src/effects/particles/ParticlePool.h`
- `/firmware/v2/src/effects/procedural/CellularAutomataEffect.h`
- `/firmware/v2/src/effects/procedural/WaveInterferenceEffect.h`

**Research sources:**
- [LearnOpenGL - Bloom](https://learnopengl.com/Advanced-Lighting/Bloom)
- [The Book of Shaders - Noise](https://thebookofshaders.com/11/)
- [Perlin Noise on Arduino](https://www.kasperkamperman.com/blog/arduino/perlin-noise-improved-noise-on-arduino/)
- [Game VFX Design](https://magicmedia.studio/news-insights/guide-to-vfx-for-gaming/)

