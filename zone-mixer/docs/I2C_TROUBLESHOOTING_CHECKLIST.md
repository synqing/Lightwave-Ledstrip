---
abstract: "Root cause diagnostic checklist for Tab5 I2C stuck LOW problems. Systematic elimination of firmware bugs, hardware defects, and electrical issues. Includes logic analyzer capture patterns, pull-up verification, and slave firmware debugging."
---

# I2C Bus Stuck LOW — Diagnostic Checklist

## When to Use This Document

Use this checklist **after** hardware recovery attempts (power cycle, bit-bang, GPIO pulse) have **failed**. At that point, the root cause is either:

1. **Slave firmware deadlock** (most likely)
2. **Electrical defect** (solder joint, wrong resistor, short)
3. **Module incompatibility** (wrong device on the I2C bus)

This guide helps you systematically identify which of the three it is.

---

## Quick Diagnosis Tree

```
Does I2C work at all?
├─ YES → Skip this document; use normal I2C debugging
└─ NO → Is SDA or SCL stuck at boot?
    ├─ YES (stuck at LOW) → Continue with full checklist below
    └─ NO (both HIGH, but no response) → Slave might be missing; check wiring
```

---

## Phase 1: Electrical Diagnosis (Takes ~10 minutes)

### 1.1 Visual Inspection

- [ ] **Grove connector:** No visible solder bridges or cold joints
- [ ] **Slave module:** LEDs, capacitors, and visible components look normal
- [ ] **Pullup resistors:** Present on Tab5's I2C pins (usually 10k on breakout boards)
- [ ] **Wiring:** SDA connects to GPIO53, SCL to GPIO54, GND to GND
- [ ] **No floating wires:** Check for unsoldered pins on the slave module

### 1.2 Multimeter Checks (DC voltage)

Measure **with power OFF**:

| Measurement | Expected | Issue If... |
|---|---|---|
| Resistance: GPIO53 to GND | 4.7k–10k | <1k = short to GND; >100k = missing pull-up |
| Resistance: GPIO54 to GND | 4.7k–10k | <1k = short to GND; >100k = missing pull-up |
| Resistance: GPIO53 to GPIO54 | >100k | <10k = cross-connection |

Measure **with power ON**, idle (no I2C traffic):

| Measurement | Expected | Issue If... |
|---|---|---|
| Voltage: GPIO53 (SDA) | 3.0–3.3V | 0–0.5V = stuck LOW (expected) |
| Voltage: GPIO54 (SCL) | 3.0–3.3V | 0–0.5V = stuck LOW (expected) |
| Voltage: Slave VCC | 3.3V | <3.0V = brown-out or power issue |

### 1.3 If Voltages Are Wrong

- [ ] **Voltage below 3.0V on VCC:** Slave is brown-out or power-starved
  - Check capacitor placement near slave power pin
  - Measure current drawn by slave (should be <10mA idle)
  - Try a different power supply or USB cable

- [ ] **SCL/SDA HIGH instead of stuck:** Bus might not be stuck; try communication
  - Pull-up resistors are working
  - Problem might be at the I2C protocol level (not electrical)

### Result: If All Electrical Checks Pass
→ Proceed to **Phase 2: Electrical Verification with Logic Analyzer**

---

## Phase 2: Electrical Verification with Logic Analyzer

### 2.1 Capture Hardware Reset Sequence

**Setup:**
- Connect logic analyzer (or ESP32 with GPIO capture code) to GPIO53 and GPIO54
- Power ON the Tab5 from completely off state
- Capture first 500 ms of boot

**Expected pattern (healthy bus):**
```
Time 0 ms: SCL=1, SDA=1 (pull-ups pull HIGH after power-on reset)
Time 0–100 ms: No activity (I2C slave booting)
Time 100+ ms: I2C master tries to communicate
```

**Actual pattern (stuck bus):**
```
Time 0 ms: SCL=0, SDA=0 (stuck at boot, not released by pull-ups)
          OR: SCL=0, SDA=1 (only SCL stuck)
Time 0–500 ms: No change (lines stay stuck)
```

### 2.2 If Stuck at Boot

**Key observation:** When did the lines go stuck?
- **During power-on reset (0–10 ms):** Slave's bootloader or initial hardware config is pulling the line
- **After reset completes (>10 ms):** Slave firmware is running and holding the line

