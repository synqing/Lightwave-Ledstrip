# VoG Implementation Plan: From Bug Fix to Divine Architecture
**Date**: January 14, 2025  
**Objective**: Fix current issues while implementing enhanced VoG architecture

## Current State Summary

### Working
- Beat detection algorithm detecting beats correctly
- Goertzel transform producing correct scaled values
- Zone mapping and normalization functioning

### Broken
1. **VoG shows 0.00**: Buffer pointers invalid by read time
2. **BPM stuck at 141**: No timeout mechanism in beat detector
3. **No spectrum snapshots**: VoG reading overwritten buffers

## Implementation Phases

### Phase 1: Critical Bug Fixes (Immediate)

#### 1.1 Fix Buffer Management
**Problem**: VoG reads pipeline buffers that are being reused/overwritten

**Solution**: Create snapshot buffers
```cpp
// In main.cpp, add:
float raw_spectrum_snapshot[96];
float agc_spectrum_snapshot[96];

// After Goertzel processes:
memcpy(raw_spectrum_snapshot, goertzel_output->data, 96 * sizeof(float));

// After AGC processes:
memcpy(agc_spectrum_snapshot, agc_output->data, 96 * sizeof(float));

// Update VoG to use snapshots
vog_node->setSpectrumSnapshots(raw_spectrum_snapshot, agc_spectrum_snapshot);
```

#### 1.2 Fix Beat Detection Issues
**Problem**: Still detecting too many beats ("crocodile lock")

**Solution 1**: Increase thresholds based on testing
```cpp
// In legacy_beat_detector.h:
static constexpr float ENERGY_THRESHOLD_MIN = 12000.0f;  // was 8000

// In beat_detector_node.h:
bool is_transient = energy_transient > (last_energy * 0.75f); // was 0.5
```

**Solution 2**: Add temporal gating
```cpp
// In legacy_beat_detector.cpp process():
float min_interval_ms = 60000.0f / (current_bpm * 1.5f);
if (now - last_beat_time < min_interval_ms) {
    return; // Too soon for another beat
}
```

#### 1.3 Fix VoG Algorithm (Critical)
**Problem**: VoG expects raw > AGC, but AGC is always higher

**Solution**: Measure rate of change, not absolute ratio
```cpp
// In VoG calculateVoG():
// Add history tracking
float ratio_history[3] = {0};
float velocity_history[2] = {0};

// Calculate dynamics
float current_ratio = raw_energy / (agc_energy + 0.001f);
float velocity = current_ratio - ratio_history[0];
float acceleration = velocity - velocity_history[0];

// Update history
ratio_history[2] = ratio_history[1];
ratio_history[1] = ratio_history[0];
ratio_history[0] = current_ratio;
velocity_history[1] = velocity_history[0];
velocity_history[0] = velocity;

// Divine moments = positive acceleration
if (acceleration > 0.05f) {  // Threshold TBD
    raw_confidence = tanhf(acceleration * 10.0f);
} else {
    raw_confidence = 0.0f;
}
```

### Phase 2: Optimize VoG for 3-Band (Week 1)

#### 2.1 Calculate Band Energies
Instead of 96 bin summation, use 3 pre-calculated bands:

```cpp
// In VoG calculateVoG():
// Assume zones already calculated: bass(0-1), mid(2-5), high(6-7)
float raw_bands[3] = {
    raw_bass_energy,    // From raw snapshot
    raw_mid_energy,     // From raw snapshot  
    raw_high_energy     // From raw snapshot
};

float agc_bands[3] = {
    agc_bass_energy,    // From AGC snapshot
    agc_mid_energy,     // From AGC snapshot
    agc_high_energy     // From AGC snapshot
};
```

#### 2.2 Implement Three Divine Metrics

