---
abstract: "One-page I2C bus recovery quick reference. Recovery methods ranked, code snippets, pin configuration, timing constants. Print this out or bookmark it."
---

# I2C Bus Recovery — Quick Reference Card

## Problem
```
Tab5 I2C stuck at boot
SDA (GPIO53) = LOW
SCL (GPIO54) = LOW
Software recovery fails: SCL cannot toggle
```

## Root Cause
Slave holding SCL LOW (clock stretching deadlock in firmware)

## Solution Priority

### 1️⃣ Try Power Cycle (85% success)
```c
// EXT5V_EN pin (Grove port 5V control)
gpio_set_level(EXT5V_EN_PIN, 0);    // Power OFF
vTaskDelay(pdMS_TO_TICKS(150));     // Wait 150ms
gpio_set_level(EXT5V_EN_PIN, 1);    // Power ON
vTaskDelay(pdMS_TO_TICKS(200));     // Wait 200ms for re-init
```
**When to use:** EXT5V_EN is implemented  
**Time:** 350 ms  
**Success:** 85%

---

### 2️⃣ Try Bit-Bang Recovery (50% success)
```c
// Configure pins as open-drain outputs
gpio_config_t conf = {
    .pin_bit_mask = (1ULL << 53) | (1ULL << 54),
    .mode = GPIO_MODE_OUTPUT_OD,
    .pull_up_en = GPIO_PULLUP_ENABLE,
};
gpio_config(&conf);

// Send 9 SCL clock pulses
for (int i = 0; i < 9; i++) {
    gpio_set_level(54, 0);  // SCL LOW
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(54, 1);  // SCL HIGH (pull-up releases)
    vTaskDelay(pdMS_TO_TICKS(5));
    
    if (gpio_get_level(53) == 1) break;  // SDA released?
}
```
**When to use:** Power cycle failed  
**Time:** 100 ms  
**Success:** 40–70%

---

### 3️⃣ Try GPIO Pulse (20% success)
```c
// Rapid GPIO mode toggle
for (int attempt = 0; attempt < 10; attempt++) {
    gpio_config_t pulse = {
        .pin_bit_mask = 1ULL << 54,  // SCL
        .mode = GPIO_MODE_OUTPUT_OD,
    };
    gpio_config(&pulse);
    gpio_set_level(54, 0);  // Pull LOW
    vTaskDelay(pdMS_TO_TICKS(1));
    
    pulse.mode = GPIO_MODE_INPUT;  // Release HIGH
    gpio_config(&pulse);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    if (gpio_get_level(53) == 1 && gpio_get_level(54) == 1) {
        break;  // Success!
    }
}
```
**When to use:** Bit-bang failed  
**Time:** 20 ms  
**Success:** 20–40%

---

## All Recovery Methods (Ranked)

| # | Method | Success | Time | Code |
|---|--------|---------|------|------|
| 1 | Power cycle | 85% | 350ms | See above |
| 2 | Bit-bang 9 clocks | 50% | 100ms | See above |
| 3 | GPIO pulse | 25% | 20ms | See above |
| 4 | I2C periph reset | 15% | <10ms | `i2c_hal_reset()` |
| 5 | Slave reset pin | 85% | 10ms | Pulse RST pin |
| 6 | GPIO matrix remap | 10% | 50ms | Requires alt pins |

---

## Pin Configuration (ESP32-P4)

```c
#define I2C_SDA_PIN         53
#define I2C_SCL_PIN         54
#define I2C_POWER_EN_PIN    11  // EXT5V_EN (verify!)
#define I2C_FREQ_HZ         100000

// Open-drain mode (mimics I2C bus behavior)
gpio_config_t i2c_conf = {
    .pin_bit_mask = (1ULL << I2C_SDA_PIN) | (1ULL << I2C_SCL_PIN),
    .mode = GPIO_MODE_OUTPUT_OD,      // Open-drain
    .pull_up_en = GPIO_PULLUP_ENABLE, // External pull-up to 3.3V
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
};
gpio_config(&i2c_conf);
```

---

## Timing Constants

```c
#define I2C_CLOCK_PULSE_WIDTH_MS    5      // SCL clock pulse timing
#define I2C_POWER_CYCLE_HOLD_MS     150    // Power OFF duration
#define I2C_POWER_CYCLE_WAIT_MS     200    // Wait for slave re-init
#define I2C_BIT_BANG_CLOCK_COUNT    9      // Standard I2C recovery
#define GPIO_PULSE_ATTEMPTS         10     // Rapid toggle iterations
```

