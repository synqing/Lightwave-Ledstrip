// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file OtaLedFeedback.h
 * @brief LED visual feedback during OTA firmware updates
 *
 * The K1 prototype has NO screen -- LEDs are the only visual feedback channel.
 * During OTA updates, users need clear visual confirmation that:
 *   1. An update is in progress (amber fill from center outward)
 *   2. The update succeeded (green flashes)
 *   3. The update failed (red flashes)
 *
 * LED Layout:
 *   - CENTER ORIGIN: LED 79/80 is the center point (per project constraint)
 *   - Progress fills outward from center to edges (0 and 159) on each strip
 *   - Both strips (0-159 and 160-319) show identical feedback
 *
 * Threading:
 *   OTA handlers run on the network task (Core 0). This module writes directly
 *   to the CRGB arrays registered with FastLED controllers, then calls
 *   FastLED.show(). During OTA, the normal render loop on Core 1 should be
 *   effectively paused because the Update library blocks flash writes.
 *   The s_otaFeedbackActive flag can be checked by the renderer to skip
 *   rendering while OTA is in progress.
 *
 * Color:
 *   - Progress: Amber/gold CRGB(255, 184, 77) matching UI accent #FFB84D
 *   - Success:  Green CRGB(0, 255, 0)
 *   - Failure:  Red CRGB(255, 0, 0)
 *
 * @see OtaBootVerifier.h (boot validation after OTA)
 * @see WsOtaCommands.cpp (WebSocket OTA upload protocol)
 * @see FirmwareHandlers.cpp (REST OTA upload handler)
 */

#pragma once

#include "../../config/features.h"

#if FEATURE_OTA_UPDATE && !defined(NATIVE_BUILD)

#include <FastLED.h>

namespace lightwaveos {
namespace core {
namespace system {

class OtaLedFeedback {
public:
    // ========================================================================
    // Constants
    // ========================================================================

    static constexpr uint16_t LEDS_PER_STRIP = 160;
    static constexpr uint16_t CENTER_POINT = 80;   // Center of each 160-LED strip

    // Colors
    static constexpr uint8_t AMBER_R = 255;
    static constexpr uint8_t AMBER_G = 184;
    static constexpr uint8_t AMBER_B = 77;

    static constexpr uint8_t SUCCESS_R = 0;
    static constexpr uint8_t SUCCESS_G = 255;
    static constexpr uint8_t SUCCESS_B = 0;

    static constexpr uint8_t FAILURE_R = 255;
    static constexpr uint8_t FAILURE_G = 0;
    static constexpr uint8_t FAILURE_B = 0;

    // Animation parameters
    static constexpr uint8_t FLASH_COUNT = 3;
    static constexpr uint32_t FLASH_ON_MS = 200;
    static constexpr uint32_t FLASH_OFF_MS = 150;

    // Brightness for OTA feedback (moderate, readable but not blinding)
    static constexpr uint8_t OTA_BRIGHTNESS = 80;

    /**
     * @brief Show OTA progress as center-outward fill
     *
     * Fills LEDs from center (79/80) outward proportional to percentage.
     * At 0%: only center LEDs lit.
     * At 50%: LEDs 40-119 lit on each strip.
     * At 100%: all 160 LEDs lit on each strip.
     *
     * Both strips show identical progress.
     *
     * @param percent Progress percentage (0-100)
     */
    static void showProgress(uint8_t percent) {
        if (FastLED.count() < 2) {
            return;  // FastLED not initialized yet
        }

        s_otaFeedbackActive = true;

        // Clamp percentage
        if (percent > 100) {
            percent = 100;
        }

        // Calculate how many LEDs to fill on each side of center
        // CENTER_POINT = 80, so each half has 80 LEDs (indices 0-79 and 80-159)
        // At 100%, fill all 80 LEDs on each side = 160 total per strip
        uint16_t fillPerHalf = (static_cast<uint32_t>(percent) * CENTER_POINT) / 100;

        // Ensure at least 1 LED lit when percent > 0 (visible feedback immediately)
        if (percent > 0 && fillPerHalf == 0) {
            fillPerHalf = 1;
        }

        // Get LED arrays from FastLED controllers
        CRGB* strip1 = FastLED[0].leds();
        CRGB* strip2 = FastLED[1].leds();
        int strip1Size = FastLED[0].size();
        int strip2Size = FastLED[1].size();

        // Set brightness for OTA feedback
        FastLED.setBrightness(OTA_BRIGHTNESS);

        // Clear both strips
        for (int i = 0; i < strip1Size; i++) {
            strip1[i] = CRGB::Black;
        }
        for (int i = 0; i < strip2Size; i++) {
            strip2[i] = CRGB::Black;
        }

        CRGB amber(AMBER_R, AMBER_G, AMBER_B);

        // Fill from center outward on strip 1
        // Left half: CENTER_POINT-1 down to CENTER_POINT-fillPerHalf
        // Right half: CENTER_POINT up to CENTER_POINT+fillPerHalf-1
        for (uint16_t i = 0; i < fillPerHalf; i++) {
            // Left of center (expanding toward LED 0)
            uint16_t leftIdx = CENTER_POINT - 1 - i;
            if (leftIdx < static_cast<uint16_t>(strip1Size)) {
                strip1[leftIdx] = amber;
            }

            // Right of center (expanding toward LED 159)
            uint16_t rightIdx = CENTER_POINT + i;
            if (rightIdx < static_cast<uint16_t>(strip1Size)) {
                strip1[rightIdx] = amber;
            }
        }

        // Strip 2 mirrors strip 1 (identical feedback)
        for (int i = 0; i < strip1Size && i < strip2Size; i++) {
            strip2[i] = strip1[i];
        }

        FastLED.show();
    }

