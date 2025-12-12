// Diagnostic test for WS2812 LED strips
// Tests basic functionality and helps identify issues

#include <FastLED.h>

#define LED_PIN_1 11
#define LED_PIN_2 12
#define NUM_LEDS 160
#define TEST_BRIGHTNESS 20  // Very low for safety

CRGB leds1[NUM_LEDS];
CRGB leds2[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n=== WS2812 LED Strip Diagnostic ===");
  Serial.println("ESP32-S3 GPIO Pin Test");
  Serial.printf("Strip 1: GPIO %d\n", LED_PIN_1);
  Serial.printf("Strip 2: GPIO %d\n", LED_PIN_2);
  Serial.printf("Chip Model: %s\n", ESP.getChipModel());
  Serial.printf("CPU Freq: %d MHz\n", ESP.getCpuFreqMHz());
  
  // Initialize pins as outputs first
  pinMode(LED_PIN_1, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);
  digitalWrite(LED_PIN_1, LOW);
  digitalWrite(LED_PIN_2, LOW);
  delay(100);
  
  // Initialize FastLED
  Serial.println("\nInitializing FastLED...");
  FastLED.addLeds<WS2812, LED_PIN_1, GRB>(leds1, NUM_LEDS);
  FastLED.addLeds<WS2812, LED_PIN_2, GRB>(leds2, NUM_LEDS);
  FastLED.setBrightness(TEST_BRIGHTNESS);
  
  // Clear all LEDs
  FastLED.clear();
  FastLED.show();
  delay(500);
  
  Serial.println("Initialization complete!\n");
  
  // Run initial test
  initialTest();
}

void initialTest() {
  Serial.println("=== Initial Test Pattern ===");
  
  // Test 1: Single LED per strip
  Serial.println("Test 1: Single LED (Red on strip 1, Blue on strip 2)");
  leds1[0] = CRGB::Red;
  leds2[0] = CRGB::Blue;
  FastLED.show();
  delay(2000);
  
  // Test 2: First 5 LEDs
  Serial.println("Test 2: First 5 LEDs (Green)");
  FastLED.clear();
  for(int i = 0; i < 5; i++) {
    leds1[i] = CRGB::Green;
    leds2[i] = CRGB::Green;
  }
  FastLED.show();
  delay(2000);
  
  // Test 3: Pattern test
  Serial.println("Test 3: Pattern (alternating colors)");
  FastLED.clear();
  for(int i = 0; i < 20; i++) {
    if(i % 2 == 0) {
      leds1[i] = CRGB::Yellow;
      leds2[i] = CRGB::Cyan;
    } else {
      leds1[i] = CRGB::Magenta;
      leds2[i] = CRGB::White;
    }
  }
  FastLED.show();
  delay(3000);
  
  FastLED.clear();
  FastLED.show();
  Serial.println("Initial test complete!\n");
}

void loop() {
  static int testMode = 0;
  
  switch(testMode) {
    case 0:
      // Solid color test
      Serial.println("Mode 0: Solid colors");
      fill_solid(leds1, NUM_LEDS, CRGB::Red);
      fill_solid(leds2, NUM_LEDS, CRGB::Blue);
      break;
      
    case 1:
      // Chase pattern
      Serial.println("Mode 1: Chase pattern");
      FastLED.clear();
      for(int i = 0; i < 10; i++) {
        int pos = (millis() / 50 + i) % NUM_LEDS;
        leds1[pos] = CHSV(i * 25, 255, 255);
        leds2[pos] = CHSV(i * 25 + 128, 255, 255);
      }
      break;
      
    case 2:
      // Gradient
      Serial.println("Mode 2: Gradient");
      fill_gradient_RGB(leds1, NUM_LEDS, CRGB::Red, CRGB::Blue);
      fill_gradient_RGB(leds2, NUM_LEDS, CRGB::Green, CRGB::Purple);
      break;
      
    case 3:
      // Sparkle
      Serial.println("Mode 3: Sparkle");
      fadeToBlackBy(leds1, NUM_LEDS, 10);
      fadeToBlackBy(leds2, NUM_LEDS, 10);
      leds1[random(NUM_LEDS)] = CRGB::White;
      leds2[random(NUM_LEDS)] = CRGB::White;
      break;
      
    case 4:
      // All off
      Serial.println("Mode 4: All off");
      FastLED.clear();
      break;
  }
  
  FastLED.show();
  
  // Change mode every 5 seconds
  EVERY_N_SECONDS(5) {
    testMode = (testMode + 1) % 5;
    Serial.printf("\nSwitching to test mode %d\n", testMode);
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  }
  
  // Small delay for smooth animation
  delay(20);
}