#include "audio_sync.h"
#include <SPIFFS.h>

// Global instance
AudioSync audioSync;

bool AudioSync::begin() {
    Serial.println("[AudioSync] Initializing...");
    
    // Ensure SPIFFS is mounted (should already be done in main setup)
    if (!SPIFFS.begin(false)) {
        Serial.println("[AudioSync] SPIFFS mount failed!");
        return false;
    }
    
    // Create audio directory if it doesn't exist
    if (!SPIFFS.exists("/audio")) {
        Serial.println("[AudioSync] Creating /audio directory");
        SPIFFS.mkdir("/audio");
    }
    
    Serial.println("[AudioSync] Ready");
    return true;
}

bool AudioSync::loadAudioData(const String& filename) {
    Serial.printf("[AudioSync] Loading audio data: %s\n", filename.c_str());
    
    // Check if file exists
    if (!SPIFFS.exists(filename)) {
        Serial.printf("[AudioSync] File not found: %s\n", filename.c_str());
        return false;
    }
    
    // Check file size to determine loading method
    File file = SPIFFS.open(filename, "r");
    size_t fileSize = file.size();
    file.close();
    
    bool success = false;
    
    if (fileSize > 5 * 1024 * 1024) {  // > 5MB
        // Use streaming mode for large files
        Serial.printf("[AudioSync] Large file (%.1f MB), using streaming mode\n", 
                     fileSize / 1024.0f / 1024.0f);
        success = decoder.loadFromFile(filename);
    } else {
        // Load entire file for small files
        Serial.printf("[AudioSync] Small file (%.1f KB), loading to memory\n", 
                     fileSize / 1024.0f);
        File file = SPIFFS.open(filename, "r");
        String jsonData = file.readString();
        file.close();
        success = decoder.loadFromJson(jsonData);
    }
    
    if (success) {
        Serial.printf("[AudioSync] Successfully loaded. Duration: %.1fs, BPM: %d\n", 
                     decoder.getDuration() / 1000.0f, decoder.getBPM());
    } else {
        Serial.println("[AudioSync] Failed to load audio data");
    }
    
    return success;
}

void AudioSync::startPlayback(unsigned long clientStartTime) {
    if (clientStartTime > 0) {
        // Calculate when to actually start based on sync offset
        unsigned long now = millis();
        unsigned long adjustedStartTime = clientStartTime + syncOffset;
        
        if (adjustedStartTime > now) {
            // Wait until start time
            delay(adjustedStartTime - now);
        }
    }
    
    decoder.startPlayback();
    syncStartTime = millis();
    active = true;
    
    Serial.printf("[AudioSync] Playback started (offset: %dms)\n", syncOffset);
}

void AudioSync::stopPlayback() {
    decoder.stopPlayback();
    active = false;
    syncStartTime = 0;
    
    // Clear the audio frame
    memset(&currentFrame, 0, sizeof(AudioFrame));
    currentFrame.silence = true;
    
    Serial.println("[AudioSync] Playback stopped");
}

void AudioSync::update() {
    if (!active) return;
    
    if (decoder.isPlaying()) {
        // Get current audio frame
        currentFrame = decoder.getCurrentFrame();
        
        // Check if we've reached the end
        if (decoder.getCurrentTime() > decoder.getDuration()) {
            Serial.println("[AudioSync] Playback completed");
            stopPlayback();
        }
    } else {
        // Decoder stopped unexpectedly
        active = false;
    }
}

float AudioSync::getCurrentTime() const {
    if (!active || syncStartTime == 0) return 0;
    return millis() - syncStartTime;
}