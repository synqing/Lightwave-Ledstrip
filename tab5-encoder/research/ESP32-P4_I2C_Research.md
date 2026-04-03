---
abstract: Comprehensive research on ESP32-P4 I2C hardware behavior, known issues, and recovery procedures. Covers hardware differences vs ESP32-S3, silicon bugs, clock stretching limitations, GPIO reset behavior, pull-up configuration, and SDA stuck low recovery techniques for Tab5 M5Stack device.
---

# ESP32-P4 I2C Hardware Research

## Executive Summary

The ESP32-P4 I2C implementation is largely compatible with ESP32-S3 at the driver level, but has specific hardware quirks and known silicon bugs. The most critical issue for Tab5 development is potential SDA stuck low after board resets due to GPIO state handling during USB CDC reset cycles.

## 1. Hardware Architecture Differences: ESP32-P4 vs ESP32-S3

### I2C Controllers

| Aspect | ESP32-P4 | ESP32-S3 |
|--------|----------|----------|
| High-Performance I2C | 2 controllers | 2 controllers |
| Low-Power I2C (LP I2C) | 1 controller (cut-down) | No LP variant |
| Max Speed | 400 kHz (Fast-mode) | 400 kHz (Fast-mode) |
| Addressing | 7-bit and 10-bit | 7-bit and 10-bit |
| Slave Support | Yes (HP I2C only) | Yes |

**LP I2C Constraints:**
- No slave mode support
- Smaller RAM footprint
- LP I2C pins hard-wired: SDA=GPIO#6, SCL=GPIO#7 (when used with LP Core)
- Cannot be freely routed via GPIO matrix

### Processor & Memory Context

| Parameter | ESP32-P4 | ESP32-S3 |
|-----------|----------|----------|
| CPU | 400 MHz dual-core RISC-V | 240 MHz dual-core Xtensa LX7 |
| SRAM | 768 KB + 8 KB zero-wait TCM | 512 KB |
| I/O | 55 GPIOs | 45 GPIOs |
| Wireless | None (requires external chip) | Wi-Fi 802.11 b/g/n + BLE 5.0 |
| Special Interfaces | MIPI-DSI, MIPI-CSI, PIE | None |

**Implication for I2C:** Faster processor may exacerbate timing issues if not carefully configured; no built-in wireless does not impact I2C.

### GPIO Matrix and Pin Routing

- **Both chips:** Full GPIO matrix flexibility for I2C (SDA/SCL can be routed to nearly any GPIO via IO MUX)
- **ESP32-P4 USB JTAG:** GPIO24 and GPIO25 reserved for USB-JTAG debugging (if enabled)
- **ESP32-P4 external JTAG alternative:** GPIO2-GPIO5 (requires eFuse configuration)
- **Conflict prevention:** Avoid assigning I2C pins to GPIO24/GPIO25 if USB-JTAG is active

**For Tab5:** Confirm I2C pin assignments do not overlap with USB-JTAG or other peripherals.

---

## 2. Known I2C Bugs and Silicon Errata

### Issue IDFGH-14623: I2C Communication Failures on ESP32-P4 v1.0 Silicon (RESOLVED)

**Reported Symptoms:**
- I2C transaction failures with error code `0x103` (ESP_ERR_INVALID_STATE)
- "Clear bus failed" messages in firmware logs
- Worked fine on ESP32-P4 v0.1 silicon with identical software

**Root Cause:**
Hardware power supply issue on the prototype board, NOT a firmware or silicon defect. The 3.3V power rail supplying the I2C peripheral circuitry was not properly enabled.

**Resolution:**
Verify and correct 3.3V power delivery on the hardware. Once fixed, I2C functions normally on v1.0 silicon with ESP-IDF 5.5+.

**Lesson for Tab5:** If SDA gets stuck low after reset, verify power delivery first before investigating firmware.

### [I2C-308] I2C Slave Fails in Multiple-read Under Non-FIFO Mode

**Affected Revision:** ESP32-P4 v1.0

**Condition:**
I2C slave cannot correctly respond when the master performs multiple-read operations using non-FIFO mode.

**Workaround:**
1. Set `I2C_FIFO_ADDR_CFG_EN = 1` (enable FIFO address config)
2. Set `I2C_SLV_TX_AUTO_START_EN = 1` (auto-start slave TX)
3. Set `I2C_FIFO_PRT_EN = 0` (disable FIFO port)
4. Master must access slave with sequence: RSTART → WRITE (slave addr, fifo addr) → RSTART → WRITE (slave addr) → READ (NACK) → STOP
5. **Limitation:** Only one byte can be read per transaction

