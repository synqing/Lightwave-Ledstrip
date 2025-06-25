/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "Arduino.h"
#include "M5UnitScroll.h"

M5UnitScroll scroll;

void setup()
{
    Serial.begin(115200);

    // basic, sda:gpio21, scl:gpio22
    // scroll.begin(&Wire, SCROLL_ADDR, 21, 22, 400000U);

    // core2, sda:gpio32, scl:gpio33
    scroll.begin(&Wire, SCROLL_ADDR, 32, 33, 400000U);

    // cores3, sda:gpio2, scl:gpio1
    // scroll.begin(&Wire, SCROLL_ADDR, 2, 1, 400000U);

    Serial.printf("firmware version:%d, i2c address:0x%X\n", scroll.getFirmwareVersion(), scroll.getI2CAddress());
}

void loop()
{
    int16_t encoder_value = scroll.getEncoderValue();
    bool btn_stauts       = scroll.getButtonStatus();
    Serial.println(encoder_value);
    if (!btn_stauts) {
        Serial.println("button press");
    }
    delay(20);
}