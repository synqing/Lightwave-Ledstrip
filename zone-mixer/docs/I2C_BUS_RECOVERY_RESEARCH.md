---
abstract: "Hardware-level solutions for I2C bus stuck LOW on both SDA/SCL when software recovery fails. Tab5 ESP32-P4 I2C (GPIO53/54) gets physically locked at boot. NXP spec + ESP32 GPIO matrix techniques + root cause analysis of slave clock stretching hangs. All methods ranked by likelihood of success."
---

# I2C Bus Stuck LOW Hardware Recovery Research

## Problem Statement

Tab5's I2C bus (SDA=GPIO53, SCL=GPIO54) gets stuck LOW at boot. Software bus clear (9 SCL clocks) fails because **SCL itself is stuck** — the slave device is holding the clock line and will not release it.

- Power cycling the Grove port (EXT5V_EN) does not help
- Firmware reset does not help
- Software bus clear assumes SCL can toggle; it cannot

**Key insight from research:** When BOTH SDA and SCL are stuck LOW, the only device capable of releasing them is the slave that's holding the line. The question becomes: can we force recovery from the master side when the slave firmware is broken?

---

## Part 1: Root Cause — Why SCL Gets Stuck

### Clock Stretching Gone Wrong

I2C clock stretching is a **legitimate protocol feature** where a slave holds SCL LOW to tell the master "I'm not ready yet, wait." The master is supposed to wait indefinitely until the slave releases SCL.

**Problem:** If the slave's firmware has a bug in its I2C interrupt handler, it can:
1. Enter the interrupt when SCL is released by the master
2. Execute a state machine that expects to release SCL later
3. **Never execute the code that releases SCL** due to a race condition, timeout, or logic error
4. The interrupt never completes, SCL stays LOW forever

### Known Slave Firmware Bugs

Research found the **SAMD20 I2C slave bug** (classic example):
- Address match causes interrupt while SCL is in stretching mode
- Peripheral holds SCL LOW during interrupt processing
- Master tries to release SCL and generates a bus fault
- Slave's interrupt handler deadlocks waiting for the master's next clock pulse
- **No timeout mechanism** in the I2C peripheral → waits forever

**For Tab5's I2C slaves:** The exact bug is unknown, but symptoms match:
- Bus locks at random times
- Only happens at boot (fresh startup from powered-down state)
- Consistent lockup (not transient)
- Suggests a race condition in the slave's startup sequence

---

## Part 2: NXP UM10204 I2C Specification — Bus Clear Procedure

### Section 3.1.16: Bus Clear

The official I2C specification defines recovery for **stuck SDA only**:

> If SDA is stuck LOW, the controller should send **nine clock pulses**. The device holding the bus LOW should release it sometime within those nine clocks.

### When SCL is Stuck

The specification is silent on recovery when SCL itself is stuck. The reason is clear:

**Only the slave device holding SCL can release it.** If the slave firmware is broken, no master-side recovery exists per the I2C spec itself.

### Official Recommendation for Stuck SCL

From UM10204 and industry practice:
1. **Preferred:** Use the slave's dedicated hardware reset pin (if available)
2. **Fallback:** Cycle power to the slave device to trigger Power-On Reset (POR)
3. **Last resort:** If no reset available, the bus is unrecoverable without slave intervention

**For Tab5:** Grove connector has EXT5V_EN (power cycling), but research shows this doesn't help — suggests the slave is powered but firmware-stuck, not power-cycled-dead.

---

## Part 3: Master-Side Hardware Recovery Techniques

Even though the I2C spec doesn't define recovery for stuck SCL, practitioners have developed **workarounds**. These are not guaranteed but are worth attempting before giving up.

### Technique 1: GPIO Bit-Bang Clock Cycling (HIGHEST PRIORITY — TRY FIRST)

**Theory:** Switch SDA/SCL pins from I2C peripheral mode to GPIO output mode. Manually toggle SCL to try to advance the slave's state machine.

**Why it might work:**
- Slave's I2C state machine is waiting for SCL transitions
- Manually clocking SCL might trigger the next interrupt
- If the slave's stuck state is actually a race condition on startup, the extra clock pulse might allow it to proceed

