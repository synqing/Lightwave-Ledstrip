# Feature Branch Documentation: Zone Composer Implementation

## Branch: `feature/zone-composer`

**Date:** 2025-12-09
**Author:** Claude Code Analysis
**Status:** Ready for Review & Staging

---

## Executive Summary

This feature branch implements a comprehensive **Zone Composer System** for the LightwaveOS LED controller, enabling independent control of up to 4 concentric LED zones with individual effect assignments. The implementation also includes a complete **web-based control interface**, 5 new **LGP Novel Physics effects**, and significant **WebServer API enhancements**.

**Total Changes:** +949 insertions, -1,143 deletions (net reduction due to removal of deprecated `script.js`)

---

## Staging Strategy

### Recommended Commit Sequence

The changes should be staged in **6 logical commits** to maintain clean git history and enable easy rollback of specific features:

---

### Commit 1: Zone Composer Core Architecture
**Files:**
```
src/effects/zones/ZoneDefinition.h       (NEW)
src/effects/zones/ZoneConfig.h           (NEW)
src/effects/zones/ZoneConfigManager.h    (NEW)
src/effects/zones/ZoneConfigManager.cpp  (NEW)
src/effects/zones/ZoneComposer.h         (NEW)
src/effects/zones/ZoneComposer.cpp       (NEW)
src/config/hardware_config.h             (MODIFIED)
```

**Commit Message:**
```
feat(zones): Implement Zone Composer multi-zone effect system

Architecture:
- ZoneDefinition: Center-origin zone layout (4 zones, 40 LEDs each)
- ZoneComposer: Buffer proxy pattern for effect isolation
- ZoneConfigManager: NVS persistence and 5 built-in presets

Zone Layout (symmetric around LED 79/80):
- Zone 0 (innermost): LEDs 60-79 + 80-99 (40 LEDs)
- Zone 1: LEDs 40-59 + 100-119 (40 LEDs)
- Zone 2: LEDs 20-39 + 120-139 (40 LEDs)
- Zone 3 (outermost): LEDs 0-19 + 140-159 (40 LEDs)

Features:
- Independent effect per zone with automatic buffer isolation
- Enable/disable individual zones
- NVS configuration persistence
- 5 built-in presets (Single, Dual, Triple, Quad Active, Alternating)
```

---

### Commit 2: LGP Novel Physics Effects
**Files:**
```
src/effects/strip/LGPNovelPhysics.h   (NEW)
src/effects/strip/LGPNovelPhysics.cpp (NEW)
src/main.cpp                           (MODIFIED - effect registration only)
```

**Commit Message:**
```
feat(effects): Add 5 LGP Novel Physics effects exploiting dual-strip interference

New Effects:
1. Chladni Plate Harmonics
   - Acoustic resonance pattern visualization
   - Sand particle migration from antinodes to nodes
   - Dual strips show top/bottom plate surfaces with 180-degree phase offset

2. Gravitational Wave Chirp
   - Binary black hole inspiral simulation (LIGO-accurate)
   - Exponential frequency evolution (chirp), merger flash, ringdown
   - Strip1=h+ polarization, Strip2=hx polarization (90-degree offset)

3. Quantum Entanglement Collapse
   - EPR paradox visualization with superposition and measurement
   - Collapse wavefront reveals perfect anti-correlation
   - Complementary colors demonstrate quantum entanglement

4. Mycelial Network Propagation
   - Fungal hyphal growth with fractal branching
   - Nutrient flow visualization with direction bias
   - Fruiting body formation at high-overlap zones

5. Riley Dissonance
   - Op Art perceptual instability (Bridget Riley inspired)
   - Binocular rivalry through frequency mismatch patterns
   - Concentric circles, stripes, checkerboard, spiral modes

All effects use encoder-mapped parameters:
- Speed (3), Intensity (4), Saturation (5), Complexity (6), Variation (7)
```

---

### Commit 3: WebServer API Expansion
**Files:**
```
src/network/WebServer.cpp (MODIFIED)
src/network/WebServer.h   (MODIFIED)
```

