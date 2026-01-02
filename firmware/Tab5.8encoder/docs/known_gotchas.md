# Tab5 Known Gotchas

This document captures Tab5-specific gotchas and workarounds discovered during development.

## I2C Bus Management

### Must Use M5.Ex_I2C

- Tab5 external I2C (Grove Port.A) is exposed as `M5.Ex_I2C`, not global `Wire`
- GPIO53/54 are mapped in M5Unified for Tab5
- Always use `M5.Ex_I2C.begin()` for Port.A initialisation

### Forbidden I2C Reset Patterns

The following patterns from AtomS3 encoder code **must not be used** on Tab5:

- `i2cDeinit(0)` - Deletes I2C0 driver (breaks Tab5)
- `periph_module_reset(PERIPH_I2C0_MODULE)` - Hardware peripheral reset (breaks Tab5)
- `Wire.end()` followed by hardware resets - Tab5 cannot tolerate this

**Why**: Tab5 uses a different I2C architecture (ESP32-P4) and these low-level resets can cause permanent bus lockup or require full MCU reset.

**Solution**: Remove all I2C recovery code that uses these patterns. If I2C communication fails, rely on simple retry logic or full MCU reset via watchdog.

## Display Controller Variants

Tab5 hardware revisions may have different display controller variants. If display bring-up is problematic:

- Keep UI minimal/serial-first for encoder project
- Display is not required for encoder functionality
- Focus on serial output for debugging

## PlatformIO ESP32-P4 Support

PlatformIO support for ESP32-P4 may be incomplete or require specific board definitions:

- **Fallback**: Use Arduino IDE if PlatformIO fails
- **Board ID**: May need to search for correct Tab5 board ID
- **Platform version**: May require specific ESP-IDF or Arduino core versions

## I2C Probe Timeouts

If I2C probe timeouts occur on GPIO53/54:

1. **Pull-ups**: Grove Port.A should have internal pull-ups, but verify
2. **Cable**: Use quality Grove cable, check for damage
3. **Power**: Ensure Tab5 has adequate power supply
4. **Device**: Verify ROTATE8 is functional
5. **Port**: Ensure using Port.A, not other ports
6. **Speed**: Try lower I2C speed (100kHz instead of 400kHz)
7. **Timeouts**: Increase I2C timeout values in configuration

## Reference Repositories

Reference repos are cloned to `_ref/` for pattern examination:

- `M5Unified/` - Tab5 mapping and Ex_I2C behaviour
- `M5Tab5-UserDemo/` - Factory demo showing Tab5 HAL/components
- `esp-bsp/` - Tab5 BSP reference (pin/bus truth)
- `xiaozhi-esp32/` - Real-world Tab5 support patterns

## Community Resources

- [M5Stack Tab5 Documentation](https://docs.m5stack.com/en/core/Tab5)
- [M5Stack Community Forum - Tab5 I2C Discussion](https://community.m5stack.com/topic/7775/i2c-on-tab5)
- [ESP-BSP Tab5 Issues](https://github.com/espressif/esp-bsp/issues/695) - Display init issues (irrelevant to encoder)

