/**
 * @file golden_runner.cpp
 * @brief G2 Generalised native golden-frame runner.
 *
 * Build: see ./build.sh (canonical source-of-truth — do not duplicate flags here).
 *
 * Usage:
 *     ./golden_runner <EffectName> <ScenarioName> <OutputPath>
 *
 *     EffectName  : BeatPulseShockwaveEffect | BeatPulseStackEffect |
 *                   BeatPulseShockwaveCascadeEffect
 *     ScenarioName: silence | beat_pulse | rms_ramp | chroma_sweep
 *     OutputPath  : path to write GFRM v2 binary capture
 *
 * Output: GFRM v2 binary (see writeHeader doc-comment).
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdlib>

// native_stubs.h is force-included via -include flag.
#include "golden_context_factory.h"

#include "../../src/effects/ieffect/BeatPulseShockwaveEffect.h"
#include "../../src/effects/ieffect/BeatPulseStackEffect.h"
#include "../../src/effects/ieffect/BeatPulseShockwaveCascadeEffect.h"

namespace lwo_eff = lightwaveos::effects::ieffect;
namespace lwo_plg = lightwaveos::plugins;

// ============================================================================
// Effect factory (hardcoded switch on class name string).
// ============================================================================

static lwo_plg::IEffect* makeEffect(const char* className) {
    if (std::strcmp(className, "BeatPulseShockwaveEffect") == 0) {
        return new lwo_eff::BeatPulseShockwaveEffect(false);  // outward
    }
    if (std::strcmp(className, "BeatPulseStackEffect") == 0) {
        return new lwo_eff::BeatPulseStackEffect();
    }
    if (std::strcmp(className, "BeatPulseShockwaveCascadeEffect") == 0) {
        return new lwo_eff::BeatPulseShockwaveCascadeEffect();
    }
    return nullptr;
}

// ============================================================================
// Scenario surface — per-frame audio state mutators.
// ============================================================================

struct Scenario {
    const char* name;
    uint16_t    frameCount;
    void      (*populate)(lwo_plg::EffectContext& ctx, uint16_t frame);
};

// Scenario: silence. Audio remains unavailable.
static void scenarioSilence(lwo_plg::EffectContext& ctx, uint16_t /*frame*/) {
    ctx.audio.available = false;
}

// Scenario: single beat at frame 10, then natural decay.
static void scenarioBeatPulse(lwo_plg::EffectContext& ctx, uint16_t frame) {
    ctx.audio.available             = true;
    ctx.audio.controlBus.tempoConfidence = 0.85f;
    if (frame == 10) {
        ctx.audio.onset.beat.fired      = true;
        ctx.audio.onset.beat.level01    = 1.0f;
        ctx.audio.onset.tempoConfidence = 0.85f;
        ctx.audio.controlBus.tempoBeatTick = true;
    } else {
        ctx.audio.onset.beat.fired      = false;
        ctx.audio.onset.beat.level01    = 0.0f;
    }
}

// Scenario: linear RMS ramp 0 -> 1 across 60 frames.
static void scenarioRmsRamp(lwo_plg::EffectContext& ctx, uint16_t frame) {
    ctx.audio.available           = true;
    const float t = (frame < 60) ? (static_cast<float>(frame) / 60.0f) : 1.0f;
    ctx.audio.controlBus.rms      = t;
    ctx.audio.controlBus.fast_rms = t;
    for (uint8_t b = 0; b < 8; ++b) {
        ctx.audio.controlBus.bands[b] = t * (0.4f + 0.6f * (1.0f - static_cast<float>(b) / 7.0f));
        ctx.audio.controlBus.heavy_bands[b] = ctx.audio.controlBus.bands[b];
    }
}

// Scenario: chroma sweep — one dominant pitch class per frame, cycling C..B.
static void scenarioChromaSweep(lwo_plg::EffectContext& ctx, uint16_t frame) {
    ctx.audio.available = true;
    const uint8_t dominant = static_cast<uint8_t>(frame % 12);
    for (uint8_t c = 0; c < 12; ++c) {
        const float v = (c == dominant) ? 1.0f : 0.05f;
        ctx.audio.controlBus.chroma[c]       = v;
        ctx.audio.controlBus.heavy_chroma[c] = v;
    }
    ctx.audio.controlBus.tempoConfidence = 0.6f;
}

static const Scenario kScenarios[] = {
    { "silence",      60, scenarioSilence      },
    { "beat_pulse",   60, scenarioBeatPulse    },
    { "rms_ramp",     60, scenarioRmsRamp      },
    { "chroma_sweep", 12, scenarioChromaSweep  },
};

static const Scenario* findScenario(const char* name) {
    for (const auto& s : kScenarios) {
        if (std::strcmp(s.name, name) == 0) return &s;
    }
    return nullptr;
}

// ============================================================================
// GFRM v2 binary writer.
//
// Stream layout:
//   Header (12 bytes):
//     [0..3]   magic        "GFRM"
//     [4]      version      uint8 = 2
//     [5]      mode         uint8 (0 = unified-mirrored, 1 = dual-channel direct)
//     [6..7]   ledsPerStrip uint16 LE = 160
//     [8]      stripCount   uint8 = 2
//     [9]      reserved     uint8 = 0
//     [10..11] frameCount   uint16 LE
//   Per frame (972 bytes):
//     [0..3]   frameIndex   uint32 LE
//     [4..7]   totalTimeMs  uint32 LE
//     [8]      beatFired    uint8  (0/1)
//     [9..11]  pad          uint8[3] = 0
//     [12..491]  strip1     RGB triplets, 160 LEDs (480 bytes)
//     [492..971] strip2     RGB triplets, 160 LEDs (480 bytes)
// ============================================================================

