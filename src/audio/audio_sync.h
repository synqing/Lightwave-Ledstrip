#pragma once

#include <Arduino.h>
#include "vp_decoder.h"
#include "audio_frame.h"

/**
 * Simple Audio Sync Manager for LightwaveOS
 * 
 * Handles VP_DECODER integration and provides audio data to effects
 */
class AudioSync {
private:
    VPDecoder decoder;
    AudioFrame currentFrame;
    bool active = false;
    unsigned long syncStartTime = 0;
    int syncOffset = 0;  // User-adjustable offset in ms
    
public:
    AudioSync() = default;
    
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
};

// Global instance
extern AudioSync audioSync;