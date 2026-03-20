#pragma once

#ifndef NATIVE_BUILD
#include <cstdint>

// ── Button events ────────────────────────────────────────────────────────────
enum class ButtonEvent : uint8_t {
    NONE       = 0,
    TAP        = 1,   // Single tap (< 500ms press)
    LONG_PRESS = 2    // Held > 1000ms
};

// ── Setup / Loop ─────────────────────────────────────────────────────────────

/// One-time setup: add 30-LED status strip (GPIO 38) to FastLED, pre-compute
/// gradient LUT, render initial palette gradient.
/// Call BEFORE actors.start() so all FastLED controllers are registered before any show().
void statusStripTouchSetup();

/// Update status strip idle animation. Call at start of loop() with millis().
/// Does not call FastLED.show(); existing render path does that.
void statusStripTouchLoop(uint32_t now);

// ── Palette control ──────────────────────────────────────────────────────────

/// Display a palette preview with a centre-out reveal transition.
void statusStripShowPalette(uint8_t paletteIndex);

/// Cycle to the next idle animation mode (overrides auto-selection).
void statusStripNextIdleMode();

// ── Button polling ───────────────────────────────────────────────────────────

/// Poll TTP223 button state. Call from loop() with millis().
/// Returns TAP or LONG_PRESS when an event completes, NONE otherwise.
/// Safe to call when TTP223 pin is -1 (always returns NONE).
ButtonEvent statusStripPollButton(uint32_t now);

// ── Degraded-mode signals (DEC-011) ─────────────────────────────────────────

/// Set audio failure state. True = start amber breathing, false = clear.
/// Amber breathing overrides normal idle animation while active.
void statusStripSetAudioFailure(bool failed);

/// Trigger a 1-second white flash (NVS corruption detected at boot).
void statusStripTriggerWhiteFlash();

/// Set low-heap flag. True = start dim pulse (5s), false = clear.
void statusStripSetLowHeap(bool low);

#endif
