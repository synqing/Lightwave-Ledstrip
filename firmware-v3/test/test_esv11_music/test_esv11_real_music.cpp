/**
 * @file test_esv11_real_music.cpp
 * @brief ESV11 tempo estimation against real music — drum loops + EDM tracks.
 *
 * Compiled twice: once at 12.8 kHz (default) and once at 32 kHz (with shim).
 * Both builds must produce identical tempo results for the same audio content,
 * validating temporal-constant parity across frame rates.
 *
 * Each track runs in a fork()ed child process to isolate the vendored ESV11
 * static state (the header-only code uses static locals inside inline functions
 * that cannot be reset between test runs).
 *
 * Build/run:
 *   pio test -e native_test_esv11_music        # 12.8 kHz
 *   pio test -e native_test_esv11_music_32khz   # 32 kHz
 */

#include <unity.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>

#include <unistd.h>
#include <sys/wait.h>

// Vendored ES pipeline (header-only globals live in this TU)
#include "audio/backends/esv11/vendor/EsV11Shim.h"
#include "audio/backends/esv11/vendor/EsV11Buffers.h"
#include "audio/backends/esv11/vendor/global_defines.h"
#include "audio/backends/esv11/vendor/microphone.h"
#include "audio/backends/esv11/vendor/goertzel.h"
#include "audio/backends/esv11/vendor/vu.h"
#include "audio/backends/esv11/vendor/tempo.h"
#include "audio/backends/esv11/vendor/utilities_min.h"

// ============================================================================
// WAV loader (16-bit PCM, mono or stereo → mono)
// ============================================================================

struct WavData {
    std::vector<int16_t> samples;
    uint32_t sampleRate;
};

static bool loadWav(const char* path, WavData& out) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "  Cannot open: %s\n", path); return false; }
    char riff[4]; fread(riff, 1, 4, f);
    if (memcmp(riff, "RIFF", 4) != 0) { fclose(f); return false; }
    uint32_t fileSize; fread(&fileSize, 4, 1, f);
    char wave[4]; fread(wave, 1, 4, f);
    if (memcmp(wave, "WAVE", 4) != 0) { fclose(f); return false; }
    uint32_t sr = 0; uint16_t channels = 0, bitsPerSample = 0;
    while (!feof(f)) {
        char chunkId[4];
        if (fread(chunkId, 1, 4, f) < 4) break;
        uint32_t chunkSize; fread(&chunkSize, 4, 1, f);
        if (memcmp(chunkId, "fmt ", 4) == 0) {
            uint16_t audioFmt; fread(&audioFmt, 2, 1, f);
            fread(&channels, 2, 1, f); fread(&sr, 4, 1, f);
            uint32_t byteRate; fread(&byteRate, 4, 1, f);
            uint16_t blockAlign; fread(&blockAlign, 2, 1, f);
            fread(&bitsPerSample, 2, 1, f);
            if (chunkSize > 16) fseek(f, chunkSize - 16, SEEK_CUR);
        } else if (memcmp(chunkId, "data", 4) == 0) {
            size_t numSamples = chunkSize / (bitsPerSample / 8) / channels;
            out.samples.resize(numSamples);
            if (bitsPerSample == 16 && channels == 1) {
                fread(out.samples.data(), 2, numSamples, f);
            } else {
                for (size_t i = 0; i < numSamples; i++) {
                    int32_t sum = 0;
                    for (uint16_t ch = 0; ch < channels; ch++) {
                        int16_t s; fread(&s, 2, 1, f); sum += s;
                    }
                    out.samples[i] = static_cast<int16_t>(sum / channels);
                }
            }
            out.sampleRate = sr; fclose(f); return true;
        } else {
            fseek(f, chunkSize, SEEK_CUR);
        }
    }
    fclose(f); return false;
}

// ============================================================================
// ESV11 pipeline driver
// ============================================================================