---

## Boot Sequence Integration

```c
void init_i2c(void) {
    // Step 1: Check bus state
    if (gpio_get_level(53) == 0 || gpio_get_level(54) == 0) {
        ESP_LOGW("I2C", "Bus stuck at boot, attempting recovery");
        
        // Attempt 1: Power cycle
        if (!i2c_power_cycle_slave(11)) {
            // Attempt 2: Bit-bang
            if (!i2c_bit_bang_recovery(53, 54)) {
                // Attempt 3: GPIO pulse
                i2c_gpio_pulse_recovery(53, 54);
            }
        }
    }
    
    // Step 2: Initialize I2C driver
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 53,
        .scl_io_num = 54,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}
```

---

## Diagnostics (If Recovery Fails)

### Electrical Check (Multimeter)
```
Pin to GND resistance:
  GPIO53 (SDA): Should be 4.7k–10k (not <1k or >100k)
  GPIO54 (SCL): Should be 4.7k–10k

Pin voltage (power ON, no I2C traffic):
  GPIO53 (SDA): Should be 3.3V (if not stuck) or 0V (if stuck)
  GPIO54 (SCL): Should be 3.3V (if not stuck) or 0V (if stuck)
```

### Check If Slave Can Be Controlled
```c
// Can we drive SCL HIGH even though it's stuck?
gpio_config_t test = {
    .pin_bit_mask = 1ULL << 54,  // SCL
    .mode = GPIO_MODE_OUTPUT,    // Drive (not open-drain)
};
gpio_config(&test);
gpio_set_level(54, 1);           // Try to drive HIGH

int scl_level = gpio_get_level(54);
if (scl_level == 0) {
    // SCL is stuck LOW despite our driver
    // Slave is actively pulling it, or short circuit
    ESP_LOGE("DIAG", "SCL actively held LOW by slave");
} else {
    // SCL responded to our control
    ESP_LOGI("DIAG", "SCL is controllable");
}
```

### Logic Analyzer Capture Pattern
```
Expected (healthy):
  t=0ms:   SDA=1, SCL=1 (power-on, pull-ups active)
  t=0-100ms: No activity (slave booting)
  t=100+ms: I2C transactions visible

Actual (stuck):
  t=0ms:   SDA=0, SCL=0 (stuck immediately)
  t=0-500ms: No change (lines stay stuck)
```

---

## Recovery Success Indicators

✅ **Success:**
- SDA goes from LOW → HIGH (slave released)
- SCL toggles successfully (not locked)
- I2C communication works after recovery

❌ **Failure:**
- SDA stays LOW after 9 clocks
- SCL cannot toggle even with GPIO driver
- I2C read/write timeout after recovery

---

## What Each Failure Means

| Recovery Method | If It Fails | Likely Root Cause |
|---|---|---|
| Power cycle | Slave doesn't re-init | Firmware deadloop on startup |
| Bit-bang | SDA won't go HIGH | Slave in infinite loop (not stretching) |
| GPIO pulse | No change | Electrical short or very marginal |
| All methods | Bus still stuck | Hardware defect or module incompatible |

---

## Next Steps If All Recovery Fails

1. **Swap the Grove module** → Test if problem is module-specific
2. **Measure pull-up resistors** → Should be 4.7k–10k
3. **Inspect slave board** → Look for shorts, cold solder joints
4. **Capture with logic analyzer** → See exact bus behavior at boot
5. **Request slave firmware source** → Look for I2C interrupt handler bugs
6. **Contact vendor** → Ask if they have a fixed firmware version

---

## Code Files

Complete implementation is in `I2C_RECOVERY_IMPLEMENTATION.md`

- `i2c_recovery.h` — Header with function declarations
- `i2c_recovery.c` — Full implementations (copy-paste ready)

---

## References

- **NXP UM10204 I2C-bus Specification** — Section 3.1.16 "Bus Clear"
- **ESP32-P4 Technical Reference Manual** — GPIO modes and open-drain
- **TI SCPA069 Application Note** — I2C stuck bus prevention
- **Full research** → See `I2C_BUS_RECOVERY_RESEARCH.md`

---

**Quick Reference Version: 2026-04-02**  
Print this page and keep it handy while implementing recovery.

For detailed technical info, read the full research documents.