**Commit Message:**
```
feat(api): Expand WebServer with Zone Composer and Visual Pipeline endpoints

New REST API Endpoints:
GET  /api/effects?start=N&count=M  - Paginated effect list with categories
GET  /api/palettes                  - Available color palettes
POST /api/speed                     - Effect animation speed (1-50)
POST /api/palette                   - Palette selection
POST /api/pipeline                  - Visual pipeline (intensity, saturation, complexity, variation)
POST /api/transitions               - Toggle random transitions

Zone Composer Endpoints:
GET  /api/zone/status               - Zone configuration and state
POST /api/zone/enable               - Enable/disable zone mode
POST /api/zone/config               - Configure zone effect/enabled state
POST /api/zone/count                - Set active zone count (1-4)
GET  /api/zone/presets              - List available presets
POST /api/zone/preset/load          - Load preset configuration
POST /api/zone/config/save          - Persist config to NVS
POST /api/zone/config/load          - Load config from NVS

WebSocket Message Types (new):
- zone.enable, zone.setCount, zone.setEffect
- zone.enableZone, zone.loadPreset
- zone.save, zone.load
- setPipeline, toggleTransitions
```

---

### Commit 4: Web Interface Implementation
**Files:**
```
data/index.html     (MODIFIED - complete redesign)
data/app.js         (NEW)
data/styles.css     (MODIFIED)
data/script.js      (DELETED)
```

**Commit Message:**
```
feat(webapp): Modern web control interface with Tailwind CSS

UI Architecture:
- Tailwind CSS via CDN with custom design tokens
- Vanilla JavaScript with WebSocket + REST API integration
- Mobile-responsive with sticky preset bar

Design System (Prism-inspired):
- Canvas: #1c2130, Surface: #252d3f, Elevated: #2f3849
- Accent Cyan: #00d4ff
- Zone Colors: Zone1=#6ee7f3, Zone2=#22dd88, Zone3=#ffb84d

Components:
1. Status Bar: FPS, Memory, Current Effect, Connection Status
2. LED Strip Visualization: 5-segment zone preview with overlays
3. Zone Composer Card: Zone tabs, effect dropdown, enable/disable
4. Global Parameters Card: Brightness, Speed, Palette, Transitions
5. Visual Pipeline Card: Collapsible intensity/saturation/complexity/variation
6. Preset Bar: Quick preset loading with save functionality

Features:
- WebSocket auto-reconnect (3s interval)
- Debounced slider API calls (150ms/250ms)
- Dynamic effect list from /api/effects
- Zone selection with visual feedback
```

---

### Commit 5: Main Application Integration
**Files:**
```
src/main.cpp              (MODIFIED)
src/config/features.h     (MODIFIED)
```

**Commit Message:**
```
feat(main): Integrate Zone Composer with main loop and serial commands

Integration Points:
- ZoneComposer initialization in setup() with preset loading
- effectUpdateCallback() routes to Zone Composer when enabled
- Serial command support for zone control

New Serial Commands:
zone status              - Show zone configuration
zone on/off              - Enable/disable zone mode
zone count <1-4>         - Set number of zones
zone <0-3> effect <id>   - Assign effect to zone
zone <0-3> enable        - Enable specific zone
zone <0-3> disable       - Disable specific zone
zone presets             - List available presets
zone preset <0-4>        - Load preset configuration
zone save                - Save config to NVS
zone load                - Load config from NVS

Feature Flag Changes:
- FEATURE_WEB_SERVER=1 (enabled for webapp)
- FEATURE_AUDIO_SYNC=0 (disabled, no microphone)
- FEATURE_BUTTON_CONTROL=0 (no physical button)
- FEATURE_SCROLL_ENCODER=0, FEATURE_ROTATE8_ENCODER=0 (HMI removed)
```

---

