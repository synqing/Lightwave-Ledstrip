/**
 * @file test_esv11_music_corpus.cpp
 * @brief ESV11 corpus regression gate on a stratified harmonixset benchmark pack.
 *
 * Usage:
 *   # 1) Build/update pack
 *   python3 tools/build_esv11_benchmark_pack.py
 *
 *   # 2) Capture baselines (one-time or after intentional algorithm changes)
 *   LW_ESV11_CAPTURE_BASELINE=1 pio test -e native_test_esv11_music_corpus
 *   LW_ESV11_CAPTURE_BASELINE=1 pio test -e native_test_esv11_music_corpus_32khz
 *
 *   # 3) Regression gate
 *   pio test -e native_test_esv11_music_corpus
 *   pio test -e native_test_esv11_music_corpus_32khz
 */

#include <unity.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <unistd.h>
#include <sys/wait.h>

// Vendored ES pipeline
#include "audio/backends/esv11/vendor/EsV11Shim.h"
#include "audio/backends/esv11/vendor/EsV11Buffers.h"
#include "audio/backends/esv11/vendor/global_defines.h"
#include "audio/backends/esv11/vendor/microphone.h"
#include "audio/backends/esv11/vendor/goertzel.h"
#include "audio/backends/esv11/vendor/vu.h"
#include "audio/backends/esv11/vendor/tempo.h"
#include "audio/backends/esv11/vendor/utilities_min.h"

namespace fs = std::filesystem;

struct WavData {
    std::vector<int16_t> samples;
    uint32_t sampleRate;
};

static bool loadWav(const char* path, WavData& out) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "  Cannot open WAV: %s\n", path);
        return false;
    }
    char riff[4];
    fread(riff, 1, 4, f);
    if (memcmp(riff, "RIFF", 4) != 0) {
        fclose(f);
        return false;
    }
    uint32_t fileSize;
    fread(&fileSize, 4, 1, f);
    (void)fileSize;

    char wave[4];
    fread(wave, 1, 4, f);
    if (memcmp(wave, "WAVE", 4) != 0) {
        fclose(f);
        return false;
    }

    uint32_t sr = 0;
    uint16_t channels = 0;
    uint16_t bitsPerSample = 0;

    while (!feof(f)) {
        char chunkId[4];
        if (fread(chunkId, 1, 4, f) < 4) {
            break;
        }
        uint32_t chunkSize = 0;
        fread(&chunkSize, 4, 1, f);

        if (memcmp(chunkId, "fmt ", 4) == 0) {
            uint16_t audioFmt = 0;
            fread(&audioFmt, 2, 1, f);
            fread(&channels, 2, 1, f);
            fread(&sr, 4, 1, f);
            uint32_t byteRate = 0;
            fread(&byteRate, 4, 1, f);
            uint16_t blockAlign = 0;
            fread(&blockAlign, 2, 1, f);
            fread(&bitsPerSample, 2, 1, f);
            (void)audioFmt;
            (void)byteRate;
            (void)blockAlign;
            if (chunkSize > 16) {
                fseek(f, static_cast<long>(chunkSize - 16), SEEK_CUR);
            }
        } else if (memcmp(chunkId, "data", 4) == 0) {
            size_t numSamples = chunkSize / (bitsPerSample / 8) / channels;
            out.samples.resize(numSamples);
            if (bitsPerSample == 16 && channels == 1) {
                fread(out.samples.data(), 2, numSamples, f);
            } else {
                for (size_t i = 0; i < numSamples; i++) {
                    int32_t sum = 0;
                    for (uint16_t ch = 0; ch < channels; ch++) {
                        int16_t s = 0;
                        fread(&s, 2, 1, f);
                        sum += s;
                    }
                    out.samples[i] = static_cast<int16_t>(sum / std::max<uint16_t>(1, channels));
                }
            }
            out.sampleRate = sr;
            fclose(f);
            return true;
        } else {
            fseek(f, static_cast<long>(chunkSize), SEEK_CUR);
        }
    }
    fclose(f);
    return false;
}

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
    bool silenceDetected;
};

