// Hardware strip test — rolling motion on both strips
// Build: pio run -e strip_test_k1v2 -t upload --upload-port /dev/cu.usbmodem101
#include <Arduino.h>
#include <FastLED.h>

#define NUM_LEDS     160
#define STRIP1_PIN   6   // Bottom
#define STRIP2_PIN   7   // Top
#define BRIGHTNESS   120
#define FPS          120

CRGB strip1[NUM_LEDS];
CRGB strip2[NUM_LEDS];

// Palette: deep ocean to warm amber (no rainbow)
static const CRGBPalette16 oceanAmber(
    CRGB(0x00, 0x1a, 0x33),  // deep navy
    CRGB(0x00, 0x4d, 0x80),  // ocean blue
    CRGB(0x00, 0x80, 0x9e),  // teal
    CRGB(0x2e, 0xa0, 0x8a),  // sea green
    CRGB(0x5c, 0xb8, 0x5c),  // sage
    CRGB(0x8a, 0xc0, 0x3e),  // chartreuse
    CRGB(0xc8, 0xa0, 0x20),  // gold
    CRGB(0xe0, 0x7a, 0x10),  // amber
    CRGB(0xd0, 0x40, 0x10),  // burnt orange
    CRGB(0xa0, 0x20, 0x20),  // deep red
    CRGB(0x60, 0x10, 0x30),  // burgundy
    CRGB(0x30, 0x08, 0x40),  // plum
    CRGB(0x10, 0x10, 0x50),  // indigo
    CRGB(0x00, 0x20, 0x60),  // midnight blue
    CRGB(0x00, 0x30, 0x70),  // royal blue
    CRGB(0x00, 0x1a, 0x33)   // back to deep navy
);

// --- Effect 1: Dual rolling waves ---
void rollingWaves(CRGB* leds, uint16_t count, uint32_t ms, int8_t direction) {
    uint8_t baseHue = ms / 30;  // slow palette drift
    for (uint16_t i = 0; i < count; i++) {
        // Two superimposed sine waves at different frequencies
        uint8_t wave1 = sin8((i * 7) + (ms / 4) * direction);
        uint8_t wave2 = sin8((i * 11) + (ms / 6) * direction);
        uint8_t bright = qadd8(wave1 >> 1, wave2 >> 1);
        uint8_t palIdx = baseHue + (i * 2) + ((ms / 20) * direction);
        leds[i] = ColorFromPalette(oceanAmber, palIdx, bright, LINEARBLEND);
    }
}

// --- Effect 2: Trailing comet flashes ---
void cometTrails(CRGB* leds, uint16_t count, uint32_t ms, int8_t direction) {
    // Fade existing
    fadeToBlackBy(leds, count, 40);

    // 3 comets at different speeds
    for (uint8_t c = 0; c < 3; c++) {
        uint16_t speed = 80 + c * 40;  // different speeds
        int16_t pos;
        if (direction > 0) {
            pos = ((ms / speed) + (c * 53)) % (count + 20) - 10;
        } else {
            pos = count - 1 - (((ms / speed) + (c * 53)) % (count + 20) - 10);
        }
        uint8_t palIdx = c * 80 + (ms / 50);
        CRGB color = ColorFromPalette(oceanAmber, palIdx, 255, LINEARBLEND);

        // Head + trail
        for (int8_t t = 0; t < 12; t++) {
            int16_t p = pos - t * direction;
            if (p >= 0 && p < (int16_t)count) {
                uint8_t fade = 255 - (t * 22);
                leds[p] += color % fade;
            }
        }
    }
}

// --- Effect 3: Expanding ripples from centre ---
void centreRipples(CRGB* leds, uint16_t count, uint32_t ms) {
    uint16_t centre = count / 2;
    fadeToBlackBy(leds, count, 30);

    // 4 ripple rings expanding from centre
    for (uint8_t r = 0; r < 4; r++) {
        float radius = fmodf((float)(ms) / 300.0f + r * 0.25f, 1.0f) * (float)centre;
        uint8_t bright = 255 - (uint8_t)(radius * 255.0f / (float)centre);
        uint8_t palIdx = r * 64 + (ms / 40);
        CRGB color = ColorFromPalette(oceanAmber, palIdx, bright, LINEARBLEND);

        int16_t posL = centre - (int16_t)radius;
        int16_t posR = centre + (int16_t)radius;
        // Draw 3-pixel wide rings
        for (int8_t w = -1; w <= 1; w++) {
            uint8_t wFade = (w == 0) ? 255 : 140;
            if (posL + w >= 0 && posL + w < (int16_t)count)
                leds[posL + w] += color % wFade;
            if (posR + w >= 0 && posR + w < (int16_t)count)
                leds[posR + w] += color % wFade;
        }
    }
}

// Cycle: 4s per effect, 3 effects
void renderFrame(uint32_t ms) {
    uint8_t phase = (ms / 4000) % 3;
    // Crossfade near boundaries
    uint16_t inPhase = ms % 4000;

    switch (phase) {
        case 0:
            rollingWaves(strip1, NUM_LEDS, ms, 1);
            rollingWaves(strip2, NUM_LEDS, ms, -1);  // opposite direction
            break;
        case 1:
            cometTrails(strip1, NUM_LEDS, ms, 1);
            cometTrails(strip2, NUM_LEDS, ms, -1);
            break;
        case 2:
            centreRipples(strip1, NUM_LEDS, ms);
            centreRipples(strip2, NUM_LEDS, ms);
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== K1 Strip Hardware Test ===");
    Serial.printf("Strip 1 (bottom): GPIO %d, %d LEDs\n", STRIP1_PIN, NUM_LEDS);
    Serial.printf("Strip 2 (top):    GPIO %d, %d LEDs\n", STRIP2_PIN, NUM_LEDS);

    FastLED.addLeds<WS2812, STRIP1_PIN, GRB>(strip1, NUM_LEDS);
    FastLED.addLeds<WS2812, STRIP2_PIN, GRB>(strip2, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);
    FastLED.clear(true);

    Serial.println("Running: waves → comets → ripples (4s each, loop)");
    Serial.println("Both strips should show identical motion in mirror.\n");
}

void loop() {
    uint32_t ms = millis();
    renderFrame(ms);
    FastLED.show();
    FastLED.delay(1000 / FPS);
}
