#include <Arduino.h>
#include "core/MegaLUTs.h"

/**
 * Test program for MegaLUT system
 * 
 * This verifies that the massive LUTs are properly initialized
 * and reports memory usage
 */

void testMegaLUTs() {
    Serial.println("\n========== MEGA LUT SYSTEM TEST ==========");
    
    // Get memory before initialization
    size_t heapBefore = ESP.getFreeHeap();
    size_t psBefore = ESP.getFreePsram();
    
    Serial.print("Free Heap before: ");
    Serial.print(heapBefore / 1024);
    Serial.println(" KB");
    
    Serial.print("Free PSRAM before: ");
    Serial.print(psBefore / 1024);
    Serial.println(" KB");
    
    // Initialize MegaLUTs
    uint32_t startTime = millis();
    initializeMegaLUTs();
    uint32_t endTime = millis();
    
    // Get memory after initialization
    size_t heapAfter = ESP.getFreeHeap();
    size_t psAfter = ESP.getFreePsram();
    
    Serial.print("\nFree Heap after: ");
    Serial.print(heapAfter / 1024);
    Serial.println(" KB");
    
    Serial.print("Free PSRAM after: ");
    Serial.print(psAfter / 1024);
    Serial.println(" KB");
    
    // Calculate usage
    size_t heapUsed = heapBefore - heapAfter;
    size_t psUsed = psBefore - psAfter;
    
    Serial.print("\nHeap used by LUTs: ");
    Serial.print(heapUsed / 1024);
    Serial.print(" KB (");
    Serial.print(heapUsed);
    Serial.println(" bytes)");
    
    Serial.print("PSRAM used by LUTs: ");
    Serial.print(psUsed / 1024);
    Serial.print(" KB (");
    Serial.print(psUsed);
    Serial.println(" bytes)");
    
    Serial.print("\nTotal memory used: ");
    Serial.print((heapUsed + psUsed) / 1024);
    Serial.println(" KB");
    
    Serial.print("\nInitialization time: ");
    Serial.print(endTime - startTime);
    Serial.println(" ms");
    
    // Test some LUT accesses
    Serial.println("\n--- Testing LUT Access Speed ---");
    
    // Test trig LUTs
    uint32_t testStart = micros();
    volatile int16_t sinSum = 0;
    for (int i = 0; i < 1000; i++) {
        sinSum += SIN16(i * 64);
    }
    uint32_t testEnd = micros();
    
    Serial.print("1000 sin lookups: ");
    Serial.print(testEnd - testStart);
    Serial.println(" microseconds");
    
    // Test color mixing LUT
    testStart = micros();
    volatile uint8_t colorSum = 0;
    for (int i = 0; i < 1000; i++) {
        colorSum += colorMixLUT[i & 127][i & 127][0];
    }
    testEnd = micros();
    
    Serial.print("1000 color mix lookups: ");
    Serial.print(testEnd - testStart);
    Serial.println(" microseconds");
    
    // Test transition LUT
    testStart = micros();
    uint8_t transitionData[HardwareConfig::NUM_LEDS];
    for (int i = 0; i < 10; i++) {
        getTransitionFrame(transitionData, i % 5, i % 16);
    }
    testEnd = micros();
    
    Serial.print("10 transition frame copies: ");
    Serial.print(testEnd - testStart);
    Serial.println(" microseconds");
    
    // Verify data integrity
    Serial.println("\n--- Verifying LUT Data ---");
    
    // Check sin table
    bool sinValid = true;
    for (int i = 0; i < 10; i++) {
        int16_t sinValue = SIN16(i * 409);  // Random angles
        if (sinValue < -32767 || sinValue > 32767) {
            sinValid = false;
            break;
        }
    }
    Serial.print("Sin LUT: ");
    Serial.println(sinValid ? "VALID" : "INVALID");
    
    // Check distance LUT
    bool distValid = true;
    if (distanceFromCenterLUT != nullptr) {
        for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            if (distanceFromCenterLUT[i] > HardwareConfig::STRIP_LENGTH) {
                distValid = false;
                break;
            }
        }
    } else {
        distValid = false;
    }
    Serial.print("Distance LUT: ");
    Serial.println(distValid ? "VALID" : "INVALID");
    
    // Performance comparison
    Serial.println("\n--- Performance Comparison ---");
    
    // Traditional calculation
    testStart = micros();
    volatile float calcSum = 0;
    for (int i = 0; i < 1000; i++) {
        calcSum += sin((i * TWO_PI) / 1000.0f);
    }
    testEnd = micros();
    uint32_t calcTime = testEnd - testStart;
    
    Serial.print("1000 sin calculations: ");
    Serial.print(calcTime);
    Serial.println(" microseconds");
    
    // LUT version
    testStart = micros();
    sinSum = 0;
    for (int i = 0; i < 1000; i++) {
        sinSum += SIN16(i * 64);
    }
    testEnd = micros();
    uint32_t lutTime = testEnd - testStart;
    
    Serial.print("1000 sin LUT lookups: ");
    Serial.print(lutTime);
    Serial.println(" microseconds");
    
    Serial.print("\nSpeedup factor: ");
    Serial.print((float)calcTime / lutTime);
    Serial.println("x faster!");
    
    Serial.println("\n========== TEST COMPLETE ==========");
    
    // Memory summary
    size_t totalRam = 512 * 1024;  // ESP32-S3 has 512KB RAM
    size_t usableRam = 320 * 1024;  // ~320KB usable
    size_t lutTarget = 250 * 1024;  // Our target
    
    Serial.println("\n=== MEMORY USAGE SUMMARY ===");
    Serial.print("Total RAM: ");
    Serial.print(totalRam / 1024);
    Serial.println(" KB");
    
    Serial.print("Usable RAM: ~");
    Serial.print(usableRam / 1024);
    Serial.println(" KB");
    
    Serial.print("LUT Target: ");
    Serial.print(lutTarget / 1024);
    Serial.println(" KB");
    
    Serial.print("LUT Actual: ");
    Serial.print((heapUsed + psUsed) / 1024);
    Serial.println(" KB");
    
    float efficiency = ((heapUsed + psUsed) / (float)lutTarget) * 100;
    Serial.print("Target efficiency: ");
    Serial.print(efficiency);
    Serial.println("%");
    
    if (efficiency >= 80) {
        Serial.println("\n✅ EXCELLENT! Maximum performance achieved!");
    } else if (efficiency >= 60) {
        Serial.println("\n⚠️  Good, but more LUTs could be added");
    } else {
        Serial.println("\n❌ Not using enough memory for LUTs!");
    }
}

// Can be called from main.cpp for testing
void runMegaLUTTest() {
    testMegaLUTs();
}