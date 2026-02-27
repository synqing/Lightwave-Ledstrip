#pragma once
// ============================================================================
// LoadingScreen - Simple loading screen for Tab5.encoder
// ============================================================================
// LVGL-based loading screen (migrated from M5GFX)
// Layout targets parity with the Tab5 deck reference ("WAITING FOR HOST" + animated dots).
// ============================================================================

#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)
    #include <M5Unified.h>
    #include <lvgl.h>
#else
    #ifdef SIMULATOR_BUILD
        #include "M5GFX_Mock.h"
    #else
        #include <M5GFX.h>
    #endif
#endif

namespace LoadingScreen {

/**
 * Show loading screen with message and encoder status
 * @param display M5GFX display reference (Legacy/ignored in LVGL mode)
 * @param message Optional subtitle line (e.g., "Initialising..."), shown under
 *                the main "WAITING FOR HOST" label
 * @param unitA Whether encoder unit A is detected
 * @param unitB Whether encoder unit B is detected
 */
#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)
void show(M5GFX& display, const char* message, bool unitA, bool unitB);
#else
void show(M5GFX& display, const char* message, bool unitA, bool unitB);
#endif

/**
 * Update loading screen (animates dots, updates message)
 * @param display M5GFX display reference (Legacy/ignored in LVGL mode)
 * @param message Optional subtitle line (can be updated based on boot state)
 * @param unitA Whether encoder unit A is detected
 * @param unitB Whether encoder unit B is detected
 */
void update(M5GFX& display, const char* message, bool unitA, bool unitB);

/**
 * Hide loading screen (clears display before UI initialization)
 * @param display M5GFX display reference (Legacy/ignored in LVGL mode)
 */
void hide(M5GFX& display);

/**
 * Enable or disable PPA acceleration at runtime.
 * (No-op in LVGL mode)
 */
void setPpaEnabled(bool enabled);

/**
 * Check if PPA acceleration is currently enabled.
 */
bool isPpaEnabled();

/**
 * Benchmark logo scaling path and return average time per draw in microseconds.
 * (Returns 0 in LVGL mode)
 */
uint32_t benchmarkLogo(M5GFX& display, uint16_t iterations, bool usePpa);

}  // namespace LoadingScreen
