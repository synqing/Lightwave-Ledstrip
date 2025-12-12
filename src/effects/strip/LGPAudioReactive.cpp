#include "../../config/features.h"

#if FEATURE_AUDIO_EFFECTS && FEATURE_AUDIO_SYNC

#include "LGPAudioReactive.h"
#include <FastLED.h>
#include "../../audio/audio_sync.h"
#include "../../audio/AudioSystem.h"
#include "../../audio/audio_frame_constants.h"
#include "../../core/EffectTypes.h"
#include "../../config/hardware_config.h"
#include <math.h>

// External dependencies
extern CRGB strip1[];
extern CRGB strip2[];
extern uint8_t gHue;
extern VisualParams visualParams;
extern AudioSynq audioSynq;

// Forward declarations for fallback effects
void solidColor();
void sinelon();
void shockwaveEffect();

// Helper function to map audio energy to brightness
inline uint8_t audioToBrightness(float energy, float scale = 1.0f) {
    return constrain(energy * 255.0f * scale, 0, 255);
}
inline uint8_t freqToHue(float freq) {
    // Map 20Hz-20kHz to full hue spectrum
    if (freq <= 0.0f) return 0;  // Safety check
    float logFreq = log10f(max(freq, 20.0f) / 20.0f) / log10f(1000.0f); // 0-1 range, prevent log(0)
    return constrain(logFreq * 255, 0, 255);
}

// ========== FREQUENCY-BASED EFFECTS ==========
void lgpFrequencyCollision() {
    if (!AudioSync.isRealAudioActive()) {
        // Fallback to static pattern
        fill_solid(strip1, HardwareConfig::STRIP1_LED_COUNT, CRGB::Black);
        fill_solid(strip2, HardwareConfig::STRIP2_LED_COUNT, CRGB::Black);
        return;
    }
    
    const AudioFrame& frame = AudioSync.getCurrentFrame();
    
    // Bass wave from left edge
    float bassPos = frame.bass_energy * HardwareConfig::STRIP1_LED_COUNT;
    
    // Treble wave from right edge  
    float treblePos = HardwareConfig::STRIP1_LED_COUNT - (frame.high_energy * HardwareConfig::STRIP1_LED_COUNT);
    
    // Mid frequencies at collision point
    float midPos = (bassPos + treblePos) / 2.0f;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 20);
    
    // Render bass wave
    for (int i = 0; i < bassPos; i++) {
        strip1[i] = CHSV(0, 255, audioToBrightness(frame.bass_energy));
        strip2[i] = CHSV(0, 255, audioToBrightness(frame.bass_energy));
    }
    
    // Render treble wave
    for (int i = treblePos; i < HardwareConfig::STRIP1_LED_COUNT; i++) {
        strip1[i] = CHSV(160, 255, audioToBrightness(frame.high_energy));
        strip2[i] = CHSV(160, 255, audioToBrightness(frame.high_energy));
    }
    
    // Collision effect at midpoint
    int collisionRadius = frame.mid_energy * 10;
    if (collisionRadius > 0) {  // Safety check
        for (int i = -collisionRadius; i <= collisionRadius; i++) {
            int pos = midPos + i;
            if (pos >= 0 && pos < HardwareConfig::STRIP1_LED_COUNT) {
                uint8_t brightness = 255 - abs(i) * (255 / collisionRadius);
                strip1[pos] += CHSV(80, 255, audioToBrightness(frame.mid_energy) * brightness / 255);
                strip2[pos] += CHSV(80, 255, audioToBrightness(frame.mid_energy) * brightness / 255);
            }
        }
    }
}

void lgpBeatInterference() {
    if (!AudioSync.isRealAudioActive()) {
        solidColor();
        return;
    }
    
    const AudioFrame& frame = AudioSync.getCurrentFrame();
    static uint8_t beatCounter = 0;
    static float lastBeatTime = 0;
    
    // Detect beat from transient
    if (frame.transient_detected && audioSynq.getCurrentTime() - lastBeatTime > 100) {
        beatCounter++;
        lastBeatTime = audioSynq.getCurrentTime();
    }
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 30);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 30);
    
    // Create interference pattern based on beat
    float phase1 = beatCounter * 0.3f;
    float phase2 = beatCounter * 0.7f;
    
    for (int i = 0; i < HardwareConfig::STRIP1_LED_COUNT; i++) {
        float wave1 = sin((i * 0.1f) + phase1) * 0.5f + 0.5f;
        float wave2 = sin((i * 0.15f) + phase2) * 0.5f + 0.5f;
        float interference = wave1 * wave2;
        
        uint8_t hue = gHue + (beatCounter * 20);
        uint8_t brightness = interference * audioToBrightness(frame.total_energy);
        
        strip1[i] = CHSV(hue, 255, brightness);
        strip2[i] = CHSV(hue + 128, 255, brightness);
    }
}

