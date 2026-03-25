#include <unity.h>

#include <algorithm>
#include <array>
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

#include "../../src/audio/onset/OnsetDetector.h"

namespace fs = std::filesystem;
using namespace lightwaveos::audio;

namespace {

constexpr uint32_t kSampleRate = 32000;
constexpr uint16_t kHopSize = 256;
constexpr uint16_t kHistorySize = 1024;

struct WavData {
    std::vector<int16_t> samples;
    uint32_t sampleRate = 0;
};

struct TrackDef {
    std::string trackId;
    std::string sourceId;
    std::string label;
    std::string bpmBucket;
    std::string audio32kPath;
};

struct TrackMetrics {
    std::string trackId;
    std::string bpmBucket;
    float durationSec = 0.0f;
    int totalFrames = 0;
    int positiveEnvFrames = 0;
    int eventCount = 0;
    int kickCount = 0;
    int snareCount = 0;
    int hihatCount = 0;
    float meanRms = 0.0f;
    float meanFlux = 0.0f;
    float meanEnv = 0.0f;
    float meanPositiveEnv = 0.0f;
    float maxFlux = 0.0f;
    float maxEnv = 0.0f;
};

static bool loadWav(const char* path, WavData& out) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        std::fprintf(stderr, "Cannot open WAV: %s\n", path);
        return false;
    }

    char riff[4];
    fread(riff, 1, 4, f);
    if (std::memcmp(riff, "RIFF", 4) != 0) {
        fclose(f);
        return false;
    }

    uint32_t fileSize = 0;
    fread(&fileSize, 4, 1, f);
    (void)fileSize;

    char wave[4];
    fread(wave, 1, 4, f);
    if (std::memcmp(wave, "WAVE", 4) != 0) {
        fclose(f);
        return false;
    }

    uint32_t sr = 0;
    uint16_t channels = 0;
    uint16_t bitsPerSample = 0;

    while (!std::feof(f)) {
        char chunkId[4];
        if (fread(chunkId, 1, 4, f) < 4) break;

        uint32_t chunkSize = 0;
        fread(&chunkSize, 4, 1, f);

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            uint16_t audioFmt = 0;
            fread(&audioFmt, 2, 1, f);
            fread(&channels, 2, 1, f);
            fread(&sr, 4, 1, f);
            uint32_t byteRate = 0;
            uint16_t blockAlign = 0;
            fread(&byteRate, 4, 1, f);
            fread(&blockAlign, 2, 1, f);
            fread(&bitsPerSample, 2, 1, f);
            (void)audioFmt;
            (void)byteRate;
            (void)blockAlign;
            if (chunkSize > 16) {
                fseek(f, static_cast<long>(chunkSize - 16), SEEK_CUR);
            }
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            size_t numSamples = chunkSize / (bitsPerSample / 8) / channels;
            out.samples.resize(numSamples);
            if (bitsPerSample == 16 && channels == 1) {
                fread(out.samples.data(), 2, numSamples, f);
            } else {
                for (size_t i = 0; i < numSamples; ++i) {
                    int32_t sum = 0;
                    for (uint16_t ch = 0; ch < channels; ++ch) {
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

static float computeRms(const int16_t* samples, size_t count) {
    if (count == 0) return 0.0f;
    double sumSq = 0.0;
    for (size_t i = 0; i < count; ++i) {
        double x = static_cast<double>(samples[i]) / 32768.0;
        sumSq += x * x;
    }
    return static_cast<float>(std::sqrt(sumSq / static_cast<double>(count)));
}

static std::vector<std::string> splitTsv(const std::string& line) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : line) {
        if (c == '\t') {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(current);
    return parts;
}

static fs::path findManifestPath() {
    if (const char* env = std::getenv("LW_ONSET_MANIFEST")) {
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

static std::vector<TrackDef> loadManifest(const fs::path& manifestPath) {
    std::ifstream in(manifestPath);
    if (!in.is_open()) return {};

    std::string headerLine;
    if (!std::getline(in, headerLine)) return {};
    std::vector<std::string> headers = splitTsv(headerLine);
    std::map<std::string, int> idx;
    for (size_t i = 0; i < headers.size(); ++i) {
        idx[headers[i]] = static_cast<int>(i);
    }

    auto col = [&](const std::vector<std::string>& row, const char* name) -> std::string {
        auto it = idx.find(name);
        if (it == idx.end()) return "";
        int i = it->second;
        if (i < 0 || static_cast<size_t>(i) >= row.size()) return "";
        return row[static_cast<size_t>(i)];
    };

    std::vector<TrackDef> tracks;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> row = splitTsv(line);
        TrackDef t;
        t.trackId = col(row, "track_id");
        t.sourceId = col(row, "source_id");
        t.label = col(row, "label");
        t.bpmBucket = col(row, "bpm_bucket");
        t.audio32kPath = col(row, "audio_32k_wav");
        if (!t.trackId.empty() && !t.audio32kPath.empty()) {
            fs::path p(t.audio32kPath);
            if (!p.is_absolute()) {
                p = manifestPath.parent_path() / p;
            }
            t.audio32kPath = p.lexically_normal().string();
            tracks.push_back(t);
        }
    }
    return tracks;
}

static size_t corpusLimit(size_t total) {
    const char* env = std::getenv("LW_ONSET_CORPUS_LIMIT");
    if (!env || env[0] == '\0') return total;
    long v = std::strtol(env, nullptr, 10);
    if (v <= 0) return total;
    return std::min<size_t>(total, static_cast<size_t>(v));
}

static float corpusDurationLimitSec() {
    const char* env = std::getenv("LW_ONSET_CORPUS_SECONDS");
    if (!env || env[0] == '\0') return 25.0f;
    float v = std::strtof(env, nullptr);
    return (v > 1.0f) ? v : 25.0f;
}

static TrackMetrics runDetectorOnTrack(const TrackDef& track,
                                       const WavData& wav,
                                       const OnsetDetector::Config& cfg,
                                       float maxSeconds) {
    OnsetDetector detector;
    detector.init(cfg);

    TrackMetrics m;
    m.trackId = track.trackId;
    m.bpmBucket = track.bpmBucket;

    const uint32_t maxSamples = static_cast<uint32_t>(
        std::min<double>(wav.samples.size(),
                         maxSeconds * static_cast<double>(kSampleRate)));

    std::array<float, kHistorySize> history = {};

    double rmsSum = 0.0;
    double fluxSum = 0.0;
    double envSum = 0.0;
    double positiveEnvSum = 0.0;

    for (uint32_t pos = 0; pos + kHopSize <= maxSamples; pos += kHopSize) {
        std::memmove(history.data(),
                     history.data() + kHopSize,
                     (kHistorySize - kHopSize) * sizeof(float));

        for (uint16_t i = 0; i < kHopSize; ++i) {
            history[kHistorySize - kHopSize + i] =
                static_cast<float>(wav.samples[pos + i]) / 32768.0f;
        }

        const float rms = computeRms(wav.samples.data() + pos, kHopSize);
        OnsetResult r = detector.process(history.data(), rms);

        m.totalFrames++;
        rmsSum += rms;
        fluxSum += r.flux;
        envSum += r.onset_env;

        if (r.flux > m.maxFlux) m.maxFlux = r.flux;
        if (r.onset_env > m.maxEnv) m.maxEnv = r.onset_env;
        if (r.onset_env > 0.0f) {
            m.positiveEnvFrames++;
            positiveEnvSum += r.onset_env;
        }
        if (r.onset_event > 0.0f) m.eventCount++;
        if (r.kick_trigger) m.kickCount++;
        if (r.snare_trigger) m.snareCount++;
        if (r.hihat_trigger) m.hihatCount++;
    }

    if (m.totalFrames > 0) {
        const double inv = 1.0 / static_cast<double>(m.totalFrames);
        m.meanRms = static_cast<float>(rmsSum * inv);
        m.meanFlux = static_cast<float>(fluxSum * inv);
        m.meanEnv = static_cast<float>(envSum * inv);
        if (m.positiveEnvFrames > 0) {
            m.meanPositiveEnv = static_cast<float>(
                positiveEnvSum / static_cast<double>(m.positiveEnvFrames));
        }
        m.durationSec = static_cast<float>(
            (static_cast<double>(m.totalFrames) * kHopSize) / kSampleRate);
    }

    return m;
}

static float eventRateHz(const TrackMetrics& m) {
    return (m.durationSec > 0.0f) ? (static_cast<float>(m.eventCount) / m.durationSec) : 0.0f;
}

static float activeFrac(const TrackMetrics& m) {
    return (m.totalFrames > 0) ? (static_cast<float>(m.positiveEnvFrames) / m.totalFrames) : 0.0f;
}

} // namespace

void setUp(void) {
}

void tearDown(void) {
}

void test_onset_music_corpus_diagnostic(void) {
    const fs::path manifestPath = findManifestPath();
    TEST_ASSERT_TRUE_MESSAGE(!manifestPath.empty(), "Onset corpus manifest not found");

    std::vector<TrackDef> tracks = loadManifest(manifestPath);
    TEST_ASSERT_TRUE_MESSAGE(!tracks.empty(), "Onset corpus manifest had no tracks");

    const size_t limit = corpusLimit(tracks.size());
    const float maxSeconds = corpusDurationLimitSec();

    OnsetDetector::Config cfg;
    // Corpus tests use raw PCM (no VU AGC): short warmup, matching floor
    cfg.warmupFrames = 50;
    cfg.activityFloor = 0.002f;
    cfg.activityRangeMin = 0.002f;

    std::printf("\n============================================================\n");
    std::printf("  ONSET CORPUS DIAGNOSTIC\n");
    std::printf("  manifest=%s\n", manifestPath.string().c_str());
    std::printf("  tracks=%zu/%zu  seconds=%.1f  mul=%.2f floor=%.4f medianWin=%u\n",
                limit, tracks.size(), maxSeconds,
                cfg.thresholdMultiplier, cfg.thresholdFloor, cfg.thresholdFrames);
    std::printf("============================================================\n");
    std::printf("%-18s %-8s %6s %6s %6s %6s %6s %6s %6s %5s %5s %5s\n",
                "track", "bucket", "evHz", "act%", "maxF", "maxE",
                "meanF", "meanE", "posE", "kick", "snr", "hh");

    int deadTracks = 0;
    int saturatedTracks = 0;
    int eventfulTracks = 0;
    int tracksWithKick = 0;
    int tracksWithSnare = 0;
    int tracksWithHihat = 0;
    float maxEventRate = 0.0f;
    float maxActivePct = 0.0f;

    for (size_t i = 0; i < limit; ++i) {
        const TrackDef& track = tracks[i];
        WavData wav;
        bool loaded = loadWav(track.audio32kPath.c_str(), wav);
        TEST_ASSERT_TRUE_MESSAGE(loaded, track.audio32kPath.c_str());
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(kSampleRate, wav.sampleRate, track.audio32kPath.c_str());

        TrackMetrics m = runDetectorOnTrack(track, wav, cfg, maxSeconds);
        const float evHz = eventRateHz(m);
        const float actPct = activeFrac(m) * 100.0f;
        if (evHz > maxEventRate) maxEventRate = evHz;
        if (actPct > maxActivePct) maxActivePct = actPct;

        if (m.eventCount > 0) eventfulTracks++;
        if (m.eventCount == 0 || m.maxEnv < 0.005f) deadTracks++;
        if (evHz > 12.0f || activeFrac(m) > 0.25f) saturatedTracks++;
        if (m.kickCount > 0) tracksWithKick++;
        if (m.snareCount > 0) tracksWithSnare++;
        if (m.hihatCount > 0) tracksWithHihat++;

        std::string shortId = track.trackId;
        if (shortId.size() > 18) shortId = shortId.substr(0, 18);
        std::printf("%-18s %-8s %6.2f %6.2f %6.3f %6.3f %6.3f %6.3f %6.3f %5d %5d %5d\n",
                    shortId.c_str(),
                    track.bpmBucket.c_str(),
                    evHz,
                    actPct,
                    m.maxFlux,
                    m.maxEnv,
                    m.meanFlux,
                    m.meanEnv,
                    m.meanPositiveEnv,
                    m.kickCount,
                    m.snareCount,
                    m.hihatCount);
    }

    std::printf("------------------------------------------------------------\n");
    std::printf("eventful=%d/%zu dead=%d saturated=%d kickTracks=%d snareTracks=%d hihatTracks=%d\n",
                eventfulTracks, limit, deadTracks, saturatedTracks,
                tracksWithKick, tracksWithSnare, tracksWithHihat);
    std::printf("maxEventRate=%.2fHz maxActive=%.2f%%\n", maxEventRate, maxActivePct);
    std::printf("============================================================\n");

    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(limit), eventfulTracks,
                                  "One or more corpus tracks produced zero onset events");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, deadTracks,
                                  "One or more corpus tracks remained onset-dead");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, saturatedTracks,
                                  "One or more corpus tracks saturated the onset detector");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_onset_music_corpus_diagnostic);
    return UNITY_END();
}