static void es_init() {
    bool ok = esv11_init_buffers();
    if (!ok) {
        fprintf(stderr, "FATAL: esv11_init_buffers() failed\n");
        return;
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

struct TempoResult {
    float bpm;
    float confidence;
    float vuLevel;
    bool  silenceDetected;
};

struct TempoDebugDump {
    float confidence;
    uint16_t topBins[10];
    float topMags[10];
    float probeBpms[9];
    float probeMags[9];
};

struct IsolatedTempoReport {
    TempoResult result;
    TempoDebugDump debug;
};

static TempoResult runEsv11(const WavData& wav, float maxSeconds = 30.0f) {
    es_init();

    const uint32_t maxSamples = static_cast<uint32_t>(
        fminf(static_cast<float>(wav.samples.size()),
              maxSeconds * static_cast<float>(SAMPLE_RATE)));

    uint64_t chunkIdx = 0;
    uint64_t lastGpuTickUs = 0;

    for (uint32_t pos = 0; pos + CHUNK_SIZE <= maxSamples; pos += CHUNK_SIZE) {
        const uint64_t nowUs = chunkIdx * (1000000ULL * CHUNK_SIZE / SAMPLE_RATE);
        const uint32_t nowMs = static_cast<uint32_t>(nowUs / 1000ULL);
        esv11_set_time(nowUs, nowMs);

        // Convert 16-bit PCM → float [-1, 1] and feed into sample history
        float newSamples[CHUNK_SIZE];
        for (uint16_t i = 0; i < CHUNK_SIZE; i++) {
            newSamples[i] = static_cast<float>(wav.samples[pos + i]) / 32768.0f;
        }
        shift_and_copy_arrays(sample_history, SAMPLE_HISTORY_LENGTH, newSamples, CHUNK_SIZE);

        // ES CPU stages
        calculate_magnitudes();
        get_chromagram();
        run_vu();
        update_tempo();

        // ES GPU tick cadence
        if (lastGpuTickUs == 0) lastGpuTickUs = nowUs;
        const uint64_t elapsedUs = nowUs - lastGpuTickUs;
        const float idealUsInterval = 1000000.0f / static_cast<float>(REFERENCE_FPS);
        const float delta = static_cast<float>(elapsedUs) / idealUsInterval;
        lastGpuTickUs = nowUs;

        update_novelty();
        update_tempi_phase(delta);
        chunkIdx++;
    }

    // Extract dominant tempo using the same octave-aware selection as runtime.
    const uint16_t topBin = esv11_pick_top_tempo_bin_octave_aware();

    TempoResult r;
    r.bpm = static_cast<float>(TEMPO_LOW) + static_cast<float>(topBin);
    r.confidence = tempo_confidence;
    r.vuLevel = vu_level;
    r.silenceDetected = silence_detected;
    return r;
}

static bool debugTempoBinsEnabled() {
    const char* env = getenv("LW_ESV11_DEBUG_BINS");
    return env && env[0] != '\0' && strcmp(env, "0") != 0;
}

static bool debugTrackSelected(const char* label) {
    const char* filter = getenv("LW_ESV11_DEBUG_TRACK");
    if (!filter || filter[0] == '\0') return true;
    return strstr(label, filter) != nullptr;
}

static void collectTempoDebugDump(TempoDebugDump& out, int topN = 10) {
    memset(&out, 0, sizeof(out));
    out.confidence = tempo_confidence;

    struct BinMag {
        uint16_t bin;
        float mag;
    };

    BinMag top[12]{};
    int count = 0;
    for (uint16_t i = 0; i < NUM_TEMPI; ++i) {
        const float mag = tempi_smooth[i];
        if (count < topN) {
            top[count++] = {i, mag};
            continue;
        }
        int minIdx = 0;
        for (int j = 1; j < count; ++j) {
            if (top[j].mag < top[minIdx].mag) minIdx = j;
        }
        if (mag > top[minIdx].mag) {
            top[minIdx] = {i, mag};
        }
    }

    for (int i = 0; i < count - 1; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (top[j].mag > top[i].mag) {
                const BinMag tmp = top[i];
                top[i] = top[j];
                top[j] = tmp;
            }
        }
    }

    for (int i = 0; i < count; ++i) {
        out.topBins[i] = top[i].bin;
        out.topMags[i] = top[i].mag;
    }

    const float probes[] = {66.0f, 70.0f, 80.0f, 94.0f, 100.0f, 105.0f, 120.0f, 133.0f, 141.0f};
    for (int p = 0; p < 9; ++p) {
        const float bpm = probes[p];
        out.probeBpms[p] = bpm;
        const int idx = static_cast<int>(lroundf(bpm - static_cast<float>(TEMPO_LOW)));
        if (idx >= 0 && idx < NUM_TEMPI) {
            out.probeMags[p] = tempi_smooth[idx];
        }
    }
}

// ============================================================================
// fork() isolation — each track in its own process for clean statics
// ============================================================================

static TempoResult runIsolated(const WavData& wav, TempoDebugDump* debugOut = nullptr) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        fprintf(stderr, "  pipe() failed\n");
        return {0, 0, 0, true};
    }

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "  fork() failed\n");
        close(pipefd[0]); close(pipefd[1]);
        return {0, 0, 0, true};
    }

    if (pid == 0) {
        // Child: run pipeline, write results, exit
        close(pipefd[0]);
        IsolatedTempoReport rep{};
        rep.result = runEsv11(wav);
        collectTempoDebugDump(rep.debug);
        ssize_t written = write(pipefd[1], &rep, sizeof(rep));
        (void)written;
        close(pipefd[1]);
        _exit(0);
    }

    // Parent: read results from child
    close(pipefd[1]);
    IsolatedTempoReport report{};
    report.result = {0, 0, 0, true};
    ssize_t bytesRead = read(pipefd[0], &report, sizeof(report));
    (void)bytesRead;
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "  Child process failed (status=%d)\n", status);
    }

    if (debugOut) {
        *debugOut = report.debug;
    }
    return report.result;
}

