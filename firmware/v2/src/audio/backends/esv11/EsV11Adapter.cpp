/**
 * @file EsV11Adapter.cpp
 */

#include "EsV11Adapter.h"

#if FEATURE_AUDIO_SYNC && FEATURE_AUDIO_BACKEND_ESV11

#include <algorithm>
#include <cmath>
#include <cstring>

namespace lightwaveos::audio::esv11 {

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

void EsV11Adapter::reset()
{
    m_binsMaxFollower = 0.1f;
    m_chromaMaxFollower = 0.2f;
    std::memset(m_heavyBands, 0, sizeof(m_heavyBands));
    std::memset(m_heavyChroma, 0, sizeof(m_heavyChroma));
    m_beatInBar = 0;
}

void EsV11Adapter::buildFrame(lightwaveos::audio::ControlBusFrame& out,
                              const EsV11Outputs& es,
                              uint32_t hopSeq)
{
    out = lightwaveos::audio::ControlBusFrame{};

    // AudioTime uses sample_index as the monotonic clock.
    out.t = lightwaveos::audio::AudioTime(es.sample_index, 12800, es.now_us);
    out.hop_seq = hopSeq;

    // Core energy / novelty proxy
    // ES vu_level tends to be a low-range linear energy; map to LWLS contract range
    // expected by existing effects (0..1, perceptually expanded).
    out.rms = clamp01(std::sqrt(std::max(0.0f, es.vu_level)) * 1.25f);
    out.flux = clamp01(es.novelty_norm_last);
    out.fast_rms = out.rms;
    out.fast_flux = out.flux;

    // When the ES backend is running on toolchains that align/scale I2S samples
    // differently, the spectrogram can end up with lower absolute magnitudes.
    //
    // LWLS effects generally expect bins64 to already be usable as a 0..1 signal
    // (e.g. sub-bass kick thresholds around 0.15..0.50). To preserve effect
    // compatibility, we apply a simple autorange follower when audio is active.
    constexpr float ACTIVE_VU_THRESHOLD = 0.01f; // Below this, treat as near-silence.
    const bool isActive = es.vu_level >= ACTIVE_VU_THRESHOLD;

    float rawBins[lightwaveos::audio::ControlBusFrame::BINS_64_COUNT];

    // bins64: clamp raw ES spectrogram
    for (uint8_t i = 0; i < lightwaveos::audio::ControlBusFrame::BINS_64_COUNT; ++i) {
        rawBins[i] = clamp01(es.spectrogram_smooth[i]);
    }

    // bins64Adaptive: ES-style autorange follower (simple max follower)
    float currentMax = 0.00001f;
    for (uint8_t i = 0; i < lightwaveos::audio::ControlBusFrame::BINS_64_COUNT; ++i) {
        currentMax = std::max(currentMax, rawBins[i]);
    }
    // Decay + rise behaviour
    const float decay = 0.995f;
    const float rise = 0.25f;
    const float floor = 0.05f;

    float decayed = m_binsMaxFollower * decay;
    if (currentMax > decayed) {
        float delta = currentMax - decayed;
        m_binsMaxFollower = decayed + delta * rise;
    } else {
        m_binsMaxFollower = decayed;
    }
    if (m_binsMaxFollower < floor) {
        m_binsMaxFollower = floor;
    }

    const float inv = isActive ? (1.0f / m_binsMaxFollower) : 1.0f;
    for (uint8_t i = 0; i < lightwaveos::audio::ControlBusFrame::BINS_64_COUNT; ++i) {
        const float v = clamp01(rawBins[i] * inv);
        out.bins64[i] = v;
        out.bins64Adaptive[i] = v;
    }

    // Aggregate 8 bands from 64 bins (mean of each 8-bin block)
    for (uint8_t band = 0; band < lightwaveos::audio::CONTROLBUS_NUM_BANDS; ++band) {
        const uint8_t start = static_cast<uint8_t>(band * 8);
        float sum = 0.0f;
        for (uint8_t i = 0; i < 8; ++i) {
            sum += out.bins64[start + i];
        }
        out.bands[band] = clamp01(sum / 8.0f);
    }

    // Chroma
    float rawChroma[lightwaveos::audio::CONTROLBUS_NUM_CHROMA];
    float chromaMax = 0.00001f;
    for (uint8_t i = 0; i < lightwaveos::audio::CONTROLBUS_NUM_CHROMA; ++i) {
        rawChroma[i] = clamp01(es.chromagram[i]);
        chromaMax = std::max(chromaMax, rawChroma[i]);
    }

    // Similar autorange follower for chroma magnitudes, gated by activity.
    const float chromaDecay = 0.995f;
    const float chromaRise = 0.35f;
    const float chromaFloor = 0.08f;
    float chromaDecayed = m_chromaMaxFollower * chromaDecay;
    if (chromaMax > chromaDecayed) {
        float delta = chromaMax - chromaDecayed;
        m_chromaMaxFollower = chromaDecayed + delta * chromaRise;
    } else {
        m_chromaMaxFollower = chromaDecayed;
    }
    if (m_chromaMaxFollower < chromaFloor) {
        m_chromaMaxFollower = chromaFloor;
    }
    const float chromaInv = isActive ? (1.0f / m_chromaMaxFollower) : 1.0f;
    for (uint8_t i = 0; i < lightwaveos::audio::CONTROLBUS_NUM_CHROMA; ++i) {
        out.chroma[i] = clamp01(rawChroma[i] * chromaInv);
    }

    // Heavy smoothing (slow envelope) purely within adapter
    const float heavy_alpha = 0.05f;
    for (uint8_t i = 0; i < lightwaveos::audio::CONTROLBUS_NUM_BANDS; ++i) {
        m_heavyBands[i] = (m_heavyBands[i] * (1.0f - heavy_alpha)) + (out.bands[i] * heavy_alpha);
        out.heavy_bands[i] = clamp01(m_heavyBands[i]);
    }
    for (uint8_t i = 0; i < lightwaveos::audio::CONTROLBUS_NUM_CHROMA; ++i) {
        m_heavyChroma[i] = (m_heavyChroma[i] * (1.0f - heavy_alpha)) + (out.chroma[i] * heavy_alpha);
        out.heavy_chroma[i] = clamp01(m_heavyChroma[i]);
    }

    // Waveform (already int16 in ES outputs)
    std::memcpy(out.waveform, es.waveform, sizeof(out.waveform));

    // ES tempo extras (consumed by renderer beat clock)
    out.es_bpm = es.top_bpm;
    out.es_tempo_confidence = clamp01(es.tempo_confidence);
    out.es_beat_tick = es.beat_tick;
    out.es_beat_strength = clamp01(es.beat_strength);

    // Phase conversion: ES phase in radians [-pi, pi] -> [0,1)
    float phase01 = (es.phase_radians + static_cast<float>(M_PI)) / (2.0f * static_cast<float>(M_PI));
    // Guard wrap
    phase01 -= std::floor(phase01);
    out.es_phase01_at_audio_t = clamp01(phase01);

    if (out.es_beat_tick) {
        m_beatInBar = static_cast<uint8_t>((m_beatInBar + 1) % 4);
    }
    out.es_beat_in_bar = m_beatInBar;
    out.es_downbeat_tick = out.es_beat_tick && (m_beatInBar == 0);
}

} // namespace lightwaveos::audio::esv11

#endif
