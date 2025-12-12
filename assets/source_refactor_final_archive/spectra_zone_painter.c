#include "spectra_zone_painter.h"
#include "esp_log.h"   // For ESP_LOGI, ESP_LOGE
#include <string.h>    // For memset
#include <math.h>      // For fmodf, fmaxf, fminf, log10f

static const char* TAG_ZONE_PAINTER = "SpectraZPainter";

// Assuming hsv() is globally accessible from leds.h and CRGBF is defined.
// If not, it would need to be declared extern here or locally implemented.
extern CRGBF hsv(float h, float s, float v);

// Helper to fill a segment of the CRGBF buffer with a solid color
static void fill_zone_color(CRGBF* buffer, uint16_t start_idx, uint16_t end_idx, CRGBF color) {
    for (uint16_t i = start_idx; i <= end_idx; ++i) {
        buffer[i] = color;
    }
}

esp_err_t spectra_zone_painter_init(
    spectra_zone_painter_context_t* ctx,
    const channel_config_t* channel_config,
    CRGBF* output_buffer,
    uint16_t output_count,
    color_palette_context_t* palette_ctx,
    visual_algorithm_context_t* algo_ctx
) {
    if (ctx == NULL || channel_config == NULL || output_buffer == NULL || output_count == 0 || palette_ctx == NULL || algo_ctx == NULL) {
        ESP_LOGE(TAG_ZONE_PAINTER, "Invalid arguments to init function.");
        return ESP_ERR_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(spectra_zone_painter_context_t)); // Clear context
    ctx->channel_config = channel_config;
    ctx->output_led_buffer = output_buffer;
    ctx->output_led_count = output_count;
    ctx->palette_ctx = palette_ctx;
    ctx->algo_ctx = algo_ctx;
    ctx->initialized = true;

    ESP_LOGI(TAG_ZONE_PAINTER, "V1_ZonePaintingModule initialized for channel with %u LEDs.", output_count);
    return ESP_OK;
}

esp_err_t spectra_zone_painter_process(
    spectra_zone_painter_context_t* ctx,
    const visual_input_features_t* visual_input
) {
    if (ctx == NULL || !ctx->initialized || visual_input == NULL || ctx->output_led_buffer == NULL || ctx->palette_ctx == NULL || ctx->algo_ctx == NULL) {
        ESP_LOGE(TAG_ZONE_PAINTER, "ZonePainter not initialized or invalid arguments for process.");
        return ESP_ERR_INVALID_ARG;
    }

    // Clear the entire output buffer for this channel before painting
    memset(ctx->output_led_buffer, 0, sizeof(CRGBF) * ctx->output_led_count);

    for (uint8_t i = 0; i < ctx->channel_config->num_zones; ++i) {
        const zone_config_t* zone = &ctx->channel_config->zones[i];

        // Ensure zone indices are within bounds of the output buffer
        uint16_t actual_start_idx = fmaxf(0, zone->start_led_idx);
        uint16_t actual_end_idx   = fminf(ctx->output_led_count - 1, zone->end_led_idx);

        if (actual_start_idx > actual_end_idx) continue; // Invalid zone

        // Calculate the segment length for this zone
        uint16_t segment_length = actual_end_idx - actual_start_idx + 1;

        // Call the visual algorithm module to process this zone
        // The visual algorithm module will handle mapping audio features to visual output
        // and coloring based on the provided palette.
        esp_err_t err = spectra_visual_algorithm_process(
            ctx->algo_ctx,
            visual_input,
            zone,
            ctx->palette_ctx,
            &ctx->output_led_buffer[actual_start_idx],
            segment_length
        );

        if (err != ESP_OK) {
            ESP_LOGE(TAG_ZONE_PAINTER, "Error processing zone %u with algorithm: %d", i, err);
            // Continue to next zone or return error, depending on desired error handling
        }
    }

    return ESP_OK;
} 