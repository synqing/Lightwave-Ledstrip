// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// I2CRecovery - I2C Error Counter (stripped — see git history for full SM)
// ============================================================================

#include "I2CRecovery.h"

bool I2CRecovery::s_initialized = false;
uint8_t I2CRecovery::s_errorCount = 0;
uint16_t I2CRecovery::s_recoverySuccesses = 0;

void I2CRecovery::init(TwoWire* wire, int sda, int scl, uint32_t freq, uint16_t timeout) {
    s_initialized = true;
    s_errorCount = 0;
    s_recoverySuccesses = 0;
    Serial.printf("[I2C_RECOVERY] Initialized: SDA=%d SCL=%d freq=%luHz\n",
                  sda, scl, (unsigned long)freq);
}

void I2CRecovery::recordError() {
    if (!s_initialized) return;
    s_errorCount++;
}

void I2CRecovery::recordSuccess() {
    if (!s_initialized) return;
    if (s_errorCount > 0) s_errorCount = 0;
}