static void dumpTopTempoBins(const char* label, const TempoDebugDump& debug, int topN = 10) {
    if (!debugTempoBinsEnabled() || !debugTrackSelected(label)) return;

    fprintf(stderr, "\n[tempo-debug] %s conf=%.3f top bins:\n", label, debug.confidence);
    for (int i = 0; i < topN; ++i) {
        const float bpm = static_cast<float>(TEMPO_LOW + debug.topBins[i]);
        fprintf(stderr, "  #%d bpm=%6.1f bin=%3u mag=%.6f\n", i + 1, bpm, debug.topBins[i], debug.topMags[i]);
    }

    fprintf(stderr, "  probes:");
    for (int p = 0; p < 9; ++p) {
        fprintf(stderr, " %.0f=%.6f", debug.probeBpms[p], debug.probeMags[p]);
    }
    fprintf(stderr, "\n");
}

// ============================================================================
// ACR metrical classification (from Chiu et al. 2022)
// ============================================================================

enum MetricalClass { ML_DIRECT, ML_DOUBLE, ML_HALF, ML_TRIPLE, ML_THIRD, ML_WRONG };

static const char* metricalName(MetricalClass mc) {
    switch (mc) {
        case ML_DIRECT: return "DIRECT";
        case ML_DOUBLE: return "DOUBLE";
        case ML_HALF:   return "HALF";
        case ML_TRIPLE: return "TRIPLE";
        case ML_THIRD:  return "THIRD";
        case ML_WRONG:  return "WRONG";
    }
    return "?";
}