void lgpSpectralMorphing() {
    if (!AudioSync.isRealAudioActive()) {
        fill_solid(strip1, HardwareConfig::STRIP1_LED_COUNT, CRGB::Black);
        fill_solid(strip2, HardwareConfig::STRIP2_LED_COUNT, CRGB::Black);
        return;
    }
    
    const AudioFrame& frame = AudioSync.getCurrentFrame();

    // Create symmetric gradient based on frequency spectrum - CENTER ORIGIN
    for (int i = 0; i < HardwareConfig::STRIP1_LED_COUNT; i++) {
        // CENTER ORIGIN: Use distance from center, not linear position
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Interpolate between bass, mid, and high based on distance from center
        float bassWeight = max(0.0f, 1.0f - normalizedDist * 2.0f);  // Bass at center
        float midWeight = max(0.0f, 1.0f - abs(normalizedDist - 0.5f) * 2.0f);  // Mid in between
        float highWeight = max(0.0f, normalizedDist - 0.5f);  // High at edges

        float energy = frame.bass_energy * bassWeight +
                      frame.mid_energy * midWeight +
                      frame.high_energy * highWeight;

        // Use palette with small offset instead of full spectrum
        uint8_t paletteIndex = normalizedDist * 30;  // 0-30 range (not rainbow)
        uint8_t brightness = audioToBrightness(energy);

        strip1[i] = ColorFromPalette(currentPalette, gHue + paletteIndex, brightness);
        strip2[i] = ColorFromPalette(currentPalette, gHue + paletteIndex + 128, brightness);
    }
}

void lgpAudioQuantumCollapse() {
    if (!AudioSync.isRealAudioActive()) {
        // Show quantum superposition pattern when no audio
        for (int i = 0; i < HardwareConfig::STRIP1_LED_COUNT; i++) {
            uint8_t brightness = inoise8(i * 10, millis() / 10) > 200 ? 255 : 0;
            strip1[i] = CHSV(gHue, 255, brightness);
            strip2[i] = CHSV(gHue + 90, 255, brightness);
        }
        return;
    }
    
    const AudioFrame& frame = AudioSync.getCurrentFrame();
    
    // Audio energy determines collapse probability
    float collapseProb = frame.total_energy;
    
    for (int i = 0; i < HardwareConfig::STRIP1_LED_COUNT; i++) {
        bool collapsed = random8() < (collapseProb * 255);
        
        if (collapsed) {
            // Collapsed state - solid color based on dominant frequency
            uint8_t hue = frame.bass_energy > frame.high_energy ? 0 : 160;
            strip1[i] = CHSV(hue, 255, 255);
            strip2[i] = CHSV(hue, 255, 255);
        } else {
            // Superposition state - flickering
            uint8_t brightness = random8() < 128 ? 0 : audioToBrightness(frame.total_energy);
            strip1[i] = CHSV(gHue + i, 255, brightness);
            strip2[i] = CHSV(gHue + i + 128, 255, brightness);
        }
    }
}

// ========== RHYTHM-BASED EFFECTS ==========

void lgpRhythmWaves() {
    if (!AudioSync.isRealAudioActive()) {
        sinelon(); // Fallback to simple wave
        return;
    }
    
    const AudioFrame& frame = AudioSync.getCurrentFrame();
    static float wavePositions[4] = {0, 0, 0, 0};
    static float waveSpeeds[4] = {1.0f, 1.5f, 2.0f, 2.5f};
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 40);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 40);
    
    // Update wave positions based on rhythm
    for (int w = 0; w < 4; w++) {
        wavePositions[w] += waveSpeeds[w] * frame.total_energy * 2.0f;
        if (wavePositions[w] >= HardwareConfig::STRIP1_LED_COUNT) {
            wavePositions[w] = 0;
        }
        
        // Draw wave
        int waveWidth = 5 + frame.total_energy * 10;
        if (waveWidth > 0) {  // Safety check
            for (int i = -waveWidth; i <= waveWidth; i++) {
                int pos = (int)wavePositions[w] + i;
                if (pos >= 0 && pos < HardwareConfig::STRIP1_LED_COUNT) {
                    uint8_t brightness = 255 - abs(i) * (255 / waveWidth);
                    brightness = brightness * audioToBrightness(frame.total_energy) / 255;
                    strip1[pos] += CHSV(gHue + w * 60, 255, brightness);
                    strip2[pos] += CHSV(gHue + w * 60 + 30, 255, brightness);
                }
            }
        }
    }
}

