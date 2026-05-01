/**
 * @file test_esv11_parity.cpp
 * @brief Parity guard for vendored Emotiscope v1.1_320 DSP pipeline (native).
 *
 * This test feeds a deterministic synthetic "music-like" signal into the ES
 * pipeline and asserts that key outputs remain stable over time.
 */

#include <unity.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Vendored ES pipeline (header-only, globals live in this TU)
#include "audio/backends/esv11/vendor/EsV11Shim.h"
#include "audio/backends/esv11/vendor/global_defines.h"
#include "audio/backends/esv11/vendor/microphone.h"
#include "audio/backends/esv11/vendor/goertzel.h"
#include "audio/backends/esv11/vendor/vu.h"
#include "audio/backends/esv11/vendor/tempo.h"
#include "audio/backends/esv11/vendor/utilities_min.h"

static constexpr float kPi = 3.14159265358979323846f;

static void es_reset_state()
{
    // Allocate heap-backed buffers (uses std::calloc on native)
    esv11_init_buffers();

    // Reset timing
    esv11_set_time(0, 0);

    // Reset microphone/DC blocker state
    dc_blocker_x_prev = 0.0f;
    dc_blocker_y_prev = 0.0f;
    memset(sample_history, 0, SAMPLE_HISTORY_LENGTH * sizeof(float));

    // Reset DSP outputs/stateful buffers (header-level arrays use sizeof)
    memset(spectrogram, 0, sizeof(spectrogram));
    memset(spectrogram_smooth, 0, sizeof(spectrogram_smooth));
    memset(spectrogram_average, 0, 12 * NUM_FREQS * sizeof(float));
    spectrogram_average_index = 0;
    memset(chromagram, 0, sizeof(chromagram));

    // Tempo globals (pointer-backed: use element count × sizeof)
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

    // VU
    init_vu();

    // Re-init tables/constants (idempotent)
    init_window_lookup();
    init_goertzel_constants();
    init_tempo_goertzel_constants();
}

static inline float clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static void feed_chunk_and_process(uint64_t chunk_index, uint64_t& sample_index, uint64_t& last_gpu_tick_us)
{
    // ES timing: 64 samples @ 12.8kHz ≈ 5000us
    const uint64_t now_us = chunk_index * 5000ULL;
    const uint32_t now_ms = static_cast<uint32_t>(now_us / 1000ULL);
    esv11_set_time(now_us, now_ms);

    // Build a deterministic synthetic signal:
    // - carrier: 220Hz sine
    // - amplitude envelope: 120 BPM pulses (2 Hz), Gaussian-ish attack
    uint32_t new_samples_raw[CHUNK_SIZE];
    float new_samples[CHUNK_SIZE];

    for (uint16_t i = 0; i < CHUNK_SIZE; ++i) {
        const float t = static_cast<float>(sample_index + i) / static_cast<float>(SAMPLE_RATE);

        const float beat_period_s = 0.5f; // 120 BPM
        float phase = fmodf(t, beat_period_s);
        // Wrap phase to [-period/2, +period/2] around beat for symmetric pulse.
        if (phase > beat_period_s * 0.5f) phase -= beat_period_s;
        const float sigma = 0.025f;
        const float pulse = expf(-0.5f * (phase * phase) / (sigma * sigma)); // 0..1
        const float env = 0.05f + 0.95f * pulse;

        const float s = sinf(2.0f * kPi * 220.0f * t) * env;

        // Convert to ES-style 18-bit signed sample embedded in a 32-bit word (>>14 yields ±131072 range).
        const float s_clamped = clampf(s, -1.0f, 1.0f);
        const int32_t sample18 = static_cast<int32_t>(lrintf(s_clamped * 131072.0f));
        const int32_t word = (sample18 << 14);
        new_samples_raw[i] = static_cast<uint32_t>(word);

        // Reuse ES capture path maths (DC blocker + clamp + scale to [-1,1]).
        float x = static_cast<float>((static_cast<int32_t>(new_samples_raw[i])) >> 14);
        float y = DC_BLOCKER_G * (x - dc_blocker_x_prev + DC_BLOCKER_R * dc_blocker_y_prev);
        dc_blocker_x_prev = x;
        dc_blocker_y_prev = y;

        if (y > 131072.0f) y = 131072.0f;
        else if (y < -131072.0f) y = -131072.0f;

        new_samples[i] = y;
    }

    dsps_mulc_f32(new_samples, new_samples, CHUNK_SIZE, recip_scale, 1, 1);
    shift_and_copy_arrays(sample_history, SAMPLE_HISTORY_LENGTH, new_samples, CHUNK_SIZE);
    sample_index += CHUNK_SIZE;

    // ES CPU stages
    calculate_magnitudes();
    get_chromagram();
    run_vu();
    update_tempo();

    // ES GPU tick cadence
    if (last_gpu_tick_us == 0) {
        last_gpu_tick_us = now_us;
    }
    const uint64_t elapsed_us = now_us - last_gpu_tick_us;
    const float ideal_us_interval = 1000000.0f / static_cast<float>(REFERENCE_FPS);
    const float delta = static_cast<float>(elapsed_us) / ideal_us_interval;
    last_gpu_tick_us = now_us;

    update_novelty();
    update_tempi_phase(delta);
}

