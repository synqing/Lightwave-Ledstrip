# AP_SOT Audio Processing Architecture Analysis

## Executive Summary

The AP_SOT codebase contains two audio processing implementations:
1. **Legacy System** (`main_legacy.cpp.bak`) - Working correctly with values 0-1
2. **Pluggable System** (`main.cpp`) - Shows values in thousands, has normalization issues

The core issue is a fundamental difference in how the two systems handle data normalization and the dual-path architecture for beat detection vs visualization.

## Data Flow Analysis

### Legacy System (Working Correctly)

```
I2S Input (int16_t: -32768 to 32767)
    ↓
Audio Processor (DC offset removal)
    ↓
Goertzel Engine (raw magnitudes: 0-10000+ range)
    ↓
Audio Features Module
    ├─→ RAW PATH: Beat Detection (uses raw Goertzel magnitudes)
    └─→ AGC PATH: Multiband AGC → Zone Mapping → Visualization (0-1 normalized)
```

**Key Points:**
- Goertzel outputs raw magnitudes in the range of 0-10000+
- AudioFeatures maintains DUAL data paths:
  - RAW frequency bins for beat detection (preserves dynamics)
  - AGC-processed bins for visualization (normalized 0-1)
- Zone energies are calculated from AGC-processed data
- Final normalization ensures zones are 0-1 range

### Pluggable System (Normalization Issues)

```
I2S Input Node
    ↓
DC Offset Node
    ↓
Goertzel Node (raw magnitudes: 0-10000+ range)
    ├─→ Beat Pipeline: Beat Detector (uses raw data)
    └─→ Main Pipeline: Multiband AGC → Zone Mapper → audio_state
```

**Key Issues Identified:**

1. **Missing Normalization**: The Zone Mapper expects input data to be already normalized (0-1), but it's receiving raw Goertzel magnitudes (0-10000+)

2. **Incorrect Data Type Conversion**: In GoertzelNode line 37:
   ```cpp
   samples[i] = (int16_t)input.data[i];
   ```
   This assumes input is float samples, but I2S provides int16_t directly

3. **Compilation Error**: Undefined variables `max_zone` and `zone_norm` in main.cpp line 160

## Core Differences

### 1. Data Normalization Location

**Legacy System:**
- Multiband AGC in `audio_features.cpp` handles normalization
- Zone calculation includes explicit normalization step (lines 213-220)
- Uses `max_zone` and `zone_norm` variables for proper scaling

**Pluggable System:**
- Zone Mapper assumes input is already normalized
- No explicit normalization of Goertzel output before Zone Mapper
- Missing the normalization logic from legacy system

### 2. Dual-Path Architecture

Both systems implement dual-path processing, but differently:

**Legacy System:**
- Explicitly maintains two arrays: `raw_frequency_bins` and `agc_processed_bins`
- Clear separation in `audio_features.cpp`

**Pluggable System:**
- Uses pipeline branching with metadata flags
- `is_raw_spectrum` and `is_agc_processed` metadata

### 3. Expected Value Ranges

| Stage | Legacy System | Pluggable System |
|-------|--------------|------------------|
| I2S Input | -32768 to 32767 | -32768 to 32767 |
| After DC Offset | ~same range | ~same range |
| Goertzel Output | 0-10000+ | 0-10000+ |
| After AGC | 0-100 (approx) | 0-100 (approx) |
| Zone Energies | 0-1 (normalized) | 0-10000+ (NOT normalized) |

## Root Cause

The pluggable system is missing the critical normalization step that the legacy system performs in `audio_features.cpp` lines 205-220. The Zone Mapper receives raw Goertzel magnitudes but doesn't normalize them, resulting in values in the thousands instead of 0-1.

## Recommendations

1. **Add normalization to Zone Mapper** or create a separate normalization node
2. **Fix the compilation error** by removing or properly defining the debug variables
3. **Fix the data type issue** in GoertzelNode
4. **Add explicit documentation** about expected input/output ranges for each node
5. **Consider adding range validation** in each node to catch these issues early