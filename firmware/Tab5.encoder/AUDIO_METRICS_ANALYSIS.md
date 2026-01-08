# Audio Metrics Analysis for Tab5 Footer

## Current Implementation
The Tab5 footer displays:
1. **BPM** - Beat per minute (from `TempoOutput.bpm`)
2. **KEY** - Musical key/chord (from `ChordState.rootNote` + `ChordState.type`)
3. **MIC** - Microphone input level in dB (from `AudioDspState.rmsPreGain` calculation)
4. **WS** - WebSocket connection status
5. **BAT** - Battery level and charging status
6. **UPTIME** - Host device uptime in seconds

## Mic Level Naming Options
The following field names are supported for mic level extraction:
- `mic` (recommended - short and clear)
- `micLevel` (more descriptive)
- `micDb` (explicitly indicates dB units)
- `inputLevel` (more generic)
- `audioInput` (very descriptive)

**Current Implementation**: Uses `mic` as primary, with fallbacks to other names.

## Other Important Audio Metrics Available (Not Currently Displayed)

### From `TempoOutput`:
- **`phase01`** - Beat phase [0, 1) - useful for beat-synchronized effects
- **`beat_tick`** - True on beat instant - useful for beat detection
- **`beat_strength`** - Smoothed magnitude of winning bin - indicates beat confidence
- **`tempoConfidence`** - How dominant the detected tempo is [0, 1]

### From `AudioDspState`:
- **`rmsRaw`** - Raw RMS before mapping (0-1)
- **`rmsMapped`** - Mapped RMS energy (0-1) - already used as "energy"
- **`flux`** - Spectral flux (novelty detection) - indicates transients/onsets
- **`agcGain`** - Current AGC gain multiplier - useful for audio level troubleshooting
- **`noiseFloor`** - Current noise floor estimate - useful for calibration
- **`clipCount`** - Number of clipped samples - indicates audio clipping

### From `ControlBusFrame`:
- **`bands[8]`** - 8-band frequency spectrum (0-1 normalized) - useful for frequency analysis
- **`chroma[12]`** - 12-bin chromagram (pitch classes) - useful for harmonic analysis
- **`fast_rms`** - Fast-attack RMS - useful for transient detection
- **`fast_flux`** - Fast-attack flux - useful for onset detection
- **`silentScale`** - Silence detection scale (0-1) - indicates if audio is silent
- **`isSilent`** - True if audio is silent - useful for auto-pause features

### From `ChordState`:
- **`keyRoot`** - Root note (0-11: C=0, C#=1, ..., B=11)
- **`keyType`** - Chord type (0=NONE, 1=MAJOR, 2=MINOR, 3=DIMINISHED, 4=AUGMENTED)
- **`keyConfidence`** - Chord detection confidence (0-1)

### From `MusicalSaliencyFrame`:
- **`harmonic`** - Harmonic novelty (chord changes)
- **`rhythmic`** - Rhythmic novelty (beat strength)
- **`timbral`** - Timbral novelty (spectral changes)
- **`dynamic`** - Dynamic novelty (amplitude changes)

### From `MusicStyle`:
- **`currentStyle`** - Detected music style (UNKNOWN, RHYTHMIC_DRIVEN, HARMONIC_DRIVEN, etc.)
- **`styleConfidence`** - Style detection confidence (0-1)

## Recommendations for Future Additions

### High Priority (User-Valuable):
1. **Flux** - Spectral flux indicates transients/onsets, useful for understanding audio dynamics
2. **Tempo Confidence** - How reliable the BPM detection is
3. **AGC Gain** - Useful for troubleshooting audio level issues
4. **Clip Count** - Indicates if audio is clipping (critical for audio quality)

### Medium Priority:
5. **Beat Strength** - Smoothed beat magnitude (complements BPM)
6. **Key Confidence** - How reliable the key detection is
7. **Silent Scale** - Indicates if audio is silent (useful for auto-pause)

### Low Priority (Technical/Diagnostic):
8. **Noise Floor** - Useful for calibration but not user-facing
9. **RMS Raw** - Technical metric, less user-friendly than mapped RMS
10. **Style Detection** - Interesting but not critical for most users

## Implementation Notes

### V2 Firmware Changes Required
The v2 firmware's `WebServer::broadcastStatus()` method needs to be updated to include audio metrics. See `V2_AUDIO_METRICS_ADDITION.md` for implementation details.

### Tab5.encoder Changes
- ✅ Replaced ENERGY with MIC level
- ✅ Added support for multiple mic level field names
- ✅ Updated footer to display mic level in dB format
- ✅ Improved debug logging to show actual message contents

### Field Name Consistency
The v2 firmware should use consistent field names:
- `bpm` - BPM value
- `key` - Musical key string (e.g., "C", "Am", "Dm")
- `mic` - Mic level in dB (recommended name)
- `uptime` - Host uptime in seconds (already present)

