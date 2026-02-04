/**
 * @file EsBeatClock.cpp
 */

#include "EsBeatClock.h"

#if FEATURE_AUDIO_SYNC && FEATURE_AUDIO_BACKEND_ESV11

#include <cmath>

namespace lightwaveos::audio::esv11 {

float EsBeatClock::clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

void EsBeatClock::reset()
{
    m_hasBase = false;
    m_lastTickT = AudioTime{};
    m_externalSync = false;
    m_externalPending = false;
    m_externalBpm = 120.0f;
    m_externalPhase01 = 0.0f;
    m_externalTick = false;
    m_externalDownbeat = false;
    m_externalBeatInBar = 0;
    m_externalT = AudioTime{};
    m_phase01 = 0.0f;
    m_bpm = 120.0f;
    m_conf = 0.0f;
    m_beatInBar = 0;
    m_downbeatTick = false;
    m_beatTick = false;
    m_beatStrength = 0.0f;
    m_snap = MusicalGridSnapshot{};
}

void EsBeatClock::setExternalSyncMode(bool enabled)
{
    m_externalSync = enabled;
    m_externalPending = false;
    if (!enabled) {
        m_externalTick = false;
        m_externalDownbeat = false;
        m_externalBeatInBar = 0;
    }
}

void EsBeatClock::injectExternalBeat(float bpm,
                                     float phase01,
                                     bool tick,
                                     bool downbeat,
                                     uint8_t beatInBar,
                                     uint64_t now_us,
                                     uint32_t sample_rate_hz)
{
    m_externalBpm = (bpm > 1.0f) ? bpm : m_externalBpm;
    m_externalPhase01 = clamp01(phase01);
    m_externalTick = tick;
    m_externalDownbeat = downbeat;
    m_externalBeatInBar = beatInBar;

    const uint64_t samples = (sample_rate_hz > 0)
        ? (now_us * static_cast<uint64_t>(sample_rate_hz) / 1000000ULL)
        : now_us * 12800ULL / 1000000ULL;
    m_externalT = AudioTime(samples, sample_rate_hz ? sample_rate_hz : 12800, now_us);
    m_externalPending = true;
}

void EsBeatClock::tick(const ControlBusFrame& latest, bool newAudioFrame, const AudioTime& render_now)
{
    // Reset per-frame tick flags
    m_beatTick = false;
    m_downbeatTick = false;

    if (!m_hasBase) {
        m_hasBase = true;
        m_lastTickT = render_now;
        m_phase01 = clamp01(latest.es_phase01_at_audio_t);
        m_bpm = (latest.es_bpm > 1.0f) ? latest.es_bpm : 120.0f;
        m_conf = clamp01(latest.es_tempo_confidence);
        m_beatInBar = latest.es_beat_in_bar;
    }

    // External injection takes precedence when enabled.
    if (m_externalSync) {
        if (m_externalPending) {
            // Resynchronise base to injected observation time.
            m_bpm = (m_externalBpm > 1.0f) ? m_externalBpm : m_bpm;
            m_conf = 1.0f; // External sources assumed confident unless specified elsewhere.
            m_phase01 = clamp01(m_externalPhase01);
            m_beatInBar = m_externalBeatInBar;
            m_lastTickT = m_externalT.sample_rate_hz ? m_externalT : render_now;

            if (m_externalTick) {
                m_phase01 = 0.0f;
                m_beatTick = true;
                m_downbeatTick = m_externalDownbeat;
                m_beatStrength = 1.0f;
            }
            m_externalPending = false;
        }
    } else {
    // Resynchronise base phase when a fresh audio frame arrives.
    if (newAudioFrame) {
        m_bpm = (latest.es_bpm > 1.0f) ? latest.es_bpm : m_bpm;
        m_conf = clamp01(latest.es_tempo_confidence);
        m_phase01 = clamp01(latest.es_phase01_at_audio_t);
        m_beatInBar = latest.es_beat_in_bar;

        if (latest.es_beat_tick) {
            // Hard align to beat boundary.
            m_phase01 = 0.0f;
            m_beatTick = true;
            m_beatStrength = clamp01(latest.es_beat_strength);
            m_downbeatTick = latest.es_downbeat_tick;
        }
    }
    }

    // Integrate phase at render cadence using sample-index timebase.
    int64_t ds = static_cast<int64_t>(render_now.sample_index) - static_cast<int64_t>(m_lastTickT.sample_index);
    if (ds < 0) ds = 0;
    const float dt_s = static_cast<float>(ds) / static_cast<float>(render_now.sample_rate_hz ? render_now.sample_rate_hz : 12800);
    m_lastTickT = render_now;

    const float bps = m_bpm / 60.0f;
    float phaseAdvance = dt_s * bps;
    m_phase01 += phaseAdvance;

    if (m_phase01 >= 1.0f) {
        m_phase01 -= std::floor(m_phase01);
        m_beatTick = true;
        m_beatStrength = m_externalSync ? 1.0f : clamp01(latest.es_beat_strength);
        m_beatInBar = static_cast<uint8_t>((m_beatInBar + 1) % 4);
        m_downbeatTick = (m_beatInBar == 0);
    }

    // Beat strength decay (simple exponential)
    const float tau = 0.30f;
    const float decay = (tau > 0.0f) ? std::exp(-dt_s / tau) : 0.0f;
    m_beatStrength *= decay;

    // Populate snapshot fields used by effects.
    m_snap.bpm_smoothed = m_bpm;
    m_snap.tempo_confidence = m_conf;
    m_snap.beat_phase01 = clamp01(m_phase01);
    m_snap.beat_tick = m_beatTick;
    m_snap.downbeat_tick = m_downbeatTick;
    m_snap.beat_in_bar = m_beatInBar;
    m_snap.beat_strength = clamp01(m_beatStrength);
}

} // namespace lightwaveos::audio::esv11

#endif
