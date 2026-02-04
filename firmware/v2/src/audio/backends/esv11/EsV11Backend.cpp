/**
 * @file EsV11Backend.cpp
 */

#include "EsV11Backend.h"

#if FEATURE_AUDIO_SYNC && FEATURE_AUDIO_BACKEND_ESV11

#include <cmath>
#include <cstring>

#include "vendor/EsV11Shim.h"

// Vendor headers (frozen ES v1.1_320 sources)
#include "vendor/global_defines.h"
#include "vendor/microphone.h"
#include "vendor/goertzel.h"
#include "vendor/vu.h"
#include "vendor/tempo.h"

namespace lightwaveos::audio::esv11 {

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

bool EsV11Backend::init()
{
    // ES vendor initialisation ordering is preserved where practical.
    // NOTE: ES expects various tables to be initialised (window lookup, goertzel constants, tempo constants).
    init_window_lookup();
    init_goertzel_constants();
    init_tempo_goertzel_constants();
    init_vu();
    init_i2s_microphone();

    m_sampleIndex = 0;
    m_lastGpuTickUs = 0;
    m_lastTopTempoIndex = 0;
    m_lastTopPhaseInverted = false;
    m_beatInBar = 0;
    m_latest = EsV11Outputs{};
    return true;
}

void EsV11Backend::tickEsGpu(float delta)
{
    // ES render-domain cadence: novelty curve update + tempi phase advance.
    update_novelty();
    update_tempi_phase(delta);
}

bool EsV11Backend::readAndProcessChunk(uint64_t now_us)
{
    const uint32_t now_ms = static_cast<uint32_t>(now_us / 1000);
    esv11_set_time(now_us, now_ms);

    acquire_sample_chunk();   // reads CHUNK_SIZE=64 and updates sample_history
    m_sampleIndex += CHUNK_SIZE;

    // ES CPU-side DSP stages (mirrors cpu_core.h ordering)
    calculate_magnitudes();
    get_chromagram();
    run_vu();
    update_tempo();

    // ES GPU-side tick with ES-style delta scaling (REFERENCE_FPS, 100)
    if (m_lastGpuTickUs == 0) {
        m_lastGpuTickUs = now_us;
    }
    uint64_t elapsed_us = now_us - m_lastGpuTickUs;
    const float ideal_us_interval = 1000000.0f / static_cast<float>(REFERENCE_FPS);
    const float delta = static_cast<float>(elapsed_us) / ideal_us_interval;
    m_lastGpuTickUs = now_us;
    tickEsGpu(delta);

    refreshOutputs(now_us);
    return true;
}

void EsV11Backend::refreshOutputs(uint64_t now_us)
{
    EsV11Outputs out{};
    out.now_us = now_us;
    out.now_ms = static_cast<uint32_t>(now_us / 1000);
    out.sample_index = m_sampleIndex;

    // Spectrum (ES uses NUM_FREQS=64)
    for (uint16_t i = 0; i < 64; ++i) {
        out.spectrogram_smooth[i] = clamp01(spectrogram_smooth[i]);
    }

    // Chroma
    for (uint8_t i = 0; i < 12; ++i) {
        out.chromagram[i] = clamp01(chromagram[i]);
    }

    // VU + novelty proxy
    out.vu_level = clamp01(vu_level);
    out.novelty_norm_last = clamp01(novelty_curve_normalized[NOVELTY_HISTORY_LENGTH - 1]);

    // Tempo: pick top bin from ES smoothed tempi bank
    float top_mag = 0.0f;
    uint16_t top_i = 0;
    for (uint16_t i = 0; i < NUM_TEMPI; ++i) {
        const float m = tempi_smooth[i];
        if (m > top_mag) {
            top_mag = m;
            top_i = i;
        }
    }

    const float bpm = static_cast<float>(TEMPO_LOW + top_i);
    out.top_bpm = bpm;
    out.tempo_confidence = clamp01(tempo_confidence);
    out.phase_radians = tempi[top_i].phase;
    out.beat_strength = clamp01(top_mag);

    // Beat tick: detect ES wrap via phase_inverted toggle on selected bin.
    // If top bin changes, reset tracking to avoid false ticks.
    bool tick = false;
    const bool phaseInverted = tempi[top_i].phase_inverted;
    if (top_i == m_lastTopTempoIndex) {
        tick = (phaseInverted != m_lastTopPhaseInverted);
    } else {
        m_lastTopTempoIndex = top_i;
        m_lastTopPhaseInverted = phaseInverted;
        tick = false;
    }
    m_lastTopPhaseInverted = phaseInverted;
    out.beat_tick = tick;

    if (tick) {
        m_beatInBar = static_cast<uint8_t>((m_beatInBar + 1) % 4);
    }

    // Waveform: downsample latest 128 from ES float sample_history into int16.
    // ES sample_history is float -1..1.
    const uint16_t N = 128;
    const float* tail = &sample_history[SAMPLE_HISTORY_LENGTH - N];
    for (uint16_t i = 0; i < N; ++i) {
        float s = tail[i];
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        out.waveform[i] = static_cast<int16_t>(s * 32767.0f);
    }

    m_latest = out;
}

void EsV11Backend::getLatestOutputs(EsV11Outputs& out) const
{
    out = m_latest;
}

} // namespace lightwaveos::audio::esv11

#endif