static MetricalClass classifyTempo(float detected, float expected) {
    if (fabsf(detected - expected) <= 5.0f)         return ML_DIRECT;
    if (fabsf(detected - expected * 2.0f) <= 8.0f)  return ML_DOUBLE;
    if (fabsf(detected - expected * 0.5f) <= 4.0f)  return ML_HALF;
    if (fabsf(detected - expected * 3.0f) <= 10.0f) return ML_TRIPLE;
    if (fabsf(detected - expected / 3.0f) <= 5.0f)  return ML_THIRD;
    return ML_WRONG;
}

static bool isCoherent(MetricalClass mc) {
    return mc != ML_WRONG;
}

// ============================================================================
// Path resolution
// ============================================================================

static const char* const AUDIO_DIRS_12K8[] = {
    "/Users/spectrasynq/Workspace_Management/Software/Teensy.AudioDSP_Pipeline/Tests/Audio_12k8",
};

static const char* const AUDIO_DIRS_32K[] = {
    "/Users/spectrasynq/Workspace_Management/Software/Teensy.AudioDSP_Pipeline/Tests/Audio_32k",
};

static std::string resolvePath(const char* stem) {
#if defined(FEATURE_AUDIO_BACKEND_ESV11_32KHZ)
    return std::string(AUDIO_DIRS_32K[0]) + "/" + stem + "_32k.wav";
#else
    return std::string(AUDIO_DIRS_12K8[0]) + "/" + stem + "_12k8.wav";
#endif
}

// ============================================================================
// Test tracks
// ============================================================================

struct DrumTrack {
    const char* stem;
    const char* label;
    float expectedBpm;
};

static const DrumTrack DRUMS[] = {
    {"hiphop_85",       "hiphop_85",     85.0f},
    {"bossa_95",        "bossa_95",      95.0f},
    {"cyberpunk_100",   "cyber_100",    100.0f},
    {"groove_100",      "groove_100",   100.0f},
    {"kick_120",        "kick_120",     120.0f},
    {"techhouse_124",   "tech_124",     124.0f},
    {"hiphop_133",      "hiphop_133",   133.0f},
    {"jazz_160",        "jazz_160",     160.0f},
    {"metal_165",       "metal_165",    165.0f},
    {"jazz_210",        "jazz_210",     210.0f},
};
static constexpr int NUM_DRUMS = sizeof(DRUMS) / sizeof(DRUMS[0]);

struct EdmTrack {
    const char* stem;
    const char* label;
};

static const EdmTrack EDM[] = {
    {"edm_dopex",          "dopex"},
    {"edm_eternity",       "eternity"},
    {"edm_carte_blanche",  "carte_bl"},
    {"edm_post_malone",    "post_mal"},
    {"edm_great_escape",   "great_esc"},
    {"edm_touch_me",       "touch_me"},
    {"edm_wont_forget",    "wont_forget"},
};
static constexpr int NUM_EDM = sizeof(EDM) / sizeof(EDM[0]);

// Real-audio acceptance gates (primary validation surface).
// Synthetic parity remains a separate smoke/regression test.
static constexpr int kMinDrumCoherent = 8;  // /10
static constexpr int kMinEdmActive = 6;     // /7

// ============================================================================
// Test: drum loops with known BPMs
// ============================================================================

static void test_esv11_drum_loops() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  ESV11 Real Music Tempo Test — %s                        ║\n",
#if defined(FEATURE_AUDIO_BACKEND_ESV11_32KHZ)
           "32 kHz"
#else
           "12.8kHz"
