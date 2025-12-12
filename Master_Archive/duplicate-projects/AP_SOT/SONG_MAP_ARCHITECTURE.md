# Song Map Architecture - Learning Songs for Perfect Playback
**Date**: January 14, 2025  
**Status**: Game-Changing Concept  
**Purpose**: Create a complete JSON fingerprint of songs for perfect visualization

## The Vision

Run a "learning pass" over a song using ALL AP_SOT nodes to create a comprehensive map. On subsequent plays, use this map for PERFECT beat detection, structure understanding, and visual synchronization.

## Song Map JSON Structure

```json
{
  "song_id": "SHA256_of_audio_fingerprint",
  "duration_ms": 185000,
  "sample_rate": 44100,
  "analyzed_at": "2025-01-14T22:00:00Z",
  "confidence": 0.98,
  
  "tempo": {
    "primary_bpm": 128.5,
    "time_signature": "4/4",
    "tempo_changes": [
      {"time_ms": 0, "bpm": 0},      // Intro
      {"time_ms": 15000, "bpm": 128}, // Song starts
      {"time_ms": 120000, "bpm": 64}, // Half-time section
      {"time_ms": 150000, "bpm": 128} // Back to normal
    ]
  },
  
  "structure": {
    "sections": [
      {
        "type": "intro",
        "start_ms": 0,
        "end_ms": 15000,
        "energy_profile": "rising",
        "avg_energy": 0.2,
        "peak_energy": 0.4
      },
      {
        "type": "verse",
        "start_ms": 15000,
        "end_ms": 45000,
        "energy_profile": "steady",
        "avg_energy": 0.5,
        "peak_energy": 0.6,
        "key_signature": "Am"
      },
      {
        "type": "chorus",
        "start_ms": 45000,
        "end_ms": 75000,
        "energy_profile": "high",
        "avg_energy": 0.8,
        "peak_energy": 0.95,
        "key_signature": "C"
      },
      {
        "type": "breakdown",
        "start_ms": 75000,
        "end_ms": 90000,
        "energy_profile": "minimal",
        "avg_energy": 0.3,
        "peak_energy": 0.4
      },
      {
        "type": "buildup",
        "start_ms": 90000,
        "end_ms": 105000,
        "energy_profile": "exponential",
        "avg_energy": 0.6,
        "peak_energy": 0.9
      },
      {
        "type": "drop",
        "start_ms": 105000,
        "end_ms": 106000,
        "energy_profile": "explosive",
        "avg_energy": 1.0,
        "peak_energy": 1.0
      }
    ]
  },
  
  "beats": {
    "count": 395,
    "confidence_threshold": 0.8,
    "events": [
      {
        "time_ms": 15234,
        "confidence": 0.95,
        "energy": 15420.5,
        "type": "kick",
        "strength": 0.9
      },
      {
        "time_ms": 15703,
        "confidence": 0.88,
        "energy": 8234.2,
        "type": "snare",
        "strength": 0.6
      }
      // ... all beat events
    ]
  },
  
  "energy_bands": {
    "sample_interval_ms": 100,
    "data": [
      {
        "time_ms": 0,
        "bass": 0.1,
        "mid": 0.15,
        "high": 0.05,
        "total": 0.12
      },
      {
        "time_ms": 100,
        "bass": 0.12,
        "mid": 0.16,
        "high": 0.06,
        "total": 0.13
      }
      // ... every 100ms
    ]
  },
  
  "vog_insights": {
    "zeus": {  // 50ms transients
      "peak_moments": [
        {"time_ms": 15234, "confidence": 0.98, "type": "kick"},
        {"time_ms": 105000, "confidence": 1.0, "type": "drop"}
      ]
    },
    "apollo": { // 500ms groove
      "groove_sections": [
        {"start_ms": 15000, "end_ms": 45000, "stability": 0.9},
        {"start_ms": 120000, "end_ms": 150000, "stability": 0.85}
      ]
    },
    "athena": { // 5s patterns
      "pattern_changes": [
        {"time_ms": 45000, "from": "verse_pattern", "to": "chorus_pattern"},
        {"time_ms": 75000, "from": "chorus_pattern", "to": "breakdown"}
      ]
    },
    "chronos": { // 30s journey
      "energy_arc": [
        {"phase": "intro", "start_ms": 0, "energy": 0.2},
        {"phase": "development", "start_ms": 30000, "energy": 0.5},
        {"phase": "climax", "start_ms": 105000, "energy": 1.0},
        {"phase": "resolution", "start_ms": 150000, "energy": 0.6}
      ]
    }
  },
  
  "visual_cues": {
    "color_moments": [
      {"time_ms": 45000, "action": "shift_to_warm", "reason": "chorus_entry"},
      {"time_ms": 75000, "action": "desaturate", "reason": "breakdown"},
      {"time_ms": 105000, "action": "white_flash", "reason": "drop"}
    ],
    "effect_triggers": [
      {"time_ms": 90000, "effect": "particle_buildup", "duration_ms": 15000},
      {"time_ms": 105000, "effect": "explosion", "intensity": 1.0}
    ]
  },
  
  "frequency_features": {
    "spectral_centroids": [...],
    "harmonic_peaks": [...],
    "key_frequencies": [
      {"freq_hz": 440, "prominence": 0.8, "musical_note": "A4"},
      {"freq_hz": 523.25, "prominence": 0.7, "musical_note": "C5"}
    ]
  }
}
```