### Commit 6: Documentation and Cleanup
**Files:**
```
data/app.js.backup          (DELETE or .gitignore)
data/index.html.backup      (DELETE or .gitignore)
data/styles.css.backup      (DELETE or .gitignore)
AURA_BUILD_PROMPT.md.backup (DELETE or .gitignore)
```

**Commit Message:**
```
chore(cleanup): Remove backup files and add documentation

Removed:
- *.backup files (development artifacts)

Note: Documentation and audit files (*.md) in root are development
references and can be .gitignored or deleted after merge.
```

---

## Technical Documentation

### 1. Zone Composer System

#### 1.1 Architecture Overview

The Zone Composer implements a **buffer proxy pattern** that isolates effect rendering:

```
┌─────────────────────────────────────────────────────────────┐
│                      Zone Composer                          │
├─────────────────────────────────────────────────────────────┤
│  for each enabled zone:                                     │
│    1. Clear strip1[]/strip2[] buffers                       │
│    2. Execute effect->function() (renders to full buffer)   │
│    3. Extract zone segment → m_outputStrip1/2               │
│    4. Clear buffers for next zone                           │
│  After all zones:                                           │
│    Copy m_outputStrip1/2 → strip1[]/strip2[]               │
│    Sync to unified leds[] buffer                            │
└─────────────────────────────────────────────────────────────┘
```

#### 1.2 Zone Layout (Center-Origin)

All zones are **symmetric around the center point (LED 79/80)**:

| Zone | Description | LED Range (Left) | LED Range (Right) | Total |
|------|-------------|------------------|-------------------|-------|
| 0    | Innermost   | 60-79            | 80-99             | 40    |
| 1    | Second ring | 40-59            | 100-119           | 40    |
| 2    | Third ring  | 20-39            | 120-139           | 40    |
| 3    | Outermost   | 0-19             | 140-159           | 40    |

#### 1.3 Buffer Architecture

```cpp
class ZoneComposer {
    // Per-zone temporary buffers (effect renders here)
    CRGB m_tempStrip1[40];
    CRGB m_tempStrip2[40];

    // Composite output buffers (all zones combined)
    CRGB m_outputStrip1[160];
    CRGB m_outputStrip2[160];
}
```

#### 1.4 Configuration Persistence

```cpp
struct ZoneConfig {
    uint8_t zoneCount;                      // 1-4
    uint8_t zoneEffects[MAX_ZONES];         // Effect ID per zone
    bool zoneEnabled[MAX_ZONES];            // Enable state per zone
    uint32_t checksum;                      // CRC32 validation
};
```

Storage: ESP32 NVS namespace `zone_config`, key `config`.

#### 1.5 Built-in Presets

| ID | Name            | Zones | Configuration                          |
|----|-----------------|-------|----------------------------------------|
| 0  | Single Zone     | 1     | Zone 0 only (center 40 LEDs)           |
| 1  | Dual Zone       | 2     | Zone 0 + Zone 1                        |
| 2  | Triple Zone     | 3     | Zones 0-2 with different effects       |
| 3  | Quad Active     | 4     | All 4 zones with varied effects        |
| 4  | Alternating     | 4     | Odd zones enabled, even disabled       |

---

### 2. LGP Novel Physics Effects

#### 2.1 Chladni Plate Harmonics (`lgpChladniHarmonics`)

**Physics Model:** Simulates Ernst Chladni's vibrating plate experiments.

```
Mode Shape:     ψ(x) = sin(n × π × x / L)
Displacement:   z(x,t) = ψ(x) × cos(ωt)
Node Strength:  1 / (|ψ(x)| + 0.1)  (particles accumulate at nodes)
```

**Dual-Strip Mapping:**
- Strip1 = Top plate surface (phase 0)
- Strip2 = Bottom plate surface (phase 180°)

**Parameters:**
- `complexity`: Mode number n (1-12)
- `variation`: Mode mixing for damped/chaotic patterns

---

#### 2.2 Gravitational Wave Chirp (`lgpGravitationalWaveChirp`)

**Physics Model:** Binary black hole inspiral with LIGO-accurate frequency evolution.

