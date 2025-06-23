#ifndef STANDING_WAVE_EFFECT_H
#define STANDING_WAVE_EFFECT_H

#include "LightGuideBase.h"

#if LED_STRIPS_MODE && FEATURE_LIGHT_GUIDE_MODE

class StandingWaveEffect : public LightGuideEffectBase {
private:
    // Standing wave specific parameters
    float wave_frequency = 2.0f;        // Base frequency for standing waves
    float frequency_offset = 0.1f;      // Small offset for interference
    uint8_t wave_count = 3;             // Number of simultaneous waves
    float amplitude_modulation = 0.5f;   // How much amplitude varies
    
    // Animation state
    float time_accumulator = 0.0f;
    float frequency_drift = 0.0f;
    
public:
    StandingWaveEffect() : LightGuideEffectBase("Standing Wave", 180, 15, 25) {
        // Initialize wave parameters for standing wave effect
        edge1_wave.frequency = wave_frequency;
        edge1_wave.amplitude = 1.0f;
        edge1_wave.wavelength = 20.0f;
        edge1_wave.decay_rate = 0.05f;
        
        edge2_wave.frequency = wave_frequency + frequency_offset;
        edge2_wave.amplitude = 1.0f;
        edge2_wave.wavelength = 20.0f;
        edge2_wave.decay_rate = 0.05f;
        edge2_wave.phase = LightGuide::PI_F;  // 180° phase offset for interference
    }
    
    void renderLightGuideEffect() override {
        updateAnimationState();
        
        // Generate standing wave patterns on each edge
        generateStandingWaves();
        
        // In interference mode, the base class will handle combining the edges
        // In other modes, each edge shows its individual wave pattern
    }
    
private:
    void updateAnimationState() {
        float delta_time = 0.016f;  // Assume ~60fps for consistent animation
        time_accumulator += delta_time * propagation_speed;
        
        // Slowly drift the frequency offset for organic movement
        frequency_drift += delta_time * 0.1f;
        float drift_amount = sin(frequency_drift) * 0.02f;
        edge2_wave.frequency = edge1_wave.frequency + frequency_offset + drift_amount;
        
        // Modulate amplitude based on time for breathing effect
        float amplitude_cycle = sin(time_accumulator * 0.5f) * amplitude_modulation + (1.0f - amplitude_modulation);
        edge1_wave.amplitude = amplitude_cycle;
        edge2_wave.amplitude = amplitude_cycle;
    }
    
    void generateStandingWaves() {
        // Generate wave pattern for Edge 1 (Strip 1)
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float position = (float)i / HardwareConfig::STRIP_LENGTH;
            float wave_sum = 0.0f;
            
            // Sum multiple wave components for complexity
            for (uint8_t w = 0; w < wave_count; w++) {
                float wave_freq = edge1_wave.frequency * (w + 1);
                float wave_phase = edge1_wave.phase + (position * wave_freq * LightGuide::TWO_PI_F);
                float wave_amplitude = edge1_wave.amplitude / (w + 1);  // Harmonics decay
                
                wave_sum += wave_amplitude * sin(wave_phase);
            }
            
            // Convert to color
            float intensity = (wave_sum + 1.0f) * 0.5f;  // Normalize to [0,1]
            intensity = constrain(intensity, 0.0f, 1.0f);
            
            // Create color based on position and wave intensity
            uint8_t hue = gHue + (position * 60) + (intensity * 30);
            float propagation_dist = position * LightGuide::PLATE_LENGTH_MM;
            CRGB color = getLightGuideColor(hue, intensity, propagation_dist);
            
            setEdge1LED(i, color);
        }
        
        // Generate wave pattern for Edge 2 (Strip 2) 
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float position = (float)i / HardwareConfig::STRIP_LENGTH;
            float wave_sum = 0.0f;
            
            // Sum multiple wave components (with different frequency)
            for (uint8_t w = 0; w < wave_count; w++) {
                float wave_freq = edge2_wave.frequency * (w + 1);
                float wave_phase = edge2_wave.phase + (position * wave_freq * LightGuide::TWO_PI_F);
                float wave_amplitude = edge2_wave.amplitude / (w + 1);
                
                wave_sum += wave_amplitude * sin(wave_phase);
            }
            
            // Convert to color (with slight hue shift for edge differentiation)
            float intensity = (wave_sum + 1.0f) * 0.5f;
            intensity = constrain(intensity, 0.0f, 1.0f);
            
            uint8_t hue = gHue + (position * 60) + (intensity * 30) + 40;  // Hue offset for Edge 2
            float propagation_dist = position * LightGuide::PLATE_LENGTH_MM;
            CRGB color = getLightGuideColor(hue, intensity, propagation_dist);
            
            setEdge2LED(i, color);
        }
    }
    
public:
    // Parameter control methods for encoder integration
    void setWaveFrequency(float frequency) {
        wave_frequency = constrain(frequency, 0.5f, 10.0f);
        edge1_wave.frequency = wave_frequency;
        edge2_wave.frequency = wave_frequency + frequency_offset;
    }
    
    void setFrequencyOffset(float offset) {
        frequency_offset = constrain(offset, 0.001f, 1.0f);
        edge2_wave.frequency = edge1_wave.frequency + frequency_offset;
    }
    
    void setWaveCount(uint8_t count) {
        wave_count = constrain(count, 1, 5);
    }
    
    void setAmplitudeModulation(float modulation) {
        amplitude_modulation = constrain(modulation, 0.0f, 1.0f);
    }
    
    // Getters for current parameters
    float getWaveFrequency() const { return wave_frequency; }
    float getFrequencyOffset() const { return frequency_offset; }
    uint8_t getWaveCount() const { return wave_count; }
    float getAmplitudeModulation() const { return amplitude_modulation; }
};

#endif // LED_STRIPS_MODE && FEATURE_LIGHT_GUIDE_MODE

#endif // STANDING_WAVE_EFFECT_H