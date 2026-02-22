# Implementation Checklist & Next Steps

**Concrete roadmap for incorporating research into firmware and authoring tools**

---

## Phase 1: Foundation (Weeks 1-2)

### Core Audio Analysis

- [ ] Implement 1024-point FFT using ESP-DSP library
- [ ] Verify FFT output against known test signals
  - [ ] Pink noise (flat spectrum)
  - [ ] Sine sweep (20-8000 Hz)
  - [ ] Isolated drums (peaks in onset detection)
  - [ ] Vocals (formants visible in spectrogram)

- [ ] Extract RMS Energy
  - [ ] Compute per-frame RMS from audio buffer
  - [ ] Normalize to 0.0-1.0 range
  - [ ] Test: Peaks should align with loud sections

- [ ] Extract Spectral Centroid
  - [ ] Compute FFT bin-weighted average
  - [ ] Normalize to 0-360° hue range
  - [ ] Test: Should shift toward blue with treble, red with bass

- [ ] Basic Smoothing (EMA)
  - [ ] Implement exponential moving average for both RMS and centroid
  - [ ] Use α = 0.02 (1-second lag at 50 Hz)
  - [ ] Verify: No jitter, smooth transitions

### Initial LED Driver Integration

- [ ] Verify LED hardware functionality
  - [ ] Can control brightness (0-255)
  - [ ] Can control hue (0-360°)
  - [ ] Can control 320 LEDs individually

- [ ] Implement basic HSV → RGB conversion
  - [ ] Test: Can display full colour wheel

- [ ] Create first mapping function
  ```cpp
  brightness = RMS_energy * 255;
  hue = (spectral_centroid / 8000) * 360;
  saturation = 0.5;  // fixed for now
  render_all_leds(hue, saturation, brightness);
  ```

- [ ] End-to-end test with music
  - [ ] Play pop song, observe brightness tracks loudness
  - [ ] Observe hue shifts with EQ changes
  - [ ] Verify response time <100ms

### Deliverable
**MVP working:** RMS → Brightness, Centroid → Hue, basic smoothing

---

## Phase 2: Completion (Weeks 3-4)

### Additional Audio Features

- [ ] Onset Strength (Spectral Flux)
  - [ ] Implement spectral flux detection
  - [ ] Compare current vs. previous FFT frame magnitude
  - [ ] Sum positive changes only
  - [ ] Test with drum-heavy and sparse tracks

- [ ] Sub-Bass Energy (0-60 Hz band)
  - [ ] Extract bins 0-2 of FFT (at 48kHz)
  - [ ] Compute ratio to total power
  - [ ] Test: High in EDM, low in acoustic

- [ ] Stereo Correlation (if L/R input available)
  - [ ] Compute phase correlation between L/R
  - [ ] Map to LED spatial distribution
  - [ ] Test: Mono → narrow, stereo → wide

### Visual Parameter Expansion

- [ ] Implement Motion Speed mapping
  - [ ] Map onset_strength → LED chase speed
  - [ ] Scale with BPM (normalize to 120 BPM = 1.0x)
  - [ ] Range 0.2x - 3.0x

- [ ] Implement Saturation mapping
  - [ ] Start simple: Base saturation = 0.5 + (onset_strength * 0.5)
  - [ ] Range 20-100%

- [ ] Implement Brightness Coupling Rules
  - [ ] Reduce saturation if brightness > 0.8
  - [ ] Darken by sub-bass energy
  - [ ] Enforce brightness floor (10%) and ceiling (100%)

- [ ] Implement Spatial Spread
  - [ ] Use stereo_correlation to modulate LED zone
  - [ ] Mono: concentrate in center
  - [ ] Stereo: spread across strip

### Performance Optimization

- [ ] Profile DSP task timing
  - [ ] Measure: FFT time, feature extraction time, mapping time
  - [ ] Target: Total <4ms per frame (20ms budget = 20% max)
  - [ ] Use ESP-IDF profiling tools

- [ ] Optimize memory usage
  - [ ] Pre-allocate buffers (no malloc in render loop)
  - [ ] Use static arrays for FFT workspace
  - [ ] Verify PSRAM usage <2MB

- [ ] Test audio latency
  - [ ] Measure: Audio capture → FFT → LED update time
  - [ ] Target: <50ms

### Quality Testing

