// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file TrinityControlBusProxy.h
 * @brief Maps Trinity ML macro values to ControlBusFrame for offline sync
 *
 * Converts pre-computed Trinity analysis (energy, vocal, bass, perc, bright)
 * into ControlBusFrame-compatible fields that effects can consume.
 *
 * Used when firmware is in "Trinity sync mode" - effects read from this proxy
 * instead of live audio analysis.
 */

#pragma once

#include "contracts/ControlBus.h"
#include "contracts/AudioTime.h"
#include <stdint.h>

#ifdef NATIVE_BUILD
#include "mocks/esp_timer_mock.h"
#else
#include <esp_timer.h>
#endif

namespace lightwaveos {
namespace audio {

/**
 * @brief Proxy that maps Trinity macros to ControlBusFrame
 *
 * Staleness: Returns inactive if no update received in 250ms.
 * This prevents "frozen" visualisations if the host disconnects.
 */
class TrinityControlBusProxy {
public:
    TrinityControlBusProxy();
    
    /**
     * @brief Update macro values from Trinity analysis
     * @param energy Overall energy (0-1) -> maps to rms
     * @param vocal Vocal presence (0-1) -> maps to chroma
     * @param bass Bass weight (0-1) -> maps to bands[0-1]
     * @param perc Percussiveness (0-1) -> maps to flux, snare/hihat
     * @param bright Brightness (0-1) -> maps to bands[6-7]
     */
    void setMacros(float energy, float vocal, float bass, float perc, float bright);
    
    /**
     * @brief Get the current ControlBusFrame (by value)
     */
    const ControlBusFrame& getFrame() const { return m_frame; }
    
    /**
     * @brief Check if proxy is active (recently updated)
     * @return true if last update was < 250ms ago
     */
    bool isActive() const;

    /**
     * @brief Mark proxy as active without changing macro values
     * Called when trinity.sync START is received to prevent race condition
     * where the first frame check happens before any macro arrives.
     */
    void markActive();

    /**
     * @brief Reset proxy state (clear frame, mark inactive)
     */
    void reset();

private:
    ControlBusFrame m_frame;
    uint64_t m_lastUpdate;  // Microseconds timestamp from esp_timer_get_time()
    
    static constexpr uint64_t STALENESS_TIMEOUT_US = 250000;  // 250ms
};

} // namespace audio
} // namespace lightwaveos