**Relevance to Tab5:** If Tab5 operates as I2C slave (unlikely), apply this workaround. More likely Tab5 is master only.

---

## 3. GPIO Behavior During Soft Reset

### USB CDC Reset Mechanism (DTR/RTS Control)

The ESP32-P4 auto-reset circuit uses USB serial converter lines:
- **DTR** → GPIO35 (low on reset)
- **RTS** → EN/CHIP_PU (low on reset)

When opening a serial port (e.g., USB CDC reset after flashing), these signals trigger a chip reset.

### I2C GPIO State During Reset

**Critical Gap:** Espressif documentation does NOT explicitly specify what happens to I2C GPIO pins (SDA/SCL) during:
1. Soft reset via USB CDC (DTR/RTS)
2. Hard reset via RESET button
3. Power-on-reset

**Standard Behavior (inferred from ESP32/ESP32-S3):**
- GPIO pins default to **high-impedance input** on reset
- External pull-up resistors pull I2C lines HIGH during reset
- If I2C slave was holding SDA low before reset, the lines should release

**Problem Scenario:**
If a slave device is in the middle of an I2C transaction when the master (P4) performs a soft reset:
1. P4 releases control of SDA/SCL
2. Slave may still be holding SDA low (e.g., STM32F030 in middle of bit transmission)
3. After reset, P4 comes back online and sees SDA stuck low
4. P4 I2C initialization may fail or timeout trying to clear the bus

### Mitigation Strategy for Tab5
- Ensure external pull-up resistors are present (4.7 kΩ typical for 3.3V)
- Implement I2C bus recovery on startup: attempt 9 clock pulses, then STOP condition
- Add watchdog timer to detect stuck bus and trigger controlled reset
- Consider adding soft reset delay (100-200 ms) to allow slave devices to complete transactions

---

## 4. I2C Clock Stretching and STM32F030 Compatibility

### ESP32-P4 Clock Stretching Support

**Status:** Supported on both master and slave modes.

**Timeout Configuration:**
- Default clock stretching timeout: ~80 µs (at 100 kHz I2C clock, 80 MHz ESP clock)
- Equivalent to 8 I2C SCL clock cycles
- Configurable via `i2c_set_timeout()` function
- If set to 0, driver uses default register value

**Calculation:**
```
timeout = 8 SCL cycles × (1 / 100 kHz) = 80 µs
```

### STM32F030 Clock Stretching Compatibility

**Critical Finding:** STM32F030 supports I2C slave clock stretching (holds SCL low while preparing response).

**Compatibility Assessment:**
- ✅ ESP32-P4 can tolerate clock stretching as a master
- ✅ ESP32-P4 can perform clock stretching as a slave (though limited by errata I2C-308)
- ⚠️ Ensure timeout is long enough for STM32F030 response time

**Recommended Configuration for Tab5 ↔ STM32F030:**
```c
i2c_set_timeout(i2c_num, timeout_ticks);
// Timeout should be > STM32F030 response time
// Suggest: 1000-5000 ticks (at 80 MHz = 12.5-62.5 µs minimum)
// For slow slaves, use -1 (maximum timeout, typically milliseconds)
```

---

## 5. I2C Bus Clear and Manual Recovery Procedures

### Hardware Bus Clear on ESP32-P4

The ESP32-P4 I2C controller has a hardware bus clear feature (exact register details require consulting Technical Reference Manual).

**Standard Registers (inferred from ESP32):**
- `I2C_FIFO` - FIFO control
- `I2C_CTR` - Control register (may include bus clear trigger)
- `I2C_STAT` - Status register (signals bus state)
- `I2C_CLK_CONF` or similar - Clock configuration

**Typical Bus Clear Procedure (hardware-assisted):**
1. Disable I2C peripheral
2. Set bus clear request bit (if available in I2C_FIFO or control register)
3. Wait for bus clear completion
4. Re-enable peripheral
5. Reinitialize driver

### Manual Software Recovery: SDA Stuck Low

When SDA is stuck low and the bus clear fails, use the **9-clock pulse recovery procedure** defined in AN-686 (Analog Devices I2C Reset Application Note):

**Procedure:**
1. Release I2C peripheral control
2. Manually toggle SCL pin as GPIO:
   - Generate 9 rising edges on SCL
   - Each pulse allows slave to release SDA
   - Keep SDA released between pulses
