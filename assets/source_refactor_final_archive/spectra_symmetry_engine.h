#ifndef SPECTRA_SYMMETRY_ENGINE_H
#define SPECTRA_SYMMETRY_ENGINE_H

#include "esp_err.h"       // For esp_err_t
#include <stdint.h>        // For uint16_t
#include "leds.h"          // For CRGBF
#include "spectra_config_manager.h" // For symmetry_mode_t

/**
 * @brief Applies symmetry transformation to a CRGBF LED buffer.
 * 
 * This function takes a half-strip of LED data and applies the specified
 * symmetry mode to fill the other half (or the entire strip if no symmetry).
 * 
 * @param buffer Pointer to the CRGBF buffer (input and output).
 * @param total_led_count The total number of LEDs in the strip (e.g., 128).
 * @param symmetry_mode The type of symmetry to apply.
 * @return esp_err_t ESP_OK on success, or an error code.
 */
esp_err_t spectra_symmetry_engine_apply(
    CRGBF* buffer,
    uint16_t total_led_count,
    symmetry_mode_t symmetry_mode
);

#endif // SPECTRA_SYMMETRY_ENGINE_H 