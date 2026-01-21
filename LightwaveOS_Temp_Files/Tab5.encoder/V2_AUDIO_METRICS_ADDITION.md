# V2 Firmware: Audio Metrics Addition Required

## Problem
The v2 firmware's `broadcastStatus()` method in `WebServer.cpp` (line 819-850) does NOT include audio metrics. The Tab5.encoder is expecting BPM, KEY, ENERGY, and UPTIME in the status message, but they're not being sent.

## Current Status Message Structure
The status message currently includes:
- Effect parameters (effectId, effectName, brightness, speed, paletteId, etc.)
- System stats (fps, cpuPercent, freeHeap, uptime)

## Required Audio Metrics to Add

### 1. Tempo/BPM Data (from `AudioNode::getTempo()`)
```cpp
TempoOutput tempo = audio->getTempo();
doc["bpm"] = tempo.bpm;                    // Current detected tempo
doc["tempoConfidence"] = tempo.confidence; // Confidence [0, 1]
doc["tempoLocked"] = tempo.locked;         // True if confidence > threshold
doc["beatStrength"] = tempo.beat_strength; // Smoothed magnitude
```

### 2. Audio Energy/RMS (from `AudioNode::getDspState()`)
```cpp
AudioDspState dsp = audio->getDspState();
doc["rms"] = dsp.rmsMapped;                // RMS energy (0-1)
doc["rmsRaw"] = dsp.rmsRaw;                // Raw RMS
doc["energy"] = dsp.rmsMapped;             // Alias for rmsMapped (for compatibility)
doc["flux"] = dsp.fluxMapped;              // Spectral flux (novelty)
```

### 3. Mic Level (calculated from `rmsPreGain`)
```cpp
float micLevelDb = (dsp.rmsPreGain > 0.0001f) 
    ? (20.0f * log10f(dsp.rmsPreGain)) 
    : -80.0f;
doc["micLevel"] = micLevelDb;              // Mic level in dB
// OR
doc["mic"] = micLevelDb;                   // Shorter name
```

### 4. Musical Key/Chord (from `ControlBusFrame::chordState`)
```cpp
const ControlBusFrame& frame = audio->getControlBus().GetFrame();
const ChordState& chord = frame.chordState;
doc["key"] = getKeyName(chord.rootNote, chord.type); // "C", "Am", "Dm", etc.
doc["keyRoot"] = chord.rootNote;            // 0-11 (C=0, C#=1, ..., B=11)
doc["keyType"] = static_cast<uint8_t>(chord.type); // 0=NONE, 1=MAJOR, 2=MINOR, etc.
doc["keyConfidence"] = chord.confidence;     // 0-1
```

### 5. Host Uptime (already included, but verify)
```cpp
doc["uptime"] = millis() / 1000;           // Already present ✓
```

## Implementation Location
**File**: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware/v2/src/network/WebServer.cpp`
**Method**: `WebServer::broadcastStatus()` (line ~819)

## Code Addition Required
```cpp
void WebServer::broadcastStatus() {
    // ... existing code ...
    
    StaticJsonDocument<512> doc;  // May need to increase size to 1024
    doc["type"] = "status";
    
    // ... existing effect/system fields ...
    
    // ADD THIS SECTION:
    #if FEATURE_AUDIO_SYNC
    auto* audio = m_orchestrator.getAudio();
    if (audio) {
        // Tempo/BPM
        audio::TempoOutput tempo = audio->getTempo();
        doc["bpm"] = tempo.bpm;
        doc["tempoConfidence"] = tempo.confidence;
        doc["tempoLocked"] = tempo.locked;
        
        // Audio Energy
        audio::AudioDspState dsp = audio->getDspState();
        doc["rms"] = dsp.rmsMapped;
        doc["energy"] = dsp.rmsMapped;  // Alias for compatibility
        doc["flux"] = dsp.fluxMapped;
        
        // Mic Level (calculated from pre-gain RMS)
        float micLevelDb = (dsp.rmsPreGain > 0.0001f) 
            ? (20.0f * log10f(dsp.rmsPreGain)) 
            : -80.0f;
        doc["mic"] = micLevelDb;  // or "micLevel" for longer name
        
        // Musical Key/Chord
        const audio::ControlBusFrame& frame = audio->getControlBus().GetFrame();
        const audio::ChordState& chord = frame.chordState;
        if (chord.confidence > 0.1f) {
            // Convert rootNote (0-11) + type to key name
            const char* keyNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            String keyName = keyNames[chord.rootNote];
            if (chord.type == audio::ChordType::MINOR) {
                keyName += "m";
            } else if (chord.type == audio::ChordType::DIMINISHED) {
                keyName += "dim";
            } else if (chord.type == audio::ChordType::AUGMENTED) {
                keyName += "aug";
            }
            doc["key"] = keyName;
        } else {
            doc["key"] = "";
        }
        doc["keyConfidence"] = chord.confidence;
    }
    #endif
    
    // ... rest of existing code ...
}
```

## Other Important Audio Metrics Available

### From `AudioDspState`:
- `rmsRaw` - Raw RMS (before mapping)
- `rmsPreGain` - RMS before AGC gain (for mic level calculation)
- `agcGain` - Current AGC gain multiplier
- `noiseFloor` - Current noise floor estimate
- `dcEstimate` - DC offset estimate
- `clipCount` - Number of clipped samples

### From `ControlBusFrame`:
- `bands[8]` - 8-band frequency spectrum (0-1 normalized)
- `chroma[12]` - 12-bin chromagram (pitch classes)
- `fast_rms` - Fast-attack RMS
- `fast_flux` - Fast-attack flux
- `silentScale` - Silence detection scale (0-1)
- `isSilent` - True if audio is silent

### From `TempoOutput`:
- `phase01` - Beat phase [0, 1)
- `beat_tick` - True on beat instant

## Mic Level Naming Options

1. **`mic`** - Short, concise (recommended)
2. **`micLevel`** - More descriptive
3. **`micDb`** - Explicitly indicates dB units
4. **`inputLevel`** - More generic
5. **`audioInput`** - Very descriptive

**Recommendation**: Use `mic` for brevity, as it's clear in context and matches the user's example "mic=-46.4dB"