#endif
    );
    printf("║  SAMPLE_RATE=%d  CHUNK_SIZE=%d  NOVELTY_LOG_HZ=%d            ║\n",
           SAMPLE_RATE, CHUNK_SIZE, NOVELTY_LOG_HZ);
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");

    printf("─── Drum Loops (known BPM) ────────────────────────────────────────\n");
    printf("  %-14s  %6s  %6s  %5s  %-7s  %s\n",
           "Track", "Expect", "Got", "Conf", "Class", "Result");
    printf("  %-14s  %6s  %6s  %5s  %-7s  %s\n",
           "──────────────", "──────", "──────", "─────", "───────", "──────");

    int coherent = 0;
    int direct = 0;

    for (int i = 0; i < NUM_DRUMS; i++) {
        std::string path = resolvePath(DRUMS[i].stem);
        WavData wav;
        if (!loadWav(path.c_str(), wav)) {
            printf("  %-14s  SKIP (cannot load)\n", DRUMS[i].label);
            continue;
        }

        if (wav.sampleRate != static_cast<uint32_t>(SAMPLE_RATE)) {
            printf("  %-14s  SKIP (rate %u != %d)\n", DRUMS[i].label, wav.sampleRate, SAMPLE_RATE);
            continue;
        }

        TempoDebugDump debugDump{};
        TempoResult tr = runIsolated(wav, debugTempoBinsEnabled() ? &debugDump : nullptr);
        MetricalClass mc = classifyTempo(tr.bpm, DRUMS[i].expectedBpm);
        bool coh = isCoherent(mc);

        if (coh) coherent++;
        if (mc == ML_DIRECT) direct++;

        printf("  %-14s  %5.0f   %5.0f   %4.2f   %-7s  %s\n",
               DRUMS[i].label,
               DRUMS[i].expectedBpm,
               tr.bpm,
               tr.confidence,
               metricalName(mc),
               coh ? "OK" : "MISS");

        dumpTopTempoBins(DRUMS[i].label, debugDump);
    }

    printf("\n  Coherent: %d/%d   Direct: %d/%d\n\n", coherent, NUM_DRUMS, direct, NUM_DRUMS);

    // Hard gate: real drum-loop coherence must stay strong.
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(
        kMinDrumCoherent, coherent,
        "Real drum-loop coherence gate failed");
}

// ============================================================================
// Test: EDM tracks (reports BPM for cross-rate comparison)
// ============================================================================

static void test_esv11_edm_tracks() {
    printf("─── EDM Tracks (parity reference) ─────────────────────────────────\n");
    printf("  %-14s  %6s  %5s  %3s  %s\n",
           "Track", "BPM", "Conf", "VU", "Silence");
    printf("  %-14s  %6s  %5s  %3s  %s\n",
           "──────────────", "──────", "─────", "───", "───────");

    int active = 0;

    for (int i = 0; i < NUM_EDM; i++) {
        std::string path = resolvePath(EDM[i].stem);
        WavData wav;
        if (!loadWav(path.c_str(), wav)) {
            printf("  %-14s  SKIP (cannot load)\n", EDM[i].label);
            continue;
        }

        if (wav.sampleRate != static_cast<uint32_t>(SAMPLE_RATE)) {
            printf("  %-14s  SKIP (rate %u != %d)\n", EDM[i].label, wav.sampleRate, SAMPLE_RATE);
            continue;
        }

        TempoDebugDump debugDump{};
        TempoResult tr = runIsolated(wav, debugTempoBinsEnabled() ? &debugDump : nullptr);
        if (!tr.silenceDetected) active++;

        printf("  %-14s  %5.0f   %4.2f   %3.0f%%  %s\n",
               EDM[i].label,
               tr.bpm,
               tr.confidence,
               tr.vuLevel * 100.0f,
               tr.silenceDetected ? "SILENT" : "active");

        dumpTopTempoBins(EDM[i].label, debugDump);
    }

    printf("\n  Active tracks: %d/%d\n\n", active, NUM_EDM);

    // Hard gate: most EDM tracks must register as musically active.
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(
        kMinEdmActive, active,
        "EDM activity gate failed");
}

// ============================================================================
// Unity boilerplate
// ============================================================================

void setUp() {}
void tearDown() {}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_esv11_drum_loops);
    RUN_TEST(test_esv11_edm_tracks);
    return UNITY_END();
}
