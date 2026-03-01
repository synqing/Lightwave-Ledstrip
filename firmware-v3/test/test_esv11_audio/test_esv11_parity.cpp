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
    static const float EXPECT_VU_LEVEL = 0.416666657f;
    static const float EXPECT_TEMPO_CONF = 0.980319679f;
    static const float EXPECT_BPM = 121.0f;
    static const float EXPECT_PHASE01 = 0.497356564f;
    static const float EXPECT_NOVELTY_LAST = 0.212867796f;

    static const float EXPECT_SPECTROGRAM_SMOOTH[NUM_FREQS] = {
        0.00390806049f,0.00193192263f,0.00449752249f,0.00199869741f,0.00462597236f,0.00237175007f,0.0053228694f,0.00281636533f,
        0.00630933046f,0.00301911612f,0.0105813639f,0.00383852073f,0.0152069693f,0.00630114088f,0.100677408f,0.0253227893f,
        0.1108284f,0.0457495339f,0.166666672f,0.0833333358f,0.166666672f,0.338951975f,0.136886135f,0.0609108321f,
        0.0838491321f,0.484729141f,0.188748404f,0.120761663f,0.0548467785f,0.188969925f,0.123410173f,0.319618255f,
        0.0303901881f,0.332003862f,0.247660041f,0.00550384074f,0.0122525031f,0.0579992682f,0.0627481863f,0.265046686f,
        0.269176394f,0.0153809628f,0.0851839557f,0.215734959f,0.0241647363f,0.184027985f,0.0580441952f,0.684320629f,
        0.277726173f,0.407329649f,0.461069107f,0.0635286644f,0.325033933f,0.00790180732f,0.0567133166f,0.00376377883f,
        0.0257864743f,0.0144006386f,0.0458476543f,0.0191103909f,0.0730519518f,0.210001633f,0.0805460289f,0.25f
    };

    static const float EXPECT_CHROMAGRAM[12] = {
        0.392942846f,0.958291173f,0.817740619f,0.476658523f,0.764511526f,0.26037398f,
        0.437296987f,0.625266671f,0.253317416f,0.872403622f,0.499019384f,0.773684144f
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
