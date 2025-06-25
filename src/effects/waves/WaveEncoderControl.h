#ifndef WAVE_ENCODER_CONTROL_H
#define WAVE_ENCODER_CONTROL_H

#include "DualStripWaveEngine.h"
#include "../../hardware/EncoderManager.h"

// Forward declaration
void updateWaveEncoderLED(uint8_t encoder_id, DualStripWaveEngine& engine);

/**
 * Handle encoder input for wave engine parameters
 * @param encoder_id: Encoder number (0-7)
 * @param delta: Encoder change (+/- rotation)
 * @param engine: Wave engine to modify
 */
void handleWaveEncoderInput(uint8_t encoder_id, int32_t delta, DualStripWaveEngine& engine) {
    switch(encoder_id) {
        case 0: // Wave type selection (sine, triangle, sawtooth, gaussian, damped)
            {
                int new_type = engine.wave_type + (delta > 0 ? 1 : -1);
                engine.wave_type = (new_type + 5) % 5; // Wrap around 0-4
                
                Serial.printf("🌊 Wave Type: %s\n", getWaveTypeName(engine.wave_type));
            }
            break;
            
        case 1: // Strip1 frequency (0.1 - 10.0 Hz)
            {
                engine.strip1_frequency = constrain(
                    engine.strip1_frequency + delta * 0.1f, 0.1f, 10.0f);
                
                Serial.printf("🎵 Strip1 Frequency: %.1f Hz\n", engine.strip1_frequency);
            }
            break;
            
        case 2: // Strip2 frequency (0.1 - 10.0 Hz)
            {
                engine.strip2_frequency = constrain(
                    engine.strip2_frequency + delta * 0.1f, 0.1f, 10.0f);
                
                Serial.printf("🎵 Strip2 Frequency: %.1f Hz\n", engine.strip2_frequency);
            }
            break;
            
        case 3: // Manual phase offset (-π to +π)
            {
                engine.manual_phase_offset = fmod(
                    engine.manual_phase_offset + delta * 0.1f, 2.0f * PI);
                
                Serial.printf("🔄 Phase Offset: %.2f rad (%.0f°)\n", 
                             engine.manual_phase_offset, 
                             engine.manual_phase_offset * 180.0f / PI);
            }
            break;
            
        case 4: // Wave speed (0.1 - 5.0x)
            {
                engine.wave_speed = constrain(
                    engine.wave_speed + delta * 0.1f, 0.1f, 5.0f);
                
                Serial.printf("⚡ Wave Speed: %.1fx\n", engine.wave_speed);
            }
            break;
            
        case 5: // Interaction mode
            {
                int new_mode = engine.interaction_mode + (delta > 0 ? 1 : -1);
                engine.interaction_mode = (new_mode + 6) % 6; // Wrap around 0-5
                
                Serial.printf("🤝 Interaction: %s\n", getInteractionModeName(engine.interaction_mode));
            }
            break;
            
        case 6: // Bidirectional toggle + Center Origin toggle
            {
                if (delta > 0) {
                    engine.bidirectional = !engine.bidirectional;
                    Serial.printf("↔️ Bidirectional: %s\n", engine.bidirectional ? "ON" : "OFF");
                } else if (delta < 0) {
                    engine.center_origin = !engine.center_origin;
                    Serial.printf("📍 Center Origin: %s\n", engine.center_origin ? "ON" : "OFF");
                }
            }
            break;
            
        case 7: // Amplitude (0.1 - 2.0)
            {
                engine.amplitude = constrain(
                    engine.amplitude + delta * 0.1f, 0.1f, 2.0f);
                
                Serial.printf("📊 Amplitude: %.1f\n", engine.amplitude);
            }
            break;
    }
    
    // Update encoder LED to reflect current state
    updateWaveEncoderLED(encoder_id, engine);
}

/**
 * Update encoder LED colors to reflect current parameter states
 */
void updateWaveEncoderLED(uint8_t encoder_id, DualStripWaveEngine& engine) {
    // LED updates are now handled by EncoderManager
    // This function is kept for compatibility but does nothing
    (void)encoder_id;  // Suppress unused parameter warning
    (void)engine;      // Suppress unused parameter warning
    return;
}

/**
 * Print wave engine status for debugging/monitoring
 */
void printWaveEngineStatus(DualStripWaveEngine& engine) {
    Serial.println("\n🌊 === DUAL-STRIP WAVE ENGINE STATUS ===");
    Serial.printf("Wave Type: %s\n", getWaveTypeName(engine.wave_type));
    Serial.printf("Strip1 Frequency: %.1f Hz\n", engine.strip1_frequency);
    Serial.printf("Strip2 Frequency: %.1f Hz\n", engine.strip2_frequency);
    Serial.printf("Beat Frequency: %.2f Hz\n", engine.beat_frequency);
    Serial.printf("Phase Offset: %.2f rad (%.0f°)\n", 
                 engine.manual_phase_offset,
                 engine.manual_phase_offset * 180.0f / PI);
    Serial.printf("Wave Speed: %.1fx\n", engine.wave_speed);
    Serial.printf("Amplitude: %.1f\n", engine.amplitude);
    Serial.printf("Interaction Mode: %s\n", getInteractionModeName(engine.interaction_mode));
    Serial.printf("Bidirectional: %s\n", engine.bidirectional ? "ON" : "OFF");
    Serial.printf("Center Origin: %s\n", engine.center_origin ? "ON" : "OFF");
    Serial.printf("Last Render Time: %d µs\n", engine.render_time_us);
    
    // Calculate and display FPS
    if (engine.render_time_us > 0) {
        float fps = 1000000.0f / engine.render_time_us;
        Serial.printf("Estimated FPS: %.1f\n", fps);
    }
    
    Serial.println("=======================================");
}

/**
 * Update performance statistics (call periodically)
 */
void updateWavePerformanceStats(DualStripWaveEngine& engine) {
    static uint32_t last_report = 0;
    static uint32_t max_render_time = 0;
    static uint32_t total_render_time = 0;
    static uint16_t sample_count = 0;
    
    // Track maximum and average render times
    if(engine.render_time_us > max_render_time) {
        max_render_time = engine.render_time_us;
    }
    
    total_render_time += engine.render_time_us;
    sample_count++;
    
    // Report every 5 seconds
    uint32_t now = millis();
    if(now - last_report >= 5000) {
        uint32_t avg_render_time = total_render_time / sample_count;
        float avg_fps = 1000000.0f / avg_render_time; // Convert µs to FPS
        
        Serial.printf("⚡ Wave Engine Performance: Avg=%.1f FPS, Max render=%.1fms, CPU=%.1f%%\n",
                     avg_fps,
                     max_render_time / 1000.0f,
                     (avg_render_time * 120) / 10000.0f); // Estimate CPU % at 120 FPS
        
        // Reset counters
        max_render_time = 0;
        total_render_time = 0;
        sample_count = 0;
        last_report = now;
    }
}

#endif // WAVE_ENCODER_CONTROL_H