    /**
     * @brief Flash all LEDs green to indicate successful OTA update
     *
     * Shows 3 green flashes. Called after Update.end() succeeds,
     * just before the device reboots.
     */
    static void showSuccess() {
        flashColor(CRGB(SUCCESS_R, SUCCESS_G, SUCCESS_B));
    }

    /**
     * @brief Flash all LEDs red to indicate failed OTA update
     *
     * Shows 3 red flashes. Called when OTA upload encounters an error
     * or is aborted.
     */
    static void showFailure() {
        flashColor(CRGB(FAILURE_R, FAILURE_G, FAILURE_B));
        s_otaFeedbackActive = false;
    }

    /**
     * @brief Restore LED control to normal effect rendering
     *
     * Clears the OTA feedback active flag so the renderer can resume.
     * Does NOT clear the LED buffers -- the renderer will overwrite
     * them on its next frame.
     */
    static void restore() {
        s_otaFeedbackActive = false;
    }

    /**
     * @brief Check if OTA LED feedback is currently active
     *
     * The renderer can check this flag to skip its normal render loop
     * while OTA is in progress, avoiding visual glitches from both
     * the OTA feedback and effect renderer writing to the same buffers.
     *
     * @return true if OTA LED feedback is actively controlling the LEDs
     */
    static bool isActive() {
        return s_otaFeedbackActive;
    }

private:
    /**
     * @brief Flash all LEDs with a given color
     *
     * Both strips flash identically. Uses blocking delays since this
     * is called during OTA completion/failure when no other LED work
     * should be happening.
     *
     * @param color The color to flash
     */
    static void flashColor(CRGB color) {
        if (FastLED.count() < 2) {
            return;
        }

        CRGB* strip1 = FastLED[0].leds();
        CRGB* strip2 = FastLED[1].leds();
        int strip1Size = FastLED[0].size();
        int strip2Size = FastLED[1].size();

        FastLED.setBrightness(OTA_BRIGHTNESS);

        for (uint8_t flash = 0; flash < FLASH_COUNT; flash++) {
            // ON: fill both strips with color
            for (int i = 0; i < strip1Size; i++) {
                strip1[i] = color;
            }
            for (int i = 0; i < strip2Size; i++) {
                strip2[i] = color;
            }
            FastLED.show();
            delay(FLASH_ON_MS);

            // OFF: clear both strips
            for (int i = 0; i < strip1Size; i++) {
                strip1[i] = CRGB::Black;
            }
            for (int i = 0; i < strip2Size; i++) {
                strip2[i] = CRGB::Black;
            }
            FastLED.show();

            // Gap between flashes (skip after last flash)
            if (flash < FLASH_COUNT - 1) {
                delay(FLASH_OFF_MS);
            }
        }
    }

    static inline bool s_otaFeedbackActive = false;
};

} // namespace system
} // namespace core
} // namespace lightwaveos

#else  // !FEATURE_OTA_UPDATE || NATIVE_BUILD

// Stub implementation for native builds and non-OTA configurations
namespace lightwaveos {
namespace core {
namespace system {

class OtaLedFeedback {
public:
    static void showProgress(uint8_t) {}
    static void showSuccess() {}
    static void showFailure() {}
    static void restore() {}
    static bool isActive() { return false; }
};

} // namespace system
} // namespace core
} // namespace lightwaveos

#endif  // FEATURE_OTA_UPDATE && !NATIVE_BUILD