void lgpEnvelopeInterference() {
    if (!AudioSync.isRealAudioActive()) {
        fill_solid(strip1, HardwareConfig::STRIP1_LED_COUNT, CRGB::Black);
        fill_solid(strip2, HardwareConfig::STRIP2_LED_COUNT, CRGB::Black);
        return;
    }
    
    const AudioFrame& frame = AudioSync.getCurrentFrame();
    static float envelope = 0;
    
    // Smooth envelope follower
    float target = frame.total_energy;
    envelope += (target - envelope) * 0.1f;
    
    // Create interference pattern with envelope-controlled intensity
    for (int i = 0; i < HardwareConfig::STRIP1_LED_COUNT; i++) {
        float wave1 = sin(i * 0.2f + millis() * 0.001f);
        float wave2 = sin(i * 0.3f - millis() * 0.0015f);
        float interference = (wave1 + wave2) * 0.5f * envelope;
        
        uint8_t brightness = abs(interference) * 255;
        uint8_t hue = interference > 0 ? gHue : gHue + 128;
        
        strip1[i] = CHSV(hue, 255, brightness);
        strip2[i] = CHSV(hue + 64, 255, brightness);
    }
}

void lgpKickShockwave() {
    if (!AudioSync.isRealAudioActive()) {
        shockwaveEffect(); // Use non-audio shockwave
        return;
    }
    
    const AudioFrame& frame = AudioSync.getCurrentFrame();
    static float shockwavePos[3] = {-1, -1, -1};
    static uint8_t shockwaveHue[3] = {0, 0, 0};
    static uint8_t nextShockwave = 0;
    
    // Detect kick drum (strong bass transient)
    if (frame.transient_detected && frame.bass_energy > 0.8f) {
        shockwavePos[nextShockwave] = HardwareConfig::STRIP1_LED_COUNT / 2;
        shockwaveHue[nextShockwave] = random8();
        nextShockwave = (nextShockwave + 1) % 3;
    }
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 30);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 30);
    
    // Update and render shockwaves
    for (int s = 0; s < 3; s++) {
        if (shockwavePos[s] >= 0) {
            // Expand shockwave
            float expansion = (HardwareConfig::STRIP1_LED_COUNT / 2) - shockwavePos[s];
            
            // Draw expanding ring
            for (int side = -1; side <= 1; side += 2) {
                int pos1 = HardwareConfig::STRIP1_LED_COUNT / 2 + side * expansion;
                int pos2 = HardwareConfig::STRIP1_LED_COUNT / 2 - side * expansion;
                
                if (pos1 >= 0 && pos1 < HardwareConfig::STRIP1_LED_COUNT) {
                    float maxExpansion = HardwareConfig::STRIP1_LED_COUNT / 2;
                    uint8_t brightness = maxExpansion > 0 ? 255 * (1.0f - expansion / maxExpansion) : 0;  // Safety check
                    strip1[pos1] = CHSV(shockwaveHue[s], 255, brightness);
                    strip2[pos1] = CHSV(shockwaveHue[s] + 30, 255, brightness);
                }
                if (pos2 >= 0 && pos2 < HardwareConfig::STRIP1_LED_COUNT) {
                    float maxExpansion = HardwareConfig::STRIP1_LED_COUNT / 2;
                    uint8_t brightness = maxExpansion > 0 ? 255 * (1.0f - expansion / maxExpansion) : 0;  // Safety check
                    strip1[pos2] = CHSV(shockwaveHue[s], 255, brightness);
                    strip2[pos2] = CHSV(shockwaveHue[s] + 30, 255, brightness);
                }
            }
            
            // Move shockwave outward
            shockwavePos[s] -= 3.0f;
            
            // Reset when reached edge
            if (shockwavePos[s] < 0) {
                shockwavePos[s] = -1;
            }
        }
    }
}

// ========== ADVANCED AUDIO ANALYSIS ==========