```cpp
struct DivineMetrics {
    float dynamic_emphasis[3];   // Per band
    float timbral_clarity[3];    // Per band
    float spectral_impact[3];    // Per band
    
    void calculate(const float raw[3], const float agc[3], const float prev_raw[3]) {
        for (int i = 0; i < 3; i++) {
            // Dynamic Emphasis: How much does this band "punch through"
            dynamic_emphasis[i] = raw[i] - (agc[i] * 1.5f);
            if (dynamic_emphasis[i] < 0) dynamic_emphasis[i] = 0;
            
            // Timbral Clarity: How dominant is this frequency
            float epsilon = 0.001f;
            timbral_clarity[i] = agc[i] / (raw[i] + epsilon);
            
            // Spectral Impact: Rate of change
            spectral_impact[i] = abs(raw[i] - prev_raw[i]);
        }
    }
};
```

### Phase 3: Integer LUT Optimization (Week 2)

#### 3.1 Build Power LUT
```cpp
// At compile time or init:
uint16_t power3_lut[1024];  // For 10-bit precision
void buildPowerLUT() {
    for (int i = 0; i < 1024; i++) {
        float normalized = i / 1024.0f;  // 0-1 range
        float cubed = normalized * normalized * normalized;
        power3_lut[i] = (uint16_t)(cubed * 65535.0f);
    }
}
```

#### 3.2 Fast Integer VoG
```cpp
uint32_t calculateFastVoG(uint16_t raw, uint16_t agc) {
    // Scale to 10-bit for LUT
    uint16_t diff = (raw > agc) ? ((raw - agc) >> 6) : 0;
    if (diff >= 1024) diff = 1023;
    
    // Lookup cubed value
    return power3_lut[diff];
}
```

### Phase 4: Prophet Algorithm (Week 3)

#### 4.1 Lightweight Impact Detector
```cpp
class SpectralProphet {
    float prev_energy = 0;
    float impact_threshold = 100.0f;
    
public:
    bool shouldCalculateVoG(float current_energy) {
        float impact = abs(current_energy - prev_energy);
        prev_energy = current_energy;
        
        if (impact > impact_threshold) {
            return true;  // Divine moment!
        }
        return false;
    }
};
```

#### 4.2 Event-Driven VoG
```cpp
void VoGNode::process() {
    // Always run prophet (cheap)
    float total_raw = raw_bands[0] + raw_bands[1] + raw_bands[2];
    
    if (prophet.shouldCalculateVoG(total_raw)) {
        // Divine moment - run full calculation
        calculateFullVoG();
        frames_since_divine = 0;
    } else {
        // Decay existing VoG
        vog_confidence *= 0.98f;
        frames_since_divine++;
        
        // Force recalc every 500ms anyway
        if (frames_since_divine > 60) {
            calculateFullVoG();
            frames_since_divine = 0;
        }
    }
}
```

## Testing Protocol

### Phase 1 Tests (Immediate)
1. Verify VoG shows > 0.00 during music
2. Verify BPM resets to 120/0 during silence
3. No performance degradation

### Phase 2 Tests (3-Band)
1. Compare 3-band VoG vs full 96-bin calculation
2. Verify similar confidence patterns
3. Measure CPU reduction

### Phase 3 Tests (Integer)
1. Verify integer VoG matches float within 5%
2. Measure CPU improvement (target: 10x faster)
3. No overflow conditions

### Phase 4 Tests (Prophet)
1. Verify prophet triggers on transients
2. VoG still responsive to music changes
3. CPU usage reduced by 80%+ in steady-state

## Risk Mitigation

### DO NOT TOUCH
- Goertzel /8 scaling
- Core beat detection algorithm  
- Energy threshold values
- Pipeline processing order

### Safe to Modify
- Buffer management (snapshots)
- Timeout logic (new feature)
- VoG calculation method
- Performance optimizations

## Success Metrics

1. **Immediate**: VoG > 0.00, BPM timeouts work
2. **Week 1**: 3-band VoG matches original quality
3. **Week 2**: 10x performance improvement
4. **Week 3**: 80% CPU reduction in steady-state

## Conclusion

This plan fixes the immediate bugs while setting up a path to the enhanced VoG architecture. Each phase is independently valuable and can be deployed incrementally.