#include "hal/esp32s3/StatusStripTouch.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <FastLED.h>

#define LW_LOG_TAG "Touch"
#include "utils/Log.h"

namespace {

// Pin and strip
static constexpr uint8_t STATUS_STRIP_PIN = 38;
static constexpr uint8_t TOUCH_UP_PIN = 6;
static constexpr uint8_t TOUCH_DOWN_PIN = 21;
static constexpr uint8_t STATUS_LED_COUNT = 10;

// Burst sampling: 48 rapid digitalRead() per poll.
// At ~1µs/read this costs ~48µs per pin, giving ~2,800 samples/sec
// instead of ~58 with single-read polling.  Noise averages out.
static constexpr uint8_t BURST_SAMPLES = 48;

// Touch timing
static constexpr uint32_t BOOT_IGNORE_MS = 700;
static constexpr uint32_t PRESS_LOCKOUT_MS = 150;

// Duty-cycle voting with burst data.
// 40ms window × ~58 Hz loop × 48 burst = ~110 samples per decision.
// Touch = HIGH% >= TOUCH_THRESHOLD;  Release = HIGH% < RELEASE_THRESHOLD.
static constexpr uint32_t VOTE_WINDOW_MS = 40;
static constexpr uint8_t TOUCH_THRESHOLD_PCT = 25;   // >=25% HIGH = touched
static constexpr uint8_t RELEASE_THRESHOLD_PCT = 10;  // <10% HIGH = released

// Strip
static constexpr uint8_t STATUS_BRIGHTNESS = 50;
static CRGB statusLeds[STATUS_LED_COUNT];
static uint8_t statusLitCount = 1;
static uint32_t statusStripBootMs = 0;

// Per-pin voting state
struct PinVoter {
    // Voting window (resets every VOTE_WINDOW_MS)
    uint16_t highCount = 0;
    uint16_t totalCount = 0;
    uint32_t windowStartMs = 0;

    // State
    bool touched = false;           // voted state (with hysteresis)
    uint32_t lastAcceptedMs = 0;    // lockout timer
};
static PinVoter voterUp;
static PinVoter voterDown;

// Burst-read a pin: 32 rapid digitalRead() in a tight loop.
// Returns count of HIGH readings (0–BURST_SAMPLES).
static uint8_t burstRead(uint8_t pin) {
    uint8_t highs = 0;
    for (uint8_t i = 0; i < BURST_SAMPLES; i++) {
        highs += digitalRead(pin);
    }
    return highs;
}

static void fillStrip() {
    CRGB dimYellow = CRGB::Yellow;
    dimYellow.nscale8(STATUS_BRIGHTNESS);
    for (uint8_t i = 0; i < statusLitCount; i++) {
        statusLeds[i] = dimYellow;
    }
    for (uint8_t i = statusLitCount; i < STATUS_LED_COUNT; i++) {
        statusLeds[i] = CRGB::Black;
    }
}

// Returns true on a newly accepted rising edge (released -> touched).
static bool processPin(uint8_t pin, PinVoter& v, uint32_t now) {
    uint8_t highs = burstRead(pin);

    // Accumulate into voting window
    v.highCount += highs;
    v.totalCount += BURST_SAMPLES;

    // Evaluate at end of each voting window
    if ((now - v.windowStartMs) < VOTE_WINDOW_MS) {
        return false;
    }

    bool accepted = false;
    if (v.totalCount > 0) {
        uint8_t pct = static_cast<uint8_t>((v.highCount * 100U) / v.totalCount);
        bool wasTouched = v.touched;

        // Hysteresis: different thresholds for touch vs release
        if (!wasTouched && pct >= TOUCH_THRESHOLD_PCT) {
            v.touched = true;
        } else if (wasTouched && pct < RELEASE_THRESHOLD_PCT) {
            v.touched = false;
        }

        // Rising edge: released -> touched
        if (v.touched && !wasTouched) {
            if ((now - v.lastAcceptedMs) >= PRESS_LOCKOUT_MS) {
                v.lastAcceptedMs = now;
                accepted = true;
            }
        }
    }

    // Reset voting window
    v.highCount = 0;
    v.totalCount = 0;
    v.windowStartMs = now;

    return accepted;
}

}  // namespace

void statusStripTouchSetup() {
    statusStripBootMs = millis();

    // TTP223 is a driven CMOS output — no pull needed (matches proven sketch).
    pinMode(TOUCH_UP_PIN, INPUT);
    pinMode(TOUCH_DOWN_PIN, INPUT);

    FastLED.addLeds<WS2812, STATUS_STRIP_PIN, GRB>(statusLeds, STATUS_LED_COUNT);

    voterUp.windowStartMs = statusStripBootMs;
    voterUp.lastAcceptedMs = statusStripBootMs;
    voterDown.windowStartMs = statusStripBootMs;
    voterDown.lastAcceptedMs = statusStripBootMs;

    statusLitCount = 1;
    fillStrip();

    LW_LOGI("StatusStrip: pin=%u, up=%u dn=%u", STATUS_STRIP_PIN, TOUCH_UP_PIN, TOUCH_DOWN_PIN);
}

void statusStripTouchLoop(uint32_t now) {
    // Boot ignore window
    if ((now - statusStripBootMs) < BOOT_IGNORE_MS) {
        voterUp.windowStartMs = now;
        voterDown.windowStartMs = now;
        return;
    }

    // Process UP pin
    if (processPin(TOUCH_UP_PIN, voterUp, now)) {
        if (statusLitCount < STATUS_LED_COUNT) {
            statusLitCount++;
        }
    }

    // Process DOWN pin
    if (processPin(TOUCH_DOWN_PIN, voterDown, now)) {
        if (statusLitCount > 1) {
            statusLitCount--;
        }
    }

    fillStrip();
}

#endif  // NATIVE_BUILD
