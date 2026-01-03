#include "EmotiscopeEngine.h"

namespace lightwaveos {
namespace audio {

void EmotiscopeEngine::init() {
    // Initialize bins
    for (int i = 0; i < EMOTISCOPE_NUM_TEMPI; ++i) {
        m_bins[i].target_bpm = EMOTISCOPE_TEMPO_LOW + i;
        m_bins[i].target_hz = m_bins[i].target_bpm / 60.0f;
        
        // Goertzel coefficients (target_hz / update_rate)
        // Update rate is ~62.5Hz (processHop)
        // Note: Emotiscope original used 100Hz reference, but here we are driven by audio hops (62.5Hz)
        // We must ensure the phase advance matches the actual delta_sec passed in updateTempo
        float normalized_freq = m_bins[i].target_hz * 0.016f; // Approx 16ms
        m_bins[i].coeff = 2.0f * cosf(2.0f * M_PI * normalized_freq);
        
        // Initialize state
        m_bins[i].sine = 0.0f;
        m_bins[i].cosine = 0.0f;
        m_bins[i].phase = 0.0f;
        m_bins[i].phase_inverted = false;
        m_bins[i].phase_radians_per_frame = 2.0f * M_PI * m_bins[i].target_hz; // Per second, scaled by delta in update
        m_bins[i].magnitude = 0.0f;
        m_bins[i].magnitude_raw = 0.0f;
        m_bins[i].beat = 0.0f;
    }

    // Clear history
    std::memset(m_noveltyHistory, 0, sizeof(m_noveltyHistory));
    std::memset(m_vuHistory, 0, sizeof(m_vuHistory));
    std::memset(m_silenceHistory, 0, sizeof(m_silenceHistory));
    m_historyIdx = 0;
    
    // Reset internal state
    m_lastWinnerPhase = 0.0f;
    m_scaleFrameCount = 0;
    
    // Reset output
    m_output = {0};
}

void EmotiscopeEngine::updateNovelty(const float* spectrum64, float rms) {
    // 1. Spectral Flux (Novelty) - v2.0 uses raw FFT bins (here 64-bin Goertzel)
    static float prevSpectrum[64] = {0};
    float spectralFlux = 0.0f;

    if (spectrum64) {
        for (int i = 0; i < 64; ++i) {
            float diff = spectrum64[i] - prevSpectrum[i];
            if (diff > 0.0f) {
                spectralFlux += diff; // Only positive changes (onsets)
            }
            prevSpectrum[i] = spectrum64[i];
        }
    }
    m_currentNovelty = spectralFlux;

    // 2. VU Derivative - v1.1 Hybrid
    static float prevRms = 0.0f;
    float vuDiff = rms - prevRms;
    if (vuDiff < 0.0f) vuDiff = 0.0f; // Half-wave rectification
    m_currentVu = vuDiff;
    prevRms = rms;

    // 3. Update History Buffers (with decay)
    m_noveltyHistory[m_historyIdx] = m_currentNovelty;
    m_vuHistory[m_historyIdx] = m_currentVu;
    
    // Decay history (v2.0 logic - keeps history dynamic)
    // Note: iterating entire buffer every frame is expensive (O(N)). 
    // Emotiscope v2.0 only decayed when silence was detected or specific conditions.
    // The "Victim" analysis said TempoTracker decays entire buffer every frame (BAD).
    // We will implement the "Smart Decay" - only decay if silence detected, or just ring buffer.
    // Actually, Emotiscope v2.0 uses a ring buffer and calculates max over it. It doesn't decay the buffer itself 
    // unless necessary. The scaling factor decays.
    
    m_historyIdx = (m_historyIdx + 1) % EMOTISCOPE_HISTORY_LENGTH;
}

void EmotiscopeEngine::calculateScaleFactors() {
    // v2.0 Dynamic Scaling
    // Update every 10 frames to save CPU
    m_scaleFrameCount++;
    if (m_scaleFrameCount % 10 != 0) return;

    // Novelty Scale
    float maxNovelty = 0.0001f;
    // Optimization: Sample history with stride 4 to reduce CPU load (512/4 = 128 checks)
    for (int i = 0; i < EMOTISCOPE_HISTORY_LENGTH; i += 4) {
        if (m_noveltyHistory[i] > maxNovelty) maxNovelty = m_noveltyHistory[i];
    }
    // Target: max value maps to ~2.0, so avg peaks are ~1.0? 
    // Reference says: scale_factor_raw = 1.0 / (max_val * 0.5);
    float targetNoveltyScale = 1.0f / (maxNovelty * 0.5f);
    m_noveltyScaleFactor = m_noveltyScaleFactor * 0.7f + targetNoveltyScale * 0.3f;

    // VU Scale
    float maxVu = 0.0001f;
    for (int i = 0; i < EMOTISCOPE_HISTORY_LENGTH; i += 4) {
        if (m_vuHistory[i] > maxVu) maxVu = m_vuHistory[i];
    }
    float targetVuScale = 1.0f / (maxVu * 0.5f);
    m_vuScaleFactor = m_vuScaleFactor * 0.7f + targetVuScale * 0.3f;
}

void EmotiscopeEngine::checkSilence(float combinedNovelty) {
    // Emotiscope Silence Detection
    m_silenceHistory[m_silenceIdx] = combinedNovelty;
    m_silenceIdx = (m_silenceIdx + 1) % 128;

    float minVal = 1000.0f;
    float maxVal = 0.0f;
    for (int i = 0; i < 128; ++i) {
        if (m_silenceHistory[i] < minVal) minVal = m_silenceHistory[i];
        if (m_silenceHistory[i] > maxVal) maxVal = m_silenceHistory[i];
    }

    float range = maxVal - minVal;
    if (range < 0.00001f) range = 0.00001f;
    
    // Silence level is inverse of dynamic range
    m_silenceLevel = 1.0f / (1.0f + range * 100.0f);
    m_silenceDetected = (m_silenceLevel > 0.5f);
}

float EmotiscopeEngine::unwrapPhase(float phase) {
    // CORRECTED Logic (Fixing the bug from reference)
    while (phase > M_PI) {
        phase -= 2.0f * M_PI;
    }
    while (phase < -M_PI) {
        phase += 2.0f * M_PI;
    }
    return phase;
}

void EmotiscopeEngine::updateTempo(float delta_sec) {
    calculateScaleFactors();

    // Combine inputs (Hybrid v1.1)
    float normalizedNovelty = m_currentNovelty * m_noveltyScaleFactor;
    float normalizedVu = m_currentVu * m_vuScaleFactor;
    
    // Mix: 50/50 mix is a good starting point for hybrid
    float inputSample = (normalizedNovelty + normalizedVu) * 0.5f;

    checkSilence(inputSample);

    // If silent, suppress input to Goertzel to prevent locking to noise
    if (m_silenceDetected) {
        inputSample *= 0.1f; // Suppress
    }

    // Goertzel Update
    // To ensure reliability, we update ALL bins or a significant subset.
    // ESP32-S3 has FP unit, 96 bins is manageable if optimized.
    // Basic Goertzel iteration:
    // s = input + coeff * s_prev - s_prev2
    // s_prev2 = s_prev
    // s_prev = s
    
    float winningMagnitude = 0.0f;
    int winningBinIdx = -1;

    for (int i = 0; i < EMOTISCOPE_NUM_TEMPI; ++i) {
        EmotiscopeBin& bin = m_bins[i];

        // Goertzel filter step
        // Re-calculate coeff based on actual delta if needed? 
        // Ideally coeff is constant for fixed sample rate. 
        // Here our sample rate is varying slightly (jitter), but we assume fixed 62.5Hz (16ms) roughly.
        // For strict correctness, we'd adjust coeff, but standard Goertzel assumes fixed Fs.
        // We'll use the pre-calced coeff.
        
        // Note: Emotiscope code used a "windowed" approach or "leaky integrator"?
        // Standard Goertzel is infinite impulse response. We need decay.
        // "Resonator" approach:
        // bin.sine = bin.sine * decay + input ... 
        
        // Using the "Resonator" logic from TempoTracker (which was based on Emotiscope):
        // It uses sine/cosine state tracking.
        
        // Let's stick to the "Perfect Hybrid" spec:
        // "Destructive Decay: ... Emotiscope only decays during silence or specific conditions."
        // Actually, we need SOME decay or the energy builds up forever.
        // Emotiscope v1.0 used `tempi[i].magnitude *= 0.99;`
        
        // 1. Phase integration
        bin.phase += bin.phase_radians_per_frame * delta_sec;
        bin.phase = unwrapPhase(bin.phase);

        // 2. Magnitude injection (project input onto rotating vector)
        // This is effectively a DFT bin update step
        bin.sine += inputSample * sinf(bin.phase);
        bin.cosine += inputSample * cosf(bin.phase);

        // 3. Apply decay (Leaky Integrator)
        // Tune this: 0.99 -> ~1.5s half life. 
        // If silence detected, decay faster?
        float decay = m_silenceDetected ? 0.95f : 0.995f; 
        bin.sine *= decay;
        bin.cosine *= decay;

        // 4. Calculate Magnitude
        bin.magnitude_raw = sqrtf(bin.sine * bin.sine + bin.cosine * bin.cosine);
        
        // 5. Quartic Scaling (v2.0)
        // Normalize against local max? Or just global scale?
        // Emotiscope v2.0 normalized the magnitude 0-1 across all bins.
        // We'll find max raw magnitude first.
    }

    // Normalize and find winner
    float maxMag = 0.0001f;
    for (int i = 0; i < EMOTISCOPE_NUM_TEMPI; ++i) {
        if (m_bins[i].magnitude_raw > maxMag) maxMag = m_bins[i].magnitude_raw;
    }

    float globalConfidence = 0.0f;

    for (int i = 0; i < EMOTISCOPE_NUM_TEMPI; ++i) {
        // Normalize 0-1
        float norm = m_bins[i].magnitude_raw / maxMag;
        
        // Quartic Scaling (v2.0) - sharpens peaks
        float quartic = norm * norm * norm * norm;
        m_bins[i].magnitude = quartic;

        if (quartic > winningMagnitude) {
            winningMagnitude = quartic;
            winningBinIdx = i;
        }
        
        // Beat signal
        m_bins[i].beat = quartic * sinf(m_bins[i].phase); // modulated by magnitude
    }

    // Update Output
    if (winningBinIdx >= 0) {
        m_output.bpm = m_bins[winningBinIdx].target_bpm;
        m_output.phase01 = (m_bins[winningBinIdx].phase + M_PI) / (2.0f * M_PI); // Map -PI..PI to 0..1
        m_output.confidence = winningMagnitude; // Quartic magnitude is confidence
        
        // Beat tick logic
        // If phase crosses 0 (or PI depending on alignment)?
        // Emotiscope used sin(phase) > 0 checks or similar.
        // Let's use simple phase wrapping check or window.
        // A simple way is: if phase is close to 0 within a small window.
        // "isOnBeat" usually means a single frame trigger.
        // We can track if phase crossed 0 since last update? 
        // For now, let's just say if phase is within small window of 0.
        // But better: use phase delta.
        
        float currentPhase = m_bins[winningBinIdx].phase;
        
        // Check for zero crossing (from negative to positive)
        // Note: phase wraps PI -> -PI. 
        // Zero crossing is ... -0.1 -> +0.1
        if (m_lastWinnerPhase < 0 && currentPhase >= 0) {
            m_output.beat_tick = true;
        } else {
            m_output.beat_tick = false;
        }
        m_lastWinnerPhase = currentPhase;
        
        m_output.beat_strength = winningMagnitude;
        m_output.locked = (winningMagnitude > 0.3f && !m_silenceDetected); // Simple lock threshold
        
        // Sync output phase to winner phase for drift correction
        // We only sync if locked or if confidence is high enough to be trusted
        if (m_output.locked) {
            m_outputPhase = m_bins[winningBinIdx].phase;
        }
    }
}

void EmotiscopeEngine::advancePhase(float delta_sec) {
    // 1. Update time
    uint32_t step_ms = static_cast<uint32_t>(delta_sec * 1000.0f + 0.5f);
    m_timeMs += step_ms;

    // 2. Determine phase increment based on current output BPM
    float bpm = m_output.bpm;
    if (bpm < EMOTISCOPE_TEMPO_LOW) bpm = 60.0f; 

    float target_hz = bpm / 60.0f;
    float phase_radians_per_frame = 2.0f * M_PI * target_hz;

    // 3. Advance phase
    float last_phase = m_outputPhase;
    m_outputPhase += phase_radians_per_frame * delta_sec;

    // 4. Wrap phase
    m_outputPhase = unwrapPhase(m_outputPhase);

    // 5. Beat tick detection (zero crossing from negative to positive)
    m_beatTick = (last_phase < 0.0f && m_outputPhase >= 0.0f);

    // Debounce
    if (m_beatTick) {
        float beat_period_ms = 60000.0f / bpm;
        if (m_timeMs - m_lastTickMs < static_cast<uint32_t>(beat_period_ms * 0.6f)) {
            m_beatTick = false;
        } else {
            m_lastTickMs = m_timeMs;
        }
    }
}

TempoOutput EmotiscopeEngine::getOutput() const {
    TempoOutput out = m_output;
    // Use smoothed/advanced phase and beat tick
    out.phase01 = (m_outputPhase + M_PI) / (2.0f * M_PI);
    out.beat_tick = m_beatTick;
    return out;
}

} // namespace audio
} // namespace lightwaveos