void lgpFFTColorMap() {
    if (!AudioSync.isRealAudioActive()) {
        // Show static palette color when no audio (no rainbow)
        fill_solid(strip1, HardwareConfig::STRIP1_LED_COUNT, ColorFromPalette(currentPalette, gHue, 64));
        fill_solid(strip2, HardwareConfig::STRIP2_LED_COUNT, ColorFromPalette(currentPalette, gHue + 128, 64));
        return;
    }

    const AudioFrame& frame = AudioSync.getCurrentFrame();

    // Map synthetic FFT bins across the strip
    int ledsPerBin = HardwareConfig::STRIP1_LED_COUNT / FFT_BIN_COUNT;

    for (int bin = 0; bin < FFT_BIN_COUNT; bin++) {
        // Use palette instead of full spectrum - map bins to small palette range
        uint8_t paletteIndex = map(bin, 0, FFT_BIN_COUNT - 1, 0, 30);  // 0-30 range (not rainbow)
        uint8_t brightness = audioToBrightness(frame.frequency_bins[bin]);

        // Fill LEDs for this bin
        int startLed = bin * ledsPerBin;
        int endLed = min(startLed + ledsPerBin, (int)HardwareConfig::STRIP1_LED_COUNT);

        for (int i = startLed; i < endLed; i++) {
            strip1[i] = ColorFromPalette(currentPalette, gHue + paletteIndex, brightness);
            strip2[i] = ColorFromPalette(currentPalette, gHue + paletteIndex + 128, brightness);
        }
    }
}

void lgpHarmonicResonance() {
    if (!AudioSync.isRealAudioActive()) {
        fill_solid(strip1, HardwareConfig::STRIP1_LED_COUNT, CRGB::Black);
        fill_solid(strip2, HardwareConfig::STRIP2_LED_COUNT, CRGB::Black);
        return;
    }
    
    const AudioFrame& frame = AudioSync.getCurrentFrame();
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 20);
    
    // Find dominant frequency (simplified - just use energy distribution)
    float fundamental = 100.0f; // Base frequency in Hz
    if (frame.high_energy > frame.bass_energy && frame.high_energy > frame.mid_energy) {
        fundamental = 1000.0f;
    } else if (frame.mid_energy > frame.bass_energy) {
        fundamental = 400.0f;
    }
    
    // Draw harmonics
    for (int harmonic = 1; harmonic <= 5; harmonic++) {
        float freq = fundamental * harmonic;
        float wavelength = 44100.0f / freq; // Samples per cycle at 44.1kHz
        float ledsPerWave = wavelength * HardwareConfig::STRIP1_LED_COUNT / 1000.0f;
        
        for (int i = 0; i < HardwareConfig::STRIP1_LED_COUNT; i++) {
            float phase = (ledsPerWave > 0 ? (i / ledsPerWave) : 0) * 2 * PI;  // Safety check
            float amplitude = sin(phase) * frame.total_energy / harmonic;
            
            if (amplitude > 0) {
                uint8_t brightness = audioToBrightness(amplitude);
                uint8_t hue = freqToHue(freq);
                strip1[i] += CHSV(hue, 255, brightness);
                strip2[i] += CHSV(hue + 15, 255, brightness);
            }
        }
    }
}

void lgpStereoPhasePattern() {
    if (!AudioSync.isRealAudioActive()) {
        // Show static phase pattern
        for (int i = 0; i < HardwareConfig::STRIP1_LED_COUNT; i++) {
            uint8_t phase = sin8(i * 5 + millis() / 10);
            strip1[i] = CHSV(gHue, 255, phase);
            strip2[i] = CHSV(gHue + 90, 255, 255 - phase);
        }
        return;
    }
    
    const AudioFrame& frame = AudioSync.getCurrentFrame();
    
    // Simulate stereo phase difference using frequency distribution
    float leftChannel = frame.bass_energy * 0.7f + frame.mid_energy * 0.3f;
    float rightChannel = frame.mid_energy * 0.3f + frame.high_energy * 0.7f;
    float phaseDiff = (leftChannel - rightChannel) * PI;
    
    for (int i = 0; i < HardwareConfig::STRIP1_LED_COUNT; i++) {
        float position = (float)i / HardwareConfig::STRIP1_LED_COUNT;
        
        // Create phase-shifted waves
        float wave1 = sin(position * 4 * PI + millis() * 0.001f);
        float wave2 = sin(position * 4 * PI + millis() * 0.001f + phaseDiff);
        
        // Color based on phase relationship
        uint8_t hue = (wave1 * wave2 > 0) ? gHue : gHue + 128;
        uint8_t brightness = audioToBrightness(abs(wave1 - wave2) * frame.total_energy);
        
        strip1[i] = CHSV(hue, 255, brightness);
        strip2[i] = CHSV(hue + 45, 255, brightness);
    }
}

