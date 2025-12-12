/*
############################################################################
############################################################################
                                 __  _
           ___  ____ ___  ____  / /_(_)_____________  ____  ___
          / _ \/ __ `__ \/ __ \/ __/ / ___/ ___/ __ \/ __ \/ _ \
         /  __/ / / / / / /_/ / /_/ (__  ) /__/ /_/ / /_/ /  __/
         \___/_/ /_/ /_/\____/\__/_/____/\___/\____/ .___/\___/
             Audio-visual engine by @lixielabs    /_/
             Released under the GPLv3 License

############################################################################
## FOREWORD ################################################################
 
Welcome to the Emotiscope Engine. This is firmware which: 
 
- Logs raw audio data from the microphone into buffers
- Detects frequencies in the audio using many Goertzel filters at once
- Detects the BPM of music
- Syncronizes to the beats of said music
- Checks the touch sensors for input
- Talks to the Emotiscope web app over a high speed ws:// connection
- Stores settings in NVS memory
- Draws custom light show modes to the LEDs which react to
  music in real-time with a variety of effects
- Runs the indicator light
- Runs the screensaver
- Applies a blue-light filter to the LEDs
- Applies gamma correction to the LEDs
- Applies error-diffusion temporal dithering to the LEDs
- Drives those 128 LEDs with an RMT driver at high frame rates
- Supports over-the-air firmware updates
- And much more

It's quite a large project, and it's all running on a dual core
ESP32-S3. (240 MHz CPU with 520 KB of RAM)

This is the main file everything else branches from, and it's where
the two cores are started. The first core runs the graphics (Core 0)
and the second core runs the audio and web server (Core 1).

If you enjoy this product or code, please consider supporting me on
GitHub. I'm a solo developer and I put a lot of time and effort into
making Emotiscope the best that it can be. Your support helps me
continue to develop and improve the Emotiscope Engine.

                                  DONATE:
                                  https://github.com/sponsors/connornishijima

                                               - Connor Nishijima, @lixielabs
*/

// ## SOFTWARE VERSION ########################################################

#define SOFTWARE_VERSION_MAJOR ( 2 )
#define SOFTWARE_VERSION_MINOR ( 0 )
#define SOFTWARE_VERSION_PATCH ( 0 )
#define TAG "EE"

// ## DEPENDENCIES ############################################################

// External dependencies ------------------------------------------------------
// C
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Espressif - Wireless (COMMENTED OUT)
// #include <esp_wifi.h>
// #include <esp_event.h>
// #include <esp_netif.h>
// #include <esp_http_server.h>
// #include "esp_http_client.h"
// #include <lwip/inet.h>
// #include "lwip/sockets.h"
// #include "lwip/netdb.h"
// #include "esp_tls.h"
// #include "esp_crt_bundle.h"
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_task_wdt.h>
#include <esp_random.h>
#include <esp_check.h>
#include <sys/param.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <freertos/event_groups.h>
#include <perfmon.h>
// #include <esp_netif.h> // Already commented above with other wireless includes

// Peripherals
#include <driver/temperature_sensor.h>
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>
#include <driver/touch_pad.h>
#include <driver/uart.h>
#include <driver/usb_serial_jtag.h>

// SIMD
#include <esp_dsp.h>
#include <dsps_mem.h>

// Internal dependencies ------------------------------------------------------

// System
#include "global_defines.h"
#include "types.h"
#include "serial.h"
#include "system.h"
#include "profiler.h"
#include "configuration.h"
#include "utilities.h"
#include "asm.h"

// Hardware
#include "microphone.h"
#include "indicator_light.h"
#include "led_driver.h"
#include "touch.h"

// DSP
#include "fft.h"
#include "goertzel.h"
#include "vu.h"
#include "tempo.h"
#include "pitch.h"

// Comms - Wireless (COMMENTED OUT)
// #include "wireless.h"
// #include "packets.h"

// Graphics
#include "perlin.h"
#include "leds.h"
#include "ui.h"
#include "standby.h"
#include "screensaver.h"
#include "light_modes.h"