- [ ] Test comprehensive track list
  - [ ] Pop (Blinding Lights - The Weeknd)
  - [ ] EDM (Scary Monsters - Skrillex)
  - [ ] Hip-Hop (Levitating - Kendrick/Drake)
  - [ ] Orchestra (Rite of Spring - Stravinsky)
  - [ ] Ambient (Music for Airports - Brian Eno)
  - [ ] Heavy Bass (Death Grips - Bottomless Pit)
  - [ ] Acoustic (Fast Car - Tracy Chapman)

- [ ] Verify against expected outputs
  - [ ] Each genre should produce characteristic response

### Deliverable
**Feature-complete MVP:** 5/7 core mappings working, 6+ test tracks validated

---

## Phase 3: Polish & Integration (Weeks 5-6)

### Song Structure Detection

- [ ] Implement Basic Beat Tracking
  - [ ] Use onset strength envelope
  - [ ] Detect tempo from peaks
  - [ ] Detect downbeats
  - [ ] Output beat times

- [ ] Implement Segment Detection (Simple)
  - [ ] Detect silence/energy gaps (verse boundaries)
  - [ ] Detect energy peaks (chorus likely here)
  - [ ] Assign phase: intro/verse/chorus/bridge/outro
  - [ ] Confidence level per segment

- [ ] Integration with Real-Time Audio
  - [ ] Continuously update beat/segment estimates
  - [ ] Make available to visual mapper

### Movement Direction Mapping

- [ ] Implement Direction Logic
  ```cpp
  if (phase.is_chorus) {
      direction = OUTWARD;  // Expand, release
  } else if (phase.is_verse) {
      direction = INWARD;   // Gather, build
  } else if (phase.is_bridge) {
      direction = ALTERNATING;  // Contrast
  }
  ```

- [ ] Implement Direction Pattern Modulation
  - [ ] Inward: Suppress outer zones, brighten center
  - [ ] Outward: Amplify outer zones, spread activation
  - [ ] Smooth transitions (4-8 bar ramps)

- [ ] Test with narrative-arc aware playlists
  - [ ] Does direction shift match emotional arc?

### User Preset System

- [ ] Implement Preset Config Structure
  ```cpp
  struct ColorPresetConfig {
      float hue_min_hz;
      float hue_max_hz;
      bool hue_reversed;
      float sat_min, sat_max;
      float bright_min, bright_max;
      float bright_gamma;
      float bass_darken_factor;
  };
  ```

- [ ] Create 3-4 Built-In Presets
  - [ ] "Energy Mirror" (direct mapping)
  - [ ] "Emotional Journey" (narrative-aware)
  - [ ] "Thermal" (frequency-only)
  - [ ] "Synesthetic" (experimental)

- [ ] Web Interface or Serial Command to Select Preset
  - [ ] Store in NVS (Non-Volatile Storage)
  - [ ] Persist across reboots

### Advanced Testing

- [ ] Perceptual Validation
  - [ ] Do users feel "brightness peaks match loud sections"? Target >90% agreement
  - [ ] Do users feel "hue shifts match musical brightness"? Target >85%
  - [ ] Do users perceive "emotional arc reinforcement"? Target >70%

- [ ] Cross-Genre Consistency
  - [ ] Does mapping generalize well across genres?
  - [ ] What breaks in unexpected genres (metal, classical, jazz)?

- [ ] Accessibility Check
  - [ ] Colour-blind users: Can they perceive energy via brightness + motion?
  - [ ] Hearing-impaired: Is visual-only experience compelling?

### Deliverable
**Polished release:** 7/7 core mappings, song structure awareness, preset system, >40/50 validation score

---

## Phase 4: Advanced Features (Ongoing)

### Machine Learning Integration

- [ ] Train/integrate emotion classifier
  - [ ] Use pre-trained model (e.g., from librosa or MusicTagger)
  - [ ] Compute valence/arousal over time
  - [ ] Smooth output (5-10s window)

- [ ] Use emotion trajectory for colour palette selection
  - [ ] Valence high + arousal high → warm, saturated
  - [ ] Valence low + arousal high → cool, saturated
  - [ ] Adjust palette in real-time

### Pre-Computed Feature Integration

- [ ] Build metadata file format
  - [ ] JSON or protobuf with: beats, segments, key, key_changes, peaks, emotion_curve
  - [ ] Store alongside audio files (or in database)

- [ ] Implement "look-ahead" choreography
  - [ ] Read next segment's characteristics
  - [ ] Prepare visual transition 2-4 bars early
  - [ ] Create intentional intensity arcs

