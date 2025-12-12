#ifndef SPECTRA_COLOR_PALETTE_H
#define SPECTRA_COLOR_PALETTE_H

#include "esp_err.h"  // For esp_err_t
#include <stdint.h>   // For uint8_t
#include "leds.h"     // For CRGBF

// Defines different types of color palettes
typedef enum {
    COLOR_PALETTE_RAINBOW,
    COLOR_PALETTE_MONO_HUE,
    COLOR_PALETTE_CUSTOM_GRADIENT,
    // Add more palette types as needed
} color_palette_type_t;

// Context structure for the Color Palette module
typedef struct color_palette_context_t {
    color_palette_type_t current_palette_type;
    // Add configuration parameters for different palette types, e.g.:
    float mono_hue_value; // For MONO_HUE palette
    // For custom gradients, you might have an array of CRGBF points and a number of points
} color_palette_context_t;

/**
 * @brief Initializes the V3_ColorPaletteModule.
 * 
 * @param ctx Pointer to the context structure to be initialized.
 * @param initial_palette_type The type of palette to initialize with.
 * @return esp_err_t ESP_OK on success, or an error code.
 */
esp_err_t spectra_color_palette_init(
    color_palette_context_t* ctx,
    color_palette_type_t initial_palette_type
);

/**
 * @brief Gets a color from the current palette based on a normalized position.
 * 
 * @param ctx Pointer to the initialized context structure.
 * @param normalized_position A float from 0.0 to 1.0 representing the position within the palette.
 * @return CRGBF The color at the given position.
 */
CRGBF spectra_color_palette_get_color(
    color_palette_context_t* ctx,
    float normalized_position
);

#endif // SPECTRA_COLOR_PALETTE_H 