static TempoResult runEsv11(const WavData& wav, float maxSeconds = 25.0f) {
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

        float newSamples[CHUNK_SIZE];
        for (uint16_t i = 0; i < CHUNK_SIZE; i++) {
            newSamples[i] = static_cast<float>(wav.samples[pos + i]) / 32768.0f;
        }
        shift_and_copy_arrays(sample_history, SAMPLE_HISTORY_LENGTH, newSamples, CHUNK_SIZE);

        calculate_magnitudes();
        get_chromagram();
        run_vu();
        update_tempo();

        if (lastGpuTickUs == 0) {
            lastGpuTickUs = nowUs;
        }
        const uint64_t elapsedUs = nowUs - lastGpuTickUs;
        const float idealUsInterval = 1000000.0f / static_cast<float>(REFERENCE_FPS);
        const float delta = static_cast<float>(elapsedUs) / idealUsInterval;
        lastGpuTickUs = nowUs;

        update_novelty();
        update_tempi_phase(delta);
        chunkIdx++;
    }

    const uint16_t topBin = esv11_pick_top_tempo_bin_octave_aware();
    TempoResult out{};
    out.bpm = static_cast<float>(TEMPO_LOW) + static_cast<float>(topBin);
    out.confidence = tempo_confidence;
    out.vuLevel = vu_level;
    out.silenceDetected = silence_detected;
    return out;
}

static TempoResult runIsolated(const WavData& wav) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        fprintf(stderr, "  pipe() failed\n");
        return {0, 0, 0, true};
    }

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "  fork() failed\n");
        close(pipefd[0]);
        close(pipefd[1]);
        return {0, 0, 0, true};
    }

    if (pid == 0) {
        close(pipefd[0]);
        TempoResult tr = runEsv11(wav);
        ssize_t written = write(pipefd[1], &tr, sizeof(tr));
        (void)written;
        close(pipefd[1]);
        _exit(0);
    }

    close(pipefd[1]);
    TempoResult result = {0, 0, 0, true};
    ssize_t bytesRead = read(pipefd[0], &result, sizeof(result));
    (void)bytesRead;
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "  Child process failed (status=%d)\n", status);
    }
    return result;
}

static std::vector<std::string> splitTsv(const std::string& line) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : line) {
        if (c == '\t') {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    parts.push_back(cur);
    return parts;
}

struct TrackDef {
    std::string trackId;
    std::string sourceId;
    std::string label;
    std::string bpmBucket;
    std::string path12k8;
    std::string path32k;
};

static fs::path findManifestPath() {
    if (const char* env = std::getenv("ESV11_BENCH_MANIFEST")) {
        fs::path p(env);
        if (fs::exists(p)) return p;
    }
    const char* candidates[] = {
        "test/music_corpus/harmonixset/esv11_benchmark/manifest.tsv",
        "../test/music_corpus/harmonixset/esv11_benchmark/manifest.tsv",
        "../../test/music_corpus/harmonixset/esv11_benchmark/manifest.tsv",
        "../../../test/music_corpus/harmonixset/esv11_benchmark/manifest.tsv",
    };
    for (const char* c : candidates) {
        fs::path p(c);
        if (fs::exists(p)) return p;
    }
    return fs::path();
}

static std::vector<TrackDef> loadManifest(const fs::path& manifest) {
    std::ifstream in(manifest);
    if (!in.is_open()) return {};

    std::string headerLine;
    if (!std::getline(in, headerLine)) return {};
    std::vector<std::string> headers = splitTsv(headerLine);
    std::map<std::string, int> idx;
    for (size_t i = 0; i < headers.size(); i++) idx[headers[i]] = static_cast<int>(i);

    auto col = [&](const std::vector<std::string>& row, const char* name) -> std::string {
        auto it = idx.find(name);
        if (it == idx.end()) return "";
        int i = it->second;
        if (i < 0 || static_cast<size_t>(i) >= row.size()) return "";
        return row[static_cast<size_t>(i)];
    };

    std::vector<TrackDef> out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> row = splitTsv(line);
        TrackDef t;
        t.trackId = col(row, "track_id");
        t.sourceId = col(row, "source_id");
        t.label = col(row, "label");
        t.bpmBucket = col(row, "bpm_bucket");
        t.path12k8 = col(row, "audio_12k8_wav");
        t.path32k = col(row, "audio_32k_wav");
        if (!t.trackId.empty() && !t.path12k8.empty() && !t.path32k.empty()) {
            out.push_back(t);
        }
    }
    return out;
}

