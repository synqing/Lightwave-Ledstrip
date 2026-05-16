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

namespace {
const lightwaveos::audio::ControlBusFrame kDefaultControlBusFrame{};
}

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static inline uint16_t q15(float x) {
    return static_cast<uint16_t>(clamp01(x) * 65535.0f + 0.5f);
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
#if FEATURE_AUDIO_HF_SEMANTICS
    m_hfEnergy = 0.0f;
    m_airEnergy = 0.0f;
    m_cymbalSustain = 0.0f;
    m_prevHfRaw = 0.0f;
    m_prevBrightness = 0.0f;
    m_hatEventAgeMs = 65535;
#endif
}

void EsV11Adapter::buildFrame(lightwaveos::audio::ControlBusFrame& out,
                              const EsV11Outputs& es,
                              uint32_t hopSeq)
{
    out = kDefaultControlBusFrame;

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
    // AGC enable gate: only disable normalisation during electrical silence
    // (microphone self-noise, no acoustic input). The follower floors (0.05 bins,
    // 0.08 chroma) cap maximum gain, so keeping AGC active at low levels is safe.
    // Previous value of 0.01 was too high — disabled AGC during quiet musical
    // passages, causing chroma-coloured effects to go invisible.
    constexpr float AGC_NOISE_FLOOR = 0.001f;
    const bool isActive = es.vu_level >= AGC_NOISE_FLOOR;

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
    // Sensory Bridge parity side-car (3.1.0 waveform) — Phase 5C resampler
    //
    // K1 captures audio at 32 kHz; OG SB 3.1.0 captured at 12.2 kHz, giving
    // 128 samples per chunk = 10.5 ms time span. K1's native 4 ms / 128
    // sample window is sub-period for any musical content < 250 Hz —
    // bass-frequency oscillations don't fit in one chunk, killing SB-style
    // waveform motion (see audit 2026-04-30).
    //
    // Resample sb_waveform[128] from a 3-chunk raw history window at
    // stride 3 → 12 ms total span (vs SB's 10.5 ms; close enough for PoC).
    // Each output position represents an audio sample at a different time
    // offset from "now", restoring the bass-cycle phase oscillations that
    // drive SB-style waveform motion across the LEDs.
    //
    // out.waveform[128] is preserved unchanged as the native 32 kHz / 4 ms
    // capture for any K1-native effect that needs raw fast audio.
    // --------------------------------------------------------------------

    // 1. Append current K1-native chunk into the raw 4-chunk history ring.
    constexpr uint8_t kSbN = lightwaveos::audio::CONTROLBUS_WAVEFORM_N;  // 128
    for (uint8_t i = 0; i < kSbN; ++i) {
        m_sbWaveformHistory[m_sbWaveformHistoryIndex][i] = es.waveform[i];
    }
    const uint8_t newestChunkIdx = m_sbWaveformHistoryIndex;
    m_sbWaveformHistoryIndex++;
    if (m_sbWaveformHistoryIndex >= SB_WAVEFORM_HISTORY) {
        m_sbWaveformHistoryIndex = 0;
    }

    // 2. Resample sb_waveform[128] from the last 382 raw samples (≈ 12 ms
    //    at 32 kHz, vs SB's 10.5 ms) using stride 3.
    //
    //    Concat ordering of the 4-slot history ring, oldest→newest:
    //      chunkLocalIdx 0..3  ↔  age 3..0 (chunks back from newest)
    //
    //    Output index i reads concat position p = 130 + i*3.
    //    p ∈ [130..511]; i=0 → oldest visible sample; i=127 → newest sample.
    //    chunkLocalIdx = p / 128 (∈ [1..3] over this range; the oldest
    //    chunk at age 3 is unread by stride-3 from start 130).
    constexpr int kSbStride = 3;
    constexpr int kSbStartConcatPos = 130;  // 511 - 127*3 (ends at newest)

    for (uint8_t i = 0; i < kSbN; ++i) {
        const int concatPos = kSbStartConcatPos + static_cast<int>(i) * kSbStride;
        const int chunkLocalIdx = concatPos / kSbN;     // 1..3 in this stride
        const int sampleInChunk = concatPos % kSbN;
        const int age = (SB_WAVEFORM_HISTORY - 1) - chunkLocalIdx;  // 0..2
        const int ringIdx =
            (static_cast<int>(newestChunkIdx) - age + SB_WAVEFORM_HISTORY) %
            SB_WAVEFORM_HISTORY;
        out.sb_waveform[i] = m_sbWaveformHistory[ringIdx][sampleInChunk];
    }

    // Peak follower (sweet spot scaling; matches Sensory Bridge 3.1.0).
    // Compute peak from the SB-compatible window we just produced — keeps
    // sb_waveform[] shape and sb_waveform_peak_scaled internally consistent.
    float maxWaveformValRaw = 0.0f;
    for (uint8_t i = 0; i < kSbN; ++i) {
        int16_t sample = out.sb_waveform[i];
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

        // Old crude onset triggers DISABLED — Path B band-ratio detector in
        // AudioActor is the sole live trigger source.  Energy values are still
        // computed above for effects that read snareEnergy/hihatEnergy directly.
        // Do NOT re-enable — these pollute traces with ONSET_KICK/SNARE/HIHAT
        // alongside the authoritative BR_ events.
        out.snareTrigger = false;
        out.hihatTrigger = false;

        m_prevSnareEnergy = out.snareEnergy;
        m_prevHihatEnergy = out.hihatEnergy;
    }

#if FEATURE_AUDIO_HF_SEMANTICS
    // Tier 1 HF semantic fields. This uses the existing 64-bin substrate only:
    // no wider projections, no bins256 dependency, and no render-side work.
    {
        float hfSum = 0.0f;
        for (uint8_t i = 50; i < lightwaveos::audio::ControlBusFrame::BINS_64_COUNT; ++i) {
            hfSum += out.bins64Adaptive[i];
        }
        const float hfRaw = clamp01(hfSum / 14.0f);

        float airSum = 0.0f;
        for (uint8_t i = 58; i < lightwaveos::audio::ControlBusFrame::BINS_64_COUNT; ++i) {
            airSum += out.bins64Adaptive[i];
        }
        const float airRaw = clamp01(airSum / 6.0f);

        float weighted = 0.0f;
        float energy = 0.0f;
        for (uint8_t i = 0; i < lightwaveos::audio::ControlBusFrame::BINS_64_COUNT; ++i) {
            const float v = out.bins64Adaptive[i];
            weighted += v * static_cast<float>(i);
            energy += v;
        }
        const float brightness = (energy > 0.001f) ? clamp01(weighted / (energy * 63.0f)) : 0.0f;
        const float brightnessDelta = brightness - m_prevBrightness;
        const float hfFlux = clamp01((hfRaw - m_prevHfRaw) * 4.0f);

        static const float hfAttack = audio::retunedAlpha(0.35f, 50.0f, audio::HOP_RATE_HZ);
        static const float hfRelease = audio::retunedAlpha(0.08f, 50.0f, audio::HOP_RATE_HZ);
        m_hfEnergy += (hfRaw - m_hfEnergy) * ((hfRaw > m_hfEnergy) ? hfAttack : hfRelease);

        static const float airAttack = audio::retunedAlpha(0.12f, 50.0f, audio::HOP_RATE_HZ);
        static const float airRelease = audio::retunedAlpha(0.025f, 50.0f, audio::HOP_RATE_HZ);
        m_airEnergy += (airRaw - m_airEnergy) * ((airRaw > m_airEnergy) ? airAttack : airRelease);

        const float sustainTarget = clamp01((hfRaw * 0.65f) + (airRaw * 0.35f));
        static const float sustainAttack = audio::retunedAlpha(0.20f, 50.0f, audio::HOP_RATE_HZ);
        static const float sustainRelease = audio::retunedAlpha(0.015f, 50.0f, audio::HOP_RATE_HZ);
        m_cymbalSustain += (sustainTarget - m_cymbalSustain) *
                           ((sustainTarget > m_cymbalSustain) ? sustainAttack : sustainRelease);

        const float lowMid = clamp01((out.bands[0] + out.bands[1] + out.bands[2] +
                                      out.bands[3] + out.bands[4]) / 5.0f);
        const float hfRatio = clamp01(hfRaw / (lowMid + 0.08f));
        const bool refractoryDone = m_hatEventAgeMs >= 70;
        const bool hatLike = refractoryDone && hfFlux > 0.16f && hfRatio > 0.85f && hfRaw > 0.08f;
        const float hatStrength = hatLike ? clamp01((hfFlux - 0.16f) * 4.0f * hfRatio) : 0.0f;

        const uint32_t stepMs = static_cast<uint32_t>(1000.0f / audio::HOP_RATE_HZ + 0.5f);
        m_hatEventAgeMs = static_cast<uint16_t>(
            (static_cast<uint32_t>(m_hatEventAgeMs) + stepMs > 65535U)
                ? 65535U
                : (static_cast<uint32_t>(m_hatEventAgeMs) + stepMs));
        if (hatStrength > 0.0f) {
            m_hatEventAgeMs = 0;
        }

        out.hfEnergy = clamp01(m_hfEnergy);
        out.hfFlux = hfFlux;
        out.hatEvent.strength = q15(hatStrength);
        out.hatEvent.confidence = q15(hatStrength > 0.0f ? hfRatio : 0.0f);
        out.hatEvent.ageMs = m_hatEventAgeMs;
        out.hatEvent.flags = 0x02U | ((hatStrength > 0.0f) ? 0x01U : 0x00U);
        out.cymbalSustain = clamp01(m_cymbalSustain);
        out.airEnergy = clamp01(m_airEnergy);
        out.spectralBrightness = brightness;
        out.spectralBrightnessDelta = (brightnessDelta < -1.0f) ? -1.0f :
                                      ((brightnessDelta > 1.0f) ? 1.0f : brightnessDelta);

        m_prevHfRaw = hfRaw;
        m_prevBrightness = brightness;
    }
#endif

    // ES tempo extras (consumed by renderer beat clock)
    out.es_bpm = es.top_bpm;
    out.es_tempo_confidence = clamp01(es.tempo_confidence);
    out.es_beat_tick = es.beat_tick;
    out.es_beat_strength = clamp01(es.beat_strength);
    out.tempoWinnerBinValid = es.tempo_winner_bin_valid;
    out.tempoWinnerBin = es.tempo_winner_bin;

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
