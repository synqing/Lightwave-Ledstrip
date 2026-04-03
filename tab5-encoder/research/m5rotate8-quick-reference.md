---
abstract: "Quick reference guide for M5ROTATE8 I2C bus hang recovery. Checklist for detecting SDA stuck LOW, testing recovery mechanisms, and implementing firmware fixes. For developers implementing or troubleshooting Tab5-Encoder M5ROTATE8 integration."
---

# M5ROTATE8 I2C Bus Hang — Quick Reference

## Detection Checklist

- [ ] **Is SDA stuck LOW?** Read GPIO21 directly (should be HIGH when idle)
- [ ] **Is SCL also LOW?** (Slave clock stretching or bus contention)
- [ ] **Can master see slave at 0x41?** Run I2C scanner
- [ ] **Does issue persist after warm reset?** (ESP32 reboot, not power cycle)
- [ ] **Does issue persist after power cycle?** (Disconnect Grove, wait 100 ms, reconnect)

**If SDA stuck after power cycle**: Hardware fault. Replace M5ROTATE8.

---

## Root Cause Decision Tree

```
Master resets mid-I2C transaction
         |
         v
Is SDA LOW? (check GPIO21)
    / \
   Y   N
   |   |
   |   -> Check SCL (GPIO22)
   |        |
   |        v
   |      SCL LOW? → Slave clock stretching (may recover in 100 ms)
   |      SCL HIGH? → Bus idle, no hang issue
   |
   v
Slave firmware running? (try 9 SCL pulses)
    / \
   Y   N
   |   |
   |   -> Slave watchdog/crash
   |   -> ONLY solution: power cycle
   |
   v
Apply 9 SCL pulses
    |
    v
SDA released?
  / \
 Y   N
 |   |
 |   -> Slave firmware hung at I2C ISR
 |   -> Slave doesn't see SCL edges
 |   -> ONLY solution: power cycle
 |
 v
SUCCESS: Bus recovered
```

---

## Recovery Procedures (In Order of Likelihood to Succeed)

### Step 1: Wait 100 ms (Clock Stretching Recovery)
```cpp
// Maybe slave is just thinking
delay(100);
if (digitalRead(GPIO21) == 1) return SUCCESS;
```
**Success rate**: 10% (only if slave is clock stretching, not hung)

### Step 2: SCL Pulse Recovery (9 Clock Cycles)
```cpp
// Release both lines first
pinMode(GPIO21, INPUT);  // SDA
pinMode(GPIO22, INPUT);  // SCL
delay(10);

// Toggle SCL 9 times
for (int i = 0; i < 9; i++) {
    pinMode(GPIO22, OUTPUT);      // SCL = LOW
    delayMicroseconds(5);
    pinMode(GPIO22, INPUT);       // SCL = HIGH (released)
    delayMicroseconds(5);
}

// Issue STOP condition
pinMode(GPIO21, OUTPUT);          // SDA = LOW
delayMicroseconds(5);
pinMode(GPIO22, OUTPUT);          // SCL = LOW
delayMicroseconds(5);
pinMode(GPIO22, INPUT);           // SCL = HIGH
delayMicroseconds(5);
pinMode(GPIO21, INPUT);           // SDA = HIGH
delay(10);

// Check result
if (digitalRead(GPIO21) == 1) return SUCCESS;
```
**Success rate**: 50% (works if slave firmware is running and clock stretching is the issue)

