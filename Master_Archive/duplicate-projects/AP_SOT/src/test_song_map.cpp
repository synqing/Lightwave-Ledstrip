/*
 * Test Song Map System
 * ====================
 * 
 * Demonstrates the song learning and predictive playback system.
 * First play learns the song, second play delivers perfection.
 */

#include <Arduino.h>
#include "../include/audio/song_map.h"
#include "../include/audio/nodes/song_learner_node.h"
#include "../include/audio/song_map_player.h"
#include "../include/audio/audio_pipeline.h"
#include "../include/audio/nodes/i2s_input_node.h"
#include "../include/audio/nodes/dc_offset_node.h"
#include "../include/audio/nodes/goertzel_node.h"
#include "../include/audio/nodes/beat_detector_node.h"

// Global objects
AudioPipeline* pipeline = nullptr;
SongLearnerNode* learner = nullptr;
SongMapPlayer* player = nullptr;
I2SInputNode* i2s_input = nullptr;

// State tracking
bool is_learning = false;
uint32_t song_start_time = 0;
char current_song_id[65] = "";

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== Song Map Test System ===");
    Serial.println("First play: Learning mode");
    Serial.println("Second play: Predictive mode");
    Serial.println("Press 'L' to start/stop learning");
    Serial.println("Press 'P' to play with prediction");
    Serial.println("Press 'S' to save current map");
    Serial.println("Press 'D' to list saved maps\n");
    
    // Initialize pipeline
    pipeline = new AudioPipeline("TestPipeline");
    
    // Create nodes
    i2s_input = new I2SInputNode();
    auto dc_offset = new DCOffsetNode();
    auto goertzel = new GoertzelNode();
    auto beat_detector = new BeatDetectorNode();
    learner = new SongLearnerNode();
    
    // Initialize I2S
    if (!i2s_input->init()) {
        Serial.println("Failed to initialize I2S!");
        return;
    }
    
    // Build pipeline
    pipeline->addNode(i2s_input);
    pipeline->addNode(dc_offset);
    pipeline->addNode(goertzel);
    pipeline->addNode(beat_detector);
    pipeline->addNode(learner);  // Learner just observes
    
    // Initialize player
    player = new SongMapPlayer();
    if (!player->init()) {
        Serial.println("Warning: Song map storage not available");
    }
    
    Serial.println("System ready!");
}

void loop() {
    static uint32_t last_status_time = 0;
    static uint32_t last_process_time = 0;
    
    // Handle serial commands
    if (Serial.available()) {
        char cmd = Serial.read();
        handleCommand(cmd);
    }
    
    // Process audio pipeline
    if (millis() - last_process_time >= 8) {  // ~125Hz
        float input_buffer[128];
        size_t samples_read = 0;
        
        // Get samples from I2S
        if (i2s_input->readSamples(input_buffer, 128, samples_read)) {
            // Process through pipeline
            PipelineError error = pipeline->process(input_buffer, samples_read);
            
            if (error == PipelineError::NONE) {
                // Get beat detection results
                AudioBuffer* beat_output = pipeline->getNodeOutput("BeatDetector");
                if (beat_output) {
                    bool beat = beat_output->metadata.beat_detected;
                    float confidence = beat_output->metadata.beat_confidence;
                    float bpm = beat_output->metadata.current_bpm;
                    
                    // Update player position if using map
                    if (player->getLoadedMap()) {
                        player->updatePosition(millis(), beat, confidence);
                    }
                    
                    // Show beat events
                    if (beat && confidence > 0.7f) {
                        if (is_learning) {
                            Serial.printf("LEARNING: Beat at %d ms (BPM: %.1f)\n",
                                        millis() - song_start_time, bpm);
                        } else if (player->getLoadedMap()) {
                            auto pred = player->getNextBeat();
                            Serial.printf("PREDICTED: Next beat in %d ms (confidence: %.2f)\n",
                                        pred.time_until_ms, pred.confidence);
                        }
                    }
                }
            }
        }
        
        last_process_time = millis();
    }
    
    // Status update
    if (millis() - last_status_time >= 1000) {
        if (is_learning) {
            uint32_t learning_time = (millis() - song_start_time) / 1000;
            Serial.printf("Learning... %d:%02d\n", learning_time / 60, learning_time % 60);
        } else if (player->getLoadedMap()) {
            showPredictiveStatus();
        }
        
        last_status_time = millis();
    }
}