## Implementation Architecture

### 1. Learning Pass System
```cpp
class SongLearner {
    // All analysis nodes running in parallel
    AudioPipeline* analysis_pipeline;
    BeatDetectorNode* beat_detector;
    VoGPantheon* vog_pantheon;  // Multiple timescale VoGs
    StructureAnalyzer* structure;
    SpectralAnalyzer* spectral;
    
    // Recording buffers
    SongMap current_map;
    
public:
    void startLearning(const char* song_id) {
        current_map.song_id = song_id;
        current_map.start_time = millis();
        
        // Enable all analysis modes
        beat_detector->enableRecording();
        vog_pantheon->enableAllOracles();
        structure->startTracking();
    }
    
    void processFrame(AudioBuffer& input) {
        // Normal pipeline processing
        pipeline->process(input);
        
        // Record everything
        recordBeatEvents();
        recordEnergyBands();
        recordVoGInsights();
        recordStructuralChanges();
        
        // Every 100ms, snapshot the state
        if (millis() - last_snapshot > 100) {
            current_map.addSnapshot(getCurrentState());
        }
    }
    
    void finishLearning() {
        // Post-process to identify patterns
        structure->identifySections();
        vog_pantheon->analyzeJourney();
        
        // Save to filesystem
        saveSongMap(current_map);
    }
};
```

### 2. Playback with Song Map
```cpp
class SmartPlayer {
    SongMap* loaded_map = nullptr;
    float playback_position_ms = 0;
    
public:
    bool loadSongMap(const char* song_id) {
        loaded_map = SongMapStorage::load(song_id);
        return loaded_map != nullptr;
    }
    
    void process(AudioBuffer& input) {
        if (!loaded_map) {
            // First time hearing this song - learn it!
            learner.processFrame(input);
            return;
        }
        
        // We know this song! Use the map!
        playback_position_ms = estimatePosition(input);
        
        // Perfect beat prediction
        BeatEvent* next_beat = loaded_map->getNextBeat(playback_position_ms);
        if (next_beat && next_beat->time_ms - playback_position_ms < 50) {
            // Pre-trigger visual effects!
            prepareVisualEffect(next_beat->type, next_beat->strength);
        }
        
        // Know exactly where we are in song structure
        SongSection* current = loaded_map->getCurrentSection(playback_position_ms);
        adjustVisualsForSection(current->type, current->energy_profile);
        
        // VoG can be perfect because it knows the future
        float future_energy = loaded_map->getEnergyAt(playback_position_ms + 1000);
        vog_node->setFutureKnowledge(future_energy);
    }
};
```

### 3. Audio Fingerprinting for Song ID
```cpp
class AudioFingerprint {
    // Use spectral peaks over first 30 seconds
    static std::string generateSongId(AudioBuffer& samples) {
        SpectralPeaks peaks = extractPeaks(samples, 0, 30000);
        return sha256(peaks.serialize());
    }
    
    // Match even if slightly different recording
    static float compareFingerprints(const std::string& fp1, 
                                   const std::string& fp2) {
        // Fuzzy matching for same song, different quality
        return spectral_similarity(fp1, fp2);
    }
};
```

## Use Cases

### 1. DJ/VJ Mode
- Pre-analyze entire music library
- Instant perfect synchronization
- Seamless transitions knowing both songs' structures
- Pre-planned visual narratives

### 2. Live Performance Enhancement
- Learn during soundcheck
- Perfect sync during actual show
- Anticipate drops and builds
- Coordinate with lighting/lasers

### 3. Streaming/Broadcasting
- Pre-analyze playlist
- Perfect visual sync for viewers
- Smooth transitions between songs
- Consistent visual quality

### 4. Personal Listening
- Learn your favorite songs
- Personalized visual experiences
- Better each time you play it
- Share song maps with friends

## Technical Benefits

1. **Perfect Timing**: Know beats before they happen
2. **Structural Awareness**: Understand verse/chorus/bridge
3. **Predictive Visuals**: Pre-load effects before drops
4. **Consistent Experience**: Same song = same perfect visuals
5. **Reduced CPU**: No real-time analysis needed on playback

## Storage Considerations

- Average song map: ~50-100KB
- 1000 songs: ~50-100MB
- Could compress further with delta encoding
- Option to store on SD card or cloud

## Privacy & Sharing

- Song maps contain no audio data
- Just timing and energy information
- Safe to share without copyright issues
- Community could share maps for popular songs

This transforms SpectraSynq from a reactive system to a predictive one - it KNOWS the music!