static void test_esv11_parity_synthetic_120bpm()
{
    es_reset_state();

    uint64_t sample_index = 0;
    uint64_t last_gpu_tick_us = 0;

    // Run long enough to fill novelty history (1024 samples @ 50Hz ≈ 20.48s).
    const uint64_t seconds = 23;
    const uint64_t total_samples = seconds * static_cast<uint64_t>(SAMPLE_RATE);
    const uint64_t total_chunks = total_samples / CHUNK_SIZE;

    for (uint64_t c = 0; c < total_chunks; ++c) {
        feed_chunk_and_process(c, sample_index, last_gpu_tick_us);
    }

    // Derive top tempo bin (matches backend logic)
    uint16_t top_i = 0;
    float top_mag = 0.0f;
    for (uint16_t i = 0; i < NUM_TEMPI; ++i) {
        if (tempi_smooth[i] > top_mag) {
            top_mag = tempi_smooth[i];
            top_i = i;
        }
    }

    const float bpm = static_cast<float>(TEMPO_LOW + top_i);
    const float phase01 = fmodf((tempi[top_i].phase + kPi) / (2.0f * kPi), 1.0f);

    // Golden expectations captured from this deterministic fixture.
    // Re-baselined 2026-04-28 after vendor/goertzel.h:28 BOTTOM_NOTE flip
    // 12 -> 6 (D#-origin -> C-origin). The test signal is a 220 Hz (A3)
    // carrier; under the C-origin lattice it lands on bin 21 (notes[48]=220 Hz)
    // instead of the legacy bin 18, so spectrogram_smooth shifts and chromagram
    // values rotate by +3 indices. See AUDIO_MUSICAL_LOGIC_SOURCE_AUDIT.md
    // and CHORD_ROOT_ORIGIN_TRACE.md.
    static const float EXPECT_VU_LEVEL = 0.416666657f;
    static const float EXPECT_TEMPO_CONF = 0.978948712f;
    static const float EXPECT_BPM = 121.0f;
    static const float EXPECT_PHASE01 = 0.496134490f;
    static const float EXPECT_NOVELTY_LAST = 0.159572199f;

    static const float EXPECT_SPECTROGRAM_SMOOTH[NUM_FREQS] = {
        0.002391044f,0.001115314f,0.002553927f,0.001224874f,0.002910629f,0.001307192f,0.002942015f,0.001416619f,
        0.003564925f,0.001661442f,0.004485366f,0.001958224f,0.005386418f,0.003320709f,0.007269650f,0.005148811f,
        0.013652798f,0.038988888f,0.055215377f,0.034722462f,0.110323511f,0.083333336f,0.166666672f,0.083333336f,
        0.202259019f,0.045541093f,0.129614860f,0.024471872f,0.258243233f,0.113982178f,0.031412862f,0.103814743f,
        0.097205065f,0.013996114f,0.089061826f,0.147679031f,0.087267958f,0.146520615f,0.146212339f,0.038602490f,
        0.037883837f,0.215764806f,0.116722696f,0.219226673f,0.178712845f,0.031622671f,0.035494156f,0.048449945f,
        0.037844602f,0.231014803f,0.134610355f,0.641347945f,0.263411433f,0.261291981f,0.491221279f,0.008622583f,
        0.112356126f,0.002084731f,0.007457251f,0.007053468f,0.032643229f,0.010963977f,0.045085102f,0.166187003f
    };

    static const float EXPECT_CHROMAGRAM[12] = {
        0.335149020f,0.427512527f,0.420261145f,0.710795999f,0.576101959f,0.631335080f,
        0.697514296f,0.367803097f,0.502162457f,0.132698283f,0.303165287f,0.288473994f
    };

    // Basic sanity (non-zero energy, bounded outputs)
    TEST_ASSERT_TRUE(vu_level >= 0.0f && vu_level <= 1.0f);
    TEST_ASSERT_TRUE(tempo_confidence >= 0.0f && tempo_confidence <= 1.0f);
    TEST_ASSERT_TRUE(bpm >= static_cast<float>(TEMPO_LOW) && bpm <= static_cast<float>(TEMPO_HIGH));
    TEST_ASSERT_TRUE(phase01 >= 0.0f && phase01 < 1.0f);
    TEST_ASSERT_TRUE(novelty_curve_normalized[NOVELTY_HISTORY_LENGTH - 1] >= 0.0f);

    // Parity guard (tolerances allow minor compiler/libm drift)
    TEST_ASSERT_FLOAT_WITHIN(0.002f, EXPECT_VU_LEVEL, vu_level);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, EXPECT_TEMPO_CONF, tempo_confidence);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, EXPECT_BPM, bpm);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, EXPECT_PHASE01, phase01);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, EXPECT_NOVELTY_LAST, novelty_curve_normalized[NOVELTY_HISTORY_LENGTH - 1]);

    for (int i = 0; i < NUM_FREQS; ++i) {
        TEST_ASSERT_FLOAT_WITHIN(0.02f, EXPECT_SPECTROGRAM_SMOOTH[i], spectrogram_smooth[i]);
    }
    for (int i = 0; i < 12; ++i) {
        TEST_ASSERT_FLOAT_WITHIN(0.03f, EXPECT_CHROMAGRAM[i], chromagram[i]);
    }
}

void setUp() {}
void tearDown() {}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_esv11_parity_synthetic_120bpm);
    return UNITY_END();
}
