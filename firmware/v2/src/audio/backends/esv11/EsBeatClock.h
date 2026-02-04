/**
 * @file EsBeatClock.h
 * @brief Renderer-domain beat clock for ES v1.1_320 tempo fields.
 *
 * Purpose:
 * - Produce smooth beatPhase() at 120 FPS without using LWLS MusicalGrid/TempoTracker.
 * - Consume ES tempo extras embedded in ControlBusFrame.
 */

#pragma once

#include "config/features.h"

#if FEATURE_AUDIO_SYNC && FEATURE_AUDIO_BACKEND_ESV11

#include <cstdint>

#include "../../contracts/AudioTime.h"
#include "../../contracts/ControlBus.h"
#include "../../contracts/MusicalGrid.h"

namespace lightwaveos::audio::esv11 {

class EsBeatClock {
public:
    EsBeatClock() = default;

    void reset();

    /**
     * @brief Tick the beat clock at render cadence (~120 FPS).
     *
     * @param latest Latest ControlBusFrame (BY VALUE read by renderer).
     * @param newAudioFrame True if a new audio snapshot arrived this frame (sequence changed).
     * @param render_now Extrapolated render-time AudioTime.
     */
    void tick(const ControlBusFrame& latest, bool newAudioFrame, const AudioTime& render_now);

    const MusicalGridSnapshot& snapshot() const { return m_snap; }

private:
    bool m_hasBase = false;
    AudioTime m_lastTickT{};

    float m_phase01 = 0.0f;
    float m_bpm = 120.0f;
    float m_conf = 0.0f;

    uint8_t m_beatInBar = 0;
    bool m_downbeatTick = false;
    bool m_beatTick = false;

    float m_beatStrength = 0.0f;

    MusicalGridSnapshot m_snap{};

    static float clamp01(float x);
};

} // namespace lightwaveos::audio::esv11

#endif
