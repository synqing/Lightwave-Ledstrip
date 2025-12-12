/*
 * SongLearnerNode - Musical Journey Learning System
 * =================================================
 * 
 * A meta-node that coordinates all analysis nodes to create a complete
 * song map during the first playback. On subsequent plays, this map
 * enables perfect predictive visualization.
 * 
 * FEATURES:
 * - Runs all analysis nodes in parallel
 * - Records beat events, energy evolution, structure
 * - Identifies intro/verse/chorus/bridge sections
 * - Creates shareable song fingerprints
 * - Enables predictive visualization on replay
 * 
 * "First time: I learn. Second time: Perfection."
 */

#ifndef SONG_LEARNER_NODE_H
#define SONG_LEARNER_NODE_H

#include "../audio_node.h"
#include "../song_map.h"
#include "beat_detector_node.h"
#include "vog_node.h"
#include <MD5Builder.h>

class SongLearnerNode : public AudioNode {
public:
    SongLearnerNode() : AudioNode("SongLearner", AudioNodeType::ANALYZER) {
        learning_active = false;
        current_map = nullptr;
        last_snapshot_time = 0;
        song_start_time = 0;
    }
    
    ~SongLearnerNode() {
        if (current_map) {
            delete current_map;
        }
    }
    
    // Start learning a new song
    void startLearning() {
        Serial.println("SongLearner: Starting learning pass...");
        
        if (current_map) delete current_map;
        current_map = new SongMap();
        
        learning_active = true;
        song_start_time = millis();
        last_snapshot_time = millis();
        
        // Reset analysis state
        section_analyzer.reset();
        fingerprint_builder.begin();
        
        // Clear histories
        energy_history.clear();
        beat_times.clear();
    }
    
    // Stop learning and finalize the map
    SongMap* finishLearning() {
        if (!learning_active || !current_map) return nullptr;
        
        Serial.println("SongLearner: Finalizing song map...");
        
        learning_active = false;
        current_map->duration_ms = millis() - song_start_time;
        current_map->analyzed_at = millis() / 1000;  // Unix timestamp
        
        // Post-process to identify structure
        identifySongSections();
        calculatePrimaryBPM();
        generateSongId();
        
        // Calculate overall confidence
        float beat_consistency = calculateBeatConsistency();
        float section_clarity = section_analyzer.getConfidence();
        current_map->confidence = (beat_consistency + section_clarity) / 2.0f;
        
        Serial.printf("Song learned! Duration: %d ms, BPM: %.1f, Confidence: %.2f\n",
                     current_map->duration_ms, current_map->primary_bpm, 
                     current_map->confidence);
        
        return current_map;
    }
    
    // Process audio and learn
    bool process(AudioBuffer& input, AudioBuffer& output) override {
        // Pass through unchanged
        memcpy(output.data, input.data, input.size * sizeof(float));
        output.size = input.size;
        output.timestamp = input.timestamp;
        output.is_silence = input.is_silence;
        output.metadata = input.metadata;
        
        if (!learning_active) return true;
        
        uint32_t current_time = millis() - song_start_time;
        
        // Record beat events
        if (output.metadata.beat_detected) {
            current_map->addBeat(
                current_time,
                output.metadata.beat_confidence,
                calculateTotalEnergy(input),
                detectBeatType(input),
                output.metadata.beat_confidence  // Use as strength for now
            );
            beat_times.push_back(current_time);
        }
        
        // Take energy snapshot every 100ms
        if (millis() - last_snapshot_time >= 100) {
            recordEnergySnapshot(input, current_time);
            last_snapshot_time = millis();
        }
        
        // Update fingerprint with spectral data
        updateFingerprint(input);
        
        // Track energy evolution for section detection
        float total_energy = calculateTotalEnergy(input);
        energy_history.push_back({current_time, total_energy});
        if (energy_history.size() > 300) {  // Keep last 30 seconds
            energy_history.erase(energy_history.begin());
        }
        
        return true;
    }
    
    // Check if currently learning
    bool isLearning() const { return learning_active; }
    
    // Get current map (may be incomplete if still learning)
    SongMap* getCurrentMap() { return current_map; }
    
private:
    bool learning_active;
    SongMap* current_map;
    uint32_t song_start_time;
    uint32_t last_snapshot_time;
    
