/*
 * SongMap - Musical Journey Fingerprinting System
 * ===============================================
 * 
 * Creates a complete temporal and spectral map of songs for perfect
 * visualization on subsequent plays. First play learns, second play
 * delivers perfection.
 * 
 * FEATURES:
 * - Complete beat timeline with confidence scores
 * - Song structure detection (intro/verse/chorus/bridge)
 * - Energy evolution tracking across multiple timescales
 * - Predictive capabilities for anticipating musical events
 * - Compact JSON storage format
 * 
 * This transforms SpectraSynq from reactive to predictive visualization.
 */

#ifndef SONG_MAP_H
#define SONG_MAP_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "audio_frame.h"

// Song structure phases
enum class SongPhase {
    UNKNOWN = 0,
    INTRO,
    VERSE,
    CHORUS,
    BRIDGE,
    BREAKDOWN,
    BUILDUP,
    DROP,
    OUTRO
};

// Beat event types
enum class BeatType {
    KICK = 0,
    SNARE,
    HIHAT,
    CRASH,
    GENERIC
};

// Individual beat event
struct BeatEvent {
    uint32_t time_ms;
    float confidence;
    float energy;
    BeatType type;
    float strength;  // 0-1 visual impact
};

// Song section descriptor
struct SongSection {
    SongPhase type;
    uint32_t start_ms;
    uint32_t end_ms;
    float avg_energy;
    float peak_energy;
    const char* energy_profile;  // "rising", "steady", "falling", "explosive"
    const char* key_signature;    // Musical key if detected
};

// Energy snapshot at a point in time
struct EnergySnapshot {
    uint32_t time_ms;
    float bass;   // 0-1 normalized
    float mid;    // 0-1 normalized
    float high;   // 0-1 normalized
    float total;  // 0-1 normalized
};

// Visual cue for effects
struct VisualCue {
    uint32_t time_ms;
    const char* action;
    const char* reason;
    float intensity;
};

// VoG insights at different timescales
struct VoGInsights {
    // Zeus - instant transients
    struct {
        std::vector<BeatEvent> peak_moments;
    } zeus;
    
    // Apollo - groove stability
    struct {
        uint32_t start_ms;
        uint32_t end_ms;
        float stability;  // 0-1 how locked the groove is
    } apollo[16];  // Max 16 groove sections
    int apollo_count;
    
    // Athena - pattern changes
    struct {
        uint32_t time_ms;
        const char* from_pattern;
        const char* to_pattern;
    } athena[32];  // Max 32 pattern changes
    int athena_count;
    
    // Chronos - overall journey
    struct {
        const char* phase;
        uint32_t start_ms;
        float energy;
    } chronos[16];  // Max 16 journey phases
    int chronos_count;
};

class SongMap {
public:
    // Song identification
    char song_id[65];      // SHA256 hash as hex string
    uint32_t duration_ms;
    uint32_t sample_rate;
    uint32_t analyzed_at;  // Unix timestamp
    float confidence;      // Overall map confidence 0-1
    
    // Tempo information
    float primary_bpm;
    const char* time_signature;
    
    // Dynamic arrays
    std::vector<BeatEvent> beats;
    std::vector<SongSection> sections;
    std::vector<EnergySnapshot> energy_timeline;
    std::vector<VisualCue> visual_cues;
    
    // VoG multi-timescale insights
    VoGInsights vog;
    
    // Constructor
    SongMap() : duration_ms(0), sample_rate(44100), confidence(0), primary_bpm(120) {
        strcpy(song_id, "");
        time_signature = "4/4";
        analyzed_at = 0;
    }
    
    // Add beat event during learning
    void addBeat(uint32_t time_ms, float confidence, float energy, 
                 BeatType type = BeatType::GENERIC, float strength = 0.5f) {
        beats.push_back({time_ms, confidence, energy, type, strength});
    }
    
    // Add energy snapshot
    void addEnergySnapshot(uint32_t time_ms, float bass, float mid, float high) {
        float total = (bass + mid + high) / 3.0f;
        energy_timeline.push_back({time_ms, bass, mid, high, total});
    }
    
    // Add section marker
    void addSection(SongPhase phase, uint32_t start_ms, uint32_t end_ms,
                   float avg_energy, float peak_energy, const char* profile) {
        sections.push_back({phase, start_ms, end_ms, avg_energy, peak_energy, profile, ""});
    }
    
    // Get next beat after given time
    BeatEvent* getNextBeat(uint32_t current_ms) {
        for (auto& beat : beats) {
            if (beat.time_ms > current_ms) {
                return &beat;
            }
        }
        return nullptr;
    }
    
    // Get current section
    SongSection* getCurrentSection(uint32_t current_ms) {
        for (auto& section : sections) {
            if (current_ms >= section.start_ms && current_ms < section.end_ms) {
                return &section;
            }
        }
        return nullptr;
    }
    
