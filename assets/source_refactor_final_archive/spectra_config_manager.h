#ifndef SPECTRA_CONFIG_MANAGER_H
#define SPECTRA_CONFIG_MANAGER_H

#include "esp_err.h" // For esp_err_t
#include <stdint.h>  // For uint32_t
#include <stdbool.h> // For bool
#include "L_common_audio_defs.h" // For audio_features_s3_t if needed for mapping types

// Forward declarations for types that might be used by the configuration
typedef struct visual_algorithm_config_t visual_algorithm_config_t;
typedef struct color_palette_config_t color_palette_config_t;

// --- Enums and Defines ---

typedef enum {
    SYMMETRY_MODE_NONE,
    SYMMETRY_MODE_HORIZONTAL_MIRROR, // Mirror across the middle
    SYMMETRY_MODE_VERTICAL_MIRROR,   // Mirror (future: top/bottom for 2D setups)
    // Add other symmetry modes as needed
} symmetry_mode_t;

// --- Zone Configuration ---

// Defines how a specific audio feature maps to a visual parameter for a zone
typedef enum {
    AUDIO_MAP_NONE,
    AUDIO_MAP_VU_LEVEL_MAIN_LINEAR,
    AUDIO_MAP_VU_LEVEL_MAIN_DBFS,
    AUDIO_MAP_L1_GOERTZEL_MAGNITUDE_BIN,
    AUDIO_MAP_L2_FFT_BAND_MAGNITUDE_BIN,
    AUDIO_MAP_CURRENT_BPM,
    AUDIO_MAP_BEAT_NOW,
    // Add more granular mappings as needed
} audio_feature_mapping_t;

// Defines the visual algorithm to be applied within a zone
typedef enum {
    VISUAL_ALGO_NONE,
    VISUAL_ALGO_SPECTRUM_BAR,
    VISUAL_ALGO_VU_METER,
    VISUAL_ALGO_BEAT_PULSE,
    // Add more algorithms as needed
} visual_algorithm_t;

typedef struct {
    uint16_t start_led_idx;    // 0-indexed start of the zone on the strip
    uint16_t end_led_idx;      // 0-indexed end of the zone on the strip (inclusive)
    audio_feature_mapping_t audio_to_param_map; // How audio maps to a specific visual parameter
    float audio_map_scale;     // Scaling factor for the audio feature
    uint32_t audio_map_idx;    // Index for bin/band if applicable (e.g., Goertzel bin index)
    visual_algorithm_t algorithm; // Visual algorithm for this zone
    // Add more zone-specific parameters like color override, brightness, etc.
} zone_config_t;

// --- Channel Configuration ---

#define MAX_ZONES_PER_CHANNEL 4 // Example: Maximum number of zones a channel can have

typedef struct {
    bool enabled;                              // Is this channel active?
    uint16_t led_count;                        // Number of LEDs on this channel's strip
    symmetry_mode_t symmetry_mode;             // Symmetry applied to this channel
    uint8_t num_zones;                         // Number of active zones for this channel
    zone_config_t zones[MAX_ZONES_PER_CHANNEL]; // Array of zone configurations
    // Pointers to active visual algorithm and color palette configurations
    // These would typically be loaded based on enum values.
    visual_algorithm_config_t* active_algo_config; 
    color_palette_config_t* active_palette_config;
    // Add other channel-specific settings like overall brightness, gamma, etc. if needed at this level
} channel_config_t;

// --- Global Configuration Structure ---

#define MAX_VISUAL_CHANNELS 2 // We plan for two independent visual channels

typedef struct {
    channel_config_t channels[MAX_VISUAL_CHANNELS]; // Configuration for each visual channel
    // Add global settings like master brightness, global animation speed, etc.
    float global_master_brightness; // Overall brightness control [0.0, 1.0]
    // Add other global configurations like delta time scaling factor, etc.
} spectra_visual_config_t;

// --- Public API for Configuration Manager ---

/**
 * @brief Initializes the configuration manager with default settings.
 * 
 * @param config Pointer to the spectra_visual_config_t structure to initialize.
 * @return esp_err_t ESP_OK on success, or an error code.
 */
esp_err_t spectra_config_manager_init(spectra_visual_config_t* config);

/**
 * @brief Loads the default configuration for the visual pipelines.
 * 
 * This function would typically load hardcoded defaults or from NVS/file system.
 * For the initial phase, it will load hardcoded basic configurations.
 * 
 * @param config Pointer to the spectra_visual_config_t structure to populate.
 * @return esp_err_t ESP_OK on success, or an error code.
 */
esp_err_t spectra_config_manager_load_defaults(spectra_visual_config_t* config);

/**
 * @brief Gets the current visual configuration.
 * 
 * @return A pointer to the global configuration structure.
 */
spectra_visual_config_t* spectra_config_manager_get_config(void);

#endif // SPECTRA_CONFIG_MANAGER_H 