    // Analysis helpers
    MD5Builder fingerprint_builder;
    
    struct EnergyPoint {
        uint32_t time_ms;
        float energy;
    };
    std::vector<EnergyPoint> energy_history;
    std::vector<uint32_t> beat_times;
    
    // Section detection
    struct SectionAnalyzer {
        float baseline_energy = 0;
        float current_avg_energy = 0;
        uint32_t section_start = 0;
        SongPhase current_phase = SongPhase::INTRO;
        
        void reset() {
            baseline_energy = 0;
            current_avg_energy = 0;
            section_start = 0;
            current_phase = SongPhase::INTRO;
        }
        
        float getConfidence() {
            // Based on how clearly sections are defined
            return 0.8f;  // TODO: Calculate based on energy variance
        }
    } section_analyzer;
    
    // Calculate total energy from frequency data
    float calculateTotalEnergy(const AudioBuffer& buffer) {
        float total = 0;
        for (size_t i = 0; i < buffer.size; i++) {
            total += buffer.data[i] * buffer.data[i];
        }
        return sqrtf(total / buffer.size);
    }
    
    // Detect beat type from frequency distribution
    BeatType detectBeatType(const AudioBuffer& buffer) {
        if (buffer.size < 32) return BeatType::GENERIC;
        
        // Simple frequency-based detection
        float bass_energy = 0;
        float mid_energy = 0;
        float high_energy = 0;
        
        // Sum energy in bands
        for (int i = 0; i < 8; i++) bass_energy += buffer.data[i];
        for (int i = 8; i < 32; i++) mid_energy += buffer.data[i];
        for (int i = 32; i < buffer.size && i < 64; i++) high_energy += buffer.data[i];
        
        // Classify based on energy distribution
        if (bass_energy > mid_energy * 2 && bass_energy > high_energy * 3) {
            return BeatType::KICK;
        } else if (mid_energy > bass_energy && mid_energy > high_energy) {
            return BeatType::SNARE;
        } else if (high_energy > bass_energy && high_energy > mid_energy) {
            return BeatType::HIHAT;
        }
        
        return BeatType::GENERIC;
    }
    
    // Record energy snapshot
    void recordEnergySnapshot(const AudioBuffer& buffer, uint32_t time_ms) {
        float bass = 0, mid = 0, high = 0;
        
        if (buffer.size >= 96) {
            // Calculate band energies (assuming 96 bins)
            for (int i = 0; i < 16; i++) bass += buffer.data[i];
            for (int i = 16; i < 64; i++) mid += buffer.data[i];
            for (int i = 64; i < 96; i++) high += buffer.data[i];
            
            // Normalize
            bass /= 16.0f;
            mid /= 48.0f;
            high /= 32.0f;
            
            // Further normalize to 0-1 range
            float max_expected = 10000.0f;  // Adjust based on actual values
            bass = constrain(bass / max_expected, 0.0f, 1.0f);
            mid = constrain(mid / max_expected, 0.0f, 1.0f);
            high = constrain(high / max_expected, 0.0f, 1.0f);
        }
        
        current_map->addEnergySnapshot(time_ms, bass, mid, high);
    }
    
    // Update audio fingerprint
    void updateFingerprint(const AudioBuffer& buffer) {
        // Use first 30 seconds for fingerprint
        if (millis() - song_start_time > 30000) return;
        
        // Add spectral peaks to fingerprint
        for (size_t i = 0; i < buffer.size && i < 32; i += 4) {
            fingerprint_builder.add((uint8_t*)&buffer.data[i], sizeof(float));
        }
    }
    
    // Generate unique song ID
    void generateSongId() {
        fingerprint_builder.calculate();
        String hash = fingerprint_builder.toString();
        strncpy(current_map->song_id, hash.c_str(), 64);
        current_map->song_id[64] = '\0';
    }
    
