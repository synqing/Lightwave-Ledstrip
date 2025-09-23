#pragma once

#include "../config/features.h"

#if FEATURE_AUDIO_SYNC
#include <Arduino.h>
#include "vp_decoder.h"
#include "audio_frame.h"
#include "i2s_mic.h"

/**
 * Simple Audio Synq Manager for LightwaveOS
 * 
 * Handles VP_DECODER integration and provides audio data to effects
 */
class AudioSynq {
private:
    VPDecoder decoder;
    AudioFrame currentFrame;
    bool active = false;
    unsigned long syncStartTime = 0;
    int syncOffset = 0;  // User-adjustable offset in ms
    
    // Audio source mode
    enum AudioSource {
        SOURCE_VP_DECODER,  // Pre-analyzed JSON data
        SOURCE_I2S_MIC     // Real-time microphone
    };
    AudioSource currentSource = SOURCE_VP_DECODER;
    
public:
    AudioSynq() = default;
    
    // Initialize the audio sync system
    bool begin();
    
    // Load audio data from JSON file
    bool loadAudioData(const String& filename);
    
    // Start synchronized playback
    void startPlayback(unsigned long clientStartTime = 0);
    
    // Stop playback
    void stopPlayback();
    
    // Update - call this in main loop
    void update();
    
    // Get current audio frame for effects
    const AudioFrame& getCurrentFrame() const { return currentFrame; }
    
    // Check if audio is playing
    bool isPlaying() const { return active && decoder.isPlaying(); }
    
    // Set sync offset (positive = delay LEDs, negative = advance LEDs)
    void setSyncOffset(int offsetMs) { syncOffset = offsetMs; }
    int getSyncOffset() const { return syncOffset; }
    
    // Get current playback time
    float getCurrentTime() const;
    
    // Get total duration
    float getDuration() const { return decoder.getDuration(); }
    
    // Microphone support
    bool startMicrophone();
    void stopMicrophone();
    bool isMicrophoneActive() const { return currentSource == SOURCE_I2S_MIC && i2sMic.isActive(); }
    
    // Switch between sources
    void setAudioSource(bool useMicrophone);
    bool isUsingMicrophone() const { return currentSource == SOURCE_I2S_MIC; }
};

// Global instance
extern AudioSynq audioSynq;
#endif // FEATURE_AUDIO_SYNC