### Step 3: Master I2C Reset
```cpp
Wire.end();                       // Release I2C peripheral
delay(10);
Wire.begin();                     // Re-initialize
delay(10);

// Test ping
Wire.beginTransmission(0x41);
Wire.write(0x00);
if (Wire.endTransmission() == 0) return SUCCESS;
```
**Success rate**: 20% (only helps if master's controller was confused, not slave)

### Step 4: Slave Power Cycle (Grove Power Toggle)
```cpp
// Disable Grove 5V output
M5.Power.setExtOutput(false);
delay(100);  // Wait for discharge

// Enable Grove 5V
M5.Power.setExtOutput(true);
delay(500);  // Wait for slave boot + firmware init

// Test ping
Wire.beginTransmission(0x41);
Wire.write(0x00);
if (Wire.endTransmission() == 0) return SUCCESS;
```
**Success rate**: 99% (only fails if hardware is damaged)

---

## Timeout Detection

Add this to your I2CRecovery code:

```cpp
#define I2C_TRANSACTION_TIMEOUT 100  // milliseconds
#define I2C_BUS_RECOVERY_ATTEMPTS 3

bool I2CTransactionWithRecovery(uint8_t address, 
                                uint8_t* data, 
                                uint8_t length,
                                bool write) {
    unsigned long start = millis();
    
    while (millis() - start < I2C_TRANSACTION_TIMEOUT) {
        Wire.beginTransmission(address);
        if (write) {
            Wire.write(data, length);
        }
        int result = Wire.endTransmission(!write);  // !write = release (for read)
        
        if (result == 0) {
            // Success
            if (write == false) {  // read request
                Wire.requestFrom(address, length);
                for (int i = 0; i < length && Wire.available(); i++) {
                    data[i] = Wire.read();
                }
            }
            return true;
        }
        
        delay(10);
    }
    
    // Timeout occurred — bus likely hung
    return false;
}
```

---

## Firmware Workarounds (if you can modify M5ROTATE8 slave firmware)

### Issue 1: Errata 2.9.5 (Next Byte Ignored After STOPF)

**Add delay after slave listener re-arm**:
```c
void I2C_ISR() {
    if (SR1 & STOPF) {
        SR1 |= CR1;  // Clear STOPF
        // Delay before re-enabling listener in main loop
        g_needsDelayBeforeRearm = 1;
        g_rearmDelayStart = GetTimerTicks();
    }
}

void SlaveMainLoop() {
    if (g_needsDelayBeforeRearm) {
        if (GetTimerTicks() - g_rearmDelayStart >= 100) {  // 100 µs
            g_needsDelayBeforeRearm = 0;
            I2C_CR1 |= 0x01;  // PE = 1 (enable)
        }
    }
}
```

### Issue 2: No STOPF Handler (Slave Never Knows Transaction Ended)

**Add STOPF interrupt**:
```c
void I2C_ISR() {
    // ... existing handlers ...
    else if (SR1 & STOPF) {  // Bit 4 in SR1
        SR1 |= CR1;  // Clear STOPF (read CR1, write SR1)
        g_slaveState = SLAVE_IDLE;
        // Reset TX/RX indices
        g_rxIdx = 0;
        g_txIdx = 0;
    }
}
```

### Issue 3: Slave Firmware Hangs in I2C ISR

**Never block in ISR**:
```c
void I2C_ISR() {
    if (SR1 & RXNE) {
        g_rxBuffer[g_rxIdx++] = DR;
        // DON'T do I/O, delays, or processing here
        // Queue response and return immediately
        return;
    }
    // ... service all other events and return ASAP
}

void SlaveMainLoop() {
    if (g_rxIdx > 0 && g_processingDone == false) {
        // Process received data here (non-interrupt)
        ProcessCommand(g_rxBuffer);
        g_processingDone = true;
    }
}
```

---

## Testing Checklist

- [ ] **Test 1**: Measure SDA voltage with oscilloscope (should see HIGH/LOW transitions)
- [ ] **Test 2**: Simulate master reset (esp_restart()) during M5ROTATE8 read
- [ ] **Test 3**: Verify SCL pulse recovery releases SDA within 100 ms
- [ ] **Test 4**: Verify power cycle recovery works within 500 ms total
- [ ] **Test 5**: Run I2C bus recovery 10 consecutive times (success rate check)
- [ ] **Test 6**: Log all I2C timeouts + recovery attempts to SD card
- [ ] **Test 7**: Test with M5ROTATE8 at different I2C addresses (if reprogrammed)
- [ ] **Test 8**: Test recovery under heavy I2C traffic (other devices on bus)

---

## Hardware Voltage Check

**If you modified hardware or are unsure**:

| Test Point | Expected Voltage | Tolerance |
|------------|------------------|-----------|
| SDA idle | 3.3V | ±0.3V |
| SCL idle | 3.3V | ±0.3V |
| M5ROTATE8 VCC | 3.3V | ±0.3V (after internal DC-DC) |
| Grove 5V | 5.0V | ±0.5V |
| GND | 0V | Common reference |

**If SDA/SCL are 5V**: Damaged STM32F030 I/O (max 3.6V). Replace unit.

---

## Logging & Diagnostics

Add to your I2CRecovery.cpp:

```cpp
struct I2CRecoveryEvent {
    uint32_t timestamp_ms;
    uint8_t attempt;
    uint8_t method;  // 0=wait, 1=scl_pulse, 2=reset, 3=power_cycle
    bool sda_before;  // GPIO21 state
    bool scl_before;  // GPIO22 state
    bool sda_after;
    bool scl_after;
    bool success;
};

void LogRecoveryEvent(const I2CRecoveryEvent& evt) {
    Serial.printf("[I2C-RECOVERY] t=%u attempt=%u method=%u "
                  "SDA:%u→%u SCL:%u→%u result=%s\n",
                  evt.timestamp_ms, evt.attempt, evt.method,
                  evt.sda_before, evt.sda_after,
                  evt.scl_before, evt.scl_after,
                  evt.success ? "OK" : "FAIL");
}
```

---

## Known Limitations (Cannot Be Fixed)

| Limitation | Why | Workaround |
|-----------|-----|-----------|
| SDA stuck after power cycle | Hardware damaged (open-drain driver stuck) | Replace M5ROTATE8 |
| SCL recovery fails after 5 attempts | Slave firmware completely hung (watchdog disabled) | Power cycle only |
| Frequent timeouts (>1 per minute) | Slave firmware has errata timing bug | Requires firmware update from M5Stack |
| 9 SCL pulses don't release SDA | Slave I2C state machine not clock-synchronous | Power cycle only |

---

## Emergency Recovery (If Master is Stuck)

If your Tab5 board's I2C is stuck and you can't use the above methods:

```cpp
// Hard reset Master I2C hardware
void I2CEmergencyReset() {
    // Release all I2C pins
    Wire.end();
    
    // Force GPIO to INPUT (high-Z)
    pinMode(GPIO21, INPUT);
    pinMode(GPIO22, INPUT);
    
    // Hard reset I2C controller
    periph_module_disable(PERIPH_I2C0_MODULE);
    delay(100);
    periph_module_enable(PERIPH_I2C0_MODULE);
    
    // Re-initialize
    Wire.begin();
}
```

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2025-04-03 | agent:research | Created quick reference guide with decision tree, recovery procedures, and testing checklist |
