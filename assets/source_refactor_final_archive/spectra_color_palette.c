#include "spectra_color_palette.h"
#include "esp_log.h" // For ESP_LOGI, ESP_LOGE
#include <string.h>  // For memset
#include <math.h>    // For fmodf

static const char* TAG_COLOR_PALETTE = "SpectraCPalette";

// Assuming hsv() is globally accessible from leds.h
extern CRGBF hsv(float h, float s, float v);

esp_err_t spectra_color_palette_init(
    color_palette_context_t* ctx,
    color_palette_type_t initial_palette_type
) {
    if (ctx == NULL) {
        ESP_LOGE(TAG_COLOR_PALETTE, "Context pointer is NULL in init.");
        return ESP_ERR_INVALID_ARG;
    }
    memset(ctx, 0, sizeof(color_palette_context_t));
    ctx->current_palette_type = initial_palette_type;
    // Set other default parameters based on initial_palette_type if needed
    ESP_LOGI(TAG_COLOR_PALETTE, "V3_ColorPaletteModule initialized with palette type: %d.", (int)initial_palette_type);
    return ESP_OK;
}

CRGBF spectra_color_palette_get_color(
    color_palette_context_t* ctx,
    float normalized_position
) {
    if (ctx == NULL) {
        ESP_LOGE(TAG_COLOR_PALETTE, "Context pointer is NULL in get_color. Returning black.");
        return (CRGBF){0.0f, 0.0f, 0.0f};
    }

    float hue, saturation, value;
    saturation = 1.0f;
    value = 1.0f; // Max brightness by default, can be modulated later

    switch (ctx->current_palette_type) {
        case COLOR_PALETTE_RAINBOW:
            hue = fmodf(normalized_position, 1.0f); // Map 0.0-1.0 to full spectrum
            break;
        case COLOR_PALETTE_MONO_HUE:
            hue = ctx->mono_hue_value; // Use a fixed hue
            value = normalized_position; // Map position to brightness for a mono gradient
            break;
        case COLOR_PALETTE_CUSTOM_GRADIENT:
            // TODO: Implement logic for custom gradients (e.g., interpolate between points)
            hue = fmodf(normalized_position, 1.0f); // Fallback to rainbow for now
            ESP_LOGW(TAG_COLOR_PALETTE, "Custom gradient not yet implemented. Falling back to rainbow.");
            break;
        default:
            hue = 0.0f; // Default to red
            saturation = 0.0f; // Grey
            value = 0.0f; // Black
            ESP_LOGW(TAG_COLOR_PALETTE, "Unknown color palette type. Returning black.");
            break;
    }

    return hsv(hue, saturation, value);
} 