// Screen Streaming
#include "preview.h"

// Testing
#include "testing.h"

// Cores
#include "gpu_core.h" // Video
#include "cpu_core.h" // Audio/Web
#include "spectra_audio_interface.h" // NEW INCLUDE

// Development Notes
//#include "notes.h"

// NEW: Definition for SpectraSynq Visual Task
// Ensure TAG is defined or replace with a local string literal for ESP_LOGI
// For example, if TAG is not globally available here:
// static const char* SPECTRA_TAG = "SpectraVisuals";

// Declare external LED buffer from leds.h
extern CRGBF leds[NUM_LEDS];

// NEW SPECTRASYNQ MODULE INCLUDES
#include "spectra_config_manager.h"
#include "spectra_visual_input.h"
#include "spectra_zone_painter.h"
#include "spectra_color_palette.h"
#include "spectra_visual_algorithm.h"
#include "spectra_symmetry_engine.h" // NEW INCLUDE
#include "spectra_post_processing.h" // NEW INCLUDE

// SpectraSynq Global Contexts and Buffers
#define SPECTRA_MAX_LEDS_PER_CHANNEL 160 // Define a max for our internal buffers
static spectra_visual_config_t s_spectra_global_config;

// Visual Input Features (one for each potential channel, even if using only one initially)
static visual_input_features_t s_visual_input_features_ch0;
static visual_input_features_t s_visual_input_features_ch1; // For future Channel 1

// Zone Painter Contexts
static spectra_zone_painter_context_t s_zone_painter_ctx_ch0;
static spectra_zone_painter_context_t s_zone_painter_ctx_ch1; // For future Channel 1

// Color Palette Contexts
static color_palette_context_t s_palette_ctx_ch0;
static color_palette_context_t s_palette_ctx_ch1; // For future Channel 1

// Visual Algorithm Contexts
static visual_algorithm_context_t s_algo_ctx_ch0;
static visual_algorithm_context_t s_algo_ctx_ch1; // For future Channel 1

// Post-Processing Contexts
static spectra_post_processing_context_t s_post_processing_ctx_ch0;
static spectra_post_processing_context_t s_post_processing_ctx_ch1; // For future Channel 1

// Our internal LED buffers (CRGBF) for each channel
// These are separate from Emotiscope's global 'leds' array.
static CRGBF s_led_buffer_ch0[SPECTRA_MAX_LEDS_PER_CHANNEL];
static CRGBF s_led_buffer_ch1[SPECTRA_MAX_LEDS_PER_CHANNEL]; // For future Channel 1