static void writeU8 (FILE* f, uint8_t  v) { std::fwrite(&v, 1, 1, f); }
static void writeU16(FILE* f, uint16_t v) { std::fwrite(&v, 2, 1, f); }
static void writeU32(FILE* f, uint32_t v) { std::fwrite(&v, 4, 1, f); }

static void writeHeader(FILE* f, uint8_t mode, uint16_t frameCount) {
    std::fwrite("GFRM", 1, 4, f);
    writeU8 (f, /*version*/ 2);
    writeU8 (f, mode);
    writeU16(f, golden::kStripLength);
    writeU8 (f, golden::kStripCount);
    writeU8 (f, /*reserved*/ 0);
    writeU16(f, frameCount);
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::fprintf(stderr,
            "Usage: %s <EffectName> <ScenarioName> <OutputPath>\n"
            "  Effects  : BeatPulseShockwaveEffect | BeatPulseStackEffect | "
            "BeatPulseShockwaveCascadeEffect\n"
            "  Scenarios: silence | beat_pulse | rms_ramp | chroma_sweep\n",
            argv[0]);
        return 2;
    }
    const char* effectName   = argv[1];
    const char* scenarioName = argv[2];
    const char* outputPath   = argv[3];

    lwo_plg::IEffect* effect = makeEffect(effectName);
    if (!effect) {
        std::fprintf(stderr, "Unknown effect: %s\n", effectName);
        return 3;
    }

    const Scenario* scenario = findScenario(scenarioName);
    if (!scenario) {
        std::fprintf(stderr, "Unknown scenario: %s\n", scenarioName);
        delete effect;
        return 4;
    }

    FILE* out = std::fopen(outputPath, "wb");
    if (!out) {
        std::fprintf(stderr, "Cannot open output: %s\n", outputPath);
        delete effect;
        return 5;
    }

    // ---- Allocate buffers and build context (C2: full defaults) ----
    CRGB leds[golden::kLedCount];
    CRGB strip1[golden::kStripLength];
    CRGB strip2[golden::kStripLength];
    std::memset(leds,   0, sizeof(leds));
    std::memset(strip1, 0, sizeof(strip1));
    std::memset(strip2, 0, sizeof(strip2));

    auto ctx = golden::makeContext(leds, strip1, strip2);

    // ---- Init effect ----
    effect->init(ctx);

    // Per-frame buffer ring so we can write the header AFTER frame 0 lets us
    // observe ctx.dualChannelMode. Static storage = no heap.
    static uint8_t frameBuf[64][972];
    if (scenario->frameCount > 64) {
        std::fprintf(stderr, "Scenario frameCount %u exceeds buffer cap 64\n",
                     scenario->frameCount);
        delete effect;
        std::fclose(out);
        return 6;
    }

    uint8_t observedMode = 0;  // 0 = unified-mirrored, 1 = dual-channel

    for (uint16_t f = 0; f < scenario->frameCount; ++f) {
        golden::beginFrame(ctx, f);
        scenario->populate(ctx, f);

        ctx.dualChannelMode = false;  // effect must opt-in each frame
        effect->render(ctx);

        // C3: post-render canonicalisation. If the effect did NOT opt into
        // dual-channel mode, mirror unified leds[] into strip buffers
        // (matches RendererActor::renderFrame semantics). Otherwise strips
        // already hold the asymmetric output.
        if (!ctx.dualChannelMode) {
            golden::mirrorUnifiedToStrips(ctx);
        }
        if (f == 0) {
            observedMode = ctx.dualChannelMode ? 1 : 0;
        }

        // Pack frame into the ring (per-frame layout, see header doc).
        uint8_t* p = frameBuf[f];
        auto putU32 = [&](uint32_t v){ std::memcpy(p, &v, 4); p += 4; };
        auto putU8  = [&](uint8_t v) { *p++ = v; };
        putU32(static_cast<uint32_t>(f));
        putU32(ctx.rawTotalTimeMs);
        putU8(ctx.audio.onset.beat.fired ? 1u : 0u);
        putU8(0); putU8(0); putU8(0);  // pad
        for (uint16_t i = 0; i < golden::kStripLength; ++i) {
            *p++ = ctx.stripLeds[0][i].r;
            *p++ = ctx.stripLeds[0][i].g;
            *p++ = ctx.stripLeds[0][i].b;
        }
        for (uint16_t i = 0; i < golden::kStripLength; ++i) {
            *p++ = ctx.stripLeds[1][i].r;
            *p++ = ctx.stripLeds[1][i].g;
            *p++ = ctx.stripLeds[1][i].b;
        }
    }

    // ---- Write header (mode known) and frame ring ----
    writeHeader(out, observedMode, scenario->frameCount);
    for (uint16_t f = 0; f < scenario->frameCount; ++f) {
        std::fwrite(frameBuf[f], 1, 972, out);
    }

    std::fclose(out);
    delete effect;

    std::fprintf(stderr,
        "GFRM v2 capture: effect=%s scenario=%s frames=%u mode=%u -> %s\n",
        effectName, scenarioName, scenario->frameCount, observedMode, outputPath);
    return 0;
}
