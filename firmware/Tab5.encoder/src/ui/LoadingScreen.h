#pragma once
// ============================================================================
// LoadingScreen - Simple loading screen for Tab5.encoder
// ============================================================================
// Direct framebuffer rendering (no sprites) to minimize memory usage
// during early boot before the main UI is initialised. Layout targets parity
// with the Tab5 deck reference ("WAITING FOR HOST" + animated dots).
// ============================================================================

#ifdef SIMULATOR_BUILD
    #include "M5GFX_Mock.h"
#else
    #include <M5GFX.h>
#endif

namespace LoadingScreen {

/**
 * Show loading screen with message and encoder status
 * @param display M5GFX display reference
 * @param message Optional subtitle line (e.g., "Initialising..."), shown under
 *                the main "WAITING FOR HOST" label
 * @param unitA Whether encoder unit A is detected
 * @param unitB Whether encoder unit B is detected
 */
void show(M5GFX& display, const char* message, bool unitA, bool unitB);

/**
 * Update loading screen (animates dots, updates message)
 * @param display M5GFX display reference
 * @param message Optional subtitle line (can be updated based on boot state)
 * @param unitA Whether encoder unit A is detected
 * @param unitB Whether encoder unit B is detected
 */
void update(M5GFX& display, const char* message, bool unitA, bool unitB);

/**
 * Hide loading screen (clears display before UI initialization)
 * @param display M5GFX display reference
 */
void hide(M5GFX& display);

/**
 * Enable or disable PPA acceleration at runtime.
 */
void setPpaEnabled(bool enabled);

/**
 * Check if PPA acceleration is currently enabled.
 */
bool isPpaEnabled();

/**
 * Benchmark logo scaling path and return average time per draw in microseconds.
 * Note: This draws to the active display.
 */
uint32_t benchmarkLogo(M5GFX& display, uint16_t iterations, bool usePpa);

}  // namespace LoadingScreen
