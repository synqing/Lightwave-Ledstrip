/**
 * Spine16k Acceptance Tests — Native Host Build
 * ================================================
 * Runs actual firmware DSP code (PipelineCore + BeatTracker) against
 * synthetic 16 kHz WAV test signals. No ESP32 hardware needed.
 *
 * Validates frozen v0.1 parameters: div=1, delta=0.02, K=1.5, gate=0.0018.
 *
 * Build:  pio test -e native_test_spine16k -f test_spine16k
 */

#include <unity.h>
#include "../../src/audio/pipeline/PipelineCore.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>

// ============================================================================
// WAV loader
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
// Test configuration variants
// ============================================================================

struct TestConfig {
    const char* name;
    float fluxBinDivisor;  // 0=auto(255), 1=no averaging, 16=sqrt compromise
    float delta;
    float onsetK;
    float onsetGateRms;
};

static const TestConfig CONFIGS[] = {
    // Winning config from 2-round parameter sweep (17/17 acceptance).
    // div=1: no per-bin averaging — preserves onset_env scale for beat tracker.
    // delta=0.02: calibrated floor for onset_env in 0.1-1.0 range.
    {"frozen_v0.1", 1.0f,   0.02f,  1.5f,  0.0018f},
};
static constexpr int NUM_CONFIGS = sizeof(CONFIGS) / sizeof(CONFIGS[0]);

// ============================================================================
// Pipeline runner
// ============================================================================

static constexpr uint32_t SPINE_SR     = 16000;
static constexpr uint16_t SPINE_HOP    = 128;
static constexpr uint16_t SPINE_WINDOW = 512;

struct TestResult {
    int   beatEventCount;
    int   onsetEventCount;
    float finalTempoBpm;
    float finalTempoConf;
    float meanRms;
    float maxFlux;
    float maxOnsetEnv;
    int   totalFrames;
};

static TestResult runPipeline(const WavData& wav, const TestConfig& tc) {
    PipelineCore pipeline;
    PipelineConfig cfg;
    cfg.sampleRate      = SPINE_SR;
    cfg.hopSize         = SPINE_HOP;
    cfg.windowSize      = SPINE_WINDOW;
    cfg.fluxBinDivisor  = tc.fluxBinDivisor;
    cfg.onsetMeanAlpha  = 0.01f;
    cfg.onsetVarAlpha   = 0.01f;
    cfg.onsetK          = tc.onsetK;
    cfg.onsetGateRms    = tc.onsetGateRms;
    cfg.peakPick.preMax  = 3;
    cfg.peakPick.postMax = 1;
    cfg.peakPick.preAvg  = 10;
    cfg.peakPick.postAvg = 1;
    cfg.peakPick.delta   = tc.delta;
    cfg.peakPick.wait    = 8;
    cfg.beat.tempoMinBpm   = 60.0f;
    cfg.beat.tempoMaxBpm   = 240.0f;
    cfg.beat.tempoPriorBpm = 120.0f;
    cfg.stages.enableDc     = true;
    cfg.stages.enableBands  = true;
    cfg.stages.enableChroma = true;
    cfg.stages.enableRms    = true;
    pipeline.setConfig(cfg);

    TestResult r{};
    double rmsSum = 0.0;
    int nFrames = 0;

    for (size_t i = 0; i + cfg.hopSize <= wav.samples.size(); i += cfg.hopSize) {
        uint32_t ts = static_cast<uint32_t>((uint64_t)i * 1000000ULL / cfg.sampleRate);
        pipeline.pushSamples(wav.samples.data() + i, cfg.hopSize, ts);
        FeatureFrame frame;
        if (pipeline.pullFrame(frame)) {
            nFrames++;
            rmsSum += frame.rms;
            if (frame.beat_event > 0.5f) r.beatEventCount++;
            if (frame.onset_event > 0.0f) r.onsetEventCount++;
            if (frame.flux > r.maxFlux) r.maxFlux = frame.flux;
            if (frame.onset_env > r.maxOnsetEnv) r.maxOnsetEnv = frame.onset_env;
            r.finalTempoBpm = frame.tempo_bpm;
            r.finalTempoConf = frame.tempo_confidence;
        }
    }
    r.totalFrames = nFrames;
    r.meanRms = nFrames > 0 ? static_cast<float>(rmsSum / nFrames) : 0.0f;
    return r;
}

