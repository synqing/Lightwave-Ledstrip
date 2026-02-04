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
     * @brief Enable/disable external sync mode (e.g. Trinity offline beat).
     *
     * When enabled, injected beats take precedence over ES audio tempo fields.
     */
    void setExternalSyncMode(bool enabled);

    /**
     * @brief Inject an external beat/tempo observation (e.g. Trinity beat).
     *
     * @param bpm Beats per minute.
     * @param phase01 Beat phase [0,1).
     * @param tick True if this observation corresponds to a beat tick.
     * @param downbeat True if this observation corresponds to a downbeat tick.
     * @param beatInBar Beat index within bar (0..3 for 4/4).
     * @param now_us Wall-clock timestamp (micros()) at injection time.
     * @param sample_rate_hz Sample rate used to construct an AudioTime for injection.
     */
    void injectExternalBeat(float bpm,
                            float phase01,
                            bool tick,
                            bool downbeat,
                            uint8_t beatInBar,
                            uint64_t now_us,
                            uint32_t sample_rate_hz = 12800);

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

    // External sync/injection (Trinity)
    bool m_externalSync = false;
    bool m_externalPending = false;
    float m_externalBpm = 120.0f;
    float m_externalPhase01 = 0.0f;
    bool m_externalTick = false;
    bool m_externalDownbeat = false;
    uint8_t m_externalBeatInBar = 0;
    AudioTime m_externalT{};

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
