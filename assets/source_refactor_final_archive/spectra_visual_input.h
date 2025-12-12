#ifndef SPECTRA_VISUAL_INPUT_H
#define SPECTRA_VISUAL_INPUT_H

#include "esp_err.h"       // For esp_err_t
#include "L_common_audio_defs.h" // For audio_features_s3_t
#include "spectra_config_manager.h" // For channel_config_t

// Defines the output format of the V0_VisualInput module
// This structure holds the relevant visual features extracted from the audio pipeline.
typedef struct {
    float vu_level_normalized;   // Overall normalized VU level (e.g., 0.0 to 1.0)
    float goertzel_magnitudes[L1_PRIMARY_NUM_BINS]; // Normalized Goertzel magnitudes
    float current_bpm;           // Current estimated BPM
    bool  beat_now;              // True if a beat was detected in the current frame
    uint64_t frame_number;       // The frame number from the audio pipeline
    uint32_t timestamp_ms;       // Timestamp of the audio data
    // Add more visual-specific features here as needed
} visual_input_features_t;

/**
 * @brief Initializes the V0_VisualInput module.
 * 
 * @param ctx Pointer to a context structure if needed for state management (NULL for now).
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t spectra_visual_input_init(void* ctx);

/**
 * @brief Processes incoming audio features and generates visual input features.
 * 
 * This function takes the raw audio features and a channel's configuration
 * to produce a set of processed visual input features ready for algorithms.
 * 
 * @param audio_features Pointer to the raw audio features (audio_features_s3_t).
 * @param channel_config Pointer to the configuration for the current visual channel.
 * @param visual_input_out Pointer to the visual_input_features_t structure to populate.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t spectra_visual_input_process(
    const audio_features_s3_t* audio_features,
    const channel_config_t* channel_config,
    visual_input_features_t* visual_input_out
);

#endif // SPECTRA_VISUAL_INPUT_H 