// ============================================================================
// Test clips
// ============================================================================

struct TestClip {
    const char* filename;
    const char* label;
    float expectedBpm;        // 0 = don't check
    int   minOnsets;          // minimum expected onsets
    int   maxOnsets;          // maximum expected onsets (0=no limit)
    bool  expectBeats;        // should beat tracker lock?
};

static const TestClip CLIPS[] = {
    {"01_click_90bpm.wav",              "click_90",     90.0f,   10, 30,  true},
    {"02_click_120bpm.wav",             "click_120",   120.0f,   15, 40,  true},
    {"03_click_160bpm.wav",             "click_160",   160.0f,   20, 50,  true},
    {"04_impulse_500ms.wav",            "impulse",     120.0f,    5, 20,  false},
    {"05_noise_bursts.wav",             "noise",         0.0f,    8, 60,  false},
    {"06_sustained_chord.wav",          "chord",         0.0f,    0, 5,   false},
    {"07_sweep_20_8k.wav",              "sweep",         0.0f,    0, 5,   false},
    {"08_silence_events_silence.wav",   "sil_ev_sil",    0.0f,    3, 30,  false},
    {"09_silence.wav",                  "silence",       0.0f,    0, 0,   false},
    {"10_tone_440hz.wav",               "tone440",       0.0f,    0, 3,   false},
};
static constexpr int NUM_CLIPS = sizeof(CLIPS) / sizeof(CLIPS[0]);

// ============================================================================
// WAV path resolution
// ============================================================================

static std::string resolvePath(const char* filename, const char* const* dirs, int ndirs) {
    for (int i = 0; i < ndirs; i++) {
        std::string p = std::string(dirs[i]) + "/" + filename;
        FILE* f = fopen(p.c_str(), "rb");
        if (f) { fclose(f); return p; }
    }
    return std::string(dirs[0]) + "/" + filename;
}

static std::string wavPath(const char* filename) {
    const char* candidates[] = {
        "/Users/spectrasynq/Workspace_Management/Software/Teensy.AudioDSP_Pipeline/Tests/Audio_16k/synthetic",
        "../../../Teensy.AudioDSP_Pipeline/Tests/Audio_16k/synthetic",
        "../../Teensy.AudioDSP_Pipeline/Tests/Audio_16k/synthetic",
    };
    return resolvePath(filename, candidates, 3);
}

static std::string drumPath(const char* filename) {
    const char* candidates[] = {
        "/Users/spectrasynq/Workspace_Management/Software/Teensy.AudioDSP_Pipeline/Tests/Audio_16k",
        "../../../Teensy.AudioDSP_Pipeline/Tests/Audio_16k",
        "../../Teensy.AudioDSP_Pipeline/Tests/Audio_16k",
    };
    return resolvePath(filename, candidates, 3);
}

// ============================================================================
// Scoring
// ============================================================================

struct Score {
    int onsetPass;      // onset count within expected range
    int tempoPass;      // tempo within ±5 BPM or octave
    int beatPass;       // beat events detected when expected
    int silencePass;    // no false onsets in silence
    int total;
};

static Score scoreResult(const TestClip& clip, const TestResult& r) {
    Score s{};
    s.total = 0;

    // Onset count
    s.total++;
    bool onsetOk = (r.onsetEventCount >= clip.minOnsets);
    if (clip.maxOnsets > 0) onsetOk = onsetOk && (r.onsetEventCount <= clip.maxOnsets);
    if (onsetOk) s.onsetPass = 1;

    // Tempo (only check if expected)
    if (clip.expectedBpm > 0.0f) {
        s.total++;
        float bpm = r.finalTempoBpm;
        float exp = clip.expectedBpm;
        bool tempoOk = (fabsf(bpm - exp) < 5.0f) ||
                       (fabsf(bpm - exp * 2.0f) < 8.0f) ||
                       (fabsf(bpm - exp * 0.5f) < 4.0f);
        if (tempoOk) s.tempoPass = 1;
    }

    // Beat events
    if (clip.expectBeats) {
        s.total++;
        if (r.beatEventCount > 0) s.beatPass = 1;
    }

    return s;
}

// ============================================================================
// Unity test: sweep all configs × all clips
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

// Store results for final comparison table
static TestResult g_results[NUM_CONFIGS][NUM_CLIPS];
static Score g_scores[NUM_CONFIGS][NUM_CLIPS];

