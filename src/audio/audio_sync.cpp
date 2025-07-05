#include "audio_sync.h"
#include <SPIFFS.h>

// Global instance
AudioSynq audioSynq;

bool AudioSynq::begin() {
    Serial.println("[AudioSynq] Initializing...");
    
    // Ensure SPIFFS is mounted (should already be done in main setup)
    if (!SPIFFS.begin(false)) {
        Serial.println("[AudioSynq] SPIFFS mount failed!");
        return false;
    }
    
    // Create audio directory if it doesn't exist
    if (!SPIFFS.exists("/audio")) {
        Serial.println("[AudioSynq] Creating /audio directory");
        SPIFFS.mkdir("/audio");
    }
    
    Serial.println("[AudioSynq] Ready");
    return true;
}

bool AudioSynq::loadAudioData(const String& filename) {
    Serial.printf("[AudioSynq] Loading audio data: %s\n", filename.c_str());
    
    // Check if file exists
    if (!SPIFFS.exists(filename)) {
        Serial.printf("[AudioSynq] File not found: %s\n", filename.c_str());
        return false;
    }
    
    // Check file size to determine loading method
    File file = SPIFFS.open(filename, "r");
    size_t fileSize = file.size();
    file.close();
    
    bool success = false;
    
    if (fileSize > 5 * 1024 * 1024) {  // > 5MB
        // Use streaming mode for large files
        Serial.printf("[AudioSynq] Large file (%.1f MB), using streaming mode\n", 
                     fileSize / 1024.0f / 1024.0f);
        success = decoder.loadFromFile(filename);
    } else {
        // Load entire file for small files
        Serial.printf("[AudioSynq] Small file (%.1f KB), loading to memory\n", 
                     fileSize / 1024.0f);
        File file = SPIFFS.open(filename, "r");
        String jsonData = file.readString();
        file.close();
        success = decoder.loadFromJson(jsonData);
    }
    
    if (success) {
        Serial.printf("[AudioSynq] Successfully loaded. Duration: %.1fs, BPM: %d\n", 
                     decoder.getDuration() / 1000.0f, decoder.getBPM());
    } else {
        Serial.println("[AudioSynq] Failed to load audio data");
    }
    
    return success;
}

void AudioSynq::startPlayback(unsigned long clientStartTime) {
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
    
    Serial.printf("[AudioSynq] Playback started (offset: %dms)\n", syncOffset);
}

void AudioSynq::stopPlayback() {
    decoder.stopPlayback();
    active = false;
    syncStartTime = 0;
    
    // Clear the audio frame
    memset(&currentFrame, 0, sizeof(AudioFrame));
    currentFrame.silence = true;
    
    Serial.println("[AudioSynq] Playback stopped");
}

void AudioSynq::update() {
    if (!active) return;
    
    if (currentSource == SOURCE_VP_DECODER) {
        if (decoder.isPlaying()) {
            // Get current audio frame from decoder
            currentFrame = decoder.getCurrentFrame();
            
            // Check if we've reached the end
            if (decoder.getCurrentTime() > decoder.getDuration()) {
                Serial.println("[AudioSynq] Playback completed");
                stopPlayback();
            }
        } else {
            // Decoder stopped unexpectedly
            active = false;
        }
    } else if (currentSource == SOURCE_I2S_MIC) {
        // Update microphone and get real-time audio frame
        i2sMic.update();
        currentFrame = i2sMic.getCurrentFrame();
    }
}

float AudioSynq::getCurrentTime() const {
    if (!active || syncStartTime == 0) return 0;
    return millis() - syncStartTime;
}

bool AudioSynq::startMicrophone() {
    Serial.println("[AudioSynq] Starting microphone mode...");
    
    // Stop any active playback
    if (active && currentSource == SOURCE_VP_DECODER) {
        stopPlayback();
    }
    
    // Check if mic is already initialized (from test)
    if (!i2sMic.isInitialized()) {
        Serial.println("[AudioSynq] I2S mic not initialized, attempting initialization...");
        if (!i2sMic.begin()) {
            Serial.println("[AudioSynq] Failed to initialize I2S microphone!");
            return false;
        }
        
        // Start capture since we just initialized
        i2sMic.startCapture();
    } else {
        Serial.println("[AudioSynq] I2S mic already initialized - reusing existing driver");
        
        // Check if it's already capturing
        if (!i2sMic.isActive()) {
            Serial.println("[AudioSynq] Starting capture on existing driver");
            i2sMic.startCapture();
        } else {
            Serial.println("[AudioSynq] Mic already capturing - perfect!");
        }
    }
    
    currentSource = SOURCE_I2S_MIC;
    active = true;
    
    Serial.println("[AudioSynq] Microphone mode active");
    return true;
}

void AudioSynq::stopMicrophone() {
    if (currentSource == SOURCE_I2S_MIC) {
        i2sMic.stopCapture();
        active = false;
        
        // Clear the audio frame
        memset(&currentFrame, 0, sizeof(AudioFrame));
        currentFrame.silence = true;
        
        Serial.println("[AudioSynq] Microphone mode stopped");
    }
}

void AudioSynq::setAudioSource(bool useMicrophone) {
    if (useMicrophone) {
        startMicrophone();
    } else {
        if (currentSource == SOURCE_I2S_MIC) {
            stopMicrophone();
        }
        currentSource = SOURCE_VP_DECODER;
    }
}