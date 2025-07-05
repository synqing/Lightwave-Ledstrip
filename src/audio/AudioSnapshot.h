#ifndef AUDIO_SNAPSHOT_H
#define AUDIO_SNAPSHOT_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "audio_frame.h"
#include "Goertzel96.h"

/**
 * AudioSnapshot - Thread-safe audio data snapshot system
 * 
 * Provides atomic snapshots of audio analysis data for effects and visualizers.
 * Uses double-buffering to prevent data races between audio processing and rendering.
 * 
 * Features:
 * - Lock-free read access for effects
 * - Atomic snapshot updates from audio thread
 * - Minimal latency and overhead
 * - Supports multiple concurrent readers
 */
class AudioSnapshot {
private:
    // Double buffer for lock-free reading
    struct SnapshotData {
        AudioFrame frame;
        float spectralBins[Goertzel96::NUM_BINS];
        float fftBins[16];  // Legacy compatibility
        uint32_t timestamp;
        bool valid;
    };
    
    // Atomic pointer swap for double buffering
    volatile SnapshotData* activeBuffer;
    SnapshotData bufferA;
    SnapshotData bufferB;
    
    // Write synchronization (audio thread only)
    SemaphoreHandle_t writeMutex;
    
    // Statistics
    uint32_t updateCount = 0;
    uint32_t missedUpdates = 0;
    
public:
    AudioSnapshot() {
        // Initialize buffers
        memset(&bufferA, 0, sizeof(SnapshotData));
        memset(&bufferB, 0, sizeof(SnapshotData));
        activeBuffer = &bufferA;
        
        // Create write mutex
        writeMutex = xSemaphoreCreateMutex();
        
        Serial.println("[AudioSnapshot] Initialized with double buffering");
    }
    
    ~AudioSnapshot() {
        if (writeMutex) {
            vSemaphoreDelete(writeMutex);
        }
    }
    
    /**
     * Update snapshot with new audio data (called from audio thread)
     * @return true if update was successful
     */
    bool update(const AudioFrame& frame, const float* spectralBins, const float* fftBins) {
        // Try to acquire write lock (non-blocking)
        if (xSemaphoreTake(writeMutex, 0) != pdTRUE) {
            missedUpdates++;
            return false;
        }
        
        // Get inactive buffer
        SnapshotData* writeBuffer = (activeBuffer == &bufferA) ? &bufferB : &bufferA;
        
        // Copy data to inactive buffer
        writeBuffer->frame = frame;
        
        if (spectralBins) {
            memcpy(writeBuffer->spectralBins, spectralBins, 
                   sizeof(float) * Goertzel96::NUM_BINS);
        }
        
        if (fftBins) {
            memcpy(writeBuffer->fftBins, fftBins, sizeof(float) * 16);
        }
        
        writeBuffer->timestamp = millis();
        writeBuffer->valid = true;
        
        // Atomic pointer swap
        activeBuffer = writeBuffer;
        
        updateCount++;
        xSemaphoreGive(writeMutex);
        
        return true;
    }
    
    /**
     * Get current snapshot (lock-free read)
     */
    bool getSnapshot(AudioFrame& frame) const {
        const SnapshotData* snapshot = (const SnapshotData*)activeBuffer;
        
        if (!snapshot->valid) {
            return false;
        }
        
        frame = snapshot->frame;
        return true;
    }
    
    /**
     * Get spectral data (lock-free read)
     */
    bool getSpectralData(float* spectralBins, int binCount) const {
        const SnapshotData* snapshot = (const SnapshotData*)activeBuffer;
        
        if (!snapshot->valid) {
            return false;
        }
        
        int copyCount = min(binCount, (int)Goertzel96::NUM_BINS);
        memcpy(spectralBins, snapshot->spectralBins, sizeof(float) * copyCount);
        
        return true;
    }
    
    /**
     * Get FFT bins for legacy effects (lock-free read)
     */
    bool getFFTBins(float* fftBins) const {
        const SnapshotData* snapshot = (const SnapshotData*)activeBuffer;
        
        if (!snapshot->valid) {
            return false;
        }
        
        memcpy(fftBins, snapshot->fftBins, sizeof(float) * 16);
        return true;
    }
    
    /**
     * Get snapshot age in milliseconds
     */
    uint32_t getAge() const {
        const SnapshotData* snapshot = (const SnapshotData*)activeBuffer;
        
        if (!snapshot->valid) {
            return UINT32_MAX;
        }
        
        return millis() - snapshot->timestamp;
    }
    
    /**
     * Get statistics
     */
    void getStats(uint32_t& updates, uint32_t& missed) const {
        updates = updateCount;
        missed = missedUpdates;
    }
    
    /**
     * Reset statistics
     */
    void resetStats() {
        updateCount = 0;
        missedUpdates = 0;
    }
};

// Global audio snapshot instance
extern AudioSnapshot audioSnapshot;

#endif // AUDIO_SNAPSHOT_H