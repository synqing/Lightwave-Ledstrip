#ifndef WAVEFORM_EFFECTS_H
#define WAVEFORM_EFFECTS_H

#include <Arduino.h>
#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"

// External dependencies
extern CRGB strip1[HardwareConfig::STRIP1_LED_COUNT];
extern CRGB strip2[HardwareConfig::STRIP2_LED_COUNT];
extern CRGB leds[HardwareConfig::NUM_LEDS];
extern uint8_t gHue;
extern CRGBPalette16 currentPalette;
extern VisualParams visualParams;

// Audio simulation data structure
struct AudioSimulator {
    // Simulated frequency bins (16 bands)
    uint8_t frequencyBins[16];
    
    // Waveform data (using FastLED integer math)
    int16_t waveformData[160];
    
    // Beat detection
    uint8_t beatCounter;
    bool beatDetected;
    uint8_t beatIntensity;
    
    // VU meter levels
    uint8_t leftChannel;
    uint8_t rightChannel;
    uint8_t peakLevel;
    
    // Update simulation
    void update();
    void generateBeat();
    void updateFrequencies();
    void updateWaveform();
};

// Global audio simulator instance
extern AudioSimulator audioSim;

// ============== WAVEFORM VISUALIZATION EFFECTS ==============
void waveformOscilloscope();      // Classic oscilloscope display
void waveformSpectrum();          // Frequency spectrum analyzer
void waveformVUMeter();           // Stereo VU meter with peak hold
void waveformBeatPulse();         // Beat-reactive pulse from center
void waveformEqualizer();         // Multi-band equalizer bars
void waveformSpectrogram();       // Scrolling frequency history
void waveformCircular();          // Circular waveform visualization
void waveformMirror();            // Mirrored waveform from center

// ============== HELPER FUNCTIONS ==============
void initAudioSimulator();
uint8_t mapFrequencyToPosition(uint8_t bin, uint8_t stripLength);
CRGB getFrequencyColor(uint8_t frequency, uint8_t intensity);

#endif // WAVEFORM_EFFECTS_H