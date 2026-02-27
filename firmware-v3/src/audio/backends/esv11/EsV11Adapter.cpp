/**
 * @file EsV11Adapter.cpp
 */

#include "EsV11Adapter.h"

#if FEATURE_AUDIO_SYNC && FEATURE_AUDIO_BACKEND_ESV11

#include <algorithm>
#include <cmath>
#include <cstring>

#include "config/audio_config.h"
#include "audio/AudioMath.h"

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

    // Sensory Bridge parity (waveform + note-chromagram)
    std::memset(m_sbWaveformHistory, 0, sizeof(m_sbWaveformHistory));
    m_sbWaveformHistoryIndex = 0;
    m_sbMaxWaveformValFollower = 750.0f;
    m_sbWaveformPeakScaled = 0.0f;
    m_sbWaveformPeakScaledLast = 0.0f;
    std::memset(m_sbNoteChroma, 0, sizeof(m_sbNoteChroma));
    m_sbChromaMaxVal = 0.0001f;
}

void EsV11Adapter::buildFrame(lightwaveos::audio::ControlBusFrame& out,
                              const EsV11Outputs& es,
                              uint32_t hopSeq)
{
    out = lightwaveos::audio::ControlBusFrame{};

    // AudioTime uses sample_index as the monotonic clock.
    out.t = lightwaveos::audio::AudioTime(es.sample_index, audio::SAMPLE_RATE, es.now_us);
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

    // Raw ES signals (for reference show parity)
    out.es_vu_level_raw = clamp01(es.vu_level);

    float rawBins[lightwaveos::audio::ControlBusFrame::BINS_64_COUNT];

    // bins64: clamp raw ES spectrogram
    for (uint8_t i = 0; i < lightwaveos::audio::ControlBusFrame::BINS_64_COUNT; ++i) {
        rawBins[i] = clamp01(es.spectrogram_smooth[i]);
        out.es_bins64_raw[i] = rawBins[i];
    }

    // bins64Adaptive: ES-style autorange follower (simple max follower)
    float currentMax = 0.00001f;
    for (uint8_t i = 0; i < lightwaveos::audio::ControlBusFrame::BINS_64_COUNT; ++i) {
        currentMax = std::max(currentMax, rawBins[i]);
    }
    // Decay + rise behaviour (retuned for current hop rate)
    static const float decay = 1.0f - audio::retunedAlpha(1.0f - 0.995f, 50.0f, audio::HOP_RATE_HZ);
    static const float rise = audio::retunedAlpha(0.25f, 50.0f, audio::HOP_RATE_HZ);
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

    // Aggregate 8 bands from 64 bins (mean of each 8-bin block).
    // Mapping: band 0 = bins 0–7 (sub-bass), band 1 = 8–15 (bass), band 2–4 = mid, band 5–7 = treble.
    // ctx.audio.bass() = avg(bands[0],bands[1]), mid() = avg(2,3,4), treble() = avg(5,6,7).
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
        out.es_chroma_raw[i] = rawChroma[i];
    }

    // Similar autorange follower for chroma magnitudes, gated by activity.
    static const float chromaDecay = 1.0f - audio::retunedAlpha(1.0f - 0.995f, 50.0f, audio::HOP_RATE_HZ);
    static const float chromaRise = audio::retunedAlpha(0.35f, 50.0f, audio::HOP_RATE_HZ);
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
    static const float heavy_alpha = audio::retunedAlpha(0.05f, 50.0f, audio::HOP_RATE_HZ);
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

    // --------------------------------------------------------------------
    // Sensory Bridge parity side-car (3.1.0 waveform)
    // --------------------------------------------------------------------
    // Store waveform into history ring buffer (4-frame history).
    for (uint8_t i = 0; i < lightwaveos::audio::CONTROLBUS_WAVEFORM_N; ++i) {
        int16_t sample = es.waveform[i];
        out.sb_waveform[i] = sample;
        m_sbWaveformHistory[m_sbWaveformHistoryIndex][i] = sample;
    }
    m_sbWaveformHistoryIndex++;
    if (m_sbWaveformHistoryIndex >= SB_WAVEFORM_HISTORY) {
        m_sbWaveformHistoryIndex = 0;
    }

    // Peak follower (sweet spot scaling; matches Sensory Bridge 3.1.0).
    float maxWaveformValRaw = 0.0f;
    for (uint8_t i = 0; i < lightwaveos::audio::CONTROLBUS_WAVEFORM_N; ++i) {
        int16_t sample = es.waveform[i];
        int16_t absSample = (sample < 0) ? -sample : sample;
        if ((float)absSample > maxWaveformValRaw) {
            maxWaveformValRaw = (float)absSample;
        }
    }

    float maxWaveformVal = maxWaveformValRaw - 750.0f;  // Sweet spot min level
    if (maxWaveformVal < 0.0f) maxWaveformVal = 0.0f;

    // SB peak follower alphas (retuned for current hop rate)
    static const float kSbPeakAttack = audio::retunedAlpha(0.25f, 50.0f, audio::HOP_RATE_HZ);
    static const float kSbPeakRelease = audio::retunedAlpha(0.005f, 50.0f, audio::HOP_RATE_HZ);
    if (maxWaveformVal > m_sbMaxWaveformValFollower) {
        float delta = maxWaveformVal - m_sbMaxWaveformValFollower;
        m_sbMaxWaveformValFollower += delta * kSbPeakAttack;
    } else if (maxWaveformVal < m_sbMaxWaveformValFollower) {
        float delta = m_sbMaxWaveformValFollower - maxWaveformVal;
        m_sbMaxWaveformValFollower -= delta * kSbPeakRelease;
        if (m_sbMaxWaveformValFollower < 750.0f) {
            m_sbMaxWaveformValFollower = 750.0f;
        }
    }

    float waveformPeakScaledRaw = 0.0f;
    if (m_sbMaxWaveformValFollower > 0.0f) {
        waveformPeakScaledRaw = maxWaveformVal / m_sbMaxWaveformValFollower;
    }
    static const float kSbScaledAttack = audio::retunedAlpha(0.25f, 50.0f, audio::HOP_RATE_HZ);
    static const float kSbScaledRelease = audio::retunedAlpha(0.25f, 50.0f, audio::HOP_RATE_HZ);
    if (waveformPeakScaledRaw > m_sbWaveformPeakScaled) {
        float delta = waveformPeakScaledRaw - m_sbWaveformPeakScaled;
        m_sbWaveformPeakScaled += delta * kSbScaledAttack;
    } else if (waveformPeakScaledRaw < m_sbWaveformPeakScaled) {
        float delta = m_sbWaveformPeakScaled - waveformPeakScaledRaw;
        m_sbWaveformPeakScaled -= delta * kSbScaledRelease;
    }

    // 3.1.0 waveform peak follower used by waveform/VU modes.
    static const float kSbLastAlpha = audio::retunedAlpha(0.05f, 50.0f, audio::HOP_RATE_HZ);
    m_sbWaveformPeakScaledLast =
        (m_sbWaveformPeakScaled * kSbLastAlpha) + (m_sbWaveformPeakScaledLast * (1.0f - kSbLastAlpha));
    out.sb_waveform_peak_scaled = m_sbWaveformPeakScaled;
    out.sb_waveform_peak_scaled_last = m_sbWaveformPeakScaledLast;

    // 3.1.0 note chromagram derived from the 64-bin note spectrogram.
    m_sbChromaMaxVal = 0.0f;
    for (uint8_t i = 0; i < lightwaveos::audio::CONTROLBUS_NUM_CHROMA; ++i) {
        m_sbNoteChroma[i] = 0.0f;
    }
    for (uint8_t octave = 0; octave < 6; ++octave) {
        for (uint8_t note = 0; note < lightwaveos::audio::CONTROLBUS_NUM_CHROMA; ++note) {
            uint16_t noteIndex = static_cast<uint16_t>(12 * octave + note);
            if (noteIndex < lightwaveos::audio::ControlBusFrame::BINS_64_COUNT) {
                float val = out.bins64Adaptive[noteIndex];
                m_sbNoteChroma[note] += val;
                if (m_sbNoteChroma[note] > 1.0f) {
                    m_sbNoteChroma[note] = 1.0f;
                }
                if (m_sbNoteChroma[note] > m_sbChromaMaxVal) {
                    m_sbChromaMaxVal = m_sbNoteChroma[note];
                }
            }
        }
    }
    if (m_sbChromaMaxVal < 0.0001f) {
        m_sbChromaMaxVal = 0.0001f;
    }
    std::memcpy(out.sb_note_chromagram, m_sbNoteChroma, sizeof(out.sb_note_chromagram));
    out.sb_chromagram_max_val = m_sbChromaMaxVal;

    // ----------------------------------------------------------------
    // Lightweight onset detection from 64-bin spectrum
    // Snare: bins 5-10 (~150-300 Hz), Hihat: bins 50-60 (~6-12 kHz)
    // Onset = current energy exceeds previous by threshold
    // ----------------------------------------------------------------
    {
        float snareSum = 0.0f;
        for (uint8_t i = 5; i <= 10 && i < lightwaveos::audio::ControlBusFrame::BINS_64_COUNT; ++i) {
            snareSum += out.bins64[i];
        }
        out.snareEnergy = clamp01(snareSum / 6.0f);

        float hihatSum = 0.0f;
        for (uint8_t i = 50; i <= 60 && i < lightwaveos::audio::ControlBusFrame::BINS_64_COUNT; ++i) {
            hihatSum += out.bins64[i];
        }
        out.hihatEnergy = clamp01(hihatSum / 11.0f);

        // One-frame onset pulse: trigger when energy jumps above previous + threshold
        constexpr float ONSET_THRESHOLD = 0.08f;
        out.snareTrigger = (out.snareEnergy > m_prevSnareEnergy + ONSET_THRESHOLD) && (out.snareEnergy > 0.10f);
        out.hihatTrigger = (out.hihatEnergy > m_prevHihatEnergy + ONSET_THRESHOLD) && (out.hihatEnergy > 0.05f);

        m_prevSnareEnergy = out.snareEnergy;
        m_prevHihatEnergy = out.hihatEnergy;
    }

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