void handleCommand(char cmd) {
    switch (cmd) {
        case 'L':
        case 'l':
            if (!is_learning) {
                startLearning();
            } else {
                stopLearning();
            }
            break;
            
        case 'P':
        case 'p':
            playWithPrediction();
            break;
            
        case 'S':
        case 's':
            saveCurrentMap();
            break;
            
        case 'D':
        case 'd':
            player->listSongMaps();
            break;
            
        case '?':
            showHelp();
            break;
    }
}

void startLearning() {
    Serial.println("\n=== STARTING LEARNING MODE ===");
    is_learning = true;
    song_start_time = millis();
    learner->startLearning();
    
    // Generate temporary song ID
    snprintf(current_song_id, sizeof(current_song_id), "temp_%lu", millis());
}

void stopLearning() {
    if (!is_learning) return;
    
    Serial.println("\n=== FINISHING LEARNING ===");
    is_learning = false;
    
    SongMap* map = learner->finishLearning();
    if (map) {
        Serial.printf("Song learned successfully!\n");
        Serial.printf("  Duration: %d seconds\n", map->duration_ms / 1000);
        Serial.printf("  BPM: %.1f\n", map->primary_bpm);
        Serial.printf("  Beats detected: %d\n", map->beats.size());
        Serial.printf("  Sections found: %d\n", map->sections.size());
        Serial.printf("  Confidence: %.2f\n", map->confidence);
        
        // Show sections
        Serial.println("\nSong structure:");
        for (const auto& section : map->sections) {
            const char* phase_names[] = {
                "Unknown", "Intro", "Verse", "Chorus", "Bridge",
                "Breakdown", "Buildup", "Drop", "Outro"
            };
            Serial.printf("  %d-%d ms: %s (energy: %.2f)\n",
                         section.start_ms, section.end_ms,
                         phase_names[(int)section.type],
                         section.avg_energy);
        }
        
        // Copy song ID
        strcpy(current_song_id, map->song_id);
    }
}

void saveCurrentMap() {
    SongMap* map = learner->getCurrentMap();
    if (!map) {
        Serial.println("No map to save!");
        return;
    }
    
    if (player->saveSongMap(map)) {
        Serial.printf("Map saved with ID: %s\n", map->song_id);
    } else {
        Serial.println("Failed to save map!");
    }
}

void playWithPrediction() {
    if (strlen(current_song_id) == 0) {
        Serial.println("No song ID available. Learn a song first!");
        return;
    }
    
    Serial.printf("\n=== LOADING MAP: %s ===\n", current_song_id);
    
    if (player->loadSongMap(current_song_id)) {
        Serial.println("Map loaded! Playing with prediction...");
        
        SongMap* map = player->getLoadedMap();
        Serial.printf("  BPM: %.1f\n", map->primary_bpm);
        Serial.printf("  Duration: %d seconds\n", map->duration_ms / 1000);
        Serial.printf("  Beats: %d\n", map->beats.size());
    } else {
        Serial.println("Failed to load map!");
    }
}

void showPredictiveStatus() {
    auto future = player->getFutureKnowledge();
    auto section = player->getCurrentSection();
    auto next_beat = player->getNextBeat();
    
    Serial.printf("Position: %d ms (confidence: %.2f)\n",
                 player->getPosition(), player->getPositionConfidence());
    
    if (section) {
        const char* phase_names[] = {
            "Unknown", "Intro", "Verse", "Chorus", "Bridge",
            "Breakdown", "Buildup", "Drop", "Outro"
        };
        Serial.printf("Section: %s (energy: %.2f)\n",
                     phase_names[(int)section->type],
                     section->avg_energy);
    }
    
    if (next_beat.time_until_ms > 0 && next_beat.time_until_ms < 1000) {
        Serial.printf("Next beat in: %d ms\n", next_beat.time_until_ms);
    }
    
    if (future.drop_coming) {
        Serial.printf(">>> DROP INCOMING IN %d ms! <<<\n", future.drop_time);
    }
    
    if (future.phase_time > 0 && future.phase_time < 5000) {
        Serial.printf("Phase change in %d ms\n", future.phase_time);
    }
}

void showHelp() {
    Serial.println("\nCommands:");
    Serial.println("  L - Start/stop learning");
    Serial.println("  P - Play with prediction");
    Serial.println("  S - Save current map");
    Serial.println("  D - List saved maps");
    Serial.println("  ? - Show this help");
}