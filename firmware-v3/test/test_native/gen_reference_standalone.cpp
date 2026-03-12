/**
 * Standalone reference trajectory generator — no PlatformIO, no Unity.
 *
 * Compiles directly with g++ against firmware headers + mock stubs.
 * Produces reference_pairs.bin (v2 multi-snapshot format) for the
 * K1 Testbed calibration bridge.
 *
 * v2 format captures snapshots at multiple frame indices per parameter
 * combination. This avoids the v1 problem where uint16 truncation drove
 * energy to zero at late frames for low-persistence combos.
 *
 * Binary layout:
 *   Header (28+ bytes):
 *     magic:       uint32  "LWRF" = 0x4C575246
 *     version:     uint32  2
 *     numPairs:    uint32  number of parameter combinations
 *     radialLen:   uint32  radial buffer length (80)
 *     numSnaps:    uint32  snapshots per pair
 *     snapFrames:  uint32[numSnaps]  frame indices for each snapshot
 *
 *   Per pair (repeated numPairs times):
 *     params:      float[6]  {offset, persistence, diffusion, dt, amount, spread}
 *     Per snapshot (repeated numSnaps times):
 *       hist:      uint16[radialLen * 3]  RGB16 history buffer
 *       tonemapped: uint8[radialLen * 3]  tone-mapped 8-bit output
 *
 * Build:
 *   g++ -std=c++17 -O2 -DNATIVE_BUILD=1 -DLIGHTWAVEOS_V2=1 -DNUM_LEDS=320 \
 *       -DCENTER_POINT=80 -I../../src -I. -Imocks \
 *       gen_reference_standalone.cpp mocks/fastled_mock.cpp \
 *       mocks/arduino_mock.cpp mocks/freertos_mock.cpp \
 *       -o gen_reference -lm
 *
 * Run:
 *   ./gen_reference [output_path]
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdlib>

// Pull in the mock infrastructure
#include "mocks/fastled_mock.h"

// Now include the real firmware headers
#define private public
#include "effects/ieffect/BeatPulseTransportCore.h"
#undef private

using namespace lightwaveos::effects::ieffect;

// ─── Parameters ───────────────────────────────────────────────────────────────

static constexpr uint32_t MAGIC   = 0x4C575246;  // "LWRF"
static constexpr uint32_t VERSION = 2;            // v2: multi-snapshot
static constexpr uint16_t RADIAL_LEN = 80;

// Snapshot frame indices — chosen to capture:
//   [1]  injection + single advection (verifies inject + first step)
//   [5]  early propagation (high energy, tests advection kernel)
//   [10] mid propagation (spread visible, diffusion effects)
//   [25] late propagation (edge sink effects, decay shape)
//   [50] terminal state (verifies long-term behavior for high-persistence combos)
static constexpr uint32_t SNAPSHOT_FRAMES[] = {1, 5, 10, 25, 50};
static constexpr uint32_t NUM_SNAPSHOTS = sizeof(SNAPSHOT_FRAMES) / sizeof(uint32_t);
static constexpr uint32_t MAX_FRAME = SNAPSHOT_FRAMES[NUM_SNAPSHOTS - 1];

// Sweep grid
static constexpr float OFFSET_VALUES[]      = {0.5f, 1.0f, 2.0f, 4.0f};
static constexpr float PERSISTENCE_VALUES[] = {0.90f, 0.95f, 0.99f};
static constexpr float DIFFUSION_VALUES[]   = {0.0f, 0.1f, 0.3f, 0.5f};
static constexpr float DT_VALUES[]          = {0.008f, 0.016f};
static constexpr float AMOUNT_VALUES[]      = {0.5f, 1.0f};
static constexpr float SPREAD_VALUES[]      = {0.2f, 0.5f, 0.8f};

static constexpr uint32_t NUM_OFFSET      = sizeof(OFFSET_VALUES) / sizeof(float);
static constexpr uint32_t NUM_PERSISTENCE = sizeof(PERSISTENCE_VALUES) / sizeof(float);
static constexpr uint32_t NUM_DIFFUSION   = sizeof(DIFFUSION_VALUES) / sizeof(float);
static constexpr uint32_t NUM_DT          = sizeof(DT_VALUES) / sizeof(float);
static constexpr uint32_t NUM_AMOUNT      = sizeof(AMOUNT_VALUES) / sizeof(float);
static constexpr uint32_t NUM_SPREAD      = sizeof(SPREAD_VALUES) / sizeof(float);

static constexpr uint32_t TOTAL_PAIRS =
    NUM_OFFSET * NUM_PERSISTENCE * NUM_DIFFUSION * NUM_DT * NUM_AMOUNT * NUM_SPREAD;

// Tone mapping (matches firmware kneeToneMap with DEFAULT_KNEE = 0.5)
static constexpr float KNEE = 0.5f;

static inline float kneeToneMap(float x) {
    if (x <= 0.0f) return 0.0f;
    return x / (x + KNEE) * (1.0f + KNEE);
}

// ─── Binary writer helpers ───────────────────────────────────────────────────

static void writeHeader(FILE* f) {
    uint32_t magic      = MAGIC;
    uint32_t version    = VERSION;
    uint32_t numPairs   = TOTAL_PAIRS;
    uint32_t radialLen  = RADIAL_LEN;
    uint32_t numSnaps   = NUM_SNAPSHOTS;

    fwrite(&magic,     sizeof(uint32_t), 1, f);
    fwrite(&version,   sizeof(uint32_t), 1, f);
    fwrite(&numPairs,  sizeof(uint32_t), 1, f);
    fwrite(&radialLen, sizeof(uint32_t), 1, f);
    fwrite(&numSnaps,  sizeof(uint32_t), 1, f);
    fwrite(SNAPSHOT_FRAMES, sizeof(uint32_t), NUM_SNAPSHOTS, f);
}

static void writeSnapshot(
    FILE* f,
    const BeatPulseTransportCore::RGB16* hist_state,
    const uint8_t* tone_mapped_state)
{
    fwrite(hist_state, sizeof(uint16_t) * 3, RADIAL_LEN, f);
    fwrite(tone_mapped_state, sizeof(uint8_t), RADIAL_LEN * 3, f);
}

static void convertToToneMapped(
    const BeatPulseTransportCore::RGB16* hist_state,
    uint8_t* tone_mapped_state)
{
    for (uint16_t i = 0; i < RADIAL_LEN; i++) {
        float r01 = static_cast<float>(hist_state[i].r) / 65535.0f;
        float g01 = static_cast<float>(hist_state[i].g) / 65535.0f;
        float b01 = static_cast<float>(hist_state[i].b) / 65535.0f;

        r01 = kneeToneMap(r01);
        g01 = kneeToneMap(g01);
        b01 = kneeToneMap(b01);

        uint32_t r8 = static_cast<uint32_t>(r01 * 255.0f + 0.5f);
        uint32_t g8 = static_cast<uint32_t>(g01 * 255.0f + 0.5f);
        uint32_t b8 = static_cast<uint32_t>(b01 * 255.0f + 0.5f);

        if (r8 > 255u) r8 = 255u;
        if (g8 > 255u) g8 = 255u;
        if (b8 > 255u) b8 = 255u;

        tone_mapped_state[i * 3 + 0] = static_cast<uint8_t>(r8);
        tone_mapped_state[i * 3 + 1] = static_cast<uint8_t>(g8);
        tone_mapped_state[i * 3 + 2] = static_cast<uint8_t>(b8);
    }
}

static uint64_t computeEnergy(const BeatPulseTransportCore::RGB16* hist_state) {
    uint64_t energy = 0;
    for (uint16_t i = 0; i < RADIAL_LEN; i++) {
        energy += hist_state[i].r + hist_state[i].g + hist_state[i].b;
    }
    return energy;
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    const char* output_path = "reference_pairs.bin";
    if (argc > 1) {
        output_path = argv[1];
    }

    // Per-snapshot byte size: uint16[80*3] + uint8[80*3] = 480 + 240 = 720
    const size_t snapBytes = RADIAL_LEN * 3 * sizeof(uint16_t) + RADIAL_LEN * 3 * sizeof(uint8_t);
    // Header size: 5 * uint32 + NUM_SNAPSHOTS * uint32
    const size_t headerBytes = 5 * sizeof(uint32_t) + NUM_SNAPSHOTS * sizeof(uint32_t);
    // Per-pair: 6 floats + NUM_SNAPSHOTS * snapBytes
    const size_t pairBytes = 6 * sizeof(float) + NUM_SNAPSHOTS * snapBytes;
    const size_t totalBytes = headerBytes + TOTAL_PAIRS * pairBytes;

    printf("SpectraSynq K1 Testbed — Reference Trajectory Generator (v2)\n");
    printf("Grid: %u offset × %u persist × %u diff × %u dt × %u amount × %u spread = %u pairs\n",
           NUM_OFFSET, NUM_PERSISTENCE, NUM_DIFFUSION, NUM_DT, NUM_AMOUNT, NUM_SPREAD, TOTAL_PAIRS);
    printf("Snapshots per pair: %u at frames [", NUM_SNAPSHOTS);
    for (uint32_t s = 0; s < NUM_SNAPSHOTS; s++) {
        printf("%u%s", SNAPSHOT_FRAMES[s], s + 1 < NUM_SNAPSHOTS ? ", " : "");
    }
    printf("]\n");
    printf("Radial length: %u\n", RADIAL_LEN);
    printf("Estimated size: %lu bytes (%.1f KB)\n", (unsigned long)totalBytes, totalBytes / 1024.0);
    printf("Output: %s\n\n", output_path);

    FILE* f = fopen(output_path, "wb");
    if (!f) {
        fprintf(stderr, "ERROR: Cannot open %s for writing\n", output_path);
        return 1;
    }

    writeHeader(f);

    BeatPulseTransportCore core;
    uint8_t tone_mapped[RADIAL_LEN * 3];
    uint32_t pair_count = 0;
    uint32_t zero_snap_count = 0;
    uint32_t total_snap_count = 0;

    for (uint32_t oi = 0; oi < NUM_OFFSET; oi++) {
        float offset = OFFSET_VALUES[oi];
        for (uint32_t pi = 0; pi < NUM_PERSISTENCE; pi++) {
            float persistence = PERSISTENCE_VALUES[pi];
            for (uint32_t di = 0; di < NUM_DIFFUSION; di++) {
                float diffusion = DIFFUSION_VALUES[di];
                for (uint32_t ti = 0; ti < NUM_DT; ti++) {
                    float dt = DT_VALUES[ti];
                    for (uint32_t ai = 0; ai < NUM_AMOUNT; ai++) {
                        float amount = AMOUNT_VALUES[ai];
                        for (uint32_t si = 0; si < NUM_SPREAD; si++) {
                            float spread = SPREAD_VALUES[si];

                            core.resetAll();
                            core.setNowMs(0);

                            CRGB white(255, 255, 255);
                            core.injectAtCentre(0, RADIAL_LEN, white, amount, spread);

                            // Write params once per pair
                            float params[6] = {offset, persistence, diffusion, dt, amount, spread};
                            fwrite(params, sizeof(float), 6, f);

                            uint32_t snapIdx = 0;
                            for (uint32_t frame = 1; frame <= MAX_FRAME; frame++) {
                                core.advectOutward(0, RADIAL_LEN, offset, persistence, diffusion, dt);
                                core.setNowMs(static_cast<uint32_t>(core.m_nowMs + (dt * 1000.0f)));

                                if (snapIdx < NUM_SNAPSHOTS && frame == SNAPSHOT_FRAMES[snapIdx]) {
                                    const BeatPulseTransportCore::RGB16* state = core.m_ps->hist[0];
                                    convertToToneMapped(state, tone_mapped);
                                    writeSnapshot(f, state, tone_mapped);

                                    uint64_t energy = computeEnergy(state);
                                    total_snap_count++;
                                    if (energy == 0) zero_snap_count++;

                                    snapIdx++;
                                }
                            }

                            pair_count++;
                            if (pair_count % 100 == 0) {
                                printf("  %u / %u pairs\n", pair_count, TOTAL_PAIRS);
                            }
                        }
                    }
                }
            }
        }
    }

    fclose(f);

    printf("\nDone. Wrote %u pairs × %u snapshots = %u total snapshots\n",
           pair_count, NUM_SNAPSHOTS, total_snap_count);
    printf("Zero-energy snapshots: %u / %u (%.1f%%)\n",
           zero_snap_count, total_snap_count,
           total_snap_count > 0 ? 100.0 * zero_snap_count / total_snap_count : 0.0);
    printf("File size: %lu bytes (%.1f KB)\n",
           (unsigned long)(headerBytes + pair_count * pairBytes),
           (headerBytes + pair_count * pairBytes) / 1024.0);
    printf("Output: %s\n", output_path);

    return 0;
}
