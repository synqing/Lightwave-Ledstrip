#include "spectra_visual_input.h"
#include "esp_log.h"   // For ESP_LOGI, ESP_LOGE
#include <string.h>    // For memset, memcpy

static const char* TAG_VISUAL_INPUT = "SpectraVInput";

// Basic float clipping function if not available elsewhere
static inline float clip_float(float val, float min_val, float max_val) {
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

esp_err_t spectra_visual_input_init(void* ctx) {
    ESP_LOGI(TAG_VISUAL_INPUT, "V0_VisualInput module initialized.");
    return ESP_OK;
}

esp_err_t spectra_visual_input_process(
    const audio_features_s3_t* audio_features,
    const channel_config_t* channel_config,
    visual_input_features_t* visual_input_out
) {
    if (audio_features == NULL || visual_input_out == NULL || channel_config == NULL) {
        ESP_LOGE(TAG_VISUAL_INPUT, "Invalid arguments to process function.");
        return ESP_ERR_INVALID_ARG;
    }

    memset(visual_input_out, 0, sizeof(visual_input_features_t)); // Clear output struct

    // Copy basic system info
    visual_input_out->frame_number = audio_features->frame_number;
    visual_input_out->timestamp_ms = audio_features->timestamp_ms_l0_in;

    // Copy tempo features
    visual_input_out->current_bpm = audio_features->current_bpm;
    visual_input_out->beat_now = audio_features->beat_now;

    // Map VU level (normalize to 0.0-1.0 range, assuming audio_features.vu_level_main_linear is somewhat linear)
    // A simple clipping to 0.0-1.0 is a starting point. Further normalization might be needed.
    visual_input_out->vu_level_normalized = clip_float(audio_features->vu_level_main_linear, 0.0f, 1.0f); // Basic normalization

    // Copy Goertzel magnitudes (ensure target array size matches source)
    // L1_PRIMARY_NUM_BINS is defined in L_common_audio_defs.h and should be consistent
    memcpy(visual_input_out->goertzel_magnitudes,
           audio_features->l1_goertzel_magnitudes,
           sizeof(float) * L1_PRIMARY_NUM_BINS);

    ESP_LOGD(TAG_VISUAL_INPUT, "Processed audio features for visual input: VU %.3f, BPM %.1f",
             visual_input_out->vu_level_normalized, visual_input_out->current_bpm);

    return ESP_OK;
} 