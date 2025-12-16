#include "spectra_symmetry_engine.h"
#include "esp_log.h"   // For ESP_LOGI, ESP_LOGE
#include <string.h>    // For memset, memcpy

static const char* TAG_SYMMETRY_ENGINE = "SpectraSymmetry";

esp_err_t spectra_symmetry_engine_apply(
    CRGBF* buffer,
    uint16_t total_led_count,
    symmetry_mode_t symmetry_mode
) {
    if (buffer == NULL || total_led_count == 0) {
        ESP_LOGE(TAG_SYMMETRY_ENGINE, "Invalid arguments to apply function.");
        return ESP_ERR_INVALID_ARG;
    }

    switch (symmetry_mode) {
        case SYMMETRY_MODE_NONE:
            // No symmetry applied, buffer is left as is
            ESP_LOGD(TAG_SYMMETRY_ENGINE, "No symmetry applied.");
            break;
        case SYMMETRY_MODE_HORIZONTAL_MIRROR: {
            // Mirror the first half of the strip to the second half
            uint16_t half_count = total_led_count / 2;
            for (uint16_t i = 0; i < half_count; ++i) {
                // Mirror from (half_count - 1 - i) for a center mirror
                // Or, if mirroring first half directly to second half: i => (total_led_count - 1 - i)
                // For a simple direct mirror of first half onto second half:
                // The i-th LED from the start mirrors to (total_led_count - 1 - i) LED from end
                // Example: buffer[0] mirrors to buffer[total_led_count - 1]
                //          buffer[1] mirrors to buffer[total_led_count - 2]
                // This implies the first half is processed (0 to half_count-1), and then mirrored to the second half (half_count to total_led_count-1).
                // The issue here is how the original `zone_painter` generates the input. 
                // If zone painter only processes half the buffer, then we just copy it over.
                // For horizontal mirror, the second half (starting from half_count) should be filled based on the first half.
                // The actual mirroring logic depends on whether the original buffer is expected to be a full strip or half a strip. 
                // Given `SymmetricalZonePainting_Architecture.md`, it implies processing half the LEDs.
                // So, we'll assume `buffer` has valid data for `half_count` LEDs, and we fill the rest.
                buffer[total_led_count - 1 - i] = buffer[i];
            }
            ESP_LOGD(TAG_SYMMETRY_ENGINE, "Horizontal mirror symmetry applied.");
            break;
        }
        case SYMMETRY_MODE_VERTICAL_MIRROR:
            // Not implemented in this basic phase
            ESP_LOGW(TAG_SYMMETRY_ENGINE, "Vertical mirror symmetry not implemented.");
            return ESP_ERR_NOT_SUPPORTED;
        default:
            ESP_LOGE(TAG_SYMMETRY_ENGINE, "Unknown symmetry mode: %d", (int)symmetry_mode);
            return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
} 