void loop_spectra_visuals(void *pvParameters) {
    ESP_LOGI(TAG, "SpectraSynq Visual Task started on Core 1.");
    audio_features_s3_t received_features;
    static int64_t last_frame_time_us = 0;

    // Get the global configuration for convenience (initialized in app_main)
    spectra_visual_config_t* current_config = spectra_config_manager_get_config();

    while (1) {
        // Calculate delta time for time-dependent effects
        int64_t current_time_us = esp_timer_get_time();
        float delta_time_ms = (float)(current_time_us - last_frame_time_us) / 1000.0f;
        last_frame_time_us = current_time_us;

        // Attempt to receive audio features from the queue
        if (spectra_audio_interface_receive(&received_features, pdMS_TO_TICKS(50)) == ESP_OK) { // 50ms timeout
            ESP_LOGI(TAG, "Visuals: Frame %llu, VU: %.3f, BPM: %.1f, Goertzel[0]: %.3f",
                     received_features.frame_number,
                     received_features.vu_level_main_linear,
                     received_features.current_bpm,
                     received_features.l1_goertzel_magnitudes[0]);

            // --- SpectraSynq Visual Pipeline Processing (Channel 0) ---
            if (current_config->channels[0].enabled) {
                // V0_VisualInput: Extract features from audio data
                spectra_visual_input_process(
                    &received_features,
                    &current_config->channels[0],
                    &s_visual_input_features_ch0
                );

                // V1_ZonePaintingModule: Process and paint LEDs for Channel 0
                // The zone painter internally calls visual algorithms and uses color palettes
                spectra_zone_painter_process(
                    &s_zone_painter_ctx_ch0,
                    &s_visual_input_features_ch0
                );

                // V2_SymmetryEngine: Apply symmetry to Channel 0's LED buffer
                spectra_symmetry_engine_apply(
                    s_led_buffer_ch0,
                    s_spectra_global_config.channels[0].led_count,
                    current_config->channels[0].symmetry_mode
                );

                // V4_PostProcessingModule: Apply post-processing effects to Channel 0's LED buffer
                spectra_post_processing_apply(
                    &s_post_processing_ctx_ch0,
                    &current_config->channels[0],
                    s_led_buffer_ch0,
                    s_spectra_global_config.channels[0].led_count,
                    delta_time_ms
                );

                // For now, copy Channel 0's buffer to Emotiscope's global 'leds' for rendering
                // This is a temporary step until V5_LED_Renderer is fully implemented for dual channels.
                for (int i = 0; i < current_config->channels[0].led_count; i++) {
                    if (i < NUM_LEDS) { // Ensure we don't write out of bounds of Emotiscope's buffer
                        leds[i] = s_led_buffer_ch0[i];
                    }
                }
            } else {
                // If channel 0 is disabled, ensure Emotiscope's buffer for it is cleared
                for (int i = 0; i < NUM_LEDS; i++) {
                    leds[i] = (CRGBF){0.0f, 0.0f, 0.0f};
                }
            }

            // Apply Emotiscope's post-processing (now operating on the copied buffer) and render to LEDs
            quantize_color_error(true); // Apply dithering and convert to 8-bit, store in raw_led_data
            transmit_leds();            // Transmit raw_led_data via RMT

        } else {
            // ESP_LOGD(TAG, "Visuals: No new audio features received. Clearing LEDs.");
            // If no data, clear the display or fade out
            for (int i = 0; i < NUM_LEDS; i++) {
                leds[i] = (CRGBF){0.0f, 0.0f, 0.0f}; // Set to black
            }
            quantize_color_error(true);
            transmit_leds();
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Maintain loop frequency
    }
}

// ## CODE ####################################################################

// EVERYTHING BEGINS HERE ON BOOT ---------------------------------------------
void app_main(void){
    // NEW: Initialize SpectraSynq Audio Interface Queue
    if (spectra_audio_interface_init(5, sizeof(audio_features_s3_t)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SpectraSynq audio interface! Halting.");
        while(1); // Halt
    }

    init_rmt_driver(); // NEW: Initialize LED RMT driver

    // Initialize NVS (Non-Volatile Storage) - Used for configuration, incl. WiFi (leave for now)
    // While NVS can store WiFi credentials, it's also used for general configuration.
    // We'll leave it for now and disable direct WiFi setup.
    //esp_err_t ret = nvs_flash_init();
    //if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //    ESP_ERROR_CHECK(nvs_flash_erase());
    //    ret = nvs_flash_init();
    //}
    //ESP_ERROR_CHECK(ret);

    // COMMENTED OUT: Emotiscope WiFi/Networking Initialization
    //init_wireless(); // (wireless.h)
    //connect_to_wifi(); // (wireless.h)
    //start_mdns_service(); // (wireless.h)
    //start_web_server(); // (wireless.h)

    // NEW: Initialize SpectraSynq Configuration Manager
    if (spectra_config_manager_init(&s_spectra_global_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SpectraSynq Config Manager! Halting.");
        while(1);
    }
    if (spectra_config_manager_load_defaults(&s_spectra_global_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load default SpectraSynq Config! Halting.");
        while(1);
    }

    // NEW: Initialize SpectraSynq Visual Input Module
    if (spectra_visual_input_init(NULL) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SpectraSynq Visual Input! Halting.");
        while(1);
    }

    // NEW: Initialize SpectraSynq Color Palette Modules
    if (spectra_color_palette_init(&s_palette_ctx_ch0, COLOR_PALETTE_RAINBOW) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Ch0 Color Palette! Halting.");
        while(1);
    }
    // Example: For Channel 1, set a mono hue palette if enabled in config
    s_palette_ctx_ch1.mono_hue_value = 0.5f; // Green hue for differentiation
    if (spectra_color_palette_init(&s_palette_ctx_ch1, COLOR_PALETTE_MONO_HUE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Ch1 Color Palette! Halting.");
        while(1);
    }

    // NEW: Initialize SpectraSynq Visual Algorithm Modules
    if (spectra_visual_algorithm_init(&s_algo_ctx_ch0, VISUAL_ALGO_VU_METER) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Ch0 Visual Algo! Halting.");
        while(1);
    }
    if (spectra_visual_algorithm_init(&s_algo_ctx_ch1, VISUAL_ALGO_SPECTRUM_BAR) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Ch1 Visual Algo! Halting.");
        while(1);
    }

    // NEW: Initialize SpectraSynq Zone Painter Modules
    // Note: Zone painter needs to know about the palette and algorithm contexts to pass to its internal calls.
    // The current design of zone_painter_process (taking only visual_input_features_t) assumes it fetches these from a global config or its own context.
    // Re-evaluating the zone_painter_init/process API for P3.T3 given current design.
    // The zone_painter_context_t should hold pointers to palette and algorithm contexts.
    // Let's pass the contexts for color palette and visual algorithm to the zone painter initialization.

    // Temporarily adjusting configuration for this phase to use a basic VU meter on Ch0 and Spectrum on Ch1 as examples
    // Make sure the config has these set for the example.
    // In spectra_config_manager_load_defaults, I set: 
    // config->channels[0].zones[0].algorithm = VISUAL_ALGO_VU_METER;
    // config->channels[1].zones[0].algorithm = VISUAL_ALGO_SPECTRUM_BAR;
    // This is already done in spectra_config_manager.c

    if (spectra_zone_painter_init(
        &s_zone_painter_ctx_ch0,
        &s_spectra_global_config.channels[0],
        s_led_buffer_ch0,
        s_spectra_global_config.channels[0].led_count, // Use configured LED count for channel
        &s_palette_ctx_ch0, // Pass Channel 0's palette context
        &s_algo_ctx_ch0     // Pass Channel 0's algorithm context
    ) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Ch0 Zone Painter! Halting.");
        while(1);
    }

    // Channel 1 initialization (even if not fully used yet, init for completeness)
    if (spectra_zone_painter_init(
        &s_zone_painter_ctx_ch1,
        &s_spectra_global_config.channels[1],
        s_led_buffer_ch1,
        s_spectra_global_config.channels[1].led_count, // Use configured LED count for channel
        &s_palette_ctx_ch1, // Pass Channel 1's palette context
        &s_algo_ctx_ch1     // Pass Channel 1's algorithm context
    ) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Ch1 Zone Painter! Halting.");
        while(1);
    }

    // NEW: Initialize SpectraSynq Post-Processing Modules
    if (spectra_post_processing_init(&s_post_processing_ctx_ch0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Ch0 Post-Processing! Halting.");
        while(1);
    }
    if (spectra_post_processing_init(&s_post_processing_ctx_ch1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Ch1 Post-Processing! Halting.");
        while(1);
    }

	// Start the main core (cpu_core.h, Audio/Web tasks) - NOW ON CORE 0
	(void)xTaskCreatePinnedToCore(loop_cpu, "loop_cpu", 8192, NULL, 1, NULL, 0);

    // NEW: Start SpectraSynq Visual Task on Core 1
    (void)xTaskCreatePinnedToCore(loop_spectra_visuals, "spectra_visual_task", 8192, NULL, 1, NULL, 1);

	#ifdef PROFILER_ENABLED
		while(1){
			vTaskDelay(1);
			log_function_stack();
		}
	#endif
}