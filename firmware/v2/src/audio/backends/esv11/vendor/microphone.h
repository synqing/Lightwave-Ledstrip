/**
 * Vendored from Emotiscope v1.1_320 (trimmed: no web/debug recording).
 *
 * Provides:
 * - init_i2s_microphone()
 * - acquire_sample_chunk()
 * - sample_history[]
 */

#pragma once

#include "EsV11Shim.h"
#include "global_defines.h"
#include "utilities_min.h"

#if !defined(NATIVE_BUILD)
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#endif

// Define I2S pins (ESP32-S3-DevKitC-1 target hardware)
#define I2S_LRCLK_PIN 12
#define I2S_BCLK_PIN  14
#define I2S_DIN_PIN   13

float sample_history[SAMPLE_HISTORY_LENGTH];
const float recip_scale = 1.0f / 131072.0f; // max 18 bit signed value

// DC Blocker filter state and coefficients
// y[n] = g * (x[n] - x[n-1] + R * y[n-1])
#define DC_BLOCKER_FC 5.0f
#define DC_BLOCKER_R  0.997545f
#define DC_BLOCKER_G  0.998772f

static float dc_blocker_x_prev = 0.0f;
static float dc_blocker_y_prev = 0.0f;

#if !defined(NATIVE_BUILD)
i2s_chan_handle_t rx_handle;
#endif

inline void init_i2s_microphone() {
#if defined(NATIVE_BUILD)
    // No-op on host.
    return;
#else
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &rx_handle);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_RIGHT,
            .ws_width = 32,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)I2S_BCLK_PIN,
            .ws = (gpio_num_t)I2S_LRCLK_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din = (gpio_num_t)I2S_DIN_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    i2s_channel_init_std_mode(rx_handle, &std_cfg);
    i2s_channel_enable(rx_handle);
#endif
}

inline void acquire_sample_chunk() {
    profile_function([&]() {
        uint32_t new_samples_raw[CHUNK_SIZE];
        float new_samples[CHUNK_SIZE];

#if defined(NATIVE_BUILD)
        memset(new_samples_raw, 0, sizeof(new_samples_raw));
#else
        size_t bytes_read = 0;
        i2s_channel_read(rx_handle, new_samples_raw, CHUNK_SIZE * sizeof(uint32_t), &bytes_read, portMAX_DELAY);
#endif

        for (uint16_t i = 0; i < CHUNK_SIZE; i++) {
            float x = (float)(((int32_t)new_samples_raw[i]) >> 14);
            float y = DC_BLOCKER_G * (x - dc_blocker_x_prev + DC_BLOCKER_R * dc_blocker_y_prev);
            dc_blocker_x_prev = x;
            dc_blocker_y_prev = y;

            if (y > 131072.0f) y = 131072.0f;
            else if (y < -131072.0f) y = -131072.0f;

            new_samples[i] = y;
        }

        dsps_mulc_f32(new_samples, new_samples, CHUNK_SIZE, recip_scale, 1, 1);
        shift_and_copy_arrays(sample_history, SAMPLE_HISTORY_LENGTH, new_samples, CHUNK_SIZE);
    }, __func__);
}