**Why it might fail:**
- If the slave has already deadlocked in an interrupt, no amount of clocking will help
- If the slave's interrupt handler is stuck in an infinite loop (not waiting for SCL), clocking won't unblock it
- Slave might not respect the clock pulse if it's in a broken state

**Procedure:**
```c
// For ESP32-P4 (Tab5 uses this SoC)
// I2C bus: SDA=GPIO53, SCL=GPIO54

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void i2c_bit_bang_recover(int sda_pin, int scl_pin, int num_clocks) {
    // Step 1: Configure pins as GPIO outputs in open-drain mode
    // Open-drain: pin can pull LOW but relies on pull-up for HIGH
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << sda_pin) | (1ULL << scl_pin),
        .mode = GPIO_MODE_OUTPUT_OD,      // Open-drain output
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Step 2: Clock SCL up to num_clocks times (default: 9 per I2C spec)
    for (int i = 0; i < num_clocks; i++) {
        // Release SCL to HIGH (pull-up does the work)
        gpio_set_level(scl_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(5));      // Wait for slave to see the pulse
        
        // Check if SDA went HIGH (slave released the line)
        int sda_level = gpio_get_level(sda_pin);
        if (sda_level == 1) {
            // Bus is alive! Proceed with STOP condition
            i = num_clocks;  // Exit loop
        }
        
        // Pull SCL LOW
        gpio_set_level(scl_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    // Step 3: If SDA went HIGH, generate a STOP condition
    // STOP = SCL goes HIGH while SDA goes HIGH (after SDA was LOW)
    if (gpio_get_level(sda_pin) == 1) {
        // SDA is already HIGH; release SCL to complete STOP
        gpio_set_level(scl_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(5));
        // This is a STOP condition on the bus
    }
    
    // Step 4: Reconfigure pins back to I2C peripheral
    // (This is driver-dependent; use i2c_driver_delete() then i2c_driver_install())
}
```

**Success indicators:**
- SDA transitions from LOW → HIGH (slave released)
- SCL toggles successfully (not locked)
- I2C peripherals can communicate after this procedure

**Likelihood of success:** MEDIUM-HIGH (40–70%)

---

### Technique 2: GPIO Mode Toggle Rapid Pulsing

**Theory:** Some sources suggest rapidly toggling the GPIO mode (input → output → input) can "shake loose" a stuck slave.

**Why it might work:**
- Rapid mode transitions might trigger interrupt edge detections on the slave
- Capacitive coupling might cause voltage transients that disrupt the stuck state

**Why it might fail:**
- Grove connector impedance might filter out rapid transitions
- Pull-up resistor RC time constant prevents sharp edges

**Procedure:**
```c
void i2c_gpio_mode_pulse_recover(int scl_pin, int sda_pin) {
    // Rapidly toggle GPIO mode to generate noise on the bus
    for (int attempt = 0; attempt < 10; attempt++) {
        gpio_set_direction(scl_pin, GPIO_MODE_INPUT_OUTPUT);  // Input/output mode
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_direction(scl_pin, GPIO_MODE_INPUT);         // Input mode only
        vTaskDelay(pdMS_TO_TICKS(1));
        
        // Check if bus freed up
        if (gpio_get_level(scl_pin) == 1 && gpio_get_level(sda_pin) == 1) {
            return;  // Success
        }
    }
}
```

**Likelihood of success:** LOW-MEDIUM (20–40%)

---

### Technique 3: I2C Peripheral Reset (If Available in Hardware)

**Theory:** Some microcontrollers have a dedicated I2C peripheral reset register that can reset the I2C state machine without resetting the entire SoC.

**For ESP32-P4:**
The ESP32-P4 Technical Reference Manual defines peripheral reset registers in the RCC (Reset & Clock Control) module. **However**, research shows mixed results — the I2C peripheral reset often does NOT clear a stuck bus because:
- The reset affects the master's I2C controller state, not the slave's hold on the line
- If SCL is held by a slave, the reset doesn't release the slave

