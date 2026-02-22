#pragma once

#ifndef NATIVE_BUILD
#include <cstdint>

/// One-time setup: add 30-LED status strip (GPIO 38) to FastLED, pre-compute
/// gradient LUT, render initial palette gradient.
/// Call BEFORE actors.start() so all FastLED controllers are registered before any show().
void statusStripTouchSetup();

/// Update status strip idle animation. Call at start of loop() with millis().
/// Does not call FastLED.show(); existing render path does that.
void statusStripTouchLoop(uint32_t now);

/// Display a palette preview with a centre-out reveal transition.
void statusStripShowPalette(uint8_t paletteIndex);

/// Cycle to the next idle animation mode (overrides auto-selection).
void statusStripNextIdleMode();

#endif