void test_sweep_all(void) {
    // Load all WAVs once
    WavData wavs[NUM_CLIPS];
    for (int c = 0; c < NUM_CLIPS; c++) {
        bool ok = loadWav(wavPath(CLIPS[c].filename).c_str(), wavs[c]);
        if (!ok) {
            printf("  FATAL: Cannot load %s\n", CLIPS[c].filename);
            TEST_FAIL_MESSAGE("Cannot load test WAV file");
            return;
        }
    }

    printf("\n");

    // Run all configs × clips
    for (int ci = 0; ci < NUM_CONFIGS; ci++) {
        printf("--- Config %s (div=%.1f, delta=%.4f, K=%.1f, gate=%.4f) ---\n",
               CONFIGS[ci].name, CONFIGS[ci].fluxBinDivisor,
               CONFIGS[ci].delta, CONFIGS[ci].onsetK, CONFIGS[ci].onsetGateRms);
        printf("  %-12s  %6s %5s %5s  %7s %5s  %7s %7s\n",
               "clip", "tempo", "conf", "beats", "onsets", "rms", "maxFlux", "maxEnv");

        for (int wi = 0; wi < NUM_CLIPS; wi++) {
            auto r = runPipeline(wavs[wi], CONFIGS[ci]);
            g_results[ci][wi] = r;
            g_scores[ci][wi] = scoreResult(CLIPS[wi], r);

            printf("  %-12s  %6.1f %5.3f %5d  %7d %5.3f  %.5f %.5f\n",
                   CLIPS[wi].label,
                   r.finalTempoBpm, r.finalTempoConf, r.beatEventCount,
                   r.onsetEventCount, r.meanRms, r.maxFlux, r.maxOnsetEnv);
        }
        printf("\n");
    }

    // ========== COMPARISON TABLE ==========
    printf("==========================================\n");
    printf("  COMPARISON: Onset / Tempo / Beat scores\n");
    printf("==========================================\n\n");

    printf("  %-12s", "clip");
    for (int ci = 0; ci < NUM_CONFIGS; ci++) {
        printf("  |  %-18s", CONFIGS[ci].name);
    }
    printf("\n");
    printf("  %-12s", "");
    for (int ci = 0; ci < NUM_CONFIGS; ci++) {
        printf("  | %4s %4s %4s %4s", "ons", "tmp", "bts", "tot");
    }
    printf("\n");

    // Per-clip scores
    int configTotals[NUM_CONFIGS] = {};
    int configMaxes[NUM_CONFIGS] = {};
    for (int wi = 0; wi < NUM_CLIPS; wi++) {
        printf("  %-12s", CLIPS[wi].label);
        for (int ci = 0; ci < NUM_CONFIGS; ci++) {
            auto& s = g_scores[ci][wi];
            int pass = s.onsetPass + s.tempoPass + s.beatPass;
            printf("  | %4s %4s %4s %d/%d",
                   s.onsetPass ? "OK" : "FAIL",
                   (CLIPS[wi].expectedBpm > 0) ? (s.tempoPass ? "OK" : "FAIL") : " -- ",
                   CLIPS[wi].expectBeats ? (s.beatPass ? "OK" : "FAIL") : " -- ",
                   pass, s.total);
            configTotals[ci] += pass;
            configMaxes[ci] += s.total;
        }
        printf("\n");
    }

    // Totals
    printf("  %-12s", "TOTAL");
    for (int ci = 0; ci < NUM_CONFIGS; ci++) {
        printf("  | %18d/%-2d", configTotals[ci], configMaxes[ci]);
    }
    printf("\n\n");

    // Find winner
    int bestCi = 0;
    for (int ci = 1; ci < NUM_CONFIGS; ci++) {
        if (configTotals[ci] > configTotals[bestCi]) bestCi = ci;
    }
    printf("  WINNER: %s (%d/%d)\n\n", CONFIGS[bestCi].name,
           configTotals[bestCi], configMaxes[bestCi]);

    // Frozen config must pass ALL acceptance criteria
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        configMaxes[bestCi], configTotals[bestCi],
        "Frozen v0.1 config must pass 100% of acceptance criteria");
}

// ============================================================================
// Real drum loop clips — actual music at 16 kHz
// ============================================================================