- [ ] Build playlist authoring tool
  - [ ] UI to analyze tracks, extract features
  - [ ] Allow manual adjustment of emotion curves
  - [ ] Export curated playlist with choreography metadata

### Installation & Performance Modes

- [ ] "DJ Mode"
  - [ ] Real-time, responsive
  - [ ] No pre-computation, fully reactive

- [ ] "Choreography Mode"
  - [ ] Uses pre-computed features
  - [ ] Intentional emotional arcs
  - [ ] Narrative-driven

- [ ] "Meditative Mode"
  - [ ] Very smooth transitions (5-30s)
  - [ ] Focus on long-term emotional trajectory
  - [ ] Reduced motion intensity

- [ ] "Concert Mode"
  - [ ] High saturation, bright, aggressive
  - [ ] Beat-locked motion
  - [ ] Sync to performer cues (MIDI/OSC)

---

## Current Status Assessment

### What You Have Now
- [x] ESP32-S3 hardware with stereo audio input
- [x] PSRAM (8MB) for DSP buffers
- [x] WS2812 LED control (320 LEDs)
- [x] Firmware architecture (AudioActor, RendererActor)
- [x] Beat tracking & timing infrastructure

### What You Need to Add (Priority Order)

#### MUST-HAVE (Required for MVP)
1. [ ] FFT-based spectral analysis (hours 1-3)
2. [ ] RMS energy extraction (hours 4-5)
3. [ ] Spectral centroid calculation (hours 6-8)
4. [ ] EMA smoothing implementation (hours 9-10)
5. [ ] RMS → brightness + centroid → hue mapping (hours 11-12)
6. [ ] End-to-end integration testing (hours 13-16)

**Est. Time: 16 hours (2 days)**

#### SHOULD-HAVE (For Phase 2)
1. [ ] Onset strength detection (hours 1-3)
2. [ ] Sub-bass power extraction (hours 4-5)
3. [ ] Saturation/dissonance mapping (hours 6-8)
4. [ ] Stereo correlation (if hardware permits) (hours 9-11)
5. [ ] Brightness coupling rules (hours 12-13)
6. [ ] Motion speed mapping (hours 14-15)
7. [ ] Comprehensive testing (hours 16-24)

**Est. Time: 24 hours (3 days)**

#### NICE-TO-HAVE (Phase 3+)
1. [ ] Beat/segment detection (hours 1-8)
2. [ ] Song phase classification (hours 9-12)
3. [ ] Direction mapping (inward/outward) (hours 13-16)
4. [ ] Preset system (hours 17-20)
5. [ ] Web UI for calibration (hours 21-32)
6. [ ] Pre-computed feature loading (hours 33-40)
7. [ ] ML emotion classifier (hours 41-56)

**Est. Time: 56 hours (7 days)**

---

## Dependencies & Tools

### Required Libraries

#### On-Device (Firmware)
- [ ] ESP-DSP (FFT, math functions)
- [ ] ARM CMSIS-DSP (optional, if using standalone)
- [ ] WS2812 LED driver (already have)
- [ ] I2S audio driver (already have)

#### Optional (Development/Offline)
- [ ] Python librosa (audio analysis)
- [ ] Essentia (feature extraction)
- [ ] Spotify Web API client (metadata)

### Development Environment
- [x] PlatformIO (already configured)
- [x] ESP-IDF (v5.0+)
- [x] Compiler: xtensa-esp32s3-elf-gcc (already have)

### Testing Tools
- [ ] Audacity (spectrogram visualization)
- [ ] Python scipy/numpy (signal processing)
- [ ] ffmpeg (batch audio processing)
- [ ] Logic Pro / Ableton (reference visualization)

---

## Success Metrics

### Functional Metrics
| Metric | Target | Phase |
|--------|--------|-------|
| FFT real-time capability | >50 Hz update rate | 1 |
| Feature extraction CPU | <4ms per frame | 2 |
| LED update latency | <50ms from audio | 2 |
| Parameter stability | <5% jitter | 2 |
| Colour range coverage | >180° hue variation | 3 |
| Motion speed range | 0.2x - 3.0x | 2 |
| Genre compatibility | Works on 6+ genres | 3 |

### Perceptual Metrics
| Metric | Target | Phase |
|--------|--------|-------|
| "Brightness matches loudness" | >90% agreement | 1 |
| "Hue matches timbre" | >85% agreement | 2 |
| "Feels in sync with music" | >80% agreement | 3 |
| "Emotional arc reinforced" | >70% agreement | 3 |
| "Natural vs. random" | >80% natural | 3 |

