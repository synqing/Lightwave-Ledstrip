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
#include "EsV11Buffers.h"
#include "global_defines.h"
#include "utilities_min.h"

#if !defined(NATIVE_BUILD)
#if __has_include("driver/i2s_std.h")
#define ESV11_HAS_I2S_STD 1
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#else
#define ESV11_HAS_I2S_STD 0
#include "driver/i2s.h"
#include "soc/i2s_reg.h"
#endif
#endif

// Define I2S pins (overridable via K1_* build flags for hardware variants)
#ifndef I2S_LRCLK_PIN
  #ifdef K1_I2S_LRCL
    #define I2S_LRCLK_PIN K1_I2S_LRCL
  #else
    #define I2S_LRCLK_PIN 12
  #endif
#endif
#ifndef I2S_BCLK_PIN
  #ifdef K1_I2S_BCLK
    #define I2S_BCLK_PIN K1_I2S_BCLK
  #else
    #define I2S_BCLK_PIN 14
  #endif
#endif
#ifndef I2S_DIN_PIN
  #ifdef K1_I2S_DOUT
    #define I2S_DIN_PIN K1_I2S_DOUT
  #else
    #define I2S_DIN_PIN 13
  #endif
#endif

// sample_history is allocated during backend initialisation (PSRAM heap where available).
const float recip_scale = 1.0f / 131072.0f; // max 18 bit signed value

// DC Blocker filter state and coefficients
// y[n] = g * (x[n] - x[n-1] + R * y[n-1])
// Guards allow override via shim for different sample rates.
#define DC_BLOCKER_FC 5.0f
#ifndef DC_BLOCKER_R
#define DC_BLOCKER_R  0.997545f
#endif
#ifndef DC_BLOCKER_G
#define DC_BLOCKER_G  0.998772f
#endif

static float dc_blocker_x_prev = 0.0f;
static float dc_blocker_y_prev = 0.0f;

#if !defined(NATIVE_BUILD)
#if ESV11_HAS_I2S_STD
i2s_chan_handle_t rx_handle;
#else
static constexpr i2s_port_t es_i2s_port = I2S_NUM_0;
#endif
#endif

inline void init_i2s_microphone() {
#if defined(NATIVE_BUILD)
    // No-op on host.
    return;
#else
#if ESV11_HAS_I2S_STD
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
#else
    // Legacy I2S driver path (Arduino-ESP32 2.x / ESP-IDF 4.4): no i2s_std.h.
    //
    // This preserves the ES v1.1_320 capture intent (pins + 12.8kHz + 32-bit slot + right channel),
    // but uses the older driver API available in this toolchain.
    //
    // IMPORTANT: LWLS v2's AudioCapture uses a known-good legacy I2S config for
    // SPH0645 on ESP32-S3, including register tweaks for alignment. The ES DSP
    // chain relies on sensible signal levels; if alignment is off, outputs
    // collapse towards noise floor (VU ~0.004, tempo confidence ~0.1).
    i2s_config_t cfg = {
        .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 512 * 2, // Stereo int32 frames; matches LWLS capture headroom
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        .bits_per_chan = I2S_BITS_PER_CHAN_32BIT,
    };

    i2s_driver_install(es_i2s_port, &cfg, 0, NULL);

    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = I2S_BCLK_PIN;
    pins.ws_io_num = I2S_LRCLK_PIN;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num = I2S_DIN_PIN;
    i2s_set_pin(es_i2s_port, &pins);

    // Match LWLS legacy alignment tweaks for SPH0645 RIGHT channel extraction.
    REG_CLR_BIT(I2S_RX_CONF_REG(es_i2s_port), I2S_RX_MSB_SHIFT);
    REG_CLR_BIT(I2S_RX_CONF_REG(es_i2s_port), I2S_RX_WS_IDLE_POL);
    REG_SET_BIT(I2S_RX_CONF_REG(es_i2s_port), I2S_RX_LEFT_ALIGN);
    REG_SET_BIT(I2S_RX_TIMING_REG(es_i2s_port), BIT(9));

    i2s_zero_dma_buffer(es_i2s_port);
#endif
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
#if ESV11_HAS_I2S_STD
        i2s_channel_read(rx_handle, new_samples_raw, CHUNK_SIZE * sizeof(uint32_t), &bytes_read, portMAX_DELAY);
#else
        // Legacy driver returns interleaved stereo frames when configured with
        // I2S_CHANNEL_FMT_RIGHT_LEFT. SPH0645 (SEL=3.3V) outputs on RIGHT channel,
        // which corresponds to offset 1 in the interleaved stream.
        int32_t stereo_raw[CHUNK_SIZE * 2];
        i2s_read(es_i2s_port, stereo_raw, CHUNK_SIZE * 2 * sizeof(int32_t), &bytes_read, portMAX_DELAY);
        for (uint16_t i = 0; i < CHUNK_SIZE; ++i) {
            new_samples_raw[i] = static_cast<uint32_t>(stereo_raw[i * 2 + 1]);
        }
#endif
#endif

        // NOTE: Emotiscope v1.1_320 uses the ESP-IDF "std" I2S driver and expects
        // samples aligned such that >>14 yields an 18-bit signed range.
        //
        // In this LWLS integration we may fall back to the legacy I2S driver
        // (Arduino toolchain/headers), which aligns SPH0645 samples differently.
        // LWLS legacy capture uses >>10 for SPH0645 on the same hardware.
#if defined(NATIVE_BUILD)
        constexpr int BIT_SHIFT = 14;
#else
#if ESV11_HAS_I2S_STD
        constexpr int BIT_SHIFT = 14;
#else
        constexpr int BIT_SHIFT = 10;
#endif
#endif

        for (uint16_t i = 0; i < CHUNK_SIZE; i++) {
            float x = (float)(((int32_t)new_samples_raw[i]) >> BIT_SHIFT);
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
