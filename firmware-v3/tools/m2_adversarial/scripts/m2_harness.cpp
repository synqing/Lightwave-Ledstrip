/**
 * @file m2_harness.cpp
 * @brief M2 ESV11 32 kHz adversarial harness — per-frame tempo trajectory logger.
 *
 * Standalone runner that drives the vendored ESV11 pipeline against an
 * adversarial WAV and emits per-frame `tempoConfidence` and `tempoPhase`
 * (radians, normalised to [0,1]) as a JSON array on stdout.
 *
 * NOT a unit test — does not link against Unity, does not register with
 * platformio.ini. Built directly by `m2_run.py` via clang/g++.
 *
 * Usage:
 *   ./m2_harness <wav_path> [max_seconds=60]
 *
 * Output (stdout): single JSON object
 *   {
 *     "wav": "<path>",
 *     "sample_rate": 32000,
 *     "chunk_size": 128,
 *     "reference_fps": 100,
 *     "frames": [
 *       {"t_s": 0.004, "conf": 0.012, "phase": 0.31, "vu": 0.001, "silence": true,
 *        "top_bpm": 60, "top_phase_norm": 0.31, "beat": -0.05},
 *       ...
 *     ],
 *     "summary": {
 *       "final_bpm": 100, "final_conf": 0.42, "frame_count": 7500,
 *       "beat_tick_count": 12
 *     }
 *   }
 *
 * A "beat tick" is detected whenever the dominant tempo bin's beat value
 * (sin(phase)) crosses zero from negative to positive — i.e. the same
 * downbeat detection used by the live runtime via tempi[bin].beat.
 *
 * British English in comments.
 */

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Vendored ESV11 (header-only globals live here)
#include "audio/backends/esv11/vendor/EsV11Shim.h"
#include "audio/backends/esv11/vendor/EsV11Buffers.h"
#include "audio/backends/esv11/vendor/global_defines.h"
#include "audio/backends/esv11/vendor/microphone.h"
#include "audio/backends/esv11/vendor/goertzel.h"
#include "audio/backends/esv11/vendor/vu.h"
#include "audio/backends/esv11/vendor/tempo.h"
#include "audio/backends/esv11/vendor/utilities_min.h"

// ============================================================================
// WAV loader (16-bit PCM, mono or stereo to mono)
// ============================================================================

struct WavData {
    std::vector<int16_t> samples;
    uint32_t sampleRate = 0;
};

static bool loadWav(const char* path, WavData& out) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "loadWav: cannot open %s\n", path);
        return false;
    }
    char riff[4]; fread(riff, 1, 4, f);
    if (memcmp(riff, "RIFF", 4) != 0) { fclose(f); return false; }
    uint32_t fileSize; fread(&fileSize, 4, 1, f); (void)fileSize;
    char wave[4]; fread(wave, 1, 4, f);
    if (memcmp(wave, "WAVE", 4) != 0) { fclose(f); return false; }

    uint32_t sr = 0;
    uint16_t channels = 0, bitsPerSample = 0;
    while (!feof(f)) {
        char chunkId[4];
        if (fread(chunkId, 1, 4, f) < 4) break;
        uint32_t chunkSize = 0; fread(&chunkSize, 4, 1, f);
        if (memcmp(chunkId, "fmt ", 4) == 0) {
            uint16_t audioFmt; fread(&audioFmt, 2, 1, f); (void)audioFmt;
            fread(&channels, 2, 1, f); fread(&sr, 4, 1, f);
            uint32_t byteRate; fread(&byteRate, 4, 1, f); (void)byteRate;
            uint16_t blockAlign; fread(&blockAlign, 2, 1, f); (void)blockAlign;
            fread(&bitsPerSample, 2, 1, f);
            if (chunkSize > 16) fseek(f, static_cast<long>(chunkSize - 16), SEEK_CUR);
        } else if (memcmp(chunkId, "data", 4) == 0) {
            size_t numSamples = chunkSize / (bitsPerSample / 8) / (channels ? channels : 1);
            out.samples.resize(numSamples);
            if (bitsPerSample == 16 && channels == 1) {
                fread(out.samples.data(), 2, numSamples, f);
            } else if (bitsPerSample == 16) {
                for (size_t i = 0; i < numSamples; i++) {
                    int32_t sum = 0;
                    for (uint16_t ch = 0; ch < channels; ch++) {
                        int16_t s; fread(&s, 2, 1, f); sum += s;
                    }
                    out.samples[i] = static_cast<int16_t>(sum / channels);
                }
            } else {
                fclose(f); return false;  // unsupported bit depth
            }
            out.sampleRate = sr;
            fclose(f);
            return true;
        } else {
            fseek(f, static_cast<long>(chunkSize), SEEK_CUR);
        }
    }
    fclose(f); return false;
}

