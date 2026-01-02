#include <Arduino.h>
#include <M5Unified.h>

// Configuration
#include "src/config/Config.h"

// I2C Scan utility (standalone)
#include "src/i2c/I2CScan.h"

// Encoder Service (includes transport and processing)
#include "src/service/EncoderService.h"

// Global instances
Rotate8Transport* transport = nullptr;
EncoderProcessing* processing = nullptr;
EncoderService* service = nullptr;

// Forward declarations
void onParamChange(uint8_t paramId, uint16_t value, bool isReset);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[BOOT] Tab5.8encoder starting...");

    // Initialise M5Unified
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.println("[BOOT] M5Unified initialised");

    // Initialise Ex_I2C for Grove Port.A (GPIO53/54)
    M5.Ex_I2C.begin();
    Serial.println("[BOOT] M5.Ex_I2C initialised (Port.A: GPIO53/54)");

    // Milestone A: I2C Scan
    Serial.println("\n[MILESTONE A] I2C Scan");
    uint8_t found = I2CScan::scanI2C(M5.Ex_I2C);
    if (found == 0) {
        Serial.println("[ERROR] No I2C devices found. Check wiring and power.");
    } else {
        Serial.printf("[MILESTONE A] Found %d device(s). Expected ROTATE8 at 0x%02X\n", found, I2C::ROTATE8_ADDRESS);
    }

    // Milestone B: ROTATE8 Transport
    Serial.println("\n[MILESTONE B] ROTATE8 Transport");
    transport = new Rotate8Transport(&M5.Ex_I2C, I2C::ROTATE8_ADDRESS, I2C::FREQ_HZ);
    if (!transport->begin()) {
        Serial.println("[ERROR] ROTATE8 transport initialisation failed");
        return;
    }
    
    // Proof-of-life: Light LED 0 green
    transport->setLED(0, 0, 255, 0);
    Serial.println("[MILESTONE B] ROTATE8 transport ready. LED 0 should be green.");

    // Milestone C: Encoder Processing
    Serial.println("\n[MILESTONE C] Encoder Processing");
    processing = new EncoderProcessing();
    processing->begin();
    processing->setCallback(onParamChange);
    Serial.println("[MILESTONE C] Encoder processing ready");

    // Milestone D: Encoder Service
    Serial.println("\n[MILESTONE D] Encoder Service");
    service = new EncoderService(*transport, *processing);
    if (!service->begin()) {
        Serial.println("[ERROR] Encoder service initialisation failed");
        return;
    }
    Serial.println("[MILESTONE D] Encoder service ready");

    Serial.println("\n[READY] All milestones complete. Turn encoders to see parameter changes.");
}

void loop() {
    M5.update();
    
    if (service) {
        service->update();
    }
    
    // Keep loop responsive
    delay(1);
}

void onParamChange(uint8_t paramId, uint16_t value, bool isReset) {
    const char* paramName = getParameterName(static_cast<Parameter>(paramId));
    if (isReset) {
        Serial.printf("[PARAM] %s reset to %d (button press)\n", paramName, value);
    } else {
        Serial.printf("[PARAM] %s changed to %d\n", paramName, value);
    }
}

