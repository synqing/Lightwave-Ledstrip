#include "../config/features.h"

#if FEATURE_AUDIO_SYNC
#include "MicTest.h"
#include "i2s_mic.h"

// Static member definitions
bool MicTest::monitoringActive = false;
unsigned long MicTest::lastStatusPrint = 0;
float MicTest::lastAudioLevel = 0.0f;

bool MicTest::testMicConnection() {
    Serial.println("\n🎤 === COMPREHENSIVE I2S MICROPHONE DEBUGGING ===");
    Serial.println("🔧 Using EXACT AP_SOT SPH0645 configuration:");
    Serial.println("   - BCLK: GPIO 16 (was GPIO 3)");
    Serial.println("   - LRCLK: GPIO 4 (was GPIO 2)");
    Serial.println("   - DIN: GPIO 10 (was GPIO 4)");
    Serial.println("   - Sample Rate: 16kHz");
    Serial.println("   - Format: 32-bit I2S, LEFT channel, 18-bit data");
    
    // Try to initialize the mic
    if (!i2sMic.begin()) {
        Serial.println("❌ I2S Microphone initialization FAILED!");
        Serial.println("   This means i2s_driver_install() or i2s_set_pin() failed");
        Serial.println("   Check ESP32-S3 GPIO availability and hardware connections");
        return false;
    }
    
    Serial.println("✅ I2S Driver installed and pins configured successfully!");
    
    // Start capture and test for a longer period with detailed monitoring
    i2sMic.startCapture();
    Serial.println("\n🎤 STARTING 10-SECOND DETAILED AUDIO TEST");
    Serial.println("   - Will show RAW I2S data, extracted values, and energy levels");
    Serial.println("   - Make noise near the microphone!");
    Serial.println("   - Look for changing hex values and non-zero energy levels\n");
    
    unsigned long startTime = millis();
    float maxLevel = 0.0f;
    int sampleCount = 0;
    int nonZeroSamples = 0;
    int totalUpdates = 0;
    
    while (millis() - startTime < 10000) {  // Extended to 10 seconds
        i2sMic.update();
        totalUpdates++;
        
        float currentLevel = i2sMic.getOverallEnergy();
        if (currentLevel > maxLevel) {
            maxLevel = currentLevel;
        }
        
        if (currentLevel > 0.001f) {  // Lower threshold to catch any activity
            sampleCount++;
        }
        
        if (currentLevel > 0.0f) {
            nonZeroSamples++;
        }
        
        // Print detailed status every second
        if ((millis() - startTime) % 1000 < 50) {
            Serial.printf("⏱️  %ds: Energy=%.6f, Bass=%.3f, Mid=%.3f, High=%.3f, Updates=%d\n",
                         (int)((millis() - startTime) / 1000),
                         currentLevel, 
                         i2sMic.getBassEnergy(),
                         i2sMic.getMidEnergy(), 
                         i2sMic.getHighEnergy(),
                         totalUpdates);
        }
        
        delay(20);  // 50Hz update rate
    }
    
    i2sMic.stopCapture();
    
    Serial.println("\n📊 === DETAILED TEST RESULTS ===");
    Serial.printf("   Total Updates: %d\n", totalUpdates);
    Serial.printf("   Max Audio Level: %.6f\n", maxLevel);
    Serial.printf("   Non-zero Samples: %d/%d (%.1f%%)\n", nonZeroSamples, totalUpdates, 
                  (float)nonZeroSamples * 100.0f / totalUpdates);
    Serial.printf("   Active Samples (>0.001): %d/%d (%.1f%%)\n", sampleCount, totalUpdates,
                  (float)sampleCount * 100.0f / totalUpdates);
    
    if (maxLevel > 0.1f && sampleCount > 10) {
        Serial.println("✅ MICROPHONE IS WORKING PERFECTLY! Strong audio detected.");
        return true;
    } else if (maxLevel > 0.001f && nonZeroSamples > 5) {
        Serial.println("⚠️  MICROPHONE RESPONDING - Weak signal detected");
        Serial.println("   📢 This could be normal for a quiet environment");
        Serial.println("   📢 Try making loud noise near the microphone");
        return true;
    } else if (nonZeroSamples > 0) {
        Serial.println("⚠️  MICROPHONE PARTIALLY WORKING - Some non-zero data");
        Serial.println("   📢 I2S is receiving data but very low levels");
        Serial.println("   📢 Check microphone power and orientation");
        return false;
    } else {
        Serial.println("❌ NO AUDIO ACTIVITY DETECTED");
        Serial.println("   🔍 I2S driver is working but receiving no varying data");
        Serial.println("   🔍 Check: Hardware connections, microphone power, pin wiring");
        Serial.println("   🔍 Expected: Changing hex values in debug output above");
        return false;
    }
}

void MicTest::printMicStatus() {
    if (!i2sMic.isActive()) {
        Serial.println("🎤 Microphone: INACTIVE");
        return;
    }
    
    float bass = i2sMic.getBassEnergy();
    float mid = i2sMic.getMidEnergy();
    float high = i2sMic.getHighEnergy();
    float overall = i2sMic.getOverallEnergy();
    bool beat = i2sMic.isBeatDetected();
    
    Serial.printf("🎤 Mic: Bass=%.2f Mid=%.2f High=%.2f Overall=%.2f %s\n", 
                  bass, mid, high, overall, beat ? "🥁BEAT" : "");
    
    // Visual level indicator
    if (overall > 0.5f) {
        Serial.print("   Level: ");
        int bars = (int)(overall * 20);
        for (int i = 0; i < bars && i < 20; i++) {
            Serial.print("█");
        }
        Serial.println();
    }
}

void MicTest::startMicMonitoring() {
    if (monitoringActive) return;
    
    Serial.println("🎤 Starting continuous microphone monitoring...");
    
    if (i2sMic.begin()) {
        i2sMic.startCapture();
        monitoringActive = true;
        lastStatusPrint = millis();
        Serial.println("✅ Microphone monitoring active - audio data will update automatically");
    } else {
        Serial.println("❌ Failed to start microphone monitoring");
    }
}

void MicTest::stopMicMonitoring() {
    if (!monitoringActive) return;
    
    i2sMic.stopCapture();
    monitoringActive = false;
    Serial.println("🎤 Microphone monitoring stopped");
}

bool MicTest::isMicProducingAudio() {
    if (!i2sMic.isActive()) return false;
    
    float currentLevel = i2sMic.getOverallEnergy();
    lastAudioLevel = currentLevel;
    
    // Print status every 5 seconds
    if (millis() - lastStatusPrint > 5000) {
        printMicStatus();
        lastStatusPrint = millis();
    }
    
    return currentLevel > 0.01f; // Return true if any audio detected
}

#endif // FEATURE_AUDIO_SYNC
