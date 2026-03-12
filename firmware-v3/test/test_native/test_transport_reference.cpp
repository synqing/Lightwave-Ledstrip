/**
 * @file test_transport_reference.cpp
 * @brief Generate reference (parameter, output) pairs from BeatPulseTransportCore
 *
 * This test harness runs the REAL BeatPulseTransportCore through a comprehensive
 * grid of parameter combinations and records the final state after 100 frames.
 * These pairs serve as ground truth for calibrating the PyTorch port.
 *
 * Output: reference_pairs.bin
 *
 * Binary format:
 *   Header:
 *     uint32_t magic = 0x4C575246  ("LWRF")
 *     uint32_t version = 1
 *     uint32_t num_pairs
 *     uint32_t radial_len (80)
 *     uint32_t num_frames (100)
 *
 *   Per pair:
 *     float params[6]  (offset, persistence, diffusion, dt, amount, spread)
 *     uint16_t final_state_16bit[80*3]  (radial × RGB, uint16)
 *     uint8_t final_state_8bit[80*3]    (radial × RGB, uint8 after tone map)
 */

#include "unity.h"
#include "effects/ieffect/BeatPulseTransportCore.h"
#include "effects/ieffect/BeatPulseRenderUtils.h"
#include "plugins/api/EffectContext.h"
#include <cstdio>
#include <cstring>
#include <cmath>

using namespace lightwaveos::effects::ieffect;
using namespace lightwaveos::plugins;

// Forward declare the private RGB16 struct by making it accessible
#define private public
#include "effects/ieffect/BeatPulseTransportCore.h"
#undef private

// Constants
static constexpr uint16_t RADIAL_LEN = 80;
static constexpr uint32_t NUM_FRAMES = 100;
static constexpr const char* OUTPUT_FILE = "reference_pairs.bin";
static constexpr uint32_t MAGIC = 0x4C575246;  // "LWRF"
static constexpr uint32_t VERSION = 1;

// Parameter grid
static constexpr float OFFSET_VALUES[] = {0.5f, 1.0f, 2.0f, 4.0f};
static constexpr float PERSISTENCE_VALUES[] = {0.90f, 0.95f, 0.99f};
static constexpr float DIFFUSION_VALUES[] = {0.0f, 0.3f, 0.6f, 1.0f};
static constexpr float DT_VALUES[] = {0.008f, 0.016f};  // 120 FPS, 60 FPS
static constexpr float AMOUNT_VALUES[] = {0.5f, 1.0f};
static constexpr float SPREAD_VALUES[] = {0.0f, 0.5f, 1.0f};

static constexpr uint32_t NUM_OFFSET = sizeof(OFFSET_VALUES) / sizeof(OFFSET_VALUES[0]);
static constexpr uint32_t NUM_PERSISTENCE = sizeof(PERSISTENCE_VALUES) / sizeof(PERSISTENCE_VALUES[0]);
static constexpr uint32_t NUM_DIFFUSION = sizeof(DIFFUSION_VALUES) / sizeof(DIFFUSION_VALUES[0]);
static constexpr uint32_t NUM_DT = sizeof(DT_VALUES) / sizeof(DT_VALUES[0]);
static constexpr uint32_t NUM_AMOUNT = sizeof(AMOUNT_VALUES) / sizeof(AMOUNT_VALUES[0]);
static constexpr uint32_t NUM_SPREAD = sizeof(SPREAD_VALUES) / sizeof(SPREAD_VALUES[0]);

static constexpr uint32_t TOTAL_PAIRS =
    NUM_OFFSET * NUM_PERSISTENCE * NUM_DIFFUSION * NUM_DT * NUM_AMOUNT * NUM_SPREAD;

/**
 * @brief Write binary reference data to file
 */
void writeReferencePairs(FILE* f, uint32_t numPairs) {
    TEST_ASSERT_NOT_NULL(f);

    // Write header
    uint32_t magic = MAGIC;
    uint32_t version = VERSION;
    uint32_t radial_len = RADIAL_LEN;
    uint32_t num_frames = NUM_FRAMES;

    size_t written = 0;
    written += fwrite(&magic, sizeof(uint32_t), 1, f);
    written += fwrite(&version, sizeof(uint32_t), 1, f);
    written += fwrite(&numPairs, sizeof(uint32_t), 1, f);
    written += fwrite(&radial_len, sizeof(uint32_t), 1, f);
    written += fwrite(&num_frames, sizeof(uint32_t), 1, f);

    TEST_ASSERT_EQUAL_INT(5, written);
}

/**
 * @brief Write a single reference pair (params + state)
 */
