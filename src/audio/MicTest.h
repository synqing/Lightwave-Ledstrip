#pragma once

#include "../config/features.h"

#if FEATURE_AUDIO_SYNC
#include <Arduino.h>

/**
 * Simple I2S Microphone Test Functions
 * Test if the I2S microphone is working correctly
 */
class MicTest {
public:
    // Test if I2S mic is responding
    static bool testMicConnection();
    
    // Print mic status and audio levels
    static void printMicStatus();
    
    // Start continuous mic monitoring (call this once)
    static void startMicMonitoring();
    
    // Stop mic monitoring
    static void stopMicMonitoring();
    
    // Check if mic is producing any audio data
    static bool isMicProducingAudio();
    
private:
    static bool monitoringActive;
    static unsigned long lastStatusPrint;
    static float lastAudioLevel;
};

#endif // FEATURE_AUDIO_SYNC
