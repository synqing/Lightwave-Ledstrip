#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>
#include <algorithm>
#include "../contracts/TempoOutput.h" // Decoupled TempoOutput struct

namespace lightwaveos {
namespace audio {

// ============================================================================
// Emotiscope Configuration
// ============================================================================

constexpr uint16_t EMOTISCOPE_NUM_TEMPI = 96;
constexpr float EMOTISCOPE_TEMPO_LOW = 48.0f; // 48-144 BPM (Standard)
constexpr float EMOTISCOPE_TEMPO_HIGH = 144.0f;
constexpr uint16_t EMOTISCOPE_HISTORY_LENGTH = 512;
constexpr float EMOTISCOPE_NOVELTY_DECAY = 0.999f;

struct EmotiscopeBin {
    float target_bpm;
    float target_hz;
    float coeff;
    float sine;
    float cosine;
    float phase;
    bool phase_inverted;
    float phase_radians_per_frame;
    float magnitude;
    float magnitude_raw;
    float beat;
};

class EmotiscopeEngine {
public:
    void init();

    /**
     * @brief Update novelty using v1.1 Hybrid Input logic
     * @param spectrum64 64-bin Goertzel spectrum (or nullptr if not ready)
     * @param rms Current RMS value for VU derivative
     */
    void updateNovelty(const float* spectrum64, float rms);

    /**
     * @brief Update Tempo using Goertzel and v2.0 Dynamic Scaling
     * @param delta_sec Time since last update
     */
    void updateTempo(float delta_sec);

    /**
     * @brief Advance phase for smooth rendering (called by Renderer)
     * @param delta_sec Time since last call
     */
    void advancePhase(float delta_sec);

    /**
     * @brief Get standard output for ControlBus
     */
    TempoOutput getOutput() const;

    float getNovelty() const { return m_currentNovelty + m_currentVu; } // Combined novelty for debug

private:
    EmotiscopeBin m_bins[EMOTISCOPE_NUM_TEMPI];
    
    // History buffers for dynamic scaling
    float m_noveltyHistory[EMOTISCOPE_HISTORY_LENGTH];
    float m_vuHistory[EMOTISCOPE_HISTORY_LENGTH];
    uint16_t m_historyIdx = 0;

    // Novelty curves (current frame)
    float m_currentNovelty = 0.0f;
    float m_currentVu = 0.0f;

    // Scaling factors
    float m_noveltyScaleFactor = 1.0f;
    float m_vuScaleFactor = 1.0f;
    
    // Silence detection
    float m_silenceLevel = 0.0f;
    bool m_silenceDetected = false;
    float m_silenceHistory[128];
    uint8_t m_silenceIdx = 0;

    // Internal state
    float m_lastWinnerPhase = 0.0f;
    uint8_t m_scaleFrameCount = 0;

    // Output smoothing
    float m_outputPhase = 0.0f;
    bool m_beatTick = false;
    uint32_t m_lastTickMs = 0;
    uint32_t m_timeMs = 0;

    // Output state
    TempoOutput m_output;

    // Internal helpers
    void calculateScaleFactors();
    void checkSilence(float combinedNovelty);
    float unwrapPhase(float phase);
};

} // namespace audio
} // namespace lightwaveos