// ============================================================================
// ESV11 init (mirrors test_esv11_real_music.cpp:es_init)
// ============================================================================

static void esv11_reset_state() {
    bool ok = esv11_init_buffers();
    if (!ok) {
        fprintf(stderr, "FATAL: esv11_init_buffers() failed\n");
        std::exit(1);
    }
    esv11_set_time(0, 0);

    dc_blocker_x_prev = 0.0f;
    dc_blocker_y_prev = 0.0f;
    memset(sample_history, 0, SAMPLE_HISTORY_LENGTH * sizeof(float));

    memset(spectrogram, 0, sizeof(spectrogram));
    memset(spectrogram_smooth, 0, sizeof(spectrogram_smooth));
    memset(spectrogram_average, 0, 12 * NUM_FREQS * sizeof(float));
    spectrogram_average_index = 0;
    memset(chromagram, 0, sizeof(chromagram));

    silence_detected = true;
    silence_level = 1.0f;
    memset(novelty_curve, 0, NOVELTY_HISTORY_LENGTH * sizeof(float));
    memset(novelty_curve_normalized, 0, NOVELTY_HISTORY_LENGTH * sizeof(float));
    memset(vu_curve, 0, NOVELTY_HISTORY_LENGTH * sizeof(float));
    memset(vu_curve_normalized, 0, NOVELTY_HISTORY_LENGTH * sizeof(float));
    memset(tempi_smooth, 0, sizeof(tempi_smooth));
    memset(tempi, 0, NUM_TEMPI * sizeof(tempo));
    tempi_power_sum = 0.0f;
    tempo_confidence = 0.0f;

    init_vu();
    init_window_lookup();
    init_goertzel_constants();
    init_tempo_goertzel_constants();
}

// ============================================================================
// Per-frame trajectory dump
// ============================================================================

