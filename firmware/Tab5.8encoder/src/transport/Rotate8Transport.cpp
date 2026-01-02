#include "Rotate8Transport.h"

int32_t Rotate8Transport::readI32LE(const uint8_t* b) {
    return (int32_t)(
        ((uint32_t)b[0]) |
        ((uint32_t)b[1] << 8) |
        ((uint32_t)b[2] << 16) |
        ((uint32_t)b[3] << 24)
    );
}

Rotate8Transport::Rotate8Transport(m5::I2C_Class* bus, uint8_t i2c_address, uint32_t freq_hz)
    : _bus(bus)
    , _i2c_address(i2c_address)
    , _freq_hz(freq_hz)
{}

bool Rotate8Transport::begin() {
    if (_bus == nullptr) {
        Serial.println("[ROTATE8 TRANSPORT] No I2C bus provided");
        return false;
    }

    // Quick probe (single address, safe)
    if (!_bus->scanID(_i2c_address, _freq_hz)) {
        Serial.println("[ROTATE8 TRANSPORT] Device not responding at address");
        return false;
    }
    
    // Optional: read version register (best-effort)
    const uint8_t version = _bus->readRegister8(_i2c_address, REG_VERSION, _freq_hz);
    Serial.printf("[ROTATE8 TRANSPORT] Connected (addr 0x%02X, ver %u)\n", _i2c_address, (unsigned)version);
    
    // Clear all LEDs (9 total, including LED 8)
    for (uint8_t i = 0; i < 9; i++) {
        setLED(i, 0, 0, 0);
    }
    
    return true;
}

int32_t Rotate8Transport::readDelta(uint8_t channel) {
    if (channel > 7) return 0;
    
    uint8_t buf[4] = {0,0,0,0};
    const uint8_t reg = (uint8_t)(REG_BASE_REL + (channel << 2));
    const bool ok = _bus->readRegister(_i2c_address, reg, buf, sizeof(buf), _freq_hz);
    if (!ok) return 0;

    const int32_t raw_value = readI32LE(buf);
    if (raw_value != 0) {
        resetCounter(channel);
    }
    return raw_value;
}

bool Rotate8Transport::readButton(uint8_t channel) {
    if (channel > 7) return false;
    const uint8_t reg = (uint8_t)(REG_BASE_BUTTON_VALUE + channel);
    // Upstream lib treats "0 == pressed"
    return _bus->readRegister8(_i2c_address, reg, _freq_hz) == 0;
}

bool Rotate8Transport::setLED(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index > 8) return false;
    
    const uint8_t reg = (uint8_t)(REG_RGB + (index * 3));
    const uint8_t rgb[3] = { r, g, b };
    return _bus->writeRegister(_i2c_address, reg, rgb, sizeof(rgb), _freq_hz);
}

bool Rotate8Transport::resetCounter(uint8_t channel) {
    if (channel > 7) return false;
    const uint8_t reg = (uint8_t)(REG_BASE_RESET + channel);
    return _bus->writeRegister8(_i2c_address, reg, 1, _freq_hz);
}

bool Rotate8Transport::isConnected() {
    if (_bus == nullptr) return false;
    return _bus->scanID(_i2c_address, _freq_hz);
}

