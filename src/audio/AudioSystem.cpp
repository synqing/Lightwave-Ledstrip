#include "AudioSystem.h"
#include <FastLED.h>

// Static member definitions
AudioFrame AudioSystem::mockFrame;
AudioFrame AudioSystem::i2sMicFrame;
AudioFrame AudioSystem::stableFrame;
float AudioSystem::mockFrequencyBins[FFT_BIN_COUNT];
unsigned long AudioSystem::lastMockUpdate = 0;
bool AudioSystem::initialized = false;
float AudioSystem::mockBassPhase = 0;
float AudioSystem::mockMidPhase = 0;
float AudioSystem::mockHighPhase = 0;
unsigned long AudioSystem::lastBeatTime = 0;
float AudioSystem::beatInterval = 500; // 120 BPM default

// Global instance
AudioSystem AudioSync;

void AudioSystem::begin() {
    Serial.println("[AudioSystem] Initializing...");
    
    // Initialize mock frame
    mockFrame.frequency_bins = mockFrequencyBins;
    mockFrame.silence = false;
    
    // Initialize with some data
    generateMockData();
    
    initialized = true;
    Serial.println("[AudioSystem] Ready - will provide mock data until real audio is available");
}

void AudioSystem::update() {
    // Update mock data every 20ms (50Hz)
    if (millis() - lastMockUpdate > 20) {
        lastMockUpdate = millis();
        
        if (!isRealAudioActive()) {
            generateMockData();
        }
    }
}

const AudioFrame& AudioSystem::getCurrentFrame() {
    // CRITICAL FIX: Always copy to stable buffer to avoid pointer lifetime issues
    
    // 1. Try snapshot first (most efficient for cross-thread access)
    static AudioFrame snapshotFrame;
    if (audioSnapshot.getSnapshot(snapshotFrame)) {
        // Check if snapshot is fresh (< 50ms old)
        if (audioSnapshot.getAge() < 50) {
            stableFrame = snapshotFrame;  // Copy to stable buffer
            return stableFrame;
        }
    }
    
    // 2. Direct I2SMic data (if snapshot is stale)
    if (i2sMic.isActive()) {
        stableFrame = i2sMic.getCurrentFrame();  // Copy to stable buffer
        return stableFrame;
    }
    
    // 3. AudioSynq real audio (file playback or other sources)
    if (audioSynq.isPlaying() || audioSynq.isMicrophoneActive()) {
        stableFrame = audioSynq.getCurrentFrame();  // Copy to stable buffer
        return stableFrame;
    }
    
    // 4. Mock data fallback
    stableFrame = mockFrame;  // Copy to stable buffer
    return stableFrame;
}

bool AudioSystem::isRealAudioActive() {
    // CRITICAL FIX: Check I2SMic directly for real audio activity
    return audioSynq.isPlaying() || audioSynq.isMicrophoneActive() || i2sMic.isActive();
}

float AudioSystem::getBassLevel() {
    const AudioFrame& frame = getCurrentFrame();
    return constrain(frame.bass_energy, 0.0f, 1.0f);
}

float AudioSystem::getMidLevel() {
    const AudioFrame& frame = getCurrentFrame();
    return constrain(frame.mid_energy, 0.0f, 1.0f);
}

float AudioSystem::getHighLevel() {
    const AudioFrame& frame = getCurrentFrame();
    return constrain(frame.high_energy, 0.0f, 1.0f);
}

float AudioSystem::getTotalLevel() {
    const AudioFrame& frame = getCurrentFrame();
    return constrain(frame.total_energy, 0.0f, 1.0f);
}

bool AudioSystem::isBeatDetected() {
    const AudioFrame& frame = getCurrentFrame();
    return frame.beat_detected;
}

float AudioSystem::getBPM() {
    const AudioFrame& frame = getCurrentFrame();
    return frame.bpm_estimate;
}

float AudioSystem::getFrequencyBin(int bin) {
    if (bin < 0 || bin >= FFT_BIN_COUNT) return 0.0f;
    
    const AudioFrame& frame = getCurrentFrame();
    if (frame.frequency_bins) {
        return frame.frequency_bins[bin];
    }
    return 0.0f;
}

