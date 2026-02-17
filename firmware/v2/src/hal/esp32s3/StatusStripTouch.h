#pragma once

#ifndef NATIVE_BUILD
#include <cstdint>

/// One-time setup: GPIO 6/21 as INPUT, add 10-LED status strip to FastLED, initial fill (1 yellow).
/// Call BEFORE actors.start() so all FastLED controllers are registered before any show().
void statusStripTouchSetup();

/// Poll touch pins, debounce, update lit count (1–10), fill status strip. Call at start of loop() with millis().
/// Does not call FastLED.show(); existing render path does that.
void statusStripTouchLoop(uint32_t now);

#endif
