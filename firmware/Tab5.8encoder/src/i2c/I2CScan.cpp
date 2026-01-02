#include "I2CScan.h"
#include <Arduino.h>

namespace I2CScan {
    uint8_t scanI2C(TwoWire& bus) {
        Serial.println("[I2C SCAN] Starting scan...");
        
        uint8_t found_count = 0;
        
        // IMPORTANT: Avoid scanning 0x00..0x07. M5Unified notes that scanning these
        // reserved addresses can cause certain ESP32 variants to stall.
        for (uint8_t address = 0x08; address < 0x78; address++) {
            bus.beginTransmission(address);
            uint8_t error = bus.endTransmission();
            
            if (error == 0) {
                Serial.printf("[I2C SCAN] Device found at address 0x%02X\n", address);
                found_count++;
            } else if (error == 4) {
                Serial.printf("[I2C SCAN] Unknown error at address 0x%02X\n", address);
            }
        }
        
        Serial.printf("[I2C SCAN] Scan complete. Found %d device(s).\n", found_count);
        return found_count;
    }
    
    uint8_t scanI2C(m5::I2C_Class& bus) {
        Serial.println("[I2C SCAN] Starting scan (m5::I2C_Class)...");
        
        uint8_t found_count = 0;
        const uint32_t freq = 100000;  // 100kHz
        
        // IMPORTANT: Avoid scanning 0x00..0x07. M5Unified notes that scanning these
        // reserved addresses can cause certain ESP32 variants to stall.
        for (uint8_t address = 0x08; address < 0x78; address++) {
            if (bus.start(address, false, freq) && bus.stop()) {
                Serial.printf("[I2C SCAN] Device found at address 0x%02X\n", address);
                found_count++;
            }
        }
        
        Serial.printf("[I2C SCAN] Scan complete. Found %d device(s).\n", found_count);
        return found_count;
    }
}

