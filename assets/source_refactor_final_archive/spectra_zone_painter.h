#ifndef SPECTRA_ZONE_PAINTER_H
#define SPECTRA_ZONE_PAINTER_H

#include "esp_err.h"       // For esp_err_t
#include <stdint.h>        // For uint16_t
#include "spectra_config_manager.h" // For channel_config_t, zone_config_t
#include "spectra_visual_input.h" // For visual_input_features_t
#include "leds.h"          // For CRGBF (output pixel format)

// Forward declarations for modules that Zone Painter might call
typedef struct visual_algorithm_context_t visual_algorithm_context_t;
typedef struct color_palette_context_t color_palette_context_t;

// Context structure for the Zone Painter module
typedef struct {
    // Pointers to the configurations needed by the zone painter
    const channel_config_t* channel_config; 
    
    // Output buffers for the LED data for each channel
    // These are the raw pixel buffers that will be passed to post-processing
    CRGBF* output_led_buffer;
    uint16_t output_led_count; // The actual number of LEDs this channel drives

    // NEW: Pointers to the associated color palette and visual algorithm contexts
    color_palette_context_t* palette_ctx;
    visual_algorithm_context_t* algo_ctx;

    // Other state variables or internal buffers if needed
    bool initialized;
} spectra_zone_painter_context_t;

/**
 * @brief Initializes the V1_ZonePaintingModule.
 * 
 * @param ctx Pointer to the context structure to be initialized.
 * @param channel_config Pointer to the configuration for this specific visual channel.
 * @param output_buffer Pointer to the CRGBF buffer where the painted LEDs will be stored.
 * @param output_count The number of LEDs this output buffer can hold.
 * @param palette_ctx Pointer to the associated color palette context.
 * @param algo_ctx Pointer to the associated visual algorithm context.
 * @return esp_err_t ESP_OK on success, or an error code.
 */
esp_err_t spectra_zone_painter_init(
    spectra_zone_painter_context_t* ctx,
    const channel_config_t* channel_config,
    CRGBF* output_buffer,
    uint16_t output_count,
    color_palette_context_t* palette_ctx,
    visual_algorithm_context_t* algo_ctx
);

/**
 * @brief Processes visual input features and paints the LED zones based on configuration.
 * 
 * This function iterates through defined zones for a channel, applies algorithms,
 * and updates the LED buffer.
 * 
 * @param ctx Pointer to the initialized context structure.
 * @param visual_input Pointer to the current visual input features (from V0_VisualInput).
 * @return esp_err_t ESP_OK on success, or an error code.
 */
esp_err_t spectra_zone_painter_process(
    spectra_zone_painter_context_t* ctx,
    const visual_input_features_t* visual_input
);

#endif // SPECTRA_ZONE_PAINTER_H 