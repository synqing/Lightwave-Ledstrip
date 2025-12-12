# SIGNAL FLOW ANALYSIS - FIRST PRINCIPLES

## 1. SOURCE: SPH0645 Microphone
- **Output**: 18-bit signed data in I2S format
- **Range**: -131,072 to +131,071
- **DC Offset**: Calibrated to ~0 for maximum dynamic range
- **Why we care about 0**: Maximum headroom in both directions = richest data

## 2. I2S CAPTURE
- **Input**: 32-bit I2S frames (18-bit data padded)
- **Conversion**: `sample32 >> 14` to extract 18-bit value
- **Scaling**: `(sample32 * 32767) >> 17` to get int16_t
- **Output Range**: -32,768 to +32,767
- **Purpose**: Standard 16-bit audio format

## 3. DC BLOCKING FILTER
- **Input**: int16_t raw samples
- **Process**: High-pass filter (y[n] = x[n] - x[n-1] + 0.995 * y[n-1])
- **Output**: int16_t samples with DC removed
- **Purpose**: Remove DC offset while preserving dynamics

## 4. PREPROCESSING (CURRENT)
- **Noise Gate**: Zero out samples below threshold
- **Pre-emphasis**: Boost high frequencies
- ~~**Normalization**: REMOVED - was destroying dynamic range~~

## 5. GOERTZEL TRANSFORM
- **Input**: int16_t samples (NEEDS FULL DYNAMIC RANGE)
- **Process**: DFT for specific frequencies
- **Output**: float magnitudes[96]
- **Key Point**: Output magnitude ∝ input amplitude!
- **Range**: 0.0 to ~65536.0 (depends on input amplitude)

## 6. BEAT DETECTION
- **Input**: Goertzel magnitudes (NEEDS FULL DYNAMIC RANGE)
- **Process**: Detect energy changes over time
- **Requirements**:
  - Must see transients (sudden energy increases)
  - Must see relative changes between quiet and loud
  - AGC/normalization DESTROYS this information!

## 7. AUDIO FEATURES
- **Input**: Goertzel magnitudes
- **Outputs**:
  - Zone energies (8 zones)
  - Global energy
  - Spectral features
  - Beat metrics

## 8. VISUALIZATION (Future)
- **Input**: Audio features
- **This is where normalization COULD happen**
- **But only for visual scaling, not analysis!**

## THE PROBLEM

Someone put AGC normalization at step 4, which:
1. Flattens all dynamics to a target RMS
2. Makes quiet sounds as loud as loud sounds
3. Destroys the amplitude information Goertzel needs
4. Eliminates the transients beat detection needs

## THE SOLUTION

1. Remove ALL normalization before analysis
2. Let each module see the full dynamic range
3. Only apply scaling for visualization AFTER analysis
4. Each module documents its input requirements

## SCALING REQUIREMENTS BY MODULE

### Goertzel
- **Input**: Full range int16_t (-32k to +32k)
- **Why**: Output magnitude proportional to input
- **Scaling**: NONE - needs raw data

### Beat Detector
- **Input**: Raw Goertzel magnitudes
- **Why**: Detects relative energy changes
- **Scaling**: NONE - needs to see dynamics

### Visualization (if needed)
- **Input**: Processed features
- **Scaling**: Can normalize here for display
- **Example**: Map 0-65536 to 0-1 for LED brightness

## CONCLUSION

The "magic 40" or whatever scaling factor was arbitrary because:
1. No one documented what each module needs
2. AGC was applied too early in the chain
3. Dynamic range was destroyed before analysis

First principles: Preserve signal integrity through analysis, only scale for output.