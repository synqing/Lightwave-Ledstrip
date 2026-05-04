/**
 * @file main.cpp
 * @brief M1 fringe visibility preflight for K1v2 hardware.
 *
 * Cycles a high-contrast visual version of the M1 phase offsets. This is a
 * human-inspection preflight only; the formal photometer protocol remains the
 * white-light FringeCoherenceGenerator.
 */

#include <Arduino.h>
#include <FastLED.h>
#include <math.h>

#include "test_modes/m1_fringe_coherence.h"

namespace {

using lightwaveos::test_modes::kFringePatternCount;
using lightwaveos::test_modes::kFringeStripLength;
using lightwaveos::test_modes::kPatternUniformControl;

constexpr uint8_t kStripAPin = 6;
constexpr uint8_t kStripBPin = 7;
constexpr uint8_t kBrightness = 220;
constexpr uint32_t kPatternDwellMs = 8000;
constexpr uint8_t kVisualBase = 72;
constexpr uint8_t kVisualAmplitude = 183;
constexpr uint8_t kWavelengthLeds = 12;
constexpr float kTwoPi = 6.28318530717958647692f;

CRGB g_stripA[kFringeStripLength];
CRGB g_stripB[kFringeStripLength];

uint8_t g_sequenceIndex = 0;
uint32_t g_lastSwitchMs = 0;

constexpr uint8_t kSequence[] = {
    kPatternUniformControl,
    0,
    1,
    2,
    3,
    4,
};

const char* patternLabel(uint8_t pattern) {
    switch (pattern) {
        case kPatternUniformControl: return "UNIFORM control";
        case 0: return "0 phase: identical strips";
        case 1: return "pi/4 phase offset";
        case 2: return "pi/2 phase offset";
        case 3: return "3pi/4 phase offset";
        case 4: return "pi phase offset";
        default: return "unknown";
    }
}

float phaseForPattern(uint8_t pattern) {
    switch (pattern) {
        case 1: return kTwoPi * 0.125f;
        case 2: return kTwoPi * 0.25f;
        case 3: return kTwoPi * 0.375f;
        case 4: return kTwoPi * 0.5f;
        case 0:
        default: return 0.0f;
    }
}

uint8_t visualSampleAt(uint16_t ledIndex, float phaseRad) {
    const float angle = (kTwoPi * static_cast<float>(ledIndex) / static_cast<float>(kWavelengthLeds)) + phaseRad;
    const float v = static_cast<float>(kVisualBase) + static_cast<float>(kVisualAmplitude) * (0.5f + 0.5f * sinf(angle));
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return static_cast<uint8_t>(v + 0.5f);
}

void renderVisualPattern() {
    const uint8_t pattern = kSequence[g_sequenceIndex];
    if (pattern == kPatternUniformControl) {
        fill_solid(g_stripA, kFringeStripLength, CRGB(96, 96, 96));
        fill_solid(g_stripB, kFringeStripLength, CRGB(96, 96, 96));
        return;
    }

    const float phaseA = 0.0f;
    const float phaseB = phaseForPattern(pattern);
    constexpr CRGB kColourA(0, 180, 255);    // cyan
    constexpr CRGB kColourB(255, 120, 0);    // amber

    for (uint16_t i = 0; i < kFringeStripLength; ++i) {
        g_stripA[i] = kColourA;
        g_stripA[i].nscale8(visualSampleAt(i, phaseA));
        g_stripB[i] = kColourB;
        g_stripB[i].nscale8(visualSampleAt(i, phaseB));
    }
}

void setSequenceIndex(uint8_t index) {
    g_sequenceIndex = index % (sizeof(kSequence) / sizeof(kSequence[0]));
    const uint8_t pattern = kSequence[g_sequenceIndex];
    g_lastSwitchMs = millis();
    Serial.printf("[M1] Pattern %u/%u: %s\r\n",
                  static_cast<unsigned>(g_sequenceIndex + 1),
                  static_cast<unsigned>(sizeof(kSequence) / sizeof(kSequence[0])),
                  patternLabel(pattern));
}

void handleSerial() {
    while (Serial.available() > 0) {
        const int c = Serial.read();
        if (c >= '0' && c <= '4') {
            const uint8_t target = static_cast<uint8_t>(c - '0');
            for (uint8_t i = 0; i < sizeof(kSequence) / sizeof(kSequence[0]); ++i) {
                if (kSequence[i] == target) {
                    setSequenceIndex(i);
                    break;
                }
            }
        } else if (c == 'u' || c == 'U') {
            setSequenceIndex(0);
        } else if (c == 'n' || c == 'N' || c == ' ') {
            setSequenceIndex(static_cast<uint8_t>(g_sequenceIndex + 1));
        } else if (c == 's' || c == 'S') {
            Serial.printf("[M1] Active: %s, dwell=%lu ms, brightness=%u\r\n",
                          patternLabel(kSequence[g_sequenceIndex]),
                          static_cast<unsigned long>(millis() - g_lastSwitchMs),
                          static_cast<unsigned>(kBrightness));
        }
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1500);

    FastLED.addLeds<WS2812B, kStripAPin, GRB>(g_stripA, kFringeStripLength);
    FastLED.addLeds<WS2812B, kStripBPin, GRB>(g_stripB, kFringeStripLength);
    FastLED.setBrightness(kBrightness);
    FastLED.clear(true);

    Serial.println();
    Serial.println("[M1] K1v2 high-contrast fringe visibility preflight");
    Serial.println("[M1] Visual mode: strip A cyan, strip B amber, strong dark/light modulation");
    Serial.println("[M1] Controls: 0..4 phase patterns, u uniform, n next, s status");
    setSequenceIndex(0);
}

void loop() {
    handleSerial();

    const uint32_t now = millis();
    if (now - g_lastSwitchMs >= kPatternDwellMs) {
        setSequenceIndex(static_cast<uint8_t>(g_sequenceIndex + 1));
    }

    renderVisualPattern();
    FastLED.show();
    delay(16);
}