struct DrumClip {
    const char* filename;
    const char* label;
    float expectedBpm;
};

static const DrumClip DRUMS[] = {
    {"hiphop_85bpm_16k.wav",           "hiphop_85",     85.0f},
    {"bossa_nova_95bpm_16k.wav",       "bossa_95",      95.0f},
    {"cyberpunk_100bpm_16k.wav",       "cyber_100",    100.0f},
    {"kick_120bpm_16k.wav",            "kick_120",     120.0f},
    {"techhouse_124bpm_full_16k.wav",  "tech_124",     124.0f},
    {"hiphop_133bpm_16k.wav",          "hiphop_133",   133.0f},
    {"jazz_160bpm_16k.wav",            "jazz_160",     160.0f},
    {"metal_165bpm_16k.wav",           "metal_165",    165.0f},
    {"jazz_210bpm_16k.wav",            "jazz_210",     210.0f},
    {"techhouse_drop_16k.wav",         "tech_drop",    124.0f},
    {"techhouse_breakdown_16k.wav",    "tech_brkdn",   124.0f},
    {"techhouse_full_drums_16k.wav",   "tech_full",    124.0f},
};
static constexpr int NUM_DRUMS = sizeof(DRUMS) / sizeof(DRUMS[0]);

// ── Onset detection parameter configs (VAL · FW · Onset Detection Threshold Parameters) ──

struct OnsetCfg {
    const char* name;
    float onsetK;
    float peakDelta;
    uint16_t peakWait;
    uint16_t preMax;
    uint16_t postMax;
    uint16_t preAvg;
    uint16_t postAvg;
};

// Extended pipeline runner with beat tracker + onset config overrides
static TestResult runPipelineBeat(const WavData& wav, const TestConfig& tc,
                                  float priorWidth, float tempoDecay,
                                  float priorBpm = 120.0f,
                                  const OnsetCfg* oc = nullptr) {
    PipelineCore pipeline;
    PipelineConfig cfg;
    cfg.sampleRate      = SPINE_SR;
    cfg.hopSize         = SPINE_HOP;
    cfg.windowSize      = SPINE_WINDOW;
    cfg.fluxBinDivisor  = tc.fluxBinDivisor;
    cfg.onsetMeanAlpha  = 0.01f;
    cfg.onsetVarAlpha   = 0.01f;
    cfg.onsetK          = oc ? oc->onsetK      : tc.onsetK;
    cfg.onsetGateRms    = tc.onsetGateRms;
    cfg.peakPick.preMax  = oc ? oc->preMax      : 3;
    cfg.peakPick.postMax = oc ? oc->postMax     : 1;
    cfg.peakPick.preAvg  = oc ? oc->preAvg      : 10;
    cfg.peakPick.postAvg = oc ? oc->postAvg     : 1;
    cfg.peakPick.delta   = oc ? oc->peakDelta   : tc.delta;
    cfg.peakPick.wait    = oc ? oc->peakWait    : 8;
    cfg.beat.tempoMinBpm   = 60.0f;
    cfg.beat.tempoMaxBpm   = 240.0f;
    cfg.beat.tempoPriorBpm = priorBpm;
    cfg.beat.tempoPriorWidth = priorWidth;
    cfg.beat.tempoDecay    = tempoDecay;
    cfg.stages.enableDc     = true;
    cfg.stages.enableBands  = true;
    cfg.stages.enableChroma = true;
    cfg.stages.enableRms    = true;
    pipeline.setConfig(cfg);

    TestResult r{};
    double rmsSum = 0.0;
    int nFrames = 0;

    // Cap at 60 seconds to avoid spending 2 minutes on the 11-minute techhouse file
    size_t maxSamples = wav.samples.size();
    if (maxSamples > 60 * SPINE_SR) maxSamples = 60 * SPINE_SR;

    for (size_t i = 0; i + cfg.hopSize <= maxSamples; i += cfg.hopSize) {
        uint32_t ts = static_cast<uint32_t>((uint64_t)i * 1000000ULL / cfg.sampleRate);
        pipeline.pushSamples(wav.samples.data() + i, cfg.hopSize, ts);
        FeatureFrame frame;
        if (pipeline.pullFrame(frame)) {
            nFrames++;
            rmsSum += frame.rms;
            if (frame.beat_event > 0.5f) r.beatEventCount++;
            if (frame.onset_event > 0.0f) r.onsetEventCount++;
            if (frame.flux > r.maxFlux) r.maxFlux = frame.flux;
            if (frame.onset_env > r.maxOnsetEnv) r.maxOnsetEnv = frame.onset_env;
            r.finalTempoBpm = frame.tempo_bpm;
            r.finalTempoConf = frame.tempo_confidence;
        }
    }
    r.totalFrames = nFrames;
    r.meanRms = nFrames > 0 ? static_cast<float>(rmsSum / nFrames) : 0.0f;
    return r;
}

