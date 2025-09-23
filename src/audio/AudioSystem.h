#pragma once

#include "../config/features.h"

#if FEATURE_AUDIO_SYNC
#include <Arduino.h>
#include "audio_sync.h"
#include "audio_frame.h"
#include "i2s_mic.h"
#include "AudioSnapshot.h"
#include "../config/hardware_config.h"

/**
 * AudioSystem - Global audio data provider for LightwaveOS
 * 
 * This class provides a simple interface for effects to access audio data
 * and generates mock data when no real audio is available.
 */
class AudioSystem {
private:
    static AudioFrame mockFrame;
    static AudioFrame i2sMicFrame;  // Store I2SMic frame to return by reference
    static AudioFrame stableFrame;   // Thread-safe stable buffer for returns
    static float mockFrequencyBins[FFT_BIN_COUNT];
    static unsigned long lastMockUpdate;
    static bool initialized;
    
    // Mock data generation parameters
    static float mockBassPhase;
    static float mockMidPhase;
    static float mockHighPhase;
    static unsigned long lastBeatTime;
    static float beatInterval;
    
public:
    // Initialize the audio system
    static void begin();
    
    // Update mock data (call in main loop)
    static void update();
    
    // Get current audio frame (real or mock)
    static const AudioFrame& getCurrentFrame();
    
    // Check if real audio is playing
    static bool isRealAudioActive();
    
    // Get specific frequency data
    static float getBassLevel();      // 0.0 - 1.0
    static float getMidLevel();       // 0.0 - 1.0
    static float getHighLevel();      // 0.0 - 1.0
    static float getTotalLevel();     // 0.0 - 1.0
    
    // Beat detection
    static bool isBeatDetected();
    static float getBPM();
    
    // Frequency bin access (0-FFT_BIN_COUNT)
    static float getFrequencyBin(int bin);
    
    // Utility functions for effects
    static float getFrequencyRange(int startBin, int endBin);
    static float getPeakFrequency();
    static int getPeakFrequencyBin();
    
    // Generate realistic mock data
    static void generateMockData();
};

// Global instance
extern AudioSystem AudioSync;
#endif // FEATURE_AUDIO_SYNC