// Note: Multi-core optimized effects would require ESP32 dual-core specific code
// For now, implementing single-core versions

void lgpParallelFFT() {
    // Simplified version - would use xTaskCreatePinnedToCore on ESP32
    lgpFFTColorMap(); // Fallback to regular FFT visualization
}

void lgpAudioConvolution() {
    if (!AudioSync.isRealAudioActive()) {
        fill_solid(strip1, HardwareConfig::STRIP1_LED_COUNT, CRGB::Black);
        fill_solid(strip2, HardwareConfig::STRIP2_LED_COUNT, CRGB::Black);
        return;
    }
    
    const AudioFrame& frame = AudioSync.getCurrentFrame();
    static float convolutionBuffer[HardwareConfig::STRIP1_LED_COUNT] = {0};
    
    // Simple convolution with exponential decay kernel
    float kernel[5] = {0.1f, 0.2f, 0.4f, 0.2f, 0.1f};
    
    // Shift buffer
    for (int i = HardwareConfig::STRIP1_LED_COUNT - 1; i > 0; i--) {
        convolutionBuffer[i] = convolutionBuffer[i-1] * 0.9f;
    }
    convolutionBuffer[0] = frame.total_energy;
    
    // Apply convolution
    for (int i = 0; i < HardwareConfig::STRIP1_LED_COUNT; i++) {
        float convolved = 0;
        for (int k = 0; k < 5; k++) {
            int idx = i - 2 + k;
            if (idx >= 0 && idx < HardwareConfig::STRIP1_LED_COUNT) {
                convolved += convolutionBuffer[idx] * kernel[k];
            }
        }
        
        uint8_t brightness = audioToBrightness(convolved);
        uint8_t hue = gHue + (i * 2);
        
        strip1[i] = CHSV(hue, 255, brightness);
        strip2[i] = CHSV(hue + 30, 255, brightness);
    }
}

void lgpNeuralBeatPredict() {
    if (!AudioSync.isRealAudioActive()) {
        // Show "thinking" pattern when no audio
        uint8_t beat = beatsin8(60);
        fill_solid(strip1, HardwareConfig::STRIP1_LED_COUNT, CHSV(gHue, 255, beat));
        fill_solid(strip2, HardwareConfig::STRIP2_LED_COUNT, CHSV(gHue + 90, 255, 255 - beat));
        return;
    }
    
    const AudioFrame& frame = AudioSync.getCurrentFrame();
    static float beatPrediction = 0;
    static float beatPhase = 0;
    static uint32_t lastBeatTime = 0;
    static float averageBeatInterval = 500; // ms
    
    // Simple beat prediction based on energy and timing
    if (frame.transient_detected && frame.bass_energy > 0.7f) {
        uint32_t now = millis();
        if (lastBeatTime > 0) {
            float interval = now - lastBeatTime;
            averageBeatInterval = averageBeatInterval * 0.9f + interval * 0.1f;
        }
        lastBeatTime = now;
        beatPhase = 0;
    }
    
    // Predict next beat
    uint32_t timeSinceLastBeat = millis() - lastBeatTime;
    beatPrediction = averageBeatInterval > 0 ? 1.0f - (float)timeSinceLastBeat / averageBeatInterval : 0.0f;  // Safety check
    beatPrediction = max(0.0f, beatPrediction);
    
    // Visualize prediction confidence
    fadeToBlackBy(strip1, HardwareConfig::STRIP1_LED_COUNT, 30);
    fadeToBlackBy(strip2, HardwareConfig::STRIP2_LED_COUNT, 30);
    
    // Draw prediction wave
    int center = HardwareConfig::STRIP1_LED_COUNT / 2;
    int width = beatPrediction * center;
    
    if (width > 0) {  // Safety check
        for (int i = -width; i <= width; i++) {
            int pos = center + i;
            if (pos >= 0 && pos < HardwareConfig::STRIP1_LED_COUNT) {
                uint8_t brightness = 255 - abs(i) * (255 / width);
                brightness = brightness * (0.5f + beatPrediction * 0.5f);
                
                // Color changes as beat approaches
                uint8_t hue = beatPrediction > 0.8f ? 0 : 96; // Red when beat imminent, green otherwise
                
                strip1[pos] = CHSV(hue, 255, brightness);
                strip2[pos] = CHSV(hue + 20, 255, brightness);
            }
        }
    }
    
    beatPhase += 0.02f;
}

#endif // FEATURE_AUDIO_EFFECTS && FEATURE_AUDIO_SYNC
