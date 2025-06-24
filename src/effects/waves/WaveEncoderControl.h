#ifndef WAVE_ENCODER_CONTROL_H
#define WAVE_ENCODER_CONTROL_H

#include "DualStripWaveEngine.h"
#include "m5rotate8.h"

// Forward declaration of M5ROTATE8 encoder instance
extern M5ROTATE8 encoder;
extern bool encoderAvailable;

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
    if (!encoderAvailable || !encoder.isConnected()) return;
    
    switch(encoder_id) {
        case 0: // Wave type - different colors for each type
            {
                CRGB colors[] = {
                    CRGB::Blue,    // Sine
                    CRGB::Green,   // Triangle  
                    CRGB::Red,     // Sawtooth
                    CRGB::Purple,  // Gaussian
                    CRGB::Orange   // Damped
                };
                CRGB color = colors[engine.wave_type];
                encoder.writeRGB(0, color.r/4, color.g/4, color.b/4);
            }
            break;
            
        case 1: // Strip1 frequency - intensity based on frequency
            {
                uint8_t intensity = map(engine.strip1_frequency * 10, 1, 100, 16, 64);
                encoder.writeRGB(1, intensity, 0, 0); // Red intensity
            }
            break;
            
        case 2: // Strip2 frequency - intensity based on frequency
            {
                uint8_t intensity = map(engine.strip2_frequency * 10, 1, 100, 16, 64);
                encoder.writeRGB(2, 0, intensity, 0); // Green intensity
            }
            break;
            
        case 3: // Phase offset - color wheel based on phase
            {
                uint8_t hue = (uint8_t)(engine.manual_phase_offset * 255 / (2*PI));
                CHSV hsv_color(hue, 255, 32);
                CRGB rgb_color = hsv_color;
                encoder.writeRGB(3, rgb_color.r, rgb_color.g, rgb_color.b);
            }
            break;
            
        case 4: // Wave speed - blue intensity
            {
                uint8_t intensity = map(engine.wave_speed * 10, 1, 50, 16, 64);
                encoder.writeRGB(4, 0, 0, intensity); // Blue intensity
            }
            break;
            
        case 5: // Interaction mode - cycling colors
            {
                CRGB mode_colors[] = {
                    CRGB::White,    // Independent
                    CRGB::Yellow,   // Interference
                    CRGB::Cyan,     // Chase
                    CRGB::Magenta,  // Reflection
                    CRGB::Orange,   // Spiral
                    CRGB::Pink      // Pulse
                };
                CRGB color = mode_colors[engine.interaction_mode];
                encoder.writeRGB(5, color.r/4, color.g/4, color.b/4);
            }
            break;
            
        case 6: // Bidirectional/Center Origin status
            {
                uint8_t r = engine.bidirectional ? 32 : 8;
                uint8_t g = engine.center_origin ? 32 : 8;
                encoder.writeRGB(6, r, g, 0);
            }
            break;
            
        case 7: // Amplitude - white intensity
            {
                uint8_t intensity = map(engine.amplitude * 10, 1, 20, 16, 64);
                encoder.writeRGB(7, intensity, intensity, intensity);
            }
            break;
    }
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