```
Chirp Frequency:  f(t) = f₀ × (1 - t/t_merge)^(-3/8 × mass_ratio)
Amplitude:        h(t) = h₀ × (1 + 2 × progress)
Ringdown:         A(t) = A₀ × exp(-t × 0.05) × sin(ω_ringdown × t)
```

**State Machine:**
1. **Inspiral**: Frequency chirps from ~1Hz to ~20Hz, amplitude increases
2. **Merger**: Bright flash at center, ~200ms duration
3. **Ringdown**: Damped sinusoidal oscillation, expanding ring

**Dual-Strip Mapping:**
- Strip1 = h+ polarization (parallel to orbital plane)
- Strip2 = hx polarization (90° rotated, orthogonal)

---

#### 2.3 Quantum Entanglement Collapse (`lgpQuantumEntanglementCollapse`)

**Physics Model:** EPR paradox visualization with wavefunction collapse.

```
Superposition:  |ψ⟩ = α|↑⟩ + β|↓⟩  (random colors, quantum foam)
Collapse:       Wavefront expands from center at speed v
Entanglement:   Color_strip2 = Complementary(Color_strip1)
```

**State Machine:**
1. **Superposition**: Random, uncorrelated colors (quantum foam)
2. **Measurement**: Triggered randomly after 1-2 seconds
3. **Collapse**: Wavefront expands, revealing deterministic colors
4. **Collapsed**: Perfect anti-correlation (complementary hues, Δhue=128)

**Parameters:**
- `complexity`: Quantum mode n (1-8) → wavefunction nodes
- `variation`: Decoherence rate at edges

---

#### 2.4 Mycelial Network Propagation (`lgpMycelialNetwork`)

**Algorithm:** Particle system with 16 hyphal growth tips.

```cpp
struct GrowthTip {
    float position;    // LED index (float for subpixel)
    float velocity;    // Growth direction and speed
    float age;         // For branching probability
    bool active;       // Enable state
};
```

**Branching Logic:**
```
P(branch) = 0.001 + complexity × 0.01
New tip velocity = -parent_velocity × (0.5 + random(0.5))
```

**Features:**
- Network density map with slow decay (0.998× per frame)
- Nutrient flow visualization (directional sine wave)
- Fruiting bodies at high dual-strip overlap (golden glow)

---

#### 2.5 Riley Dissonance (`lgpRileyDissonance`)

**Op Art Theory:** Binocular rivalry through frequency mismatch.

```
Base Frequency:     f_base = 8 + complexity × 12  (8-20 cycles)
Frequency Mismatch: Δf = 0.02 + variation × 0.15  (2-17%)
Strip1 Frequency:   f₁ = f_base × (1 + Δf/2)
Strip2 Frequency:   f₂ = f_base × (1 - Δf/2)
Beat Frequency:     |f₁ - f₂| = f_base × Δf
```

**Pattern Modes:**
| Complexity Range | Pattern Type     | Formula |
|------------------|------------------|---------|
| 0.00-0.25        | Concentric Rings | sin(r × f × 2π) |
| 0.25-0.50        | Linear Stripes   | sin(x × f × 2π) |
| 0.50-0.75        | Checkerboard     | sin(x×f) × sin(r×f) |
| 0.75-1.00        | Spirals          | sin((θ + r×3) × f/4) |

**Contrast Enhancement:**
```
pattern_out = tanh(pattern_in × contrast) / tanh(contrast)
contrast = 1 + intensity × 4  (1-5× boost)
```

---

### 3. WebServer API Reference

#### 3.1 REST Endpoints