    // Get energy at specific time (with interpolation)
    float getEnergyAt(uint32_t time_ms, int band = -1) {
        if (energy_timeline.empty()) return 0.5f;
        
        // Find surrounding snapshots
        EnergySnapshot* before = nullptr;
        EnergySnapshot* after = nullptr;
        
        for (size_t i = 0; i < energy_timeline.size(); i++) {
            if (energy_timeline[i].time_ms <= time_ms) {
                before = &energy_timeline[i];
            }
            if (energy_timeline[i].time_ms >= time_ms && !after) {
                after = &energy_timeline[i];
                break;
            }
        }
        
        // Interpolate if we have both points
        if (before && after && before != after) {
            float t = (float)(time_ms - before->time_ms) / 
                     (float)(after->time_ms - before->time_ms);
            
            switch (band) {
                case 0: return lerp(before->bass, after->bass, t);
                case 1: return lerp(before->mid, after->mid, t);
                case 2: return lerp(before->high, after->high, t);
                default: return lerp(before->total, after->total, t);
            }
        }
        
        // Return closest point
        if (before) {
            switch (band) {
                case 0: return before->bass;
                case 1: return before->mid;
                case 2: return before->high;
                default: return before->total;
            }
        }
        
        return 0.5f;  // Default middle energy
    }
    
    // Serialize to JSON
    void toJson(JsonDocument& doc) {
        doc["song_id"] = song_id;
        doc["duration_ms"] = duration_ms;
        doc["sample_rate"] = sample_rate;
        doc["analyzed_at"] = analyzed_at;
        doc["confidence"] = confidence;
        
        // Tempo
        JsonObject tempo = doc.createNestedObject("tempo");
        tempo["primary_bpm"] = primary_bpm;
        tempo["time_signature"] = time_signature;
        
        // Beats (simplified for size)
        JsonArray beats_array = doc.createNestedArray("beats");
        for (const auto& beat : beats) {
            JsonObject beat_obj = beats_array.createNestedObject();
            beat_obj["t"] = beat.time_ms;
            beat_obj["c"] = beat.confidence;
            beat_obj["e"] = beat.energy;
            beat_obj["s"] = beat.strength;
        }
        
        // Sections
        JsonArray sections_array = doc.createNestedArray("sections");
        for (const auto& section : sections) {
            JsonObject section_obj = sections_array.createNestedObject();
            section_obj["type"] = (int)section.type;
            section_obj["start"] = section.start_ms;
            section_obj["end"] = section.end_ms;
            section_obj["avg_e"] = section.avg_energy;
            section_obj["peak_e"] = section.peak_energy;
        }
        
        // Energy timeline (sampled for size)
        JsonArray energy_array = doc.createNestedArray("energy");
        for (size_t i = 0; i < energy_timeline.size(); i += 10) { // Every 10th sample
            const auto& snapshot = energy_timeline[i];
            JsonArray e = energy_array.createNestedArray();
            e.add(snapshot.time_ms);
            e.add(snapshot.bass);
            e.add(snapshot.mid);
            e.add(snapshot.high);
        }
    }
    
    // Load from JSON
    bool fromJson(const JsonDocument& doc) {
        if (!doc.containsKey("song_id")) return false;
        
        strcpy(song_id, doc["song_id"]);
        duration_ms = doc["duration_ms"];
        sample_rate = doc["sample_rate"];
        analyzed_at = doc["analyzed_at"];
        confidence = doc["confidence"];
        
        // Tempo
        primary_bpm = doc["tempo"]["primary_bpm"];
        time_signature = doc["tempo"]["time_signature"];
        
        // Clear existing data
        beats.clear();
        sections.clear();
        energy_timeline.clear();
        
        // Load beats
        JsonArray beats_array = doc["beats"];
        for (JsonObject beat_obj : beats_array) {
            BeatEvent beat;
            beat.time_ms = beat_obj["t"];
            beat.confidence = beat_obj["c"];
            beat.energy = beat_obj["e"];
            beat.strength = beat_obj["s"];
            beat.type = BeatType::GENERIC;  // Simplified
            beats.push_back(beat);
        }
        
        // Load sections
        JsonArray sections_array = doc["sections"];
        for (JsonObject section_obj : sections_array) {
            SongSection section;
            section.type = (SongPhase)section_obj["type"].as<int>();
            section.start_ms = section_obj["start"];
            section.end_ms = section_obj["end"];
            section.avg_energy = section_obj["avg_e"];
            section.peak_energy = section_obj["peak_e"];
            sections.push_back(section);
        }
        
        // Load energy timeline
        JsonArray energy_array = doc["energy"];
        for (JsonArray e : energy_array) {
            EnergySnapshot snapshot;
            snapshot.time_ms = e[0];
            snapshot.bass = e[1];
            snapshot.mid = e[2];
            snapshot.high = e[3];
            snapshot.total = (snapshot.bass + snapshot.mid + snapshot.high) / 3.0f;
            energy_timeline.push_back(snapshot);
        }
        
        return true;
    }
    
private:
    float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }
};

#endif // SONG_MAP_H