### 2.3 Test Pattern: Manually Clock SCL

**Procedure:**
1. Power OFF Tab5
2. Set GPIO54 (SCL) as a regular GPIO output
3. Power ON Tab5
4. Measure: Does SCL respond to your GPIO commands, or is it held LOW by the slave?

**Code to test:**
```c
void test_scl_controllability(void) {
    // After power-on, before I2C init
    gpio_config_t test_conf = {
        .pin_bit_mask = 1ULL << 54,  // SCL pin
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&test_conf);
    
    // Try to drive SCL HIGH
    gpio_set_level(54, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    int scl_level = gpio_get_level(54);
    
    if (scl_level == 1) {
        ESP_LOGI("TEST", "SCL is controllable! Slave is not holding it.");
    } else {
        ESP_LOGI("TEST", "SCL is stuck LOW even with GPIO driver. Slave is actively holding it.");
    }
}
```

**Result:**
- **SCL is controllable:** Electrical issue (short, parasitic capacitance)
- **SCL is stuck even with GPIO driver:** Slave is actively driving it LOW (firmware bug)

### Result: If Electrical Verification Passes
→ Proceed to **Phase 3: Slave Firmware Diagnostics**

---

## Phase 3: Slave Firmware Debugging

### Assumption at This Point
- Power cycle and bit-bang recovery have **failed**
- Electrical checks show no obvious shorts
- **SCL is actively held LOW by the slave device**

This means the slave's firmware is stuck in a state that holds SCL LOW. The question is: **why?**

### 3.1 Hypothesis: Slave Bootloader Hangs

**Scenario:** The slave's bootloader or initial I2C peripheral configuration pulls the line and never releases it.

**Test:**
- [ ] **Does slave have a status LED?** If it's off/dim, bootloader might have failed
- [ ] **Can you access the slave's debug serial port?** Connect UART and check for boot messages
- [ ] **Is there a reset pin you can pulse?** (Not all Grove modules expose it)

**If accessible:** Look for:
- Repeated I2C address collision messages
- Power-on reset loops
- Stuck in a wait-for-slave state

### 3.2 Hypothesis: I2C Interrupt Handler Race Condition

**Scenario:** The slave's firmware correctly initializes, but an I2C interrupt handler gets stuck.

**This is the most common cause.** Typical bug pattern:

```c
// BROKEN: Slave I2C interrupt handler
void i2c_isr(void) {
    if (i2c_get_address_match()) {
        // Slave received address byte
        // Firmware expects to send data next
        
        // BUG: If data is not ready, handler waits forever
        while (data_is_not_ready) {
            // No timeout! No escape mechanism!
            // SCL stays LOW due to clock stretching
        }
    }
}
```

**If you can access slave firmware:**
- Search for I2C interrupt handlers that **lack timeout mechanisms**
- Look for `while()` loops that wait on I2C events with no escape clause
- Check for missing interrupt acknowledgement or register clearing

### 3.3 Test: Force Slave Into I2C Slave Mode

If the slave can be reconfigured, try:

```c
// On slave device, restart I2C in slave mode with timeout
void slave_i2c_with_timeout(void) {
    // Configure as I2C slave with generous timeout
    i2c_config_t slave_config = {
        .mode = I2C_MODE_SLAVE,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .slave.addr_10bit_en = false,
        .slave.slave_addr = 0x50,  // Slave address
    };
    
    i2c_param_config(I2C_NUM, &slave_config);
    i2c_driver_install(I2C_NUM, slave_config.mode, 256, 256, 0);
    
    // Set a clock stretching timeout (if supported)
    // This prevents the slave from holding the bus indefinitely
}
```

### 3.4 If Slave Firmware Is Not Accessible

You have three options:

1. **Request firmware source** from the slave's vendor/author
2. **Swap the module** for a known-working version
3. **Accept that the slave needs patching** and plan for a hardware revision

### Result: If Slave Firmware Bug Is Confirmed
→ Proceed to **Phase 4: Long-Term Solutions**

---

## Phase 4: Long-Term Solutions

### Option A: Fix Slave Firmware (If Possible)

**Steps:**
1. Obtain slave firmware source or disassembly
2. Locate I2C interrupt handler
3. Add clock stretching timeout (typical: 100–1000 ms)
4. Ensure handler always releases SCL
5. Recompile, test

