#include "spectra_config_manager.h"
#include "esp_log.h" // For ESP_LOGI, ESP_LOGE
#include <string.h>  // For memset

static const char* TAG_CONFIG_MANAGER = "SpectraConfig";

// Static global instance of the configuration
static spectra_visual_config_t s_spectra_visual_config;

esp_err_t spectra_config_manager_init(spectra_visual_config_t* config) {
    if (config == NULL) {
        ESP_LOGE(TAG_CONFIG_MANAGER, "Config pointer is NULL in init.");
        return ESP_ERR_INVALID_ARG;
    }
    memset(config, 0, sizeof(spectra_visual_config_t)); // Clear all members to 0
    config->global_master_brightness = 1.0f; // Default to full brightness
    ESP_LOGI(TAG_CONFIG_MANAGER, "Configuration manager initialized.");
    return ESP_OK;
}

esp_err_t spectra_config_manager_load_defaults(spectra_visual_config_t* config) {
    if (config == NULL) {
        ESP_LOGE(TAG_CONFIG_MANAGER, "Config pointer is NULL in load_defaults.");
        return ESP_ERR_INVALID_ARG;
    }

    // --- Channel 0 Configuration (Example: Full strip VU meter) ---
    config->channels[0].enabled = true;
    config->channels[0].led_count = 128; // Assuming 128 LEDs per strip
    config->channels[0].symmetry_mode = SYMMETRY_MODE_NONE; // No symmetry for this example
    config->channels[0].num_zones = 1;
    config->channels[0].zones[0] = (zone_config_t) {
        .start_led_idx = 0,
        .end_led_idx = 127,
        .audio_to_param_map = AUDIO_MAP_VU_LEVEL_MAIN_LINEAR,
        .audio_map_scale = 1.0f,
        .audio_map_idx = 0, // Not applicable for VU level
        .algorithm = VISUAL_ALGO_VU_METER,
    };

    // --- Channel 1 Configuration (Example: Symmetrical Spectrum Bar) ---
    config->channels[1].enabled = true;
    config->channels[1].led_count = 128; // Assuming 128 LEDs per strip
    config->channels[1].symmetry_mode = SYMMETRY_MODE_HORIZONTAL_MIRROR; // Horizontal mirror
    config->channels[1].num_zones = 1;
    config->channels[1].zones[0] = (zone_config_t) {
        .start_led_idx = 0,
        .end_led_idx = 63, // Half the strip for symmetry
        .audio_to_param_map = AUDIO_MAP_L1_GOERTZEL_MAGNITUDE_BIN,
        .audio_map_scale = 0.5f, // Example scaling
        .audio_map_idx = 5, // Example: Use Goertzel bin 5 (mid-range frequency)
        .algorithm = VISUAL_ALGO_SPECTRUM_BAR,
    };

    ESP_LOGI(TAG_CONFIG_MANAGER, "Default configurations loaded.");
    return ESP_OK;
}

spectra_visual_config_t* spectra_config_manager_get_config(void) {
    return &s_spectra_visual_config;
} 