float AudioSystem::getFrequencyRange(int startBin, int endBin) {
    float sum = 0.0f;
    int count = 0;
    
    startBin = constrain(startBin, 0, FFT_BIN_COUNT - 1);
    endBin = constrain(endBin, startBin, FFT_BIN_COUNT - 1);
    
    for (int i = startBin; i <= endBin; i++) {
        sum += getFrequencyBin(i);
        count++;
    }
    
    return count > 0 ? sum / count : 0.0f;
}

float AudioSystem::getPeakFrequency() {
    int peakBin = getPeakFrequencyBin();
    // Convert bin to approximate frequency (assuming 44.1kHz sample rate)
    return (peakBin * 22050.0f) / FFT_BIN_COUNT;
}

int AudioSystem::getPeakFrequencyBin() {
    const AudioFrame& frame = getCurrentFrame();
    if (!frame.frequency_bins) return 0;
    
    int peakBin = 0;
    float peakValue = 0.0f;
    
    for (int i = 0; i < FFT_BIN_COUNT; i++) {
        if (frame.frequency_bins[i] > peakValue) {
            peakValue = frame.frequency_bins[i];
            peakBin = i;
        }
    }
    
    return peakBin;
}

void AudioSystem::generateMockData() {
    // Update phase oscillators
    mockBassPhase += 0.02f;    // Slow bass pulse
    mockMidPhase += 0.05f;     // Medium speed mids
    mockHighPhase += 0.15f;    // Fast highs
    
    // Generate frequency bins with realistic distribution
    for (int i = 0; i < FFT_BIN_COUNT; i++) {
        float freq = (float)i / FFT_BIN_COUNT;
        float amplitude = 0.0f;
        
        if (i < FFT_BIN_COUNT / 8) {
            // Bass frequencies (0-1/8) - strong, slow pulses
            amplitude = (sin(mockBassPhase + i * 0.1f) + 1.0f) * 0.4f;
            amplitude *= (1.0f - freq * 8.0f) * 0.8f; // Taper off
        } else if (i < FFT_BIN_COUNT / 2) {
            // Mid frequencies (1/8-1/2) - moderate energy
            amplitude = (sin(mockMidPhase + i * 0.2f) + 1.0f) * 0.2f;
            amplitude *= (1.0f - (freq - 0.125f) * 2.0f) * 0.5f;
        } else {
            // High frequencies (1/2-1) - sparkly, fast changes
            amplitude = (sin(mockHighPhase + i * 0.5f) + 1.0f) * 0.1f;
            amplitude *= inoise8(i + millis() / 10) / 255.0f * 0.3f;
        }
        
        // Add some random variation
        amplitude += (inoise8(i * 10 + millis() / 100) / 255.0f - 0.5f) * 0.1f;
        mockFrequencyBins[i] = constrain(amplitude, 0.0f, 1.0f);
    }
    
    // Calculate energy bands
    mockFrame.bass_energy = getFrequencyRange(0, FFT_BIN_COUNT / 8);
    mockFrame.mid_energy = getFrequencyRange(FFT_BIN_COUNT / 8, FFT_BIN_COUNT / 2);
    mockFrame.high_energy = getFrequencyRange(FFT_BIN_COUNT / 2, FFT_BIN_COUNT - 1);
    mockFrame.total_energy = (mockFrame.bass_energy + mockFrame.mid_energy + mockFrame.high_energy) / 3.0f;
    
    // Beat detection simulation
    unsigned long now = millis();
    mockFrame.beat_detected = false;
    
    // Generate beats at ~120 BPM with some variation
    if (now - lastBeatTime > beatInterval) {
        if (mockFrame.bass_energy > 0.6f) {
            mockFrame.beat_detected = true;
            mockFrame.beat_confidence = mockFrame.bass_energy;
            lastBeatTime = now;
            
            // Vary the beat interval slightly for realism
            beatInterval = 500 + (inoise8(now / 1000) - 128) * 0.2f;
        }
    }
    
    // BPM estimation
    mockFrame.bpm_estimate = 60000.0f / beatInterval;
    
    // Transient detection (random spikes)
    mockFrame.transient_detected = (random8() < 5) && (mockFrame.total_energy > 0.5f);
    
    // Never silent in mock mode
    mockFrame.silence = false;
}