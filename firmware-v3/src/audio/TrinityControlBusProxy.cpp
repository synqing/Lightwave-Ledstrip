// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file TrinityControlBusProxy.cpp
 * @brief Implementation of TrinityControlBusProxy
 */

#include "TrinityControlBusProxy.h"
#include <math.h>
#include <string.h>

namespace lightwaveos {
namespace audio {

TrinityControlBusProxy::TrinityControlBusProxy()
    : m_lastUpdate(0)
{
    reset();
}

void TrinityControlBusProxy::setMacros(float energy, float vocal, float bass, float perc, float bright)
{
    // Clamp all inputs to [0, 1]
    energy = fmaxf(0.0f, fminf(1.0f, energy));
    vocal = fmaxf(0.0f, fminf(1.0f, vocal));
    bass = fmaxf(0.0f, fminf(1.0f, bass));
    perc = fmaxf(0.0f, fminf(1.0f, perc));
    bright = fmaxf(0.0f, fminf(1.0f, bright));
    
    // Update timestamp
    #ifdef NATIVE_BUILD
    m_lastUpdate = esp_timer_get_time();
    #else
    m_lastUpdate = esp_timer_get_time();
    #endif
    
    // Map energy -> rms and fast_rms
    m_frame.rms = energy;
    m_frame.fast_rms = energy;
    
    // Map percussiveness -> flux and fast_flux
    m_frame.flux = perc;
    m_frame.fast_flux = perc;
    
    // Map bass_weight -> bands[0-1] (low frequencies)
    // Distribute bass across first two bands
    m_frame.bands[0] = bass * 0.6f;  // Sub-bass
    m_frame.bands[1] = bass * 0.4f;  // Bass
    // Clear mid/high bands (will be set by brightness)
    m_frame.bands[2] = 0.0f;
    m_frame.bands[3] = 0.0f;
    m_frame.bands[4] = 0.0f;
    m_frame.bands[5] = 0.0f;
    
    // Map brightness -> bands[6-7] (high frequencies)
    m_frame.bands[6] = bright * 0.5f;  // Presence
    m_frame.bands[7] = bright * 0.5f;  // Brilliance
    
    // Map vocal_presence -> chroma (distribute across all chroma bins)
    // Vocal content typically spans multiple pitch classes
    float vocalPerBin = vocal / 12.0f;
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        m_frame.chroma[i] = vocalPerBin;
    }
    
    // Map percussiveness -> snare/hihat energy
    m_frame.snareEnergy = perc * 0.7f;  // Snare typically in mid-low range
    m_frame.hihatEnergy = perc * 0.8f;  // Hi-hat in high range
    m_frame.snareTrigger = (perc > 0.5f);  // Trigger on strong percussive events
    m_frame.hihatTrigger = (perc > 0.6f);
    
    // Copy to heavy_bands and heavy_chroma (smoothed versions)
    memcpy(m_frame.heavy_bands, m_frame.bands, sizeof(m_frame.bands));
    memcpy(m_frame.heavy_chroma, m_frame.chroma, sizeof(m_frame.chroma));
    
    // Update AudioTime (use current monotonic time)
    #ifdef NATIVE_BUILD
    m_frame.t.monotonic_us = esp_timer_get_time();
    #else
    m_frame.t.monotonic_us = esp_timer_get_time();
    #endif
    m_frame.t.sample_index = 0;  // Not applicable for offline sync
    m_frame.t.sample_rate_hz = 0;
    
    // Increment hop sequence
    m_frame.hop_seq++;
}

bool TrinityControlBusProxy::isActive() const
{
    if (m_lastUpdate == 0) {
        return false;
    }

    #ifdef NATIVE_BUILD
    uint64_t now = esp_timer_get_time();
    #else
    uint64_t now = esp_timer_get_time();
    #endif

    uint64_t elapsed = (now > m_lastUpdate) ? (now - m_lastUpdate) : 0;
    return elapsed < STALENESS_TIMEOUT_US;
}

void TrinityControlBusProxy::markActive()
{
    // Update timestamp without changing macro values
    // This primes the proxy when trinity.sync START is received,
    // ensuring isActive() returns true before first macro arrives.
    #ifdef NATIVE_BUILD
    m_lastUpdate = esp_timer_get_time();
    #else
    m_lastUpdate = esp_timer_get_time();
    #endif
}

void TrinityControlBusProxy::reset()
{
    // Zero out the frame
    m_frame = {};
    m_lastUpdate = 0;
    
    // Set default AudioTime
    m_frame.t = AudioTime();
}

} // namespace audio
} // namespace lightwaveos