struct BaselineRow {
    float bpm;
    float confidence;
    float vu;
    bool silence;
};

static bool parseBool01(const std::string& s) {
    if (s == "1" || s == "true" || s == "TRUE" || s == "True") return true;
    return false;
}

static std::map<std::string, BaselineRow> loadBaseline(const fs::path& path) {
    std::map<std::string, BaselineRow> out;
    std::ifstream in(path);
    if (!in.is_open()) return out;
    std::string headerLine;
    if (!std::getline(in, headerLine)) return out;
    std::vector<std::string> headers = splitTsv(headerLine);
    std::map<std::string, int> idx;
    for (size_t i = 0; i < headers.size(); i++) idx[headers[i]] = static_cast<int>(i);

    auto col = [&](const std::vector<std::string>& row, const char* name) -> std::string {
        auto it = idx.find(name);
        if (it == idx.end()) return "";
        int i = it->second;
        if (i < 0 || static_cast<size_t>(i) >= row.size()) return "";
        return row[static_cast<size_t>(i)];
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> row = splitTsv(line);
        std::string id = col(row, "track_id");
        if (id.empty()) continue;
        BaselineRow b{};
        b.bpm = std::strtof(col(row, "bpm").c_str(), nullptr);
        b.confidence = std::strtof(col(row, "confidence").c_str(), nullptr);
        b.vu = std::strtof(col(row, "vu_level").c_str(), nullptr);
        b.silence = parseBool01(col(row, "silence"));
        out[id] = b;
    }
    return out;
}

static bool shouldCaptureBaseline() {
    const char* env = std::getenv("LW_ESV11_CAPTURE_BASELINE");
    if (!env) return false;
    return parseBool01(env);
}

static fs::path baselinePathForManifest(const fs::path& manifestPath) {
#if defined(FEATURE_AUDIO_BACKEND_ESV11_32KHZ)
    return manifestPath.parent_path() / "baseline_32k.tsv";
#else
    return manifestPath.parent_path() / "baseline_12k8.tsv";
#endif
}

static void writeBaseline(const fs::path& path,
                          const std::vector<TrackDef>& tracks,
                          const std::map<std::string, TempoResult>& results) {
    std::ofstream out(path, std::ios::trunc);
    out << "track_id\tsource_id\tlabel\tbpm\tconfidence\tvu_level\tsilence\n";
    for (const auto& t : tracks) {
        auto it = results.find(t.trackId);
        if (it == results.end()) continue;
        const TempoResult& r = it->second;
        out << t.trackId << "\t"
            << t.sourceId << "\t"
            << t.label << "\t"
            << r.bpm << "\t"
            << r.confidence << "\t"
            << r.vuLevel << "\t"
            << (r.silenceDetected ? "1" : "0") << "\n";
    }
}

