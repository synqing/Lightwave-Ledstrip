#include "spectra_visual_algorithm.h"
#include "esp_log.h"   // For ESP_LOGI, ESP_LOGE
#include <string.h>    // For memset
#include <math.h>      // For fmaxf, fminf, fmodf

static const char* TAG_VISUAL_ALGO = "SpectraVAlgo";

esp_err_t spectra_visual_algorithm_init(
    visual_algorithm_context_t* ctx,
    visual_algorithm_t initial_algo_type
) {
    if (ctx == NULL) {
        ESP_LOGE(TAG_VISUAL_ALGO, "Context pointer is NULL in init.");
        return ESP_ERR_INVALID_ARG;
    }
    memset(ctx, 0, sizeof(visual_algorithm_context_t));
    ctx->current_algorithm_type = initial_algo_type;
    ESP_LOGI(TAG_VISUAL_ALGO, "V4_VisualAlgorithmModule initialized with algorithm type: %d.", (int)initial_algo_type);
    return ESP_OK;
}

esp_err_t spectra_visual_algorithm_process(
    visual_algorithm_context_t* ctx,
    const visual_input_features_t* visual_input,
    const zone_config_t* zone_config,
    color_palette_context_t* palette_ctx,
    CRGBF* target_led_buffer_start,
    uint16_t segment_length
) {
    if (ctx == NULL || visual_input == NULL || zone_config == NULL || palette_ctx == NULL || target_led_buffer_start == NULL || segment_length == 0) {
        ESP_LOGE(TAG_VISUAL_ALGO, "Invalid arguments to process function.");
        return ESP_ERR_INVALID_ARG;
    }

    float mapped_value = 0.0f;

    // Map audio feature to a generic mapped_value (copied from Zone Painter for direct use here)
    switch (zone_config->audio_to_param_map) {
        case AUDIO_MAP_VU_LEVEL_MAIN_LINEAR:
            mapped_value = visual_input->vu_level_normalized;
            break;
        case AUDIO_MAP_VU_LEVEL_MAIN_DBFS:
            mapped_value = visual_input->vu_level_normalized; // Placeholder, proper dBFS mapping needed
            break;
        case AUDIO_MAP_L1_GOERTZEL_MAGNITUDE_BIN:
            if (zone_config->audio_map_idx < L1_PRIMARY_NUM_BINS) {
                mapped_value = visual_input->goertzel_magnitudes[zone_config->audio_map_idx];
            }
            break;
        case AUDIO_MAP_L2_FFT_BAND_MAGNITUDE_BIN:
            mapped_value = 0.0f; // Not yet populated in visual_input_features_t
            break;
        case AUDIO_MAP_CURRENT_BPM:
            mapped_value = visual_input->current_bpm / 200.0f; // Normalize BPM
            break;
        case AUDIO_MAP_BEAT_NOW:
            mapped_value = visual_input->beat_now ? 1.0f : 0.0f;
            break;
        case AUDIO_MAP_NONE:
        default:
            mapped_value = 0.0f; // No mapping or unsupported
            break;
    }

    // Apply scaling factor and clip
    mapped_value *= zone_config->audio_map_scale;
    mapped_value = fmaxf(0.0f, fminf(1.0f, mapped_value)); // Clip to 0-1

    switch (zone_config->algorithm) {
        case VISUAL_ALGO_VU_METER: {
            uint16_t fill_count = (uint16_t)(mapped_value * segment_length);
            fill_count = fminf(fill_count, segment_length); // Ensure it doesn't go past end

            // Fill visible part of the VU meter
            for (uint16_t i = 0; i < fill_count; ++i) {
                // Position for color palette, e.g., mapping LED index to hue
                float pos = (float)i / (float)segment_length;
                // Brightness from mapped_value, or a static 1.0f
                target_led_buffer_start[i] = spectra_color_palette_get_color(palette_ctx, pos);
                target_led_buffer_start[i].r *= mapped_value;
                target_led_buffer_start[i].g *= mapped_value;
                target_led_buffer_start[i].b *= mapped_value;
            }
            // Clear the rest of the zone
            for (uint16_t i = fill_count; i < segment_length; ++i) {
                target_led_buffer_start[i] = (CRGBF){0.0f, 0.0f, 0.0f};
            }
            break;
        }
        case VISUAL_ALGO_SPECTRUM_BAR: {
            // For a spectrum bar, mapped_value is the magnitude of a specific bin.
            // Color determined by palette, brightness by mapped_value.
            float pos_for_color = (float)zone_config->audio_map_idx / (float)L1_PRIMARY_NUM_BINS; // Use bin index as position for color
            CRGBF color = spectra_color_palette_get_color(palette_ctx, pos_for_color);
            color.r *= mapped_value;
            color.g *= mapped_value;
            color.b *= mapped_value;

            for (uint16_t i = 0; i < segment_length; ++i) {
                target_led_buffer_start[i] = color;
            }
            break;
        }
        case VISUAL_ALGO_BEAT_PULSE: {
            if (visual_input->beat_now) {
                CRGBF pulse_color = spectra_color_palette_get_color(palette_ctx, 0.0f); // Use first color from palette (e.g. red for rainbow)
                for (uint16_t i = 0; i < segment_length; ++i) {
                    target_led_buffer_start[i] = pulse_color;
                }
            } else {
                // Fade out or clear if no beat
                for (uint16_t i = 0; i < segment_length; ++i) {
                    target_led_buffer_start[i] = (CRGBF){0.0f, 0.0f, 0.0f};
                }
            }
            break;
        }
        case VISUAL_ALGO_NONE:
        default:
            // Do nothing, leave buffer as is (or clear to black)
            for (uint16_t i = 0; i < segment_length; ++i) {
                target_led_buffer_start[i] = (CRGBF){0.0f, 0.0f, 0.0f};
            }
            break;
    }

    return ESP_OK;
} 