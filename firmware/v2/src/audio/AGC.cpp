/**
 * @file AGC.cpp
 * @brief Automatic Gain Control implementation
 *
 * @author LightwaveOS Team
 * @version 2.0.0 - Tempo Tracking Dual-Bank Architecture
 */

#include "AGC.h"

namespace lightwaveos {
namespace audio {

AGC::AGC(float attackTime, float releaseTime, float targetLevel)
    : m_gain(1.0f)
    , m_peak(targetLevel)  // Initialize to target to prevent initial overshoot
    , m_targetLevel(targetLevel)
    , m_attackTime(attackTime)
    , m_releaseTime(releaseTime)
    , m_maxGain(1.0f)  // Default: attenuation-only (no boost)
{
    // Validate parameters
    if (m_targetLevel < 0.1f) m_targetLevel = 0.1f;
    if (m_targetLevel > 1.0f) m_targetLevel = 1.0f;
    if (m_attackTime < 0.001f) m_attackTime = 0.001f;   // Min 1ms
    if (m_releaseTime < 0.01f) m_releaseTime = 0.01f;   // Min 10ms
}

void AGC::update(float peakMagnitude, float dt) {
    // Validate inputs
    if (dt <= 0.0f || dt > 1.0f) {
        return;  // Invalid time delta
    }
    if (peakMagnitude < 0.0f) {
        peakMagnitude = 0.0f;
    }

    // Update peak tracker with exponential decay
    // Peak follows input with slow decay to track envelope
    const float peakDecayAlpha = 1.0f - expf(-dt / m_releaseTime);

    if (peakMagnitude > m_peak) {
        // Fast attack when input exceeds current peak
        const float attackAlpha = 1.0f - expf(-dt / m_attackTime);
        m_peak = (1.0f - attackAlpha) * m_peak + attackAlpha * peakMagnitude;
    } else {
        // Slow release when input below current peak
        m_peak = (1.0f - peakDecayAlpha) * m_peak + peakDecayAlpha * peakMagnitude;
    }

    // Enforce minimum peak to prevent divide-by-zero
    if (m_peak < MIN_PEAK) {
        m_peak = MIN_PEAK;
    }

    // Compute desired gain to normalize peak to target level
    float desiredGain = m_targetLevel / m_peak;

    // Clamp desired gain to valid range
    desiredGain = std::max(MIN_GAIN, std::min(m_maxGain, desiredGain));

    // Smooth gain changes with attack/release
    if (desiredGain < m_gain) {
        // Attack: gain decreasing (input getting louder)
        const float attackAlpha = 1.0f - expf(-dt / m_attackTime);
        m_gain = (1.0f - attackAlpha) * m_gain + attackAlpha * desiredGain;
    } else {
        // Release: gain increasing (input getting quieter)
        const float releaseAlpha = 1.0f - expf(-dt / m_releaseTime);
        m_gain = (1.0f - releaseAlpha) * m_gain + releaseAlpha * desiredGain;
    }

    // Enforce gain limits
    m_gain = std::max(MIN_GAIN, std::min(m_maxGain, m_gain));
}

void AGC::reset() {
    m_gain = 1.0f;
    m_peak = m_targetLevel;
}

} // namespace audio
} // namespace lightwaveos
