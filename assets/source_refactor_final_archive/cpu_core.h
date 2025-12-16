/*
------------------------------------------------------------------------
                                                                  _
                                                                 | |
  ___   _ __    _   _             ___    ___    _ __    ___      | |__
 / __| | '_ \  | | | |           / __|  / _ \  | '__|  / _ \     | '_ \ 
| (__  | |_) | | |_| |          | (__  | (_) | | |    |  __/  _  | | | |
 \___| | .__/   \__,_|           \___|  \___/  |_|     \___| (_) |_| |_|
       | |              ______
       |_|             |______|

Main loop of the CPU core
*/

#include <stdint.h> // For uint8_t, int64_t, uint32_t
#include "esp_timer.h" // For esp_timer_get_time
#include "esp_log.h"   // For ESP_LOGI
#include "freertos/FreeRTOS.h" // For FreeRTOS types
#include "freertos/task.h"   // For xTaskCreatePinnedToCore
#include "goertzel.h"          // NEW INCLUDE for spectrogram_smooth
#include "vu.h"                // NEW INCLUDE for vu_level
#include "spectra_audio_interface.h" // NEW INCLUDE for audio features queue
#include "L_common_audio_defs.h" // NEW INCLUDE for audio_features_s3_t definition

// Assuming other necessary headers for project-specific functions
// (like start_profile, watch_cpu_fps, acquire_sample_chunk etc.)
// are included elsewhere or in a main project header.

// TAG for logging, if not defined elsewhere, should be defined here, e.g.:
// static const char *TAG = "cpu_core";

// Declare global variables from other files if they are not exposed via extern in their headers
extern float spectrogram_smooth[NUM_FREQS]; // From goertzel.h
extern float vu_level;                      // From vu.h

void run_cpu() {
	start_profile(__COUNTER__, __func__);

	// Update the FPS_CPU variable
	watch_cpu_fps();  // (system.h)

	static uint8_t iter = 0;
	iter++;
	if(iter == 10){
		iter = 0;
		run_indicator_light();
		sync_configuration_to_file_system(); // (configuration.h)
	}

	// Get new audio chunk from the I2S microphone
	acquire_sample_chunk();	 // (microphone.h)

	int64_t processing_start_us = esp_timer_get_time(); // -------------------------------

	int64_t start_time;
	int64_t end_time;
	int64_t duration;

	static uint8_t step = 0;
	step++;
	
	start_time = esp_timer_get_time();
	calculate_magnitudes();  // (goertzel.h)
	end_time = esp_timer_get_time();
	duration = end_time - start_time;
	if(step == 0){ ESP_LOGI(TAG, "GOR: %lld us", duration); }

	perform_fft();	 // (fft.h)	
	
	//estimate_pitch(); // (pitch.h)

	get_chromagram();        // (goertzel.h)

	run_vu(); // (vu.h)

    // NEW: Populate and send SpectraSynq audio features
    audio_features_s3_t current_audio_features;
    memset(&current_audio_features, 0, sizeof(audio_features_s3_t)); // Clear the struct

    // Map Emotiscope's Goertzel magnitudes to SpectraSynq's L1 bins
    for (int i = 0; i < NUM_FREQS && i < L1_PRIMARY_NUM_BINS; i++) {
        current_audio_features.l1_goertzel_magnitudes[i] = spectrogram_smooth[i];
    }

    // Map Emotiscope's VU level
    current_audio_features.vu_level_main_linear = vu_level;
    // Emotiscope's vu_level is already scaled, so for now, we'll assume linear.
    // For dBFS, we'd need a reference max value.
    if (vu_level > 0.0f) {
        current_audio_features.vu_level_main_dbfs = 20.0f * log10f(vu_level);
    } else {
        current_audio_features.vu_level_main_dbfs = -90.0f; // Very low dBFS for silence
    }


    // Placeholder for clipping detection (Emotiscope's direct clipping flag isn't easily accessible here)
    current_audio_features.is_clipping_detected = false;

    // Placeholder for frame number and timestamp
    static uint64_t frame_count = 0;
    frame_count++;
    current_audio_features.frame_number = frame_count;
    current_audio_features.timestamp_ms_l0_in = esp_timer_get_time() / 1000; // Milliseconds

    // Tempo information (from Emotiscope's update_tempo()) - Assuming global `tempo_bpm` and `beat_detected` are available
    // if not, these will need to be extracted from tempo.h or other global context.
    // For now, setting to default/dummy values if not immediately available.
    extern float tempo_bpm; // From tempo.h
    extern bool beat_detected; // From tempo.h
    current_audio_features.current_bpm = tempo_bpm;
    current_audio_features.beat_now = beat_detected;
    // tempo_confidence, beat_count, processing_time_us_L2_fft, etc., will be left at 0 for now as Emotiscope doesn't expose them directly in the same way.

    // Send the populated struct to the queue
    if (spectra_audio_interface_send(&current_audio_features, 0) != ESP_OK) { // 0 timeout means non-blocking
        ESP_LOGW(TAG, "Failed to send audio features to queue (queue full/blocked)");
    }

	read_touch(); // (touch.h)

	update_tempo();	 // (tempo.h)

	check_serial();

	uint32_t processing_end_us = esp_timer_get_time(); // --------------------------------

	//------------------------------------------------------------------------------------
	// CPU USAGE CALCULATION
	uint32_t processing_us_spent = processing_end_us - processing_start_us;
	uint32_t audio_core_us_per_loop = 1000000.0 / FPS_CPU;
	float audio_frame_to_processing_ratio = processing_us_spent / (float)audio_core_us_per_loop;
	CPU_CORE_USAGE = audio_frame_to_processing_ratio;

	update_stats(); // (profiler.h)

	run_screen_preview(); // (preview.h)

	//check_boot_button();

	end_profile();
}

void loop_cpu(void *pvParameters) {
	// Initialize all peripherals (system.h) 
	init_system();

	// Start GPU core
	// (void)xTaskCreatePinnedToCore(loop_gpu, "loop_gpu", 8192, NULL, 1, NULL, 1);

	while (1) {
		// COMMENTED OUT: Emotiscope's internal CPU loop calls, as SpectraSynq now handles visuals
		// run_cpu();
		// run_cpu();
		// run_cpu();
		// run_cpu();

		// COMMENTED OUT: Wireless connectivity check and related functions
		/*
		if(esp_wifi_is_connected() == true){
			static uint8_t iter = 0;
			iter++;

			if(iter == 0){
				improv_current_state = IMPROV_CURRENT_STATE_PROVISIONED;
				discovery_check_in();
			}
		}
		*/
	}
}