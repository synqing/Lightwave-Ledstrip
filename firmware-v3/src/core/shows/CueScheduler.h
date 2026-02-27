// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file CueScheduler.h
 * @brief Time-sorted queue for executing show cues at the correct time
 *
 * LightwaveOS v2 - Show System
 *
 * Cues are stored in PROGMEM and accessed sequentially.
 * RAM: 8 bytes
 */

#pragma once

#include <Arduino.h>
#include <pgmspace.h>
#include "ShowTypes.h"

namespace lightwaveos {
namespace shows {

/**
 * @brief Time-sorted queue for executing show cues
 */
class CueScheduler {
public:
    // Maximum cues to return in single getReadyCues call
    static constexpr uint8_t MAX_CUES_PER_FRAME = 4;

    CueScheduler() : m_cues(nullptr), m_cueCount(0), m_nextIndex(0) {}

    // Load cues from PROGMEM array
    void loadCues(const ShowCue* cues, uint8_t count) {
        m_cues = cues;
        m_cueCount = count;
        m_nextIndex = 0;
    }

    // Reset to beginning of cue list
    void reset() {
        m_nextIndex = 0;
    }

    // Seek to time position - advance nextIndex to match
    void seekTo(uint32_t timeMs) {
        m_nextIndex = 0;
        if (m_cues == nullptr || m_cueCount == 0) return;

        // Find first cue that hasn't fired yet
        while (m_nextIndex < m_cueCount) {
            ShowCue cue;
            memcpy_P(&cue, &m_cues[m_nextIndex], sizeof(ShowCue));
            if (cue.timeMs > timeMs) {
                break;
            }
            m_nextIndex++;
        }
    }

    // Get cues ready to fire at currentTimeMs
    // Returns number of cues written to outCues
    // outCues must have space for at least MAX_CUES_PER_FRAME entries
    uint8_t getReadyCues(uint32_t currentTimeMs, ShowCue* outCues) {
        if (m_cues == nullptr || m_nextIndex >= m_cueCount) {
            return 0;
        }

        uint8_t count = 0;

        while (m_nextIndex < m_cueCount && count < MAX_CUES_PER_FRAME) {
            // Read cue from PROGMEM
            ShowCue cue;
            memcpy_P(&cue, &m_cues[m_nextIndex], sizeof(ShowCue));

            // Check if this cue should fire now
            if (cue.timeMs <= currentTimeMs) {
                outCues[count] = cue;
                count++;
                m_nextIndex++;
            } else {
                // Cues are sorted, so no more ready cues
                break;
            }
        }

        return count;
    }

    // Check if there are more cues to execute
    bool hasMoreCues() const {
        return m_nextIndex < m_cueCount;
    }

    // Get current position in cue list
    uint8_t getNextIndex() const {
        return m_nextIndex;
    }

    // Get total cue count
    uint8_t getCueCount() const {
        return m_cueCount;
    }

    // Peek at next cue time without advancing (returns UINT32_MAX if no more cues)
    uint32_t peekNextCueTime() const {
        if (m_cues == nullptr || m_nextIndex >= m_cueCount) {
            return UINT32_MAX;
        }
        ShowCue cue;
        memcpy_P(&cue, &m_cues[m_nextIndex], sizeof(ShowCue));
        return cue.timeMs;
    }

private:
    const ShowCue* m_cues;    // PROGMEM pointer
    uint8_t m_cueCount;
    uint8_t m_nextIndex;
};

} // namespace shows
} // namespace lightwaveos

