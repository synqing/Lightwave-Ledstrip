/*
 * SongMapPlayer - Predictive Visualization Engine
 * ===============================================
 * 
 * Uses pre-learned song maps to deliver perfect visualization.
 * Knows what's coming and prepares visual effects in advance.
 * 
 * FEATURES:
 * - Loads song maps from storage
 * - Matches current audio to timeline
 * - Predicts upcoming beats and drops
 * - Pre-loads visual effects
 * - Provides future knowledge to all nodes
 * 
 * "I've heard this before. Let me show you perfection."
 */

#ifndef SONG_MAP_PLAYER_H
#define SONG_MAP_PLAYER_H

#include <Arduino.h>
#include "song_map.h"
#include "audio_frame.h"
#include <FS.h>
#include <SPIFFS.h>

class SongMapPlayer {
public:
    SongMapPlayer() {
        loaded_map = nullptr;
        playback_position_ms = 0;
        position_confidence = 0;
        last_beat_time = 0;
        sync_offset_ms = 0;
    }
    
    ~SongMapPlayer() {
        if (loaded_map) {
            delete loaded_map;
        }
    }
    
    // Initialize filesystem
    bool init() {
        if (!SPIFFS.begin(true)) {
            Serial.println("SongMapPlayer: Failed to mount SPIFFS");
            return false;
        }
        Serial.println("SongMapPlayer: Filesystem ready");
        return true;
    }
    
    // Load a song map by ID
    bool loadSongMap(const char* song_id) {
        char filename[128];
        snprintf(filename, sizeof(filename), "/songmaps/%s.json", song_id);
        
        File file = SPIFFS.open(filename, "r");
        if (!file) {
            Serial.printf("SongMapPlayer: Map not found: %s\n", filename);
            return false;
        }
        
        // Read JSON
        size_t size = file.size();
        if (size > 65536) {  // 64KB max
            Serial.println("SongMapPlayer: Map file too large");
            file.close();
            return false;
        }
        
        // Allocate and read
        char* buffer = new char[size + 1];
        file.readBytes(buffer, size);
        buffer[size] = '\0';
        file.close();
        
        // Parse JSON
        StaticJsonDocument<16384> doc;  // 16KB should be enough
        DeserializationError error = deserializeJson(doc, buffer);
        delete[] buffer;
        
        if (error) {
            Serial.printf("SongMapPlayer: JSON parse error: %s\n", error.c_str());
            return false;
        }
        
        // Create new map
        if (loaded_map) delete loaded_map;
        loaded_map = new SongMap();
        
        if (!loaded_map->fromJson(doc)) {
            Serial.println("SongMapPlayer: Failed to load map data");
            delete loaded_map;
            loaded_map = nullptr;
            return false;
        }
        
        Serial.printf("SongMapPlayer: Loaded map for song %s (%.1f BPM, %d beats)\n",
                     song_id, loaded_map->primary_bpm, loaded_map->beats.size());
        
        // Reset playback state
        playback_position_ms = 0;
        position_confidence = 0;
        sync_offset_ms = 0;
        
        return true;
    }
    
    // Save a song map
    bool saveSongMap(SongMap* map) {
        if (!map || strlen(map->song_id) == 0) return false;
        
        // Create directory if needed
        if (!SPIFFS.exists("/songmaps")) {
            SPIFFS.mkdir("/songmaps");
        }
        
        char filename[128];
        snprintf(filename, sizeof(filename), "/songmaps/%s.json", map->song_id);
        
        // Serialize to JSON
        StaticJsonDocument<16384> doc;
        map->toJson(doc);
        
        // Write to file
        File file = SPIFFS.open(filename, "w");
        if (!file) {
            Serial.printf("SongMapPlayer: Failed to create file: %s\n", filename);
            return false;
        }
        
        size_t written = serializeJson(doc, file);
        file.close();
        
        Serial.printf("SongMapPlayer: Saved map %s (%d bytes)\n", 
                     map->song_id, written);
        
        return written > 0;
    }
    
    // Update playback position based on audio analysis
    void updatePosition(uint32_t current_time_ms, bool beat_detected, float beat_confidence) {
        if (!loaded_map) return;
        
        // Simple position tracking for now
        // TODO: Implement proper audio fingerprint matching
        
        if (beat_detected && beat_confidence > 0.7f) {
            // Find closest beat in map
            BeatEvent* closest = nullptr;
            float min_distance = 1000000;
            
            for (auto& beat : loaded_map->beats) {
                float distance = abs((int)beat.time_ms - (int)playback_position_ms);
                if (distance < min_distance) {
                    min_distance = distance;
                    closest = &beat;
                }
            }
            
            if (closest && min_distance < 500) {  // Within 500ms
                // Sync to this beat
                int32_t offset = closest->time_ms - playback_position_ms;
                sync_offset_ms = (sync_offset_ms * 0.9f) + (offset * 0.1f);  // Smooth adjustment
                position_confidence = min(position_confidence + 0.1f, 1.0f);
                
                Serial.printf("SongMapPlayer: Synced to beat at %d ms (offset: %d)\n",
                             closest->time_ms, (int)sync_offset_ms);
            }
        }
        
        // Update position
        uint32_t delta = current_time_ms - last_update_time;
        playback_position_ms += delta;
        playback_position_ms += (int)(sync_offset_ms * 0.01f);  // Gradual sync
        last_update_time = current_time_ms;
        
        // Clamp to song duration
        if (loaded_map->duration_ms > 0) {
            playback_position_ms = playback_position_ms % loaded_map->duration_ms;
        }
    }
    