void writeReferencePair(
    FILE* f,
    float offset,
    float persistence,
    float diffusion,
    float dt,
    float amount,
    float spread,
    const BeatPulseTransportCore::RGB16* hist_state,
    const uint8_t* tone_mapped_state
) {
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_NOT_NULL(hist_state);
    TEST_ASSERT_NOT_NULL(tone_mapped_state);

    // Write params
    float params[6] = {offset, persistence, diffusion, dt, amount, spread};
    size_t written = fwrite(params, sizeof(float), 6, f);
    TEST_ASSERT_EQUAL_INT(6, written);

    // Write 16-bit HDR state (80 × 3 channels)
    written = fwrite(hist_state, sizeof(uint16_t), RADIAL_LEN * 3, f);
    TEST_ASSERT_EQUAL_INT(RADIAL_LEN * 3, written);

    // Write 8-bit tone-mapped state
    written = fwrite(tone_mapped_state, sizeof(uint8_t), RADIAL_LEN * 3, f);
    TEST_ASSERT_EQUAL_INT(RADIAL_LEN * 3, written);
}

/**
 * @brief Convert 16-bit HDR to 8-bit using tone mapping (matching firmware)
 */
void convertToToneMapped(
    const BeatPulseTransportCore::RGB16* hist_state,
    uint8_t* tone_mapped_state
) {
    TEST_ASSERT_NOT_NULL(hist_state);
    TEST_ASSERT_NOT_NULL(tone_mapped_state);

    for (uint16_t i = 0; i < RADIAL_LEN; i++) {
        // Use same tone-mapping as BeatPulseTransportCore::toCRGB8
        auto kneeToneMap = [](float in01, float knee = 0.5f) -> float {
            if (in01 <= 0.0f) return 0.0f;
            const float mapped = in01 / (in01 + knee);
            return mapped * (1.0f + knee);
        };

        // Normalise 16-bit to 0..1
        float r01 = static_cast<float>(hist_state[i].r) / 65535.0f;
        float g01 = static_cast<float>(hist_state[i].g) / 65535.0f;
        float b01 = static_cast<float>(hist_state[i].b) / 65535.0f;

        // Apply knee tone-map
        r01 = kneeToneMap(r01);
        g01 = kneeToneMap(g01);
        b01 = kneeToneMap(b01);

        // Apply gain (1.0) and convert to 8-bit
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

/**
 * @brief Main test: generate all reference pairs
 */
void test_generate_transport_reference_pairs() {
    UnityPrintf("Generating %lu transport reference pairs...\n", TOTAL_PAIRS);

    // Open output file
    FILE* f = fopen(OUTPUT_FILE, "wb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "Failed to open output file for writing");

    // Write header (with placeholder for num_pairs)
    writeReferencePairs(f, TOTAL_PAIRS);

    // Create transport core
    BeatPulseTransportCore core;

    // Buffer for tone-mapped output
    uint8_t tone_mapped_state[RADIAL_LEN * 3];

    uint32_t pair_count = 0;

    // Iterate through all parameter combinations
    for (uint32_t offset_idx = 0; offset_idx < NUM_OFFSET; offset_idx++) {
        float offset = OFFSET_VALUES[offset_idx];

        for (uint32_t persist_idx = 0; persist_idx < NUM_PERSISTENCE; persist_idx++) {
            float persistence = PERSISTENCE_VALUES[persist_idx];

            for (uint32_t diff_idx = 0; diff_idx < NUM_DIFFUSION; diff_idx++) {
                float diffusion = DIFFUSION_VALUES[diff_idx];

                for (uint32_t dt_idx = 0; dt_idx < NUM_DT; dt_idx++) {
                    float dt = DT_VALUES[dt_idx];

                    for (uint32_t amount_idx = 0; amount_idx < NUM_AMOUNT; amount_idx++) {
                        float amount = AMOUNT_VALUES[amount_idx];

                        for (uint32_t spread_idx = 0; spread_idx < NUM_SPREAD; spread_idx++) {
                            float spread = SPREAD_VALUES[spread_idx];

                            // Reset for this parameter set
                            core.resetAll();
                            core.setNowMs(0);

                            // Inject white pulse at centre
                            CRGB white_pulse(255, 255, 255);
                            core.injectAtCentre(0, RADIAL_LEN, white_pulse, amount, spread);

                            // Run 100 frames of advection
                            for (uint32_t frame = 0; frame < NUM_FRAMES; frame++) {
                                core.advectOutward(0, RADIAL_LEN, offset, persistence, diffusion, dt);
                                core.setNowMs(static_cast<uint32_t>(core.m_nowMs + (dt * 1000.0f)));
                            }

                            // Extract final state
                            const BeatPulseTransportCore::RGB16* final_state = core.m_ps->hist[0];

                            // Convert to tone-mapped 8-bit
                            convertToToneMapped(final_state, tone_mapped_state);

                            // Verify state is not all-zero (injection should produce something)
                            uint32_t total_energy = 0;
                            for (uint16_t i = 0; i < RADIAL_LEN * 3; i++) {
                                total_energy += final_state[i / 3].raw[i % 3];
                            }
                            TEST_ASSERT_GREATER_THAN_UINT32(0, total_energy);

                            // Write pair
                            writeReferencePair(f, offset, persistence, diffusion, dt, amount, spread,
                                             final_state, tone_mapped_state);

                            pair_count++;

                            if (pair_count % 100 == 0) {
                                UnityPrintf("  Wrote %lu / %lu pairs\n", pair_count, TOTAL_PAIRS);
                            }
                        }
                    }
                }
            }
        }
    }

    fclose(f);

    TEST_ASSERT_EQUAL_UINT32(TOTAL_PAIRS, pair_count);
    UnityPrintf("Successfully wrote %lu reference pairs to %s\n", pair_count, OUTPUT_FILE);
}

/**
 * @brief Verify output file structure
 */
void test_verify_reference_file_structure() {
    FILE* f = fopen(OUTPUT_FILE, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "Reference file should exist after generation");

    // Read header
    uint32_t magic, version, num_pairs, radial_len, num_frames;
    size_t read = 0;
    read += fread(&magic, sizeof(uint32_t), 1, f);
    read += fread(&version, sizeof(uint32_t), 1, f);
    read += fread(&num_pairs, sizeof(uint32_t), 1, f);
    read += fread(&radial_len, sizeof(uint32_t), 1, f);
    read += fread(&num_frames, sizeof(uint32_t), 1, f);

    TEST_ASSERT_EQUAL_INT(5, read);
    TEST_ASSERT_EQUAL_HEX32(MAGIC, magic);
    TEST_ASSERT_EQUAL_UINT32(VERSION, version);
    TEST_ASSERT_EQUAL_UINT32(TOTAL_PAIRS, num_pairs);
    TEST_ASSERT_EQUAL_UINT32(RADIAL_LEN, radial_len);
    TEST_ASSERT_EQUAL_UINT32(NUM_FRAMES, num_frames);

    // Spot-check first pair structure
    float params[6];
    read = fread(params, sizeof(float), 6, f);
    TEST_ASSERT_EQUAL_INT(6, read);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, params[0]);   // First offset
    TEST_ASSERT_EQUAL_FLOAT(0.90f, params[1]);  // First persistence

    // Read first pair's 16-bit state (check for non-zero)
    uint16_t hist_buf[RADIAL_LEN * 3];
    read = fread(hist_buf, sizeof(uint16_t), RADIAL_LEN * 3, f);
    TEST_ASSERT_EQUAL_INT(RADIAL_LEN * 3, read);

    uint32_t energy = 0;
    for (uint16_t i = 0; i < RADIAL_LEN * 3; i++) {
        energy += hist_buf[i];
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0, energy);

    // Verify file size matches expected
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fclose(f);

    // Header (20 bytes) + pairs * (6*4 + 80*3*2 + 80*3*1) bytes
    long pair_size = 6 * sizeof(float) + RADIAL_LEN * 3 * sizeof(uint16_t) + RADIAL_LEN * 3 * sizeof(uint8_t);
    long expected_size = 20 + TOTAL_PAIRS * pair_size;

    TEST_ASSERT_EQUAL_INT(expected_size, file_size);
    UnityPrintf("Reference file structure verified: %ld bytes\n", file_size);
}

/**
 * @brief Test that different parameters produce different outputs
 */
void test_transport_parameters_affect_output() {
    BeatPulseTransportCore core1, core2;
    uint8_t state1[RADIAL_LEN * 3], state2[RADIAL_LEN * 3];

    // Run with offset=0.5
    core1.resetAll();
    core1.setNowMs(0);
    CRGB white(255, 255, 255);
    core1.injectAtCentre(0, RADIAL_LEN, white, 1.0f, 0.5f);
    for (uint32_t i = 0; i < NUM_FRAMES; i++) {
        core1.advectOutward(0, RADIAL_LEN, 0.5f, 0.99f, 0.1f, 0.016f);
    }
    convertToToneMapped(core1.m_ps->hist[0], state1);

    // Run with offset=4.0 (different parameter)
    core2.resetAll();
    core2.setNowMs(0);
    core2.injectAtCentre(0, RADIAL_LEN, white, 1.0f, 0.5f);
    for (uint32_t i = 0; i < NUM_FRAMES; i++) {
        core2.advectOutward(0, RADIAL_LEN, 4.0f, 0.99f, 0.1f, 0.016f);
    }
    convertToToneMapped(core2.m_ps->hist[0], state2);

    // States should differ
    uint32_t diff_count = 0;
    for (uint16_t i = 0; i < RADIAL_LEN * 3; i++) {
        if (state1[i] != state2[i]) {
            diff_count++;
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0, diff_count);
}
