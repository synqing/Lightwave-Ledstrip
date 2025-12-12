// Test program for M5Unit-Scroll encoder I2C diagnostics
// This helps identify I2C communication issues

#include <Wire.h>

// Configuration
#define I2C_SDA_PIN 11
#define I2C_SCL_PIN 12
#define SCROLL_ADDR 0x40

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n=== M5Unit-Scroll I2C Diagnostic Test ===");
    Serial.printf("SDA: GPIO%d, SCL: GPIO%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
    Serial.printf("Expected address: 0x%02X\n\n", SCROLL_ADDR);
    
    // Initialize I2C
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);  // 400kHz
    
    // Initial scan
    scanI2CBus();
    
    // Test communication with scroll encoder
    testScrollEncoder();
}

void loop() {
    static uint32_t lastScan = 0;
    static uint32_t lastTest = 0;
    uint32_t now = millis();
    
    // Periodic I2C scan every 5 seconds
    if (now - lastScan > 5000) {
        lastScan = now;
        Serial.println("\n--- Periodic I2C Scan ---");
        scanI2CBus();
    }
    
    // Test scroll encoder every second
    if (now - lastTest > 1000) {
        lastTest = now;
        testScrollEncoder();
    }
}

void scanI2CBus() {
    Serial.println("Scanning I2C bus...");
    int nDevices = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.print("  Device found at 0x");
            if (address < 16) Serial.print("0");
            Serial.print(address, HEX);
            
            if (address == SCROLL_ADDR) {
                Serial.print(" <-- M5Unit-Scroll");
            }
            Serial.println();
            nDevices++;
        } else if (error == 4) {
            Serial.print("  Unknown error at address 0x");
            if (address < 16) Serial.print("0");
            Serial.println(address, HEX);
        }
    }
    
    if (nDevices == 0) {
        Serial.println("  No I2C devices found!");
    } else {
        Serial.printf("  Found %d device(s)\n", nDevices);
    }
}

void testScrollEncoder() {
    Serial.print("\nTesting scroll encoder at 0x40: ");
    
    // Test 1: Basic communication
    Wire.beginTransmission(SCROLL_ADDR);
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.printf("FAILED (error: %d)\n", error);
        Serial.println("  Error codes: 1=too long, 2=NACK addr, 3=NACK data, 4=other");
        return;
    }
    
    Serial.print("Communication OK, ");
    
    // Test 2: Read firmware version (register 0xFE)
    Wire.beginTransmission(SCROLL_ADDR);
    Wire.write(0xFE);  // Firmware version register
    error = Wire.endTransmission(false);  // Repeated start
    
    if (error != 0) {
        Serial.printf("Write reg failed (error: %d)\n", error);
        return;
    }
    
    Wire.requestFrom(SCROLL_ADDR, (uint8_t)1);
    if (Wire.available()) {
        uint8_t version = Wire.read();
        Serial.printf("Firmware: V%d\n", version);
    } else {
        Serial.println("No data received");
    }
    
    // Test 3: Try to read encoder value (register 0x10)
    static int16_t lastValue = 0;
    Wire.beginTransmission(SCROLL_ADDR);
    Wire.write(0x10);  // Encoder value register
    error = Wire.endTransmission(false);
    
    if (error == 0) {
        Wire.requestFrom(SCROLL_ADDR, (uint8_t)2);
        if (Wire.available() >= 2) {
            int16_t value = Wire.read() | (Wire.read() << 8);
            if (value != lastValue) {
                Serial.printf("  Encoder value changed: %d -> %d\n", lastValue, value);
                lastValue = value;
            }
        }
    }
    
    // Test 4: Check button status (register 0x20)
    Wire.beginTransmission(SCROLL_ADDR);
    Wire.write(0x20);  // Button register
    error = Wire.endTransmission(false);
    
    if (error == 0) {
        Wire.requestFrom(SCROLL_ADDR, (uint8_t)1);
        if (Wire.available()) {
            uint8_t button = Wire.read();
            if (button == 0x00) {
                Serial.println("  BUTTON PRESSED!");
            }
        }
    }
}