| Method | Endpoint | Body | Response |
|--------|----------|------|----------|
| GET | `/api/status` | - | `{effect, brightness, gHue, network{...}}` |
| GET | `/api/effects?start=0&count=100` | - | `{effects[{id,name,category}], total}` |
| GET | `/api/palettes` | - | `{palettes[{id,name}]}` |
| POST | `/api/effect` | `{effect: N}` | `{status: "ok"}` |
| POST | `/api/brightness` | `{brightness: 0-255}` | `{status: "ok"}` |
| POST | `/api/speed` | `{speed: 1-50}` | `{status: "ok"}` |
| POST | `/api/palette` | `{paletteId: N}` | `{status: "ok"}` |
| POST | `/api/pipeline` | `{intensity, saturation, complexity, variation}` | `{status: "ok"}` |
| POST | `/api/transitions` | `{enabled: bool}` | `{status: "ok"}` |
| GET | `/api/zone/status` | - | `{enabled, zoneCount, zones[...]}` |
| POST | `/api/zone/enable` | `{enabled: bool}` | `{status: "ok"}` |
| POST | `/api/zone/config` | `{zoneId, effectId?, enabled?}` | `{status: "ok"}` |
| POST | `/api/zone/count` | `{count: 1-4}` | `{status: "ok"}` |
| GET | `/api/zone/presets` | - | `{presets[{id,name}]}` |
| POST | `/api/zone/preset/load` | `{presetId: 0-4}` | `{status: "ok"}` |
| POST | `/api/zone/config/save` | - | `{status: "ok"}` |
| POST | `/api/zone/config/load` | - | `{status: "ok"}` |

#### 3.2 WebSocket Messages

**Client → Server:**
```json
{"type": "setEffect", "effectId": 5}
{"type": "nextEffect"}
{"type": "prevEffect"}
{"type": "setBrightness", "value": 200}
{"type": "setSpeed", "value": 25}
{"type": "setPalette", "paletteId": 3}
{"type": "setPipeline", "intensity": 128, "saturation": 200, ...}
{"type": "toggleTransitions", "enabled": true}
{"type": "zone.enable", "enable": true}
{"type": "zone.setCount", "count": 3}
{"type": "zone.setEffect", "zoneId": 0, "effectId": 5}
{"type": "zone.enableZone", "zoneId": 1, "enabled": false}
{"type": "zone.loadPreset", "presetId": 2}
{"type": "zone.save"}
{"type": "zone.load"}
```

**Server → Client:**
```json
{"type": "status", "effect": 5, "brightness": 200, "freeHeap": 250000, "network": {...}}
{"type": "network", "event": "wifi_connected", "ip": "192.168.1.100", "ssid": "..."}
{"type": "effectChange", "effectId": 5, "name": "Fire"}
```

---

### 4. Web Interface (app.js)

#### 4.1 State Management

```javascript
const state = {
    // Connection
    ws: null,
    connected: false,

    // Zone Composer
    zoneMode: false,
    zoneCount: 3,
    selectedZone: 1,
    zones: [{id:1, effectId:0, enabled:true}, ...],

    // Global Parameters
    brightness: 210,
    speed: 32,
    paletteId: 0,
    intensity: 128,
    saturation: 200,
    complexity: 45,
    variation: 180,
    transitions: false,

    // Performance
    fps: 0,
    memoryFree: 0,
};
```

#### 4.2 API Client Pattern

```javascript
const API = {
    async request(endpoint, options = {}) {
        const response = await fetch(`/api${endpoint}`, {
            headers: {'Content-Type': 'application/json'},
            ...options,
        });
        return response.json();
    },

    setBrightness: (value) => this.request('/brightness', {
        method: 'POST',
        body: JSON.stringify({brightness: value}),
    }),
    // ...
};
```

#### 4.3 Debouncing Strategy

| Control Type | Debounce Delay | Rationale |
|--------------|----------------|-----------|
| Brightness/Speed Sliders | 150ms | Responsive feel, moderate API load |
| Pipeline Sliders | 250ms | Batched updates (4 params at once) |
| Zone Selection | Immediate | UI state only, no API |
| Effect Selection | Immediate | Critical user intent |

---

### 5. Configuration Changes

#### 5.1 features.h Changes

