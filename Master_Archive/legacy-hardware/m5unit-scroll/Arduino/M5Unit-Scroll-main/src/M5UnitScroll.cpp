/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "M5UnitScroll.h"

/*! @brief Initialize the Scroll. */
bool M5UnitScroll::begin(TwoWire *wire, uint8_t addr, uint8_t sda, uint8_t scl, uint32_t speed) {
    _wire  = wire;
    _addr  = addr;
    _sda   = sda;
    _scl   = scl;
    _speed = speed;
    _wire->begin(_sda, _scl);
    _wire->setClock(_speed);
    delay(10);
    _wire->beginTransmission(_addr);
    uint8_t error = _wire->endTransmission();
    if (error == 0) {
        return true;
    } else {
        return false;
    }
}

/*! @brief Write a certain length of data to the specified register address. */
void M5UnitScroll::writeBytes(uint8_t addr, uint8_t reg, uint8_t *buffer, uint8_t length) {
    _wire->beginTransmission(addr);
    _wire->write(reg);
    for (int i = 0; i < length; i++) {
        _wire->write(*(buffer + i));
    }
    _wire->endTransmission();
}

/*! @brief Read a certain length of data to the specified register address. */
void M5UnitScroll::readBytes(uint8_t addr, uint8_t reg, uint8_t *buffer, uint8_t length) {
    uint8_t index = 0;
    _wire->beginTransmission(addr);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(addr, length);
    for (int i = 0; i < length; i++) {
        buffer[index++] = _wire->read();
    }
}

/*! @brief Read the encoder value.
    @return The value of the encoder that was read */
int16_t M5UnitScroll::getEncoderValue(void) {
    int16_t value = 0;

    readBytes(_addr, ENCODER_REG, (uint8_t *)&value, 2);
    return value;
}

/*! @brief Read the encoder inc value.
    @return The value of the encoder that was read */
int16_t M5UnitScroll::getIncEncoderValue(void) {
    int16_t value = 0;

    readBytes(_addr, INC_ENCODER_REG, (uint8_t *)&value, 2);
    return value;
}

/*! @brief Get the current status of the rotary encoder button.
    @return true if the button was pressed, otherwise false. */
bool M5UnitScroll::getButtonStatus(void) {
    uint8_t data;
    readBytes(_addr, BUTTON_REG, &data, 1);
    return data == 0x00;
}

/*! @brief Set the color of the LED (HEX). */
void M5UnitScroll::setLEDColor(uint32_t color, uint8_t index) {
    uint8_t data[4];
    data[3] = color & 0xff;
    data[2] = (color >> 8) & 0xff;
    data[1] = (color >> 16) & 0xff;
    data[0] = index;
    writeBytes(_addr, RGB_LED_REG, data, 4);
}

/*! @brief Get the color of the LED (HEX).
    @return The value of the led that was read */
uint32_t M5UnitScroll::getLEDColor(void) {
    uint8_t data[4];
    uint32_t value = 0;

    readBytes(_addr, RGB_LED_REG, data, 4);
    value = (data[3] | (data[2] << 8) | (data[1] << 16));
    return value;
}

void M5UnitScroll::setEncoderValue(int16_t encoder) {
    writeBytes(_addr, ENCODER_REG, (uint8_t *)&encoder, 4);
}

void M5UnitScroll::resetEncoder(void) {
    uint8_t data = 1;
    writeBytes(_addr, 0x40, &data, 1);
}

/*! @brief Get the dev status.
    @return 1 if the dev working, otherwise 0.. */
bool M5UnitScroll::getDevStatus(void) {
    _wire->beginTransmission(_addr);
    if (_wire->endTransmission() == 0)
        return true;
    else
        return false;
}

uint8_t M5UnitScroll::getBootloaderVersion(void) {
    _wire->beginTransmission(_addr);
    _wire->write(BOOTLOADER_VERSION_REG);
    _wire->endTransmission(false);

    uint8_t RegValue;

    _wire->requestFrom(_addr, 1);
    RegValue = _wire->read();
    return RegValue;
}

uint8_t M5UnitScroll::getFirmwareVersion(void) {
    _wire->beginTransmission(_addr);
    _wire->write(FIRMWARE_VERSION_REG);
    _wire->endTransmission(false);

    uint8_t RegValue;

    _wire->requestFrom(_addr, 1);
    RegValue = _wire->read();
    return RegValue;
}

uint8_t M5UnitScroll::setI2CAddress(uint8_t addr) {
    uint8_t temp[2] = {0};

    temp[0] = I2C_ADDRESS_REG;

    _wire->beginTransmission(_addr);
    _wire->write(temp[0]);
    _wire->write(addr);
    _wire->endTransmission();
    _addr = addr;
    return _addr;
}

uint8_t M5UnitScroll::getI2CAddress(void) {
    uint8_t temp[2] = {0};

    temp[0] = I2C_ADDRESS_REG;

    _wire->beginTransmission(_addr);
    _wire->write(temp[0]);
    _wire->endTransmission(false);

    uint8_t RegValue;

    _wire->requestFrom(_addr, 1);
    RegValue = _wire->read();
    return RegValue;
}

void M5UnitScroll::jumpBootloader(void) {
    uint8_t value = 1;

    writeBytes(_addr, JUMP_TO_BOOTLOADER_REG, (uint8_t *)&value, 1);
}