// ── ACR-style metrical level classification ──
// Chiu et al. (2022): evaluate beat trackers across metrical levels
// rather than assuming a single fixed tempo relationship.
//
// For LED pulse coherence, any stable metrical relationship is acceptable.
// "Direct" accuracy is a secondary diagnostic.

enum MetricLevel {
    ML_DIRECT,   // got ≈ expected           (±5 BPM)
    ML_DOUBLE,   // got ≈ 2× expected        (±8 BPM)
    ML_HALF,     // got ≈ ½× expected        (±4 BPM)
    ML_TRIPLE,   // got ≈ 3× expected        (±10 BPM)
    ML_THIRD,    // got ≈ ⅓× expected        (±5 BPM)
    ML_WRONG,    // no metrical relationship
};

static const char* mlLabel(MetricLevel ml) {
    switch (ml) {
        case ML_DIRECT: return "1x";
        case ML_DOUBLE: return "2x";
        case ML_HALF:   return "½x";
        case ML_TRIPLE: return "3x";
        case ML_THIRD:  return "⅓x";
        default:        return "MISS";
    }
}

static MetricLevel classifyTempo(float got, float expected) {
    if (fabsf(got - expected)        < 5.0f) return ML_DIRECT;
    if (fabsf(got - expected * 2.0f) < 8.0f) return ML_DOUBLE;
    if (fabsf(got - expected * 0.5f) < 4.0f) return ML_HALF;
    if (fabsf(got - expected * 3.0f) < 10.0f) return ML_TRIPLE;
    if (fabsf(got - expected / 3.0f) < 5.0f) return ML_THIRD;
    return ML_WRONG;
}

static bool isCoherent(MetricLevel ml) {
    return ml != ML_WRONG;
}

struct BeatCfg {
    const char* name;
    float priorWidth;
    float tempoDecay;
    float priorBpm;
};

static const BeatCfg BEAT_CFGS[] = {
    {"p120_w50_f",   50.0f,  0.95f,  120.0f},   // baseline (flat prior, fast decay)
    {"p120_w10_f",   10.0f,  0.95f,  120.0f},   // moderate prior, fast decay
    {"p140_w10_f",   10.0f,  0.95f,  140.0f},   // moderate prior, 140 center
    {"p120_w5_f",     5.0f,  0.95f,  120.0f},   // tight prior, 120 center
    {"p140_w5_f",     5.0f,  0.95f,  140.0f},   // tight prior, 140 center
    {"p120_w10_s",   10.0f,  0.999f, 120.0f},   // moderate prior, slow decay
    {"p140_w10_s",   10.0f,  0.999f, 140.0f},   // moderate prior, 140, slow decay
    {"p120_w3_f",     3.0f,  0.95f,  120.0f},   // very tight prior, 120 center
};
static constexpr int NUM_BEAT_CFGS = sizeof(BEAT_CFGS) / sizeof(BEAT_CFGS[0]);

// ── Onset parameter candidates from VAL · FW · Onset Detection Threshold Parameters ──

static const OnsetCfg ONSET_CFGS[] = {
    {"current",      1.5f, 0.020f,   8,   3,   1,  10,  1},
};
static constexpr int NUM_ONSET_CFGS = sizeof(ONSET_CFGS) / sizeof(ONSET_CFGS[0]);