static void test_esv11_harmonixset_corpus_regression() {
    fs::path manifestPath = findManifestPath();
    TEST_ASSERT_TRUE_MESSAGE(!manifestPath.empty(), "Benchmark manifest not found");

    std::vector<TrackDef> tracks = loadManifest(manifestPath);
    TEST_ASSERT_TRUE_MESSAGE(!tracks.empty(), "Benchmark manifest is empty or unreadable");

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  ESV11 Harmonixset Corpus Regression — %s                   ║\n",
#if defined(FEATURE_AUDIO_BACKEND_ESV11_32KHZ)
           "32 kHz"
#else
           "12.8kHz"
#endif
    );
    printf("║  SAMPLE_RATE=%d  tracks=%d                                  ║\n",
           SAMPLE_RATE, static_cast<int>(tracks.size()));
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");

    std::map<std::string, TempoResult> observed;
    int loaded = 0;
    for (const auto& t : tracks) {
#if defined(FEATURE_AUDIO_BACKEND_ESV11_32KHZ)
        const std::string& wavPath = t.path32k;
#else
        const std::string& wavPath = t.path12k8;
#endif
        WavData wav;
        if (!loadWav(wavPath.c_str(), wav)) {
            printf("  %-18s  SKIP (cannot load)\n", t.trackId.c_str());
            continue;
        }
        if (wav.sampleRate != static_cast<uint32_t>(SAMPLE_RATE)) {
            printf("  %-18s  SKIP (rate %u != %d)\n", t.trackId.c_str(), wav.sampleRate, SAMPLE_RATE);
            continue;
        }

        TempoResult tr = runIsolated(wav);
        observed[t.trackId] = tr;
        loaded++;
        printf("  %-18s  bpm=%5.1f  conf=%4.2f  vu=%3.0f%%  %s\n",
               t.trackId.c_str(),
               tr.bpm,
               tr.confidence,
               tr.vuLevel * 100.0f,
               tr.silenceDetected ? "SILENT" : "active");
    }

    TEST_ASSERT_TRUE_MESSAGE(loaded > 0, "No tracks loaded from manifest");

    fs::path baselinePath = baselinePathForManifest(manifestPath);
    if (shouldCaptureBaseline()) {
        writeBaseline(baselinePath, tracks, observed);
        printf("\n  Baseline captured: %s\n\n", baselinePath.c_str());
        TEST_ASSERT_TRUE(true);
        return;
    }

    std::map<std::string, BaselineRow> baseline = loadBaseline(baselinePath);
    TEST_ASSERT_TRUE_MESSAGE(!baseline.empty(), "Baseline missing; run with LW_ESV11_CAPTURE_BASELINE=1");

    int compared = 0;
    int bpmOk = 0;
    int confOk = 0;
    int silenceOk = 0;
    int missing = 0;
    float maxBpmDiff = 0.0f;
    std::string maxBpmTrack;

    for (const auto& t : tracks) {
        auto obsIt = observed.find(t.trackId);
        if (obsIt == observed.end()) continue;
        auto baseIt = baseline.find(t.trackId);
        if (baseIt == baseline.end()) {
            missing++;
            continue;
        }
        compared++;
        const TempoResult& o = obsIt->second;
        const BaselineRow& b = baseIt->second;

        float dbpm = fabsf(o.bpm - b.bpm);
        float dconf = fabsf(o.confidence - b.confidence);
        if (dbpm <= 1.0f) bpmOk++;
        if (dconf <= 0.20f) confOk++;
        if (o.silenceDetected == b.silence) silenceOk++;
        if (dbpm > maxBpmDiff) {
            maxBpmDiff = dbpm;
            maxBpmTrack = t.trackId;
        }
    }

    printf("\n  Compared: %d  missing-baseline: %d\n", compared, missing);
    printf("  BPM<=1.0: %d/%d  Conf<=0.20: %d/%d  Silence match: %d/%d\n",
           bpmOk, compared, confOk, compared, silenceOk, compared);
    printf("  Max BPM drift: %.2f (%s)\n\n", maxBpmDiff, maxBpmTrack.c_str());

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, missing, "Missing tracks in baseline");
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(
        static_cast<int>(std::ceil(static_cast<double>(compared) * 0.98)),
        bpmOk,
        "BPM drift exceeded gate"
    );
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(
        static_cast<int>(std::ceil(static_cast<double>(compared) * 0.95)),
        confOk,
        "Confidence drift exceeded gate"
    );
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(
        static_cast<int>(std::ceil(static_cast<double>(compared) * 0.98)),
        silenceOk,
        "Silence-state drift exceeded gate"
    );
}

void setUp() {}
void tearDown() {}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_esv11_harmonixset_corpus_regression);
    return UNITY_END();
}