3. Check if SDA releases after each pulse
4. Once SDA is high, generate START condition
5. Generate STOP condition
6. Re-initialize I2C driver

**Pseudo-code:**
```cpp
void i2c_manual_bus_recovery(int sda_pin, int scl_pin) {
    // Configure as GPIO (not I2C peripheral)
    gpio_set_direction(scl_pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_direction(sda_pin, GPIO_MODE_INPUT);
    
    // 9 clock pulses
    for (int i = 0; i < 9; i++) {
        gpio_set_level(scl_pin, 0);  // SCL low
        delay_us(100);
        gpio_set_level(scl_pin, 1);  // SCL high
        delay_us(100);
        
        // Check if SDA released
        if (gpio_get_level(sda_pin) == 1) {
            break;  // SDA is free
        }
    }
    
    // START condition: SDA goes low while SCL high
    gpio_set_direction(sda_pin, GPIO_MODE_OUTPUT_OD);
    delay_us(50);
    gpio_set_level(scl_pin, 0);  // SCL low
    delay_us(50);
    
    // STOP condition: SDA goes high while SCL high
    gpio_set_level(scl_pin, 1);  // SCL high
    delay_us(50);
    gpio_set_level(sda_pin, 1);  // SDA high
    delay_us(50);
    
    // Restore to I2C peripheral control
    gpio_set_direction(sda_pin, GPIO_MODE_INPUT);
    gpio_set_direction(scl_pin, GPIO_MODE_INPUT);
}
```

**Limitations:**
- Only works if SDA is low (SCL can still be controlled)
- If both SDA and SCL are stuck low, a power-cycle reset may be required
- Success depends on slave device state and I2C implementation

---

## 6. Internal Pull-Up Configuration

### ESP32-P4 I2C Pull-Up Options

| Pull-up | Value | Notes |
|---------|-------|-------|
| Internal | ~45 kΩ | Weak; enabled by default in I2C driver |
| Recommended external | 4.7 kΩ (3.3V) | Standard I2C value |
| Alt external | 2.4 kΩ (3.3V fast bus) | For higher speeds or long wires |

**When Internal Pull-ups Are Insufficient:**
- Longer I2C bus runs (>20 cm)
- Capacitive loading from multiple devices
- Higher I2C speeds (>100 kHz)
- Heavily loaded bus

**For Tab5 Recommendations:**
- Use **external 4.7 kΩ pull-ups** on both SDA and SCL
- Pull-ups should connect to **3.3V** (not 5V on Tab5)
- Enables hardware bus recovery and improves signal integrity

### GPIO Configuration After Wire.end()

**Observation:** The Arduino `Wire.end()` function does NOT explicitly set I2C GPIO pins to floating state.

**Actual Behavior:**
- `Wire.end()` disables the I2C peripheral
- GPIO pins remain in their last configured state (typically open-drain output disabled)
- **External pull-up resistors** actually keep the lines HIGH by default
- GPIO does NOT go to high-impedance floating unless explicitly configured

**For Bus Recovery:** After `Wire.end()`, manually set GPIO pins to INPUT or explicit floating state if needed to allow external recovery circuits to work.

---

## 7. Low-Power (LP) I2C Isolation

### LP I2C on ESP32-P4

**Architecture:**
- Separate 1× I2C controller in low-power domain
- Shares I2C protocol but with reduced features
- No slave mode support
- Can run from XTAL clock (no power management overhead)

### Potential Interference

**Risk:** If both HP I2C and LP I2C are enabled and routed to overlapping GPIO pins, undefined behavior may occur.

**Current Documentation Gap:** Espressif does not explicitly document LP I2C ↔ HP I2C interference scenarios.

**Mitigation for Tab5:**
- Use only HP I2C (I2C0 or I2C1), NOT LP I2C
- Do NOT enable LP I2C unless explicitly needed
- If using LP I2C for a separate bus (unlikely), verify GPIO routing does not overlap

---

## 8. Reset and SDA Stuck Low: Detailed Analysis

### Sequence of Events Leading to SDA Stuck Low

1. **Tab5 (P4 master) performing I2C read from device (slave)**
   - Sending clock pulses, reading data

2. **USB CDC reset triggered** (via DTR/RTS lines)
   - P4 resets immediately
   - I2C peripheral shuts down without cleanup

3. **Slave device state at reset moment**
   - If slave was in middle of sending bit, may hold SDA low
   - If slave firmware has watchdog or timeout, may hold SDA during reset recovery