    // Get current playback position
    uint32_t getPosition() const { return playback_position_ms; }
    float getPositionConfidence() const { return position_confidence; }
    
    // Get next beat prediction
    BeatPrediction getNextBeat() {
        BeatPrediction pred = {0, 0, 0, BeatType::GENERIC};
        
        if (!loaded_map) return pred;
        
        BeatEvent* next = loaded_map->getNextBeat(playback_position_ms);
        if (next) {
            pred.time_ms = next->time_ms;
            pred.time_until_ms = next->time_ms - playback_position_ms;
            pred.confidence = next->confidence * position_confidence;
            pred.type = next->type;
        }
        
        return pred;
    }
    
    // Get current section
    SongSection* getCurrentSection() {
        if (!loaded_map) return nullptr;
        return loaded_map->getCurrentSection(playback_position_ms);
    }
    
    // Get energy at current or future position
    float getEnergyAt(int32_t offset_ms = 0, int band = -1) {
        if (!loaded_map) return 0.5f;
        
        uint32_t target_time = playback_position_ms + offset_ms;
        return loaded_map->getEnergyAt(target_time, band);
    }
    
    // Provide future knowledge to other nodes
    struct FutureKnowledge {
        float energy_1s;      // Energy 1 second from now
        float energy_5s;      // Energy 5 seconds from now
        bool drop_coming;     // Major drop within 5 seconds
        uint32_t drop_time;   // Time until drop
        SongPhase next_phase; // Next song section
        uint32_t phase_time;  // Time until phase change
    };
    
    FutureKnowledge getFutureKnowledge() {
        FutureKnowledge future;
        
        if (!loaded_map) {
            future.energy_1s = 0.5f;
            future.energy_5s = 0.5f;
            future.drop_coming = false;
            future.drop_time = 0;
            future.next_phase = SongPhase::UNKNOWN;
            future.phase_time = 0;
            return future;
        }
        
        // Get future energy levels
        future.energy_1s = getEnergyAt(1000);
        future.energy_5s = getEnergyAt(5000);
        
        // Check for drops (sudden energy increase)
        future.drop_coming = false;
        future.drop_time = 0;
        
        float current_energy = getEnergyAt(0);
        for (int i = 100; i <= 5000; i += 100) {
            float future_energy = getEnergyAt(i);
            if (future_energy > current_energy * 2.0f) {
                future.drop_coming = true;
                future.drop_time = i;
                break;
            }
        }
        
        // Find next phase change
        SongSection* current = getCurrentSection();
        future.next_phase = SongPhase::UNKNOWN;
        future.phase_time = 0;
        
        if (current) {
            // Look for next section
            for (auto& section : loaded_map->sections) {
                if (section.start_ms > playback_position_ms) {
                    future.next_phase = section.type;
                    future.phase_time = section.start_ms - playback_position_ms;
                    break;
                }
            }
        }
        
        return future;
    }
    
    // Check if a song map exists
    bool hasSongMap(const char* song_id) {
        char filename[128];
        snprintf(filename, sizeof(filename), "/songmaps/%s.json", song_id);
        return SPIFFS.exists(filename);
    }
    
    // List all available song maps
    void listSongMaps() {
        File root = SPIFFS.open("/songmaps");
        if (!root || !root.isDirectory()) {
            Serial.println("SongMapPlayer: No songmaps directory");
            return;
        }
        
        Serial.println("Available song maps:");
        File file = root.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                Serial.printf("  - %s (%d bytes)\n", file.name(), file.size());
            }
            file = root.openNextFile();
        }
    }
    
    // Get loaded map (for analysis/debugging)
    SongMap* getLoadedMap() { return loaded_map; }
    
    struct BeatPrediction {
        uint32_t time_ms;       // Absolute time of beat
        uint32_t time_until_ms; // Time until beat
        float confidence;       // Prediction confidence
        BeatType type;         // Type of beat
    };
    
private:
    SongMap* loaded_map;
    uint32_t playback_position_ms;
    float position_confidence;
    uint32_t last_update_time;
    uint32_t last_beat_time;
    float sync_offset_ms;
};

#endif // SONG_MAP_PLAYER_H