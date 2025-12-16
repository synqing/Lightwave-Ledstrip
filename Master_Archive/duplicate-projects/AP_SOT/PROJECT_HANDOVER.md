# AP_SOT Project Handover Document
## Critical Context for Next Agent - January 13, 2025

### IMMEDIATE ISSUE: Zone Energies Stuck at 1.0
The audio pipeline has a CRITICAL BUG where all zone energies are stuck at 1.0, resulting in:
- Bass = 2.0 (zones 0+1)
- Mid = 4.0 (zones 2+3+4+5)  
- High = 2.0 (zones 6+7)
- Beat detection = 0.0 BPM (always)

**The Goertzel frequency analysis IS WORKING** (producing magnitudes ~2000-4000 per bin), but the zone calculation is broken.

### Key Discoveries This Session

1. **DC Offset Strategy**
   - The legacy system used **RAW MODE** (no DC offset) - "10x better than alternatives"
   - I just changed audio_processing.cpp to use RAW mode (no offset)
   - The calibrator was never completing, always showing offset=360.0
   - SPH0645 mic might already output centered data

2. **Audio Pipeline Architecture**
   - AudioPipeline.cpp is DEAD CODE - not used
   - Real pipeline: main.cpp → AudioProcessor → SIMDGoertzel → AudioFeatures → audio_state
   - Many stub files (i2s_handler.cpp, signal_processor.cpp, etc) are just "TODO: Implement"

3. **Working Components**
   - I2S audio capture: ✓ Working (16kHz, 128 samples)
   - DC blocking filter: ✓ Working
   - Goertzel analysis: ✓ Working (96 bins, huge magnitudes)
   - Global energy calculation: ✓ Working
   - Zone energy calculation: ✗ BROKEN (all zones = 1.0)

### Technical Details

**Hardware:**
- ESP32-S3 with SPH0645 I2S microphone
- I2S pins: BCLK=16, LRCLK=4, DIN=10
- LEFT channel (not RIGHT)
- 18-bit data in 32-bit frames

**Signal Flow:**
1. I2S capture → 32-bit frames
2. Shift right 14 bits → 18-bit samples
3. RAW mode (no DC offset)
4. DC blocking filter
5. Noise gate + pre-emphasis
6. Goertzel → 96 frequency bins
7. Zone calculation → BROKEN HERE

### Debug Output Added
- Line 86-94 in audio_features.cpp: Zone0 calculation debug
- Line 90-94 in main.cpp: Zone values debug
- Shows zones are hardcoded to 1.0 somehow

### User Context
- User is frustrated with agents making assumptions
- Wants FIRST PRINCIPLES approach
- Previous agents corrupted the codebase
- Working version exists in LEGACY_CLEANUP/SpectraSynq_Clean_Export/
- User's upload port: /dev/tty.usbmodem1401 (NEVER use 2101)

### Next Steps Required
1. Find why zones are stuck at 1.0
2. Check if audio_state is properly initialized
3. Verify zone calculation math
4. Compare with legacy implementation
5. Test with music to verify beat detection

### Critical Files
- `/src/main.cpp` - Main loop
- `/src/audio/audio_processing.cpp` - I2S and DC handling (just changed to RAW mode)
- `/src/audio/audio_features.cpp` - Zone calculation (has debug at line 90)
- `/include/audio/audio_state.h` - State structure definition

### Current Todos
1. ✓ Verify DC offset calibration
2. Pending: Monitor DC calibration stability
3. In Progress: Fix zone energies stuck at 1.0
4. In Progress: Compare beat detection before/after
5. Pending: Investigate measured offset difference
6. ✓ Upload main.cpp
7. Pending: Clean up stub files
8. Pending: Fix DC calibrator not completing

The system is SO CLOSE to working - just need to fix the zone calculation bug!