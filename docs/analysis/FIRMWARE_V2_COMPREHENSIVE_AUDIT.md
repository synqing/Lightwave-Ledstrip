# LightwaveOS v2 Comprehensive Architecture Audit

**Date:** 2026-01-09  
**Purpose:** Complete architectural assessment before PDF enhancement plan translation  
**Status:** ✅ Complete

---

## Executive Summary

This audit provides a comprehensive review of the LightwaveOS v2 firmware architecture, identifying all major subsystems, their relationships, and current implementation status. This serves as the foundation for translating PDF enhancement recommendations into actionable improvements.

**Key Finding:** Many advanced audio features are **already implemented** but **significantly underutilized** by effects. The system has sophisticated infrastructure that exceeds what most effects currently leverage.

---

## Architecture Overview

### Core Distribution (ESP32-S3 Dual-Core)

```
┌─────────────────────────────────────────────────────────────┐
│                    Core 0 (Network/Input)                   │
│  - AudioNode (I2S capture, K1 analysis, ControlBus)        │
│  - NetworkNode (WebServer, WiFiManager, REST/WS)            │
│  - HmiNode (Encoder polling)                                │
│  - SyncManagerNode (Multi-device sync)                      │
│  - ShowNode (Choreographed shows)                           │
└─────────────────────────────────────────────────────────────┘
                            │
                    MessageBus (lock-free)
                            │
┌─────────────────────────────────────────────────────────────┐
│                    Core 1 (Rendering)                       │
│  - RendererNode (120 FPS LED rendering)                     │
│  - StateStoreNode (CQRS state management)                  │
│  - ZoneComposer (Multi-zone rendering)                       │
│  - TransitionEngine (12 transition types)                   │
│  - ColorCorrectionEngine (Post-processing)                  │
└─────────────────────────────────────────────────────────────┘
```

---

## Major Subsystems

### 1. Audio Pipeline (Core 0)

#### Architecture

```
I2S Capture (16kHz, 128 samples/hop @ 125 Hz)
    ↓
AudioNode (FreeRTOS task, Priority 4)
    ├─→ AudioCapture (I2S DMA)
    │
    ├─→ K1AudioFrontEnd (Dual-Bank Goertzel)
    │   ├─→ AudioRingBuffer (4096 samples history)
    │   │
    │   ├─→ RhythmBank (125 Hz, every hop)
    │   │   ├─→ GoertzelBank (24 bins, 60-600 Hz)
    │   │   ├─→ NoiseFloor subtraction
    │   │   ├─→ AGC (attenuation-only)
    │   │   └─→ NoveltyFlux (onset detection)
    │   │
    │   └─→ HarmonyBank (62.5 Hz, every 2 hops)
    │       ├─→ GoertzelBank (64 bins, 110-4186 Hz)
    │       ├─→ NoiseFloor subtraction
    │       ├─→ AGC (mild boost)
    │       ├─→ ChromaExtractor (64→12 pitch classes)
    │       └─→ ChromaStability (temporal smoothing)
    │
    ├─→ FeatureBus (publish AudioFeatureFrame)
    │   └─→ TempoTracker (consume rhythm_novelty for onset detection)
    │
    ├─→ ControlBus (attack/release smoothing)
    │   ├─→ ChordDetector (triad analysis from chroma)
    │   ├─→ MusicalSaliency (4 dimensions: harmonic, rhythmic, timbral, dynamic)
    │   └─→ StyleDetector (6 music styles: rhythmic, harmonic, melodic, texture, dynamic)
    │
    └─→ SnapshotBuffer<ControlBusFrame> (lock-free cross-core sharing)
```

#### Key Components

| Component | Update Rate | Purpose | Status |
|-----------|-------------|---------|--------|
| **AudioCapture** | 125 Hz | I2S DMA capture | ✅ Implemented |
| **K1AudioFrontEnd** | 125 Hz | Dual-bank Goertzel analysis | ✅ Implemented |
| **RhythmBank** | 125 Hz | Beat/tempo features | ✅ Implemented |
| **HarmonyBank** | 62.5 Hz | Harmonic/chord features | ✅ Implemented |
| **TempoTracker** | 125 Hz | BPM estimation, beat detection | ✅ Implemented |
| **ControlBus** | 125 Hz | Attack/release smoothing | ✅ Implemented |
| **MusicalGrid** | 125 Hz | PLL-style beat phase tracking | ✅ Implemented |
| **ChordDetector** | 62.5 Hz | Triad detection (MAJOR/MINOR/DIM/AUG) | ✅ Implemented |
| **MusicalSaliency** | 125 Hz | 4D saliency metrics | ✅ Implemented (UNUSED) |
| **StyleDetector** | 62.5 Hz | Music style classification | ✅ Implemented (UNUSED) |

