/**
 * @file arduino_mock.cpp
 * @brief Arduino mock implementation for native unit tests
 */

#ifdef NATIVE_BUILD

#include "Arduino.h"

MockSerial Serial;

#endif // NATIVE_BUILD