| Flag | Old | New | Reason |
|------|-----|-----|--------|
| `FEATURE_WEB_SERVER` | 0 | 1 | Enable webapp |
| `FEATURE_AUDIO_SYNC` | 1 | 0 | No microphone hardware |
| `FEATURE_BUTTON_CONTROL` | 1 | 0 | No physical button |
| `FEATURE_SCROLL_ENCODER` | 1 | 0 | HMI removed |
| `FEATURE_ROTATE8_ENCODER` | 1 | 0 | HMI removed |

#### 5.2 hardware_config.h Additions

```cpp
// Zone Composer Configuration
constexpr uint8_t MAX_ZONES = 4;
constexpr uint8_t ZONE_SIZE = 40;
constexpr uint8_t ZONE_SEGMENT_SIZE = 20;
```

---

### 6. File Inventory

#### 6.1 New Files (13 files)

| File | Lines | Purpose |
|------|-------|---------|
| `src/effects/zones/ZoneDefinition.h` | 90 | Zone LED range definitions |
| `src/effects/zones/ZoneConfig.h` | ~50 | Configuration struct + presets |
| `src/effects/zones/ZoneConfigManager.h` | 39 | NVS persistence interface |
| `src/effects/zones/ZoneConfigManager.cpp` | ~150 | NVS implementation |
| `src/effects/zones/ZoneComposer.h` | 72 | Zone composer class |
| `src/effects/zones/ZoneComposer.cpp` | 260 | Zone rendering logic |
| `src/effects/strip/LGPNovelPhysics.h` | 72 | Novel physics declarations |
| `src/effects/strip/LGPNovelPhysics.cpp` | 633 | 5 novel physics effects |
| `data/app.js` | 618 | Web UI JavaScript |

#### 6.2 Modified Files (7 files)

| File | Changes | Impact |
|------|---------|--------|
| `src/main.cpp` | +135 lines | Zone integration, serial commands |
| `src/network/WebServer.cpp` | +425 lines | New API endpoints |
| `src/network/WebServer.h` | +7 lines | New method declarations |
| `src/config/features.h` | Flag changes | Feature toggles |
| `src/config/hardware_config.h` | +7 lines | Zone constants |
| `data/index.html` | Complete rewrite | Modern UI |
| `data/styles.css` | Minimal changes | Toggle styling |

#### 6.3 Deleted Files (1 file)

| File | Reason |
|------|--------|
| `data/script.js` | Replaced by `app.js` |

---

## Testing Recommendations

### Unit Tests
1. Zone buffer isolation (effect renders don't bleed)
2. Zone boundary validation (no out-of-bounds writes)
3. NVS save/load round-trip
4. Preset loading correctness

### Integration Tests
1. WebSocket reconnection after disconnect
2. API endpoint response validation
3. Zone mode enable/disable transitions
4. Effect transitions with Zone Composer enabled

### Manual Testing Checklist
- [ ] Web UI loads on `http://lightwaveos.local`
- [ ] Zone tabs switch correctly with visual feedback
- [ ] Effect dropdown populates from API
- [ ] Sliders update values with visual response
- [ ] Preset load/save works
- [ ] Serial `zone status` shows correct config
- [ ] Novel physics effects render on both strips
- [ ] Memory stable after 1 hour operation

---

## Known Limitations

1. **Zone Rendering Overhead**: Each zone requires a full effect render + buffer copy. With 4 zones, this is 4× the effect computation per frame.

2. **Effect Compatibility**: Effects that maintain internal state across frames may behave unexpectedly in zone mode (state resets each zone render).

3. **WebSocket Single Client**: Status broadcasts go to all clients; no per-client state.

4. **Effect Categories**: Currently hardcoded by ID ranges in WebServer.cpp; should be metadata in effects array.

---

## Future Enhancements

1. **Zone Blending Modes**: Additive, overlay, or crossfade between zone edges
2. **Zone Transitions**: Animated zone expansion/contraction
3. **Effect Parameters Per Zone**: Independent speed/intensity per zone
4. **Custom Zone Boundaries**: User-defined LED ranges via web UI
5. **Effect Categories API**: Structured effect metadata endpoint

---

*Document generated by Claude Code automated analysis*