#### Audio Data Available to Effects

**ControlBusFrame** (published every hop, 125 Hz):
- RMS/Flux (smoothed + fast variants)
- 8-band frequency analysis (60 Hz - 7.8 kHz)
- 12-bin chromagram (pitch classes C-B)
- Chord state (root, type, confidence)
- Musical saliency (4 dimensions)
- Music style (6 categories)
- Percussion triggers (snare, hihat)
- 64-bin Goertzel spectrum (110-4186 Hz) - **UNUSED**
- 128-sample waveform (time-domain)

**MusicalGridSnapshot** (updated every hop):
- Beat phase (0.0-1.0)
- BPM (30-200)
- Tempo confidence (0.0-1.0)
- Beat/downbeat triggers
- Beat strength

#### Current Usage Analysis

From `docs/audio-visual/AUDIO_REACTIVE_EFFECTS_ANALYSIS.md`:

| Feature | Usage | Status |
|---------|-------|--------|
| RMS/Flux | **Used** | ✅ Active |
| 8-band analysis | **Used** | ✅ Active |
| Chroma (raw) | **Used** | ⚠️ Bug: should use `heavy_chroma[]` |
| Chroma (heavy) | **Used** | ✅ Active |
| Beat phase | **Used** | ✅ Active |
| Chord detection | **Used** | ✅ Active |
| Musical saliency | **Unused** | ❌ Available but ignored |
| Style detection | **Unused** | ❌ Available but ignored |
| 64-bin spectrum | **Unused** | ❌ Available but ignored |
| Percussion triggers | **Partial** | ⚠️ Some effects use, most don't |

**Critical Finding:** Only ~40% of available audio features are actively used by effects.

---

### 2. Visual Pipeline (Core 1)

#### Architecture

```
RendererNode::renderFrame() (120 FPS)
    ↓
AudioContext Preparation (if FEATURE_AUDIO_SYNC)
    ├─→ Read ControlBusFrame (lock-free)
    ├─→ Extrapolate AudioTime
    ├─→ Feed TempoTracker → MusicalGrid
    └─→ Build AudioContext
    ↓
Zone Mode Check
    ├─→ ZoneComposer::render() (if enabled)
    │   ├─→ For each zone (1-4):
    │   │   ├─→ Render effect to zone buffer
    │   │   ├─→ Apply audio parameter mappings (if configured)
    │   │   └─→ Extract zone segment
    │   └─→ Composite zones with blend modes
    │
    └─→ Single-Effect Mode (if zones disabled)
        ├─→ Build EffectContext
        ├─→ Apply AudioEffectMapping (Phase 4)
        ├─→ IEffect::render()
        └─→ Apply ModifierStack (post-render)
    ↓
ColorCorrectionEngine::processBuffer()
    ├─→ Auto-exposure (BT.601 luminance)
    ├─→ V-clamping (white accumulation prevention)
    ├─→ Saturation boost
    ├─→ White/Brown guardrail
    └─→ Gamma correction
    ↓
FastLED.show() (hardware output)
```

#### Key Components

| Component | Purpose | Status |
|-----------|---------|--------|
| **RendererNode** | 120 FPS render loop | ✅ Implemented |
| **ZoneComposer** | Multi-zone rendering (1-4 zones) | ✅ Implemented |
| **TransitionEngine** | 12 transition types (CENTER ORIGIN) | ✅ Implemented |
| **ModifierStack** | Post-processing enhancements | ✅ Implemented |
| **ColorCorrectionEngine** | Auto-exposure, guardrails, gamma | ✅ Implemented |
| **AudioEffectMapping** | Runtime audio→visual bindings | ✅ Implemented |

#### Effect System

**IEffect Interface:**
- `bool init(EffectContext&)` - Reset state
- `void render(EffectContext&)` - Render frame
- `void cleanup()` - Cleanup (usually no-op)
- `const EffectMetadata& getMetadata() const` - Metadata

**Effect Count:** 90+ effects organized by category:
- Core Effects (15)
- LGP Interference (12)
- LGP Chromatic (8)
- LGP Quantum (7)
- LGP Organic (8)
- LGP Physics (10)
- LGP Geometric (6)
- LGP Perlin (12)
- Audio-Reactive (8)

**Pattern Registry:** Taxonomy system with:
- 10 pattern families
- Pattern tags (STANDING, TRAVELING, MOIRE, DEPTH, etc.)
- Metadata (name, story, optical intent, related patterns)
- LGP sensitivity flags
- Color correction skip flags