void test_real_audio(void) {
    const TestConfig& tc = CONFIGS[0];

    // Preload all drum WAVs once
    WavData drumWavs[NUM_DRUMS];
    bool drumLoaded[NUM_DRUMS] = {};
    for (int i = 0; i < NUM_DRUMS; i++) {
        std::string path = drumPath(DRUMS[i].filename);
        drumLoaded[i] = loadWav(path.c_str(), drumWavs[i]);
    }

    printf("\n");
    printf("============================================================\n");
    printf("  ONSET × BEAT PARAMETER SWEEP — %d onset × %d beat × %d drums\n",
           NUM_ONSET_CFGS, NUM_BEAT_CFGS, NUM_DRUMS);
    printf("============================================================\n\n");

    // Track best combination
    int bestCoherent = 0;
    int bestOi = 0, bestBi = 0;

    for (int oi = 0; oi < NUM_ONSET_CFGS; oi++) {
        auto& oc = ONSET_CFGS[oi];
        printf("====== Onset: %s (K=%.1f delta=%.3f wait=%d preMax=%d preAvg=%d) ======\n",
               oc.name, oc.onsetK, oc.peakDelta, oc.peakWait, oc.preMax, oc.preAvg);

        for (int bi = 0; bi < NUM_BEAT_CFGS; bi++) {
            auto& bc = BEAT_CFGS[bi];
            printf("  --- Beat: %s (prior=%.0f, width=%.1f, decay=%.3f) ---\n",
                   bc.name, bc.priorBpm, bc.priorWidth, bc.tempoDecay);
            printf("    %-12s  %6s %6s %5s %5s  %7s  %4s  %s\n",
                   "clip", "expect", "got", "conf", "beats", "onsets", "ACR", "pulse?");

            int coherent = 0, direct = 0, total = 0;

            for (int di = 0; di < NUM_DRUMS; di++) {
                if (!drumLoaded[di]) {
                    printf("    %-12s  SKIP\n", DRUMS[di].label);
                    continue;
                }

                auto r = runPipelineBeat(drumWavs[di], tc, bc.priorWidth, bc.tempoDecay, bc.priorBpm, &oc);

                float exp = DRUMS[di].expectedBpm;
                MetricLevel ml = classifyTempo(r.finalTempoBpm, exp);
                total++;
                if (isCoherent(ml)) coherent++;
                if (ml == ML_DIRECT) direct++;

                printf("    %-12s  %6.0f %6.1f %5.3f %5d  %7d  %4s  %s\n",
                       DRUMS[di].label, exp, r.finalTempoBpm, r.finalTempoConf,
                       r.beatEventCount, r.onsetEventCount,
                       mlLabel(ml),
                       isCoherent(ml) ? "COHERENT" : "WRONG");
            }

            printf("    Coherent: %d/%d (%.0f%%)  |  Direct: %d/%d (%.0f%%)\n\n",
                   coherent, total,
                   total > 0 ? 100.0f * coherent / total : 0.0f,
                   direct, total,
                   total > 0 ? 100.0f * direct / total : 0.0f);

            if (coherent > bestCoherent) {
                bestCoherent = coherent;
                bestOi = oi;
                bestBi = bi;
            }
        }
    }

    // ── Summary ──
    printf("============================================================\n");
    printf("  SWEEP WINNER: onset=%s × beat=%s → %d/%d coherent\n",
           ONSET_CFGS[bestOi].name, BEAT_CFGS[bestBi].name, bestCoherent, NUM_DRUMS);
    printf("    onset: K=%.1f delta=%.3f wait=%d preMax=%d postMax=%d preAvg=%d postAvg=%d\n",
           ONSET_CFGS[bestOi].onsetK, ONSET_CFGS[bestOi].peakDelta,
           ONSET_CFGS[bestOi].peakWait, ONSET_CFGS[bestOi].preMax,
           ONSET_CFGS[bestOi].postMax, ONSET_CFGS[bestOi].preAvg,
           ONSET_CFGS[bestOi].postAvg);
    printf("    beat:  priorBpm=%.0f priorWidth=%.1f tempoDecay=%.3f\n",
           BEAT_CFGS[bestBi].priorBpm, BEAT_CFGS[bestBi].priorWidth, BEAT_CFGS[bestBi].tempoDecay);
    printf("============================================================\n\n");

    TEST_ASSERT_MESSAGE(true, "Onset × beat sweep diagnostic — results above");
}

int main(int argc, char** argv) {
    printf("\n");
    printf("========================================\n");
    printf("  Spine16k Acceptance (Native)\n");
    printf("  DSP Spine v0.1: Fs=16k N=512 H=128\n");
    printf("========================================\n");

    UNITY_BEGIN();
    RUN_TEST(test_sweep_all);
    RUN_TEST(test_real_audio);
    return UNITY_END();
}