    // Identify song sections from energy patterns
    void identifySongSections() {
        if (energy_history.size() < 50) return;  // Need at least 5 seconds
        
        // Calculate average energy levels
        float total_avg = 0;
        for (const auto& point : energy_history) {
            total_avg += point.energy;
        }
        total_avg /= energy_history.size();
        
        // State machine for section detection
        SongPhase current_phase = SongPhase::INTRO;
        uint32_t phase_start = 0;
        float phase_energy_sum = 0;
        float phase_peak = 0;
        int phase_samples = 0;
        
        for (size_t i = 0; i < energy_history.size(); i++) {
            const auto& point = energy_history[i];
            phase_energy_sum += point.energy;
            phase_peak = max(phase_peak, point.energy);
            phase_samples++;
            
            // Detect phase transitions
            SongPhase new_phase = current_phase;
            
            if (current_phase == SongPhase::INTRO && point.energy > total_avg * 0.8f) {
                new_phase = SongPhase::VERSE;
            } else if (current_phase == SongPhase::VERSE && point.energy > total_avg * 1.3f) {
                new_phase = SongPhase::CHORUS;
            } else if (current_phase == SongPhase::CHORUS && point.energy < total_avg * 0.7f) {
                new_phase = SongPhase::BREAKDOWN;
            } else if (current_phase == SongPhase::BREAKDOWN && i < energy_history.size() - 10) {
                // Look ahead for buildup
                float future_energy = 0;
                for (int j = 1; j <= 10 && i + j < energy_history.size(); j++) {
                    future_energy += energy_history[i + j].energy;
                }
                future_energy /= 10;
                if (future_energy > point.energy * 1.5f) {
                    new_phase = SongPhase::BUILDUP;
                }
            }
            
            // Phase changed - record the section
            if (new_phase != current_phase) {
                float avg_energy = phase_energy_sum / phase_samples;
                const char* profile = "steady";
                
                if (phase_peak > avg_energy * 1.5f) profile = "dynamic";
                else if (current_phase == SongPhase::BUILDUP) profile = "rising";
                else if (current_phase == SongPhase::OUTRO) profile = "falling";
                
                current_map->addSection(current_phase, phase_start, point.time_ms,
                                       avg_energy / total_avg, phase_peak / total_avg,
                                       profile);
                
                // Start new phase
                current_phase = new_phase;
                phase_start = point.time_ms;
                phase_energy_sum = 0;
                phase_peak = 0;
                phase_samples = 0;
            }
        }
        
        // Add final section
        if (phase_samples > 0) {
            float avg_energy = phase_energy_sum / phase_samples;
            current_map->addSection(current_phase, phase_start, 
                                   energy_history.back().time_ms,
                                   avg_energy / total_avg, phase_peak / total_avg,
                                   "steady");
        }
    }
    
    // Calculate primary BPM from beat intervals
    void calculatePrimaryBPM() {
        if (beat_times.size() < 4) {
            current_map->primary_bpm = 120.0f;  // Default
            return;
        }
        
        // Calculate intervals
        std::vector<float> intervals;
        for (size_t i = 1; i < beat_times.size(); i++) {
            float interval = beat_times[i] - beat_times[i-1];
            if (interval > 200 && interval < 2000) {  // 30-300 BPM range
                intervals.push_back(interval);
            }
        }
        
        if (intervals.empty()) {
            current_map->primary_bpm = 120.0f;
            return;
        }
        
        // Find median interval (more robust than mean)
        std::sort(intervals.begin(), intervals.end());
        float median_interval = intervals[intervals.size() / 2];
        
        current_map->primary_bpm = 60000.0f / median_interval;
    }
    
    // Calculate beat timing consistency
    float calculateBeatConsistency() {
        if (beat_times.size() < 8) return 0.5f;
        
        // Calculate interval variance
        std::vector<float> intervals;
        for (size_t i = 1; i < beat_times.size(); i++) {
            intervals.push_back(beat_times[i] - beat_times[i-1]);
        }
        
        float mean = 0;
        for (float interval : intervals) mean += interval;
        mean /= intervals.size();
        
        float variance = 0;
        for (float interval : intervals) {
            variance += (interval - mean) * (interval - mean);
        }
        variance /= intervals.size();
        
        // Convert variance to 0-1 consistency score
        float cv = sqrt(variance) / mean;  // Coefficient of variation
        return 1.0f / (1.0f + cv);  // Higher consistency = lower variation
    }
};

#endif // SONG_LEARNER_NODE_H