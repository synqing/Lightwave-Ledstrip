// Ultra-simple LED test for debugging
// Tests one LED at a time on GPIO 11 and 12

#include <FastLED.h>

#define LED_PIN_1 11
#define LED_PIN_2 12
#define NUM_LEDS 160

CRGB leds1[NUM_LEDS];
CRGB leds2[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("Starting simple LED test...");
  Serial.println("GPIO 11 and 12");
  
  // Try WS2812B first
  FastLED.addLeds<WS2812B, LED_PIN_1, GRB>(leds1, NUM_LEDS);
  FastLED.addLeds<WS2812B, LED_PIN_2, GRB>(leds2, NUM_LEDS);
  
  // Set very low brightness
  FastLED.setBrightness(30);
  
  // Clear everything
  FastLED.clear();
  FastLED.show();
  delay(100);
  
  Serial.println("Setup complete!");
}

void loop() {
  // Test 1: Light first LED on strip 1 (GPIO 11) - RED
  Serial.println("Test 1: First LED on GPIO 11 - RED");
  FastLED.clear();
  leds1[0] = CRGB(255, 0, 0);  // Red
  FastLED.show();
  delay(2000);
  
  // Test 2: Light first LED on strip 2 (GPIO 12) - BLUE  
  Serial.println("Test 2: First LED on GPIO 12 - BLUE");
  FastLED.clear();
  leds2[0] = CRGB(0, 0, 255);  // Blue
  FastLED.show();
  delay(2000);
  
  // Test 3: Light both - different colors
  Serial.println("Test 3: Both strips - RED on 11, BLUE on 12");
  leds1[0] = CRGB(255, 0, 0);  // Red on strip 1
  leds2[0] = CRGB(0, 0, 255);  // Blue on strip 2
  FastLED.show();
  delay(2000);
  
  // Test 4: Try first 10 LEDs with green
  Serial.println("Test 4: First 10 LEDs GREEN on both strips");
  FastLED.clear();
  for(int i = 0; i < 10; i++) {
    leds1[i] = CRGB(0, 255, 0);  // Green
    leds2[i] = CRGB(0, 255, 0);  // Green
  }
  FastLED.show();
  delay(3000);
  
  // Test 5: Single white LED to test power
  Serial.println("Test 5: Single WHITE LED on each strip");
  FastLED.clear();
  leds1[0] = CRGB(50, 50, 50);  // Dim white
  leds2[0] = CRGB(50, 50, 50);  // Dim white  
  FastLED.show();
  delay(2000);
  
  Serial.println("Loop complete, restarting...\n");
}