struct FrameRow {
    double t_s;
    float conf;
    float phase_rad;     // dominant-bin phase in [-pi, pi]
    float vu;
    bool silence;
    int top_bin;
    float top_bpm;
    float beat;          // sin(phase) for dominant bin
};

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <wav_path> [max_seconds=60]\n", argv[0]);
        return 1;
    }
    const char* wavPath = argv[1];
    const float maxSeconds = (argc >= 3) ? static_cast<float>(atof(argv[2])) : 60.0f;

    WavData wav;
    if (!loadWav(wavPath, wav)) {
        fprintf(stderr, "loadWav failed: %s\n", wavPath);
        return 1;
    }
    if (wav.sampleRate != SAMPLE_RATE) {
        fprintf(stderr, "WARN: WAV sample_rate=%u, harness compiled for %d. "
                        "Re-encode the WAV (e.g. with sox) before running.\n",
                wav.sampleRate, SAMPLE_RATE);
    }

    esv11_reset_state();

    const uint32_t maxSamples = static_cast<uint32_t>(
        fminf(static_cast<float>(wav.samples.size()),
              maxSeconds * static_cast<float>(SAMPLE_RATE)));

    std::vector<FrameRow> rows;
    rows.reserve(maxSamples / CHUNK_SIZE + 4);

    uint64_t chunkIdx = 0;
    uint64_t lastGpuTickUs = 0;
    int beatTickCount = 0;
    float prevBeat = 0.0f;
    int prevTopBin = -1;

    for (uint32_t pos = 0; pos + CHUNK_SIZE <= maxSamples; pos += CHUNK_SIZE) {
        const uint64_t nowUs = chunkIdx * (1000000ULL * CHUNK_SIZE / SAMPLE_RATE);
        const uint32_t nowMs = static_cast<uint32_t>(nowUs / 1000ULL);
        esv11_set_time(nowUs, nowMs);

        float newSamples[CHUNK_SIZE];
        for (uint16_t i = 0; i < CHUNK_SIZE; i++) {
            newSamples[i] = static_cast<float>(wav.samples[pos + i]) / 32768.0f;
        }
        shift_and_copy_arrays(sample_history, SAMPLE_HISTORY_LENGTH, newSamples, CHUNK_SIZE);

        calculate_magnitudes();
        get_chromagram();
        run_vu();
        update_tempo();

        if (lastGpuTickUs == 0) lastGpuTickUs = nowUs;
        const uint64_t elapsedUs = nowUs - lastGpuTickUs;
        const float idealUsInterval = 1000000.0f / static_cast<float>(REFERENCE_FPS);
        const float delta = static_cast<float>(elapsedUs) / idealUsInterval;
        lastGpuTickUs = nowUs;

        update_novelty();
        update_tempi_phase(delta);

        // Dominant tempo bin via the same selector as live runtime
        const uint16_t topBin = esv11_pick_top_tempo_bin_octave_aware();
        const float beat = (topBin < NUM_TEMPI) ? tempi[topBin].beat : 0.0f;
        const float phase = (topBin < NUM_TEMPI) ? tempi[topBin].phase : 0.0f;

        // Beat tick = zero-crossing negative -> positive of `beat`
        // Do NOT count ticks across a top-bin switch (phase discontinuity).
        if (prevTopBin == static_cast<int>(topBin)
            && prevBeat < 0.0f && beat >= 0.0f) {
            beatTickCount++;
        }
        prevBeat = beat;
        prevTopBin = static_cast<int>(topBin);

        FrameRow row;
        row.t_s     = static_cast<double>(nowUs) / 1.0e6;
        row.conf    = tempo_confidence;
        row.phase_rad = phase;
        row.vu      = vu_level;
        row.silence = silence_detected;
        row.top_bin = static_cast<int>(topBin);
        row.top_bpm = static_cast<float>(TEMPO_LOW) + static_cast<float>(topBin);
        row.beat    = beat;
        rows.push_back(row);

        chunkIdx++;
    }

    // Emit JSON to stdout
    printf("{\n");
    printf("  \"wav\": \"%s\",\n", wavPath);
    printf("  \"sample_rate\": %d,\n", SAMPLE_RATE);
    printf("  \"chunk_size\": %d,\n", CHUNK_SIZE);
    printf("  \"reference_fps\": %d,\n", REFERENCE_FPS);
    printf("  \"frames\": [\n");
    for (size_t i = 0; i < rows.size(); ++i) {
        const FrameRow& r = rows[i];
        // phase normalised to [0,1) for downstream coherence checks
        const float phase_norm = (r.phase_rad + static_cast<float>(M_PI))
                               / (2.0f * static_cast<float>(M_PI));
        printf("    {\"t_s\":%.4f,\"conf\":%.4f,\"phase_rad\":%.4f,\"phase_norm\":%.4f,"
               "\"vu\":%.4f,\"silence\":%s,\"top_bin\":%d,\"top_bpm\":%.1f,\"beat\":%.4f}%s\n",
               r.t_s, r.conf, r.phase_rad, phase_norm, r.vu,
               r.silence ? "true" : "false", r.top_bin, r.top_bpm, r.beat,
               (i + 1 < rows.size()) ? "," : "");
    }
    printf("  ],\n");
    printf("  \"summary\": {\n");
    printf("    \"frame_count\": %zu,\n", rows.size());
    printf("    \"beat_tick_count\": %d,\n", beatTickCount);
    if (!rows.empty()) {
        printf("    \"final_bpm\": %.1f,\n", rows.back().top_bpm);
        printf("    \"final_conf\": %.4f\n", rows.back().conf);
    } else {
        printf("    \"final_bpm\": 0.0,\n    \"final_conf\": 0.0\n");
    }
    printf("  }\n");
    printf("}\n");
    return 0;
}
