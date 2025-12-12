# Normalization Fix Summary

## Problem
The pluggable pipeline was showing energy values in the thousands (e.g., 2847.9) instead of the expected 0-1 range that the legacy system produced.

## Root Cause
The Zone Mapper node was receiving raw Goertzel magnitudes (0-10000+ range) from the AGC node but wasn't normalizing them. The legacy system explicitly normalized values in the `AudioFeatures::updateZoneEnergies()` function.

## Solution Implemented

### 1. Modified Zone Mapper Node (`zone_mapper_node.h`)
Added normalization logic similar to the legacy system:

```cpp
// Calculate average energy per zone
float raw_energies[MAX_ZONES];
float max_energy = 0.0f;

// Find maximum energy across all zones
for (size_t i = 0; i < num_zones; i++) {
    if (zone_counts[i] > 0) {
        raw_energies[i] = zone_accumulator[i] / zone_counts[i];
    } else {
        raw_energies[i] = 0.0f;
    }
    
    // Track maximum for normalization
    if (raw_energies[i] > max_energy) {
        max_energy = raw_energies[i];
    }
}

// Normalize to 0-1 range (similar to legacy system)
float normalization_factor = (max_energy > 0.01f) ? (0.95f / max_energy) : 1.0f;
```

### 2. Fixed `updateAudioState()` in main.cpp
Removed the duplicate normalization logic that was trying to normalize already-normalized values:

```cpp
// Zone mapper already outputs normalized values (0-1) with gamma applied
audio_state.core.zone_energies[i] = sum / (end - start);
```

### 3. Added Debug Output
Enhanced debug output in Zone Mapper to show:
- Maximum raw energy value
- Normalization factor applied
- Average output after normalization

## Expected Results
- Energy values should now be in the 0-1 range
- Zone energies should be properly normalized
- Beat detection should still work on raw data (dual-path architecture preserved)

## Testing
The fix has been compiled and uploaded to the device on port /dev/cu.usbmodem1401. 
Debug output has been enabled to verify the normalization is working correctly.

## Architecture Notes
The dual-path architecture is preserved:
- **RAW Path**: Goertzel → Beat Detection (preserves dynamics)
- **AGC Path**: Goertzel → AGC → Zone Mapper (with normalization) → Visualization