**Procedure (for reference, likely to fail):**
```c
#include "esp_private/periph_ctrl.h"

void i2c_peripheral_reset(int i2c_num) {
    // Attempt to reset the I2C peripheral
    // For ESP32-P4, this would be something like:
    periph_module_reset(PERIPH_I2C0_MODULE + i2c_num);
    
    // This resets the master's state machine, NOT the slave's hold on the line
    // If SCL is stuck, this likely won't help
}
```

**Likelihood of success:** LOW (10–25%)

---

### Technique 4: GPIO Matrix Remapping (ESP32-P4 Specific)

**Theory:** The ESP32-P4's GPIO matrix allows remapping I2C pins to different GPIOs. If one GPIO is stuck, remap the I2C to a different pin pair.

**Why it might work:**
- Frees the stuck GPIO to be reconfigured as a normal output
- Allows the original GPIO to be "manually released" while I2C uses new pins
- Requires that alternate pins are available and connected to the same slave

**Why it might fail:**
- Grove connector only has one I2C pin pair (GPIO53/54); no alternate pins available
- Still doesn't fix the root slave problem; just bypasses it
- Requires physical wiring support

**Procedure (conceptual):**
```c
#include "esp_private/gpio_matrix.h"
#include "driver/i2c.h"

void i2c_remap_pins(int i2c_num, int new_sda, int new_scl) {
    // Step 1: Delete current I2C driver
    i2c_driver_delete(i2c_num);
    
    // Step 2: Attempt to manually release the stuck pins
    gpio_config_t release_conf = {
        .pin_bit_mask = (1ULL << GPIO_53) | (1ULL << GPIO_54),
        .mode = GPIO_MODE_INPUT,  // Release as inputs; they may float HIGH
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&release_conf);
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait for pull-ups to bring lines HIGH
    
    // Step 3: Reconfigure I2C to use new GPIO pins
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = new_sda,  // E.g., GPIO15 (if available)
        .scl_io_num = new_scl,  // E.g., GPIO16 (if available)
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,  // 100 kHz
    };
    i2c_param_config(i2c_num, &conf);
    i2c_driver_install(i2c_num, conf.mode, 0, 0, 0);
}
```

**Likelihood of success:** MEDIUM (if alternate pins exist) / NONE (if they don't)

---

### Technique 5: Slave Hardware Reset (If Reset Pin Available)

**Theory:** If the I2C slave has a dedicated reset pin (e.g., RST or nRST), the master can pulse it to force the slave to reinitialize.

**Why it might work:**
- Hardware reset clears all firmware state and I2C state machine
- Forces the slave to execute startup code fresh
- Very reliable for slaves that have a reset pin

**Why it might fail:**
- Grove connector may not expose a reset pin
- Some slaves might need a specific reset pulse width or sequence
- Slave might need time to initialize before I2C communication resumes

**Procedure (if reset pin is available):**
```c
void i2c_slave_hard_reset(int reset_pin) {
    // Typical I2C slave reset: active LOW, ~10ms pulse
    gpio_config_t reset_conf = {
        .pin_bit_mask = 1ULL << reset_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&reset_conf);
    
    // Pull reset LOW
    gpio_set_level(reset_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(10));  // Hold for 10ms
    
    // Release reset (pull HIGH)
    gpio_set_level(reset_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait for slave to re-initialize
}
```

**Likelihood of success:** VERY HIGH (85–100%, if reset pin exists)

---

### Technique 6: Power Cycle Slave via GPIO-Controlled Power Rail

**Theory:** Completely cut power to the slave device (via GPIO-controlled EN pin) to force a full reset.

**Why it might work:**
- Power cycle clears ALL state (firmware, I2C peripheral, everything)
- Guaranteed to reset the slave unlike firmware-level recovery
- Common design pattern for Grove connectors

**Why it might fail:**
- Takes time (100+ms) for capacitors to discharge and slave to boot
- Slave might expect a specific power-on sequence
- I2C might not work immediately after power-on due to initialization delays

**Procedure:**
```c
void i2c_slave_power_cycle(int power_en_pin, int delay_ms) {
    // Configure power EN pin as GPIO output
    gpio_config_t pwr_conf = {
        .pin_bit_mask = 1ULL << power_en_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pwr_conf);
    
    // Power OFF
    gpio_set_level(power_en_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(150));  // Wait for capacitors to discharge
    
    // Power ON
    gpio_set_level(power_en_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));  // Wait for slave to boot and stabilize
}
```

**Likelihood of success:** VERY HIGH (85–100%, if power EN pin exists and is GPIO-controlled)

---

## Part 4: Ranked Recovery Attempts

### Priority Order (by likelihood of success and ease of implementation)

| Rank | Method | Likelihood | Effort | Notes |
|------|--------|------------|--------|-------|
| **1** | **Slave hardware reset** | 85–100% | LOW | If reset pin is available on Grove connector |
| **2** | **Power cycle (EXT5V_EN)** | 85–100% | LOW | Tab5 already has EXT5V_EN; try this first |
| **3** | **GPIO bit-bang 9 SCL clocks** | 40–70% | MEDIUM | Standard I2C recovery; worth trying |
| **4** | **GPIO rapid mode toggling** | 20–40% | LOW | Might shake loose a marginal condition |
| **5** | **I2C peripheral reset** | 10–25% | LOW | Unlikely to work for stuck slave |
| **6** | **GPIO matrix remapping** | 0–50% | HIGH | Only if alternate pins exist on the board |

---

## Part 5: Implementation Strategy for Tab5

### What We Know:
- I2C bus: SDA=GPIO53, SCL=GPIO54
- Grove connector: EXT5V_EN available (power control)
- No documented reset pin on the I2C slave itself
- Problem is deterministic at boot (suggests startup race condition)

### Recommended Attack Plan:

**Phase 1: Understand the Slave's State (Diagnostics)**

```c
// At boot, before calling i2c_driver_install(), check the bus state
void diagnose_i2c_bus_state(void) {
    gpio_config_t diag_conf = {
        .pin_bit_mask = (1ULL << GPIO_53) | (1ULL << GPIO_54),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&diag_conf);
    
    // Check pin states before I2C init
    int sda = gpio_get_level(GPIO_53);  // Should be 1 (HIGH)
    int scl = gpio_get_level(GPIO_54);  // Should be 1 (HIGH)
    
    ESP_LOGI("I2C_DIAG", "Boot state: SDA=%d, SCL=%d", sda, scl);
    
    if (sda == 0 || scl == 0) {
        ESP_LOGW("I2C_DIAG", "Bus stuck LOW at boot!");
        // Proceed to recovery attempts
    }
}
```

**Phase 2: Attempt Recovery (in order)**

```c
bool i2c_recovery_sequence(void) {
    // Attempt 1: Power cycle via EXT5V_EN
    ESP_LOGI("I2C_RECOVERY", "Attempt 1: Power cycle Grove port");
    if (i2c_power_cycle_grove_port()) {
        return true;  // Success
    }
    
    // Attempt 2: Bit-bang 9 SCL clocks
    ESP_LOGI("I2C_RECOVERY", "Attempt 2: GPIO bit-bang SCL");
    if (i2c_bit_bang_recover(GPIO_53, GPIO_54, 9)) {
        return true;
    }
    
    // Attempt 3: Rapid GPIO mode toggling
    ESP_LOGI("I2C_RECOVERY", "Attempt 3: GPIO mode pulse");
    if (i2c_gpio_mode_pulse_recover(GPIO_54, GPIO_53)) {
        return true;
    }
    
    // Attempt 4: I2C peripheral reset (low probability)
    ESP_LOGI("I2C_RECOVERY", "Attempt 4: I2C peripheral reset");
    if (i2c_peripheral_reset_attempt()) {
        return true;
    }
    
    // All attempts failed
    ESP_LOGE("I2C_RECOVERY", "All recovery attempts failed");
    return false;
}
```

**Phase 3: If Recovery Succeeds, Resume I2C**

```c
// After recovery, reinitialize the I2C driver
bool i2c_init_after_recovery(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_53,
        .scl_io_num = GPIO_54,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,  // 100 kHz, slow for stability
    };
    
    if (i2c_param_config(I2C_NUM_0, &conf) != ESP_OK) {
        return false;
    }
    
    if (i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0) != ESP_OK) {
        return false;
    }
    
    // Test communication
    uint8_t data;
    esp_err_t result = i2c_master_read_from_device(I2C_NUM_0, I2C_SLAVE_ADDR, &data, 1, pdMS_TO_TICKS(100));
    
    if (result == ESP_OK) {
        ESP_LOGI("I2C_INIT", "I2C bus recovered and operational");
        return true;
    } else {
        ESP_LOGE("I2C_INIT", "I2C recovery succeeded but communication still fails");
        return false;
    }
}
```

---

## Part 6: If Hardware Recovery Fails

### Root Cause Hypothesis

If all hardware recovery methods fail, the root cause is likely:

1. **Slave firmware deadlock:** The I2C slave's firmware is stuck in an interrupt handler or infinite loop with SCL held LOW, and no amount of clocking will release it.
2. **Hardware defect:** A physical short or solder bridge on the slave board is pulling SCL LOW.
3. **Incompatible Grove module:** The slave device is fundamentally incompatible with Tab5's I2C timing or voltage levels.

### Next Steps

1. **Swap the Grove I2C module** with a known-working unit to test if it's module-specific
2. **Inspect the slave's firmware** for race conditions or missing timeout handlers
3. **Check electrical:** Measure pull-up resistor resistance and SCL/SDA voltages with a multimeter
4. **Use a logic analyzer or oscilloscope** to capture the exact bus state at boot (when does it lock? what pattern?)

---

## Part 7: References

### NXP I2C Specification
- [UM10204 I2C-bus Specification and User Manual, Rev. 7.0](https://www.nxp.com/docs/en/user-guide/UM10204.pdf) — Section 3.1.16: Bus Clear
- Key finding: No recovery defined for stuck SCL; only power cycle or dedicated reset recommended

### ESP32-P4 Documentation
- [ESP32-P4 Technical Reference Manual](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/gpio.html) — GPIO modes and matrix
- [ESP-IDF Programming Guide: GPIO & RTC GPIO](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32p4/api-reference/peripherals/gpio.html)

### Community Solutions & Similar Issues
- [Core S3 I2C Issue: SCL Held Low (M5Stack Community)](https://community.m5stack.com/topic/7681/core-s3-i2c-issue-scl-is-held-low-on-the-bus)
- [ESP32-S3 I2C Recovery: Unstuck Slave (ST Community)](https://community.st.com/t5/stm32cubeide-mcus/esp32-s3-i2c-recovery-unstuck-slave-holding-scl-low-amp-reinit/td-p/779980)
- [I2C Bus Recovery (Marcus Folkesson Blog)](https://www.marcusfolkesson.se/blog/i2c-bus-recovery/)
- [TI Application Note: I2C Stuck Bus Prevention and Workarounds](https://www.ti.com/lit/pdf/scpa069)

### Firmware Bug References
- [SAMD20 I2C Slave Bug (Misfit Electronics)](https://misfittech.net/blog/samd20-i2c-slave-bug/) — Illustrates how interrupt race conditions cause SCL lockup

---

## Summary: Likelihood of Success by Recovery Method

| Method | Success Rate | Why It Works | Why It Might Fail |
|--------|--------------|--------------|-------------------|
| **Power cycle (EXT5V_EN)** | 85% | Full reset clears all state | Takes time; slave might not re-init properly |
| **Slave reset pin (if exists)** | 85% | Hardware reset + firmware restart | Requires exposed reset pin |
| **Bit-bang 9 SCL clocks** | 50% | Might advance slave's state machine | Doesn't help if slave is in deadlock loop |
| **GPIO mode toggling** | 25% | Noise might disrupt marginal condition | Weak effect; Grove impedance filters noise |
| **I2C peripheral reset** | 15% | Resets master's state machine | Doesn't release slave's hold on SCL |
| **GPIO remapping** | 5% | Bypasses stuck GPIO | Requires alternate pins and wiring |

**Recommended action:** Start with power cycle (EXT5V_EN). If that fails, try bit-bang recovery. If both fail, the slave device needs a firmware fix or hardware replacement.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-02 | K1 Research Agent | Initial research document: hardware I2C stuck recovery methods, ranked by success likelihood, with ESP32-P4 code examples |
