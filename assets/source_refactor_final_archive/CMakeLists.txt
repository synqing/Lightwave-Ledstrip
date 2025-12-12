# CMakeLists.txt for SpectraSynq Emotiscope Refactor Component

# Standard boilerplate for ESP-IDF projects (REMOVE THESE LINES FOR COMPONENT CMAKE)
# cmake_minimum_required(VERSION 3.16)
# include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project name (REMOVE THIS LINE FOR COMPONENT CMAKE)
# project(SpectraSynq_Emotiscope_Refactor)

# Define the source files for this component
# Add all .c and .S files present in the SPECTRASYNQ_EMOTISCOPE_REFACTOR/ directory
set(SRCS
    Emotiscope.c
    # Emotiscope original modules (now assumed to be mostly header-only or included elsewhere)
    # cpu_core.c # Is cpu_core.h
    # gpu_core.c # Is gpu_core.h
    # led_driver.c # Is led_driver.h
    # leds.c # Is leds.h
    # light_modes.c # Is light_modes.h
    # microphone.c # Is microphone.h
    # goertzel.c # Is goertzel.h
    # vu.c # Is vu.h
    # tempo.c # Is tempo.h
    # pitch.c # Is pitch.h
    # system.c # Is system.h
    # serial.c # Is serial.h
    # configuration.c # Is configuration.h
    # utilities.c # Is utilities.h
    # touch.c # Is touch.h
    # indicator_light.c # Is indicator_light.h
    # fft.c # Is fft.h
    # perlin.c # Is perlin.h
    # ui.c # Is ui.h
    # standby.c # Is standby.h
    # screensaver.c # Is screensaver.h
    # preview.c # Is preview.h
    # testing.c # Is testing.h

    # Our new SpectraSynq modules
    spectra_audio_interface.c
    spectra_config_manager.c
    spectra_visual_input.c
    spectra_zone_painter.c
    spectra_color_palette.c
    spectra_visual_algorithm.c
    spectra_symmetry_engine.c
    spectra_post_processing.c

    # Assembly files (SIMD)
    dsps_add_f32_ae32_fast.S
    dsps_addc_f32_ae32_fast.S
    dsps_mul_f32_ae32_fast.S
    dsps_mulc_f32_ae32_fast.S
)

# Explicitly add this directory as an include path for the component
set(COMPONENT_PRIV_INCLUDE_DIRS .)

# Link to necessary ESP-IDF components and other dependencies
# Based on the headers seen in .c and .h files:
# esp_log, freertos, driver, nvs_flash, esp_system, esp_timer, esp_heap_caps, esp_random, esp_check
# lwip, mbedtls, esp_netif, esp_event for wireless (though commented out, still needed if wireless.h is processed)
# esp_dsp for SIMD operations

# Add any extra component directories that are NOT part of the standard IDF path
# This assumes 'components/esp-dsp' from the root Emotiscope project is copied/symlinked
# into SPECTRASYNQ_EMOTISCOPE_REFACTOR/components/ or referenced via an absolute path if necessary.
# For now, let's assume that the build system will find `components/esp-dsp` if it is in the root `Emotiscope` project,
# as IDF_PATH is set at the workspace level. If we need to refer to a local copy, we would do: 
# set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/components/esp-dsp)

# Standard component dependencies - this lists components that our project depends on
# This list is derived from the includes in .c and .h files of the refactored project.
idf_component_register(
    SRCS "${SRCS}"
    INCLUDE_DIRS "."
    REQUIRES
        esp_common
        freertos
        log
        driver
        nvs_flash
        esp_system
        esp_timer
        heap_caps
        esp_random
        esp_rom
        newlib
        # Add these only if actually needed and not commented out:
        # esp_wifi
        # esp_event
        # esp_netif
        # lwip
        # mbedtls # Often required for WiFi/TLS
        # esp_http_server
        # esp_http_client
        # esp_tls
        # esp_crt_bundle
        # For DSP operations (these are usually found via IDF_PATH components or global settings)
        esp_dsp # Explicitly add esp-dsp as a required component
)