---

### 3. State Management (CQRS)

#### Architecture

```
StateStore (double-buffered)
    ├─→ SystemState[0] (active, lock-free reads)
    ├─→ SystemState[1] (inactive, write target)
    └─→ Atomic index swap after write
```

**Performance:**
- Query: ~10ns (lock-free pointer dereference)
- Command: <1ms (mutex + copy + notify)
- State size: ~100 bytes

**Commands:**
- `SetEffectCommand`
- `SetBrightnessCommand`
- `SetPaletteCommand`
- `ZoneEnableCommand`
- `ZoneSetEffectCommand`
- `TriggerTransitionCommand`
- `SetVisualParamsCommand`
- ... (20+ commands)

**Subscribers:** Max 8 (for state change notifications)

---

### 4. Network Subsystem (Core 0)

#### Architecture

```
WebServer (Port 80)
    ├─→ REST API v1 (46+ endpoints)
    │   ├─→ Device endpoints
    │   ├─→ Effect endpoints
    │   ├─→ Zone endpoints
    │   ├─→ Parameter endpoints
    │   ├─→ Transition endpoints
    │   ├─→ Audio endpoints (if FEATURE_AUDIO_SYNC)
    │   └─→ Batch operations
    │
    ├─→ WebSocket Gateway (/ws)
    │   ├─→ 109 registered commands
    │   ├─→ Rate limiting (50 msg/sec)
    │   └─→ Connection management
    │
    └─→ Streaming Subsystem
        ├─→ LED frame streaming (20 FPS, binary)
        ├─→ Audio frame streaming (30 FPS, if FEATURE_AUDIO_SYNC)
        └─→ Benchmark streaming (10 FPS, if FEATURE_AUDIO_BENCHMARK)
```

**WebSocket Command Capacity:**
- Maximum handlers: 128
- Current registrations: 109
- Memory usage: ~2.05 KB

**Rate Limiting:**
- HTTP: 20 requests/second
- WebSocket: 50 messages/second
- Per-IP token bucket with sliding window

---

### 5. Audio-to-Visual Mapping (Phase 4)

#### Architecture

```
AudioMappingRegistry (singleton)
    ├─→ EffectAudioMapping[MAX_EFFECTS] (96 effects)
    │   └─→ AudioParameterMapping[MAX_MAPPINGS_PER_EFFECT] (8 mappings)
    │       ├─→ AudioSource (19 sources: RMS, bands, beat phase, etc.)
    │       ├─→ VisualTarget (7 targets: brightness, speed, intensity, etc.)
    │       ├─→ MappingCurve (6 types: LINEAR, SQUARED, SQRT, LOG, EXP, INVERTED)
    │       ├─→ Input/output ranges
    │       ├─→ Smoothing alpha (IIR)
    │       └─→ Additive/replacement mode
    │
    └─→ applyMappings() (called BEFORE effect render)
```

**Audio Sources (19):**
- Energy: RMS, FAST_RMS, FLUX, FAST_FLUX
- Bands: BAND_0 through BAND_7 (8 individual)
- Aggregated: BASS, MID, TREBLE, HEAVY_BASS
- Timing: BEAT_PHASE, BPM, TEMPO_CONFIDENCE

**Visual Targets (7):**
- BRIGHTNESS, SPEED, INTENSITY, SATURATION, COMPLEXITY, VARIATION, HUE

**Status:** ✅ Implemented, but REST/WebSocket API endpoints not yet exposed

---

### 6. Modifier System (Phase A)

#### Architecture

```
ModifierStack (max 8 modifiers)
    ├─→ Pre-render modifiers (modify ctx before render)
    │   └─→ SpeedModifier
    │
    └─→ Post-render modifiers (transform leds[] after render)
        ├─→ IntensityModifier
        ├─→ ColorShiftModifier
        ├─→ MirrorModifier
        └─→ GlitchModifier
```

**Status:** ✅ Implemented (5 modifiers), but REST/WebSocket API endpoints not yet exposed

---

### 7. Audio Behavior Selector (MIS Phase 3)

#### Architecture

```
AudioBehaviorSelector (mixin for effects)
    ├─→ Narrative phase detection (BUILD/HOLD/RELEASE/REST)
    │   ├─→ Energy thresholds
    │   ├─→ Flux analysis
    │   └─→ Beat proximity
    │
    ├─→ Behavior registration (max 8 behaviors per effect)
    │   └─→ Priority-based selection
    │
    ├─→ Behavior selection
    │   ├─→ Phase-based recommendation
    │   ├─→ Style-based recommendation
    │   └─→ Fallback handling
    │
    └─→ Transition management
        ├─→ Smooth transitions (configurable duration)
        └─→ Progress tracking
```

**Visual Behaviors:**
- BREATHE_WITH_DYNAMICS
- PULSE_ON_BEAT
- SHIMMER_WITH_MELODY
- TEXTURE_FLOW
- ... (10+ behaviors)

**Status:** ✅ Implemented, but only used by `BreathingEffect` currently

---

### 8. Show System

#### Architecture

```
ShowNode (Core 0, 20 Hz update rate)
    ├─→ ShowDefinition (PROGMEM)
    │   ├─→ ShowChapter[] (narrative structure)
    │   ├─→ ShowCue[] (timed events)
    │   └─→ ParameterSweeper (concurrent interpolations)
    │
    ├─→ CueScheduler (time-sorted queue)
    │   └─→ Cue types: EFFECT, PARAMETER_SWEEP, ZONE_CONFIG, TRANSITION, NARRATIVE, PALETTE
    │
    └─→ NarrativeEngine integration
        └─→ Phase modulation on chapter transitions
```

**Built-in Shows:** 10 pre-defined shows in PROGMEM

**Status:** ✅ Implemented

---

### 9. Sync System (Multi-Device)

#### Architecture

```
SyncManagerNode (Core 0)
    ├─→ PeerDiscovery (mDNS browsing)
    ├─→ PeerManager (WebSocket client connections)
    ├─→ LeaderElection (Bully algorithm, highest UUID wins)
    ├─→ CommandSerializer (binary encoding)
    ├─→ StateSerializer (full state snapshots)
    └─→ ConflictResolver (last-write-wins)
```

**State Machine:**
```
INITIALIZING → DISCOVERING → ELECTING → LEADING/FOLLOWING → SYNCHRONIZED
                    ↑                            │
                    └── RECONNECTING ←───────────┘
```

**Status:** ✅ Implemented (requires FEATURE_MULTI_DEVICE)

---

### 10. Narrative Engine

#### Architecture

```
NarrativeEngine (singleton)
    ├─→ NarrativeCycle (BUILD → HOLD → RELEASE → REST)
    │   ├─→ Phase durations (configurable)
    │   ├─→ Easing curves (15 types)
    │   └─→ Breathing/snap modulation
    │
    └─→ Zone offsets (spatial choreography)
        └─→ Per-zone phase offsets for wave effects
```

**Status:** ✅ Implemented, integrated with ShowNode

---

## Feature Implementation Status

### Audio Features

| Feature | Status | Usage | Notes |
|---------|--------|-------|-------|
| K1 Dual-Bank Goertzel | ✅ Implemented | ✅ Active | 24+64 bins |
| TempoTracker | ✅ Implemented | ✅ Active | Onset-based BPM |
| MusicalGrid | ✅ Implemented | ✅ Active | PLL beat phase |
| ChordDetector | ✅ Implemented | ✅ Active | Triad analysis |
| MusicalSaliency | ✅ Implemented | ❌ Unused | 4D metrics available |
| StyleDetector | ✅ Implemented | ❌ Unused | 6 styles available |
| 64-bin FFT | ✅ Implemented | ❌ Unused | Full spectrum available |
| AudioEffectMapping | ✅ Implemented | ⚠️ Partial | API not exposed |
| AudioBehaviorSelector | ✅ Implemented | ⚠️ Partial | Only 1 effect uses |

### Visual Features

| Feature | Status | Usage | Notes |
|---------|--------|-------|-------|
| IEffect System | ✅ Implemented | ✅ Active | 90+ effects |
| ZoneComposer | ✅ Implemented | ✅ Active | 1-4 zones |
| TransitionEngine | ✅ Implemented | ✅ Active | 12 types |
| ModifierStack | ✅ Implemented | ⚠️ Partial | API not exposed |
| ColorCorrectionEngine | ✅ Implemented | ✅ Active | Auto-exposure, gamma |
| PatternRegistry | ✅ Implemented | ✅ Active | Taxonomy system |
| NarrativeEngine | ✅ Implemented | ✅ Active | Phase modulation |

### Network Features

| Feature | Status | Usage | Notes |
|---------|--------|-------|-------|
| REST API v1 | ✅ Implemented | ✅ Active | 46+ endpoints |
| WebSocket | ✅ Implemented | ✅ Active | 109 commands |
| LED Streaming | ✅ Implemented | ✅ Active | 20 FPS binary |
| Audio Streaming | ✅ Implemented | ⚠️ Partial | Requires FEATURE_AUDIO_SYNC |
| Rate Limiting | ✅ Implemented | ✅ Active | Per-IP token bucket |

---

## Critical Gaps Identified

### 1. Underutilized Audio Features

**Problem:** Many sophisticated audio features are implemented but not used by effects.

**Examples:**
- Musical saliency (4 dimensions) - **0 effects use**
- Style detection (6 categories) - **0 effects use**
- 64-bin Goertzel spectrum - **0 effects use**
- AudioBehaviorSelector - **1 effect uses** (BreathingEffect)

**Impact:** Effects are missing opportunities for intelligent, adaptive responses to music.

### 2. Missing API Exposure

**Problem:** Advanced features implemented but not accessible via REST/WebSocket.

**Examples:**
- AudioEffectMapping - No endpoints for configuration
- ModifierStack - No endpoints for adding/removing modifiers
- AudioBehaviorSelector - No endpoints for behavior configuration

**Impact:** Users cannot configure these features without firmware changes.

### 3. Known Bugs

**Problem:** AudioWaveformEffect uses raw `chroma[]` instead of `heavy_chroma[]`.

**Impact:** Causes flicker in chroma-based effects.

**Location:** `firmware/v2/src/effects/ieffect/AudioWaveformEffect.cpp` (lines 106, 118)

---

## Architecture Strengths

1. **Sophisticated Audio Pipeline:** K1 dual-bank Goertzel provides rich feature extraction
2. **Modular Design:** Clear separation of concerns (audio/visual/network/state)
3. **Thread Safety:** Lock-free cross-core communication, CQRS state management
4. **Extensibility:** IEffect plugin system, modifier stack, audio mapping
5. **Performance:** Optimized for 120 FPS rendering, efficient memory usage
6. **Comprehensive Features:** Show system, sync system, narrative engine, transitions

---

## Recommendations for PDF Translation

Based on this audit, the PDF enhancement plan should focus on:

1. **Phase 1 (Quick Wins):**
   - Fix AudioWaveformEffect chroma bug
   - Expose AudioEffectMapping API
   - Expose ModifierStack API
   - Add percussion trigger effects

2. **Phase 2 (Advanced Features):**
   - Leverage musical saliency in effects
   - Leverage style detection for adaptive routing
   - Use 64-bin spectrum for detailed frequency visualization
   - Expand AudioBehaviorSelector usage

3. **Phase 3 (Adaptive Intelligence):**
   - Style-based effect routing
   - Saliency-triggered transitions
   - Temporal context awareness
   - Unified color mapping system

---

## File Inventory

### Core Architecture
- `firmware/v2/src/core/actors/` - Actor system (10 files)
- `firmware/v2/src/core/state/` - CQRS state management (9 files)
- `firmware/v2/src/core/persistence/` - NVS storage (14 files)
- `firmware/v2/src/core/shows/` - Show system (7 files)
- `firmware/v2/src/core/narrative/` - Narrative engine (2 files)

### Audio Pipeline
- `firmware/v2/src/audio/k1/` - K1 front-end (41 files)
- `firmware/v2/src/audio/contracts/` - Audio contracts (10 files)
- `firmware/v2/src/audio/tempo/` - TempoTracker (2 files)
- `firmware/v2/src/audio/` - Core audio components (30+ files)

### Visual Pipeline
- `firmware/v2/src/effects/ieffect/` - 90+ effects (189 files)
- `firmware/v2/src/effects/zones/` - Zone system (5 files)
- `firmware/v2/src/effects/transitions/` - Transitions (4 files)
- `firmware/v2/src/effects/modifiers/` - Modifiers (15 files)
- `firmware/v2/src/effects/enhancement/` - Color correction (8 files)

### Network
- `firmware/v2/src/network/webserver/handlers/` - REST handlers (48 files)
- `firmware/v2/src/network/webserver/ws/` - WebSocket commands (41 files)
- `firmware/v2/src/network/` - Core network (10+ files)

**Total:** 759 files in firmware/v2/src/

---

## Conclusion

The LightwaveOS v2 firmware has a **sophisticated, well-architected foundation** with many advanced features already implemented. The primary opportunity is to **better utilize existing capabilities** rather than building new infrastructure.

The PDF enhancement plan should focus on:
1. **Exposing** existing features via API
2. **Leveraging** underutilized audio features in effects
3. **Fixing** known bugs
4. **Expanding** usage of advanced systems (AudioBehaviorSelector, saliency, style detection)

This audit provides the complete context needed for accurate translation of PDF recommendations into actionable improvements.

---

**Audit Completed:** 2026-01-09  
**Next Steps:** Update PDF translation plan with architecture insights