### Performance Metrics
| Metric | Target | Phase |
|--------|--------|-------|
| Memory usage | <2MB PSRAM | 1 |
| CPU headroom | >70% available | 2 |
| Responsiveness lag | <100ms | 2 |
| Stability (no crashes) | 24h uptime | 2 |

---

## Integration Roadmap

### Into AudioActor
```cpp
class AudioActor {
    // Existing
    void process_audio_frame(const float* audio);

    // New: Add DSP features
    struct AudioFeatures {
        float rms_energy;
        float spectral_centroid_hz;
        float onset_strength;
        float sub_bass_power;
        float stereo_width;
        SongPhase current_phase;
    };

    AudioFeatures compute_features(const float* audio_frame);
    const AudioFeatures& get_current_features() const;
};
```

### Into RendererActor
```cpp
class RendererActor {
    // Existing
    void render_frame(const uint32_t* led_data);

    // New: Add visual mapping
    struct VisualParameters {
        float brightness, hue, saturation;
        float motion_speed;
        float spatial_spread;
        enum DirectionMode direction;
    };

    void map_audio_to_visual(
        const AudioFeatures& audio,
        VisualParameters& visual
    );

    void apply_visual_parameters(
        const VisualParameters& visual,
        uint32_t* led_data
    );
};
```

### Message Flow
```
AudioActor
  ├─ Capture I2S audio
  ├─ Compute features (FFT, RMS, onset, etc.)
  ├─ Send AudioFeatures to RendererActor
  └─ (Frequency: 50 Hz = every 20ms)

RendererActor
  ├─ Receive AudioFeatures
  ├─ Map to VisualParameters
  ├─ Apply motion/direction logic
  ├─ Render to LED buffer
  └─ (Frequency: 120 Hz = every 8.3ms)
```

---

## Documentation Structure

### After Implementation
```
firmware/v2/docs/
├── AUDIO_VISUAL_PARAMETER_MAPPING_FIRMWARE.md  [DONE: Pseudocode]
├── AUDIO_VISUAL_TIMING_ARCHITECTURE.md         [DONE: Timing specs]
└── CALIBRATION_GUIDE.md                         [NEW: Per-device tuning]

docs/
├── MUSIC_STRUCTURE_EMOTIONAL_MAPPING_RESEARCH.md [DONE: Full research]
├── AUDIO_VISUAL_QUICK_REFERENCE.md               [DONE: TL;DR]
├── RESEARCH_SOURCES_AND_KEY_FINDINGS.md          [DONE: Bibliography]
└── IMPLEMENTATION_CHECKLIST_AND_NEXT_STEPS.md    [YOU ARE HERE]
```

---

## Final Notes

### Key Principle
This research foundation exists to make **musical emotion visible**. Every mapping should serve this goal:
- Does it reinforce emotional arc? ✓
- Does it make narrative clear? ✓
- Does it feel natural (not random)? ✓
- Does it work across genres? ✓

### Testing Mindset
Don't optimize prematurely. Start with simple, direct mappings:
1. RMS → brightness (most intuitive)
2. Centroid → hue (well-researched)
3. Onset → speed (perceptually clear)

Add complexity (saturation, direction, presets) once basics work perfectly.

### User-Centric Approach
Build **configurability** from day one:
- Allow users to adjust smoothing constants
- Allow colour palette selection
- Allow directional mapping inversion
- Listen to feedback and iterate

### Long-Term Vision
Your LED choreography system should become a **storytelling platform** where music creates not just audio experience, but emotional visual journey.

The research shows it's possible. The implementation roadmap above makes it achievable.

---

## Contact & Questions

For clarification on any research findings, refer to:
- **Music theory questions:** See MUSIC_STRUCTURE_EMOTIONAL_MAPPING_RESEARCH.md Part 1-2
- **Audio features:** See AUDIO_VISUAL_PARAMETER_MAPPING_FIRMWARE.md Part 1-2
- **Colour psychology:** See MUSIC_STRUCTURE_EMOTIONAL_MAPPING_RESEARCH.md Part 4
- **Implementation details:** See AUDIO_VISUAL_QUICK_REFERENCE.md Part 1-2 & 10-17
- **All sources:** See RESEARCH_SOURCES_AND_KEY_FINDINGS.md

