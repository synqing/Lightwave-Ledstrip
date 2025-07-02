// LED Type Detection Test
// Tries different LED chipsets to find the correct one

#include <FastLED.h>

#define LED_PIN 11  // Test on GPIO 11 first
#define NUM_LEDS 10 // Just test first 10 LEDs

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("LED Type Detection Test");
  Serial.println("Testing GPIO 11 with different LED types");
  Serial.println("Watch for which test makes the LEDs light up!");
  
  // Set low brightness for all tests
  FastLED.setBrightness(30);
}

void testLEDType(const char* typeName) {
  Serial.print("Testing: ");
  Serial.println(typeName);
  
  // Clear and set pattern
  FastLED.clear();
  for(int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(i * 25, 255, 255); // Rainbow pattern
  }
  FastLED.show();
  delay(3000);
  
  // Clear
  FastLED.clear();
  FastLED.show();
  delay(500);
}

void loop() {
  // Test 1: WS2812B with GRB (most common)
  FastLED.clear();
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  testLEDType("WS2812B - GRB");
  
  // Test 2: WS2812 with RGB
  FastLED.clear();
  FastLED.addLeds<WS2812, LED_PIN, RGB>(leds, NUM_LEDS);
  testLEDType("WS2812 - RGB");
  
  // Test 3: WS2811 with RGB
  FastLED.clear();
  FastLED.addLeds<WS2811, LED_PIN, RGB>(leds, NUM_LEDS);
  testLEDType("WS2811 - RGB");
  
  // Test 4: WS2813 with GRB
  FastLED.clear();
  FastLED.addLeds<WS2813, LED_PIN, GRB>(leds, NUM_LEDS);
  testLEDType("WS2813 - GRB");
  
  // Test 5: SK6812 with GRB
  FastLED.clear();
  FastLED.addLeds<SK6812, LED_PIN, GRB>(leds, NUM_LEDS);
  testLEDType("SK6812 - GRB");
  
  // Test 6: Try different color orders with WS2812B
  FastLED.clear();
  FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);
  testLEDType("WS2812B - RGB order");
  
  FastLED.clear();
  FastLED.addLeds<WS2812B, LED_PIN, BRG>(leds, NUM_LEDS);
  testLEDType("WS2812B - BRG order");
  
  Serial.println("\nAll tests complete. Restarting...\n");
  delay(5000);
}