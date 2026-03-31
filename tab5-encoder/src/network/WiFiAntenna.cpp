// ============================================================================
// WiFiAntenna - Tab5 runtime antenna selection (MMCX vs internal 3D)
// ============================================================================
// PI4IOE5V6408 IO expander 0, pin P0: HIGH = external MMCX, LOW = internal 3D.
// Pin MUST be configured as push-pull OUTPUT before use — the IO expander
// powers up with all pins as INPUT/high-impedance. Without explicit config,
// digitalWrite() writes to the output latch but has no physical effect.
// ============================================================================

#include "network/WiFiAntenna.h"

#if ENABLE_WIFI

#include <M5Unified.h>
#include <WiFi.h>

static bool s_useExternal = true;
static bool s_pinConfigured = false;

void initWiFiAntennaPin() {
    auto& ioe = M5.getIOExpander(0);
    // Configure pin P0 as push-pull output (power-on default is INPUT/hi-Z)
    ioe.setDirection(0, true);       // P0 = OUTPUT
    ioe.setHighImpedance(0, false);  // P0 = push-pull (not floating)
    ioe.digitalWrite(0, 1);          // Default: external MMCX

    // Verify the write took effect — silent I2C failure leaves P0 = LOW (internal)
    bool actual = ioe.getWriteValue(0);
    if (!actual) {
        Serial.println("[Antenna] ERROR: P0 readback LOW after setting HIGH — retrying");
        ioe.digitalWrite(0, 1);
        delay(5);
        actual = ioe.getWriteValue(0);
    }

    s_useExternal = true;
    s_pinConfigured = true;
    Serial.printf("[Antenna] Pin P0 configured as OUTPUT, set to external MMCX (readback: %s)\n",
                  actual ? "HIGH" : "LOW — FAILED");
}

void setWiFiAntenna(bool useExternal) {
    if (!s_pinConfigured) {
        initWiFiAntennaPin();
    }
    auto& ioe = M5.getIOExpander(0);
    ioe.digitalWrite(0, useExternal ? 1 : 0);
    s_useExternal = useExternal;
    Serial.printf("[Antenna] Set to %s\n", useExternal ? "external MMCX" : "internal 3D");
}

bool isWiFiAntennaExternal() {
    return s_useExternal;
}

#endif