4. **P4 comes back online**
   - Bootloader runs, firmware loads
   - I2C driver initialization attempts to scan bus
   - Detects SDA held low, tries `i2c_bus_clear()` (internal 9-pulse procedure)
   - If slave continues holding SDA, procedure may timeout

5. **Result:** I2C bus remains unavailable

### Why This Happens

**Root Cause:** Asynchronous reset of master without coordinated cleanup of slave state.

**Why STM32F030 might hold SDA:**
- Firmware watchdog triggered by master reset
- Mid-transaction without state machine cleanup
- Pull-down transistor left enabled due to firmware crash

### Prevention and Recovery

**Prevention:**
1. Add 100-500 ms startup delay before first I2C communication
2. Implement hardware debounce (1 µF capacitor on EN pin) for stable reset
3. Add pull-up resistors (4.7 kΩ) to ensure lines release

**Recovery at Startup:**
1. Attempt standard I2C bus clear (9 pulses)
2. If bus clear fails (SDA remains low), implement manual GPIO recovery
3. Add watchdog timer: if I2C remains stuck, trigger system reset
4. Log event for diagnostics

---

## 9. Comparison: ESP32-P4 vs ESP32-S3 I2C Stability

| Factor | ESP32-P4 | ESP32-S3 |
|--------|----------|----------|
| I2C Core Implementation | Nearly identical | Nearly identical |
| Clock Stretching | Supported | Supported |
| Hardware Bus Clear | Yes (likely) | Yes (yes) |
| Known Slave Bugs | I2C-308 (non-FIFO) | Various (less recent) |
| Reset Behavior | DTR/RTS via USB | DTR/RTS via USB or RESET |
| GPIO Release on Reset | Not explicitly documented | Not explicitly documented |
| LP I2C | 1 controller (new) | Not available |

**Verdict:** P4 is comparable to S3 stability-wise, but LP I2C adds complexity if used.

---

## 10. Troubleshooting Checklist for Tab5 I2C Issues

### If I2C device not detected at startup:

- [ ] 1. Check power supply to I2C peripheral and pull-ups (3.3V)
- [ ] 2. Verify pull-up resistors are present (4.7 kΩ typical)
- [ ] 3. Check GPIO pin assignments don't conflict with USB-JTAG (GPIO24/25)
- [ ] 4. Add 100-500 ms startup delay before first I2C scan
- [ ] 5. Enable I2C bus clear on initialization
- [ ] 6. Check for SDA stuck low: Use oscilloscope or GPIO read
- [ ] 7. If SDA stuck low, implement manual GPIO recovery procedure

### If I2C communication intermittently fails:

- [ ] 1. Check clock stretching timeout is sufficient (use i2c_set_timeout)
- [ ] 2. Verify no bus conflicts with other peripherals
- [ ] 3. Increase I2C clock divider (slow down bus speed)
- [ ] 4. Ensure STM32F030 firmware is stable (check for watchdog resets)
- [ ] 5. Monitor SDA/SCL with logic analyzer during failures
- [ ] 6. Check for capacitive loading on bus

### If SDA gets stuck low after USB CDC reset:

- [ ] 1. Verify hardware power delivery to P4 and slave
- [ ] 2. Add pull-up capacitor on EN pin (1 µF)
- [ ] 3. Implement manual bus recovery (9 pulses + START + STOP)
- [ ] 4. Add watchdog timer to detect and recover stuck bus
- [ ] 5. Consider adding reset sequence delay to allow slave cleanup

---

## References

- [ESP32-P4 I2C Documentation (ESP-IDF v5.5.2)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/i2c.html)
- [ESP32-P4 GitHub Issue IDFGH-14623](https://github.com/espressif/esp-idf/issues/15374)
- [ESP32-P4 Errata Documentation](https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32p4/index.html)
- [Analog Devices AN-686: I2C Reset Application Note](https://www.analog.com/media/en/technical-documentation/application-notes/54305147357414an686_0.pdf)
- [ESP32 I2C Clock Stretching Issue (GitHub #2551)](https://github.com/espressif/esp-idf/issues/2551)
- [STMicroelectronics I2C Slave Clock Stretching](https://community.st.com/t5/stm32-mcus-products/i2c-slave-clock-stretching-to-allow-time-to-prepare-response/td-p/570801)
- [Espressif esptool Documentation - Boot Mode Selection](https://docs.espressif.com/projects/esptool/en/latest/esp32p4/advanced-topics/boot-mode-selection.html)

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-03 | research:web-search | Initial comprehensive research on ESP32-P4 I2C hardware, silicon bugs, reset behavior, and recovery procedures |