**Example fix:**
```c
// FIXED: I2C interrupt handler with timeout
static uint32_t scl_hold_start_ms = 0;
const uint32_t SCL_HOLD_TIMEOUT_MS = 100;

void i2c_isr(void) {
    uint32_t now = esp_timer_get_time() / 1000;
    
    if (i2c_get_address_match()) {
        scl_hold_start_ms = now;
        
        // Wait for data to be ready, BUT with timeout
        while (data_is_not_ready) {
            if (now - scl_hold_start_ms > SCL_HOLD_TIMEOUT_MS) {
                // Timeout! Bail out and release SCL
                break;
            }
            now = esp_timer_get_time() / 1000;
        }
        
        // Send data (or NACK if timeout)
        send_i2c_response();
    }
    
    scl_hold_start_ms = 0;  // Clear for next transaction
}
```

### Option B: Replace Slave Module

If firmware is not available and the vendor won't fix it:

- [ ] Source an alternative I2C slave module with the same functionality
- [ ] Test with the new module to verify it doesn't have the same bug
- [ ] Plan a hardware revision to swap modules

### Option C: Isolate the Problematic Slave on a Separate I2C Bus

If the Tab5 hardware has a second I2C port:

```c
// Use separate I2C bus for the problematic slave
i2c_config_t isolated_bus = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = GPIO_XX,  // Different pins
    .scl_io_num = GPIO_YY,
    .master.clk_speed = 100000,
};
i2c_param_config(I2C_NUM_1, &isolated_bus);
i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0);

// Use I2C_NUM_1 only for the problematic device
// Keep I2C_NUM_0 for other devices
```

### Option D: Add a Watchdog at Firmware Initialization

Even if you can't fix the slave, you can detect the hung state early:

```c
void i2c_init_with_watchdog(void) {
    // Create a task that monitors I2C bus health
    xTaskCreate(i2c_watchdog_task, "i2c_watchdog", 2048, NULL, 5, NULL);
    
    // Start I2C driver
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

void i2c_watchdog_task(void *arg) {
    while (1) {
        // Every 500 ms, attempt a simple I2C read
        uint8_t dummy;
        esp_err_t result = i2c_master_read_from_device(
            I2C_NUM_0,
            0x50,  // Slave address
            &dummy,
            1,
            pdMS_TO_TICKS(100)
        );
        
        if (result != ESP_OK) {
            // I2C transaction failed; bus might be stuck
            ESP_LOGE("I2C_WDT", "I2C bus unresponsive; attempting recovery");
            i2c_attempt_recovery(53, 54, 11);  // Retry recovery
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

---

## Summary: Decision Tree for Root Cause

```
Does I2C work after recovery attempts?
├─ YES → Document and move on (recovery works)
└─ NO:
    Does electrical check pass? (no shorts, correct voltages)
    ├─ NO → Fix electrical issue (resistors, solder, wiring)
    └─ YES:
        Is SCL stuck LOW even with GPIO driver?
        ├─ NO → Electrical; check wiring and capacitance
        └─ YES:
            Can you access slave firmware?
            ├─ YES → Fix I2C interrupt timeout
            └─ NO:
                Can you swap the module?
                ├─ YES → Replace with known-good version
                └─ NO → Accept limitation; isolate on separate bus + watchdog
```

---

## Appendix: Quick Reference — What Each Recovery Method Tells You

| Recovery Method | Success Means | Failure Means |
|---|---|---|
| **Power cycle** | Slave firmware resets cleanly; I2C starts fresh | Slave power supply issue OR firmware deadlock on every boot |
| **Bit-bang 9 clocks** | Slave state machine advances after clock pulses | Slave is in infinite loop, not clock-stretching |
| **GPIO pulse** | Noise disrupts marginal electrical condition | Electrical is clean; slave is firmly stuck |
| **I2C periph reset** | Master state machine was corrupt | Slave is holding the line (not master issue) |
| **All fail + EXT5V_EN works** | Slave firmware deadlock on every startup | — |
| **All fail + EXT5V_EN fails** | Hardware defect or incompatible slave module | — |

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-02 | K1 Research Agent | Diagnostic checklist: electrical verification, slave firmware debugging, decision tree, long-term solutions |
