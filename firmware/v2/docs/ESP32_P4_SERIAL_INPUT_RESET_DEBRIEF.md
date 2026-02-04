# ESP32-P4 Serial Input Reset Issue - Technical Debrief

**Date:** February 2025  
**Platform:** ESP32-P4 (Waveshare ESP32-P4-WIFI6)  
**ESP-IDF Version:** v5.5.2  
**Status:** UNRESOLVED - All attempted solutions have failed

---

## 1. Problem Statement

### 1.1 Observed Behavior
When any keyboard character is sent to the ESP32-P4 via the USB Serial JTAG interface, the device **immediately resets**. This occurs:
- On any single keystroke (letters, numbers, special characters)
- Regardless of what the firmware does with the input
- Before any command processing logic executes

### 1.2 Expected Behavior
The serial command handler (`handleSerialCommands()` in `main.cpp`) should:
1. Detect available characters via `Serial.available()`
2. Read characters via `Serial.read()`
3. Process commands (effect changes, brightness, status display, etc.)
4. NOT reset or crash

### 1.3 Impact
- Cannot change LED effects at runtime
- Cannot adjust brightness/speed/palette
- Cannot query device status
- Device is effectively read-only after boot
- Only way to change behavior is recompile and reflash

---

## 2. System Architecture

### 2.1 Hardware Configuration
```
ESP32-P4 (Waveshare ESP32-P4-WIFI6)
├── USB-C Port → USB Serial JTAG Controller (built into SoC)
│   ├── CDC-ACM serial interface (for stdio/console)
│   └── JTAG interface (for debugging)
├── UART0 (internal, directly wired to USB Serial JTAG)
└── No external USB-UART bridge chip
```

### 2.2 Console Configuration (sdkconfig)
```
CONFIG_ESP_CONSOLE_UART_DEFAULT=y           # Primary console on UART
CONFIG_ESP_CONSOLE_UART_NUM=0               # UART0
CONFIG_ESP_CONSOLE_UART_BAUDRATE=115200
CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=y  # Secondary on USB Serial JTAG
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED=y    # USB Serial JTAG enabled
CONFIG_USJ_ENABLE_USB_SERIAL_JTAG=y             # USJ hardware enabled
```

**Critical Point:** The ESP-IDF console subsystem owns and manages the USB Serial JTAG peripheral. It registers a VFS (Virtual File System) driver that handles stdin/stdout via the USB Serial JTAG hardware.

### 2.3 Software Stack
```
Application Layer
├── main.cpp::handleSerialCommands()
│   └── Calls Serial.available() and Serial.read()
│
Arduino Compatibility Layer (Arduino.h)
├── HardwareSerial class
│   ├── available() - Check for input
│   └── read() - Read character
│
ESP-IDF Layers (CONFLICT ZONE)
├── VFS Layer (esp_vfs)
│   └── USB Serial JTAG VFS driver
│       └── Maps /dev/usbserjtag to stdin/stdout
├── USB Serial JTAG Driver (esp_driver_usb_serial_jtag)
│   └── Ring buffers, interrupts, FreeRTOS integration
└── USB Serial JTAG HAL (Low-Level)
    └── usb_serial_jtag_ll_* functions
        └── Direct hardware register access
```

### 2.4 Main Loop Structure
```cpp
// main.cpp lines 493-496
while (true) {
    handleSerialCommands(actors, renderer);  // <-- CRASH HAPPENS HERE
    vTaskDelay(pdMS_TO_TICKS(10));
}

// handleSerialCommands() lines 66-98
while (Serial.available()) {      // <-- OR HERE
    char c = Serial.read();       // <-- OR HERE
    // ... command processing
}
```

---

## 3. Attempted Solutions and Failure Analysis

### 3.1 Attempt #1: stdio fgetc()/ungetc() Pattern

**Implementation:**
```cpp
int available() {
    int c = std::fgetc(stdin);
    if (c != EOF) {
        std::ungetc(c, stdin);  // Push back for later read()
        return 1;
    }
    std::clearerr(stdin);
    return 0;
}

int read() {
    int c = std::fgetc(stdin);
    if (c == EOF) {
        std::clearerr(stdin);
        return -1;
    }
    return c;
}
```

**Also included:**
```cpp
void begin(unsigned long) {
    setvbuf(stdin, NULL, _IONBF, 0);  // Disable buffering
    int flags = fcntl(fileno(stdin), F_GETFL, 0);
    fcntl(fileno(stdin), F_SETFL, flags | O_NONBLOCK);  // Non-blocking
}
```

**Result:** DEVICE RESETS ON ANY INPUT

**Analysis - Why It Failed:**

1. **ungetc() Buffer Limitation:** The C standard only guarantees ONE character of pushback. On ESP-IDF's VFS layer for USB Serial JTAG, this may not be properly implemented or may conflict with internal buffering.

2. **VFS Layer Complexity:** The call path is:
   ```
   fgetc(stdin) 
   → VFS lookup for stdin fd
   → usb_serial_jtag_vfs.c::usb_serial_jtag_read()
   → Internal locking (_lock_acquire_recursive)
   → usb_serial_jtag_read_char()
   → rx_func() pointer (either driver or no-driver mode)
   ```
   
3. **Potential Race Condition:** The VFS layer has internal peek buffering (`s_ctx.peek_char`). Our `ungetc()` may conflict with this, causing state corruption.

4. **Non-blocking Mode Issues:** Setting `O_NONBLOCK` via `fcntl()` on the VFS-managed stdin may not propagate correctly to the USB Serial JTAG driver.

---

### 3.2 Attempt #2: select() with Zero Timeout

**Implementation:**
```cpp
int available() {
    fd_set readfds;
    struct timeval tv = {0, 0};  // Zero timeout = poll
    
    FD_ZERO(&readfds);
    FD_SET(fileno(stdin), &readfds);
    
    int result = select(fileno(stdin) + 1, &readfds, nullptr, nullptr, &tv);
    return (result > 0 && FD_ISSET(fileno(stdin), &readfds)) ? 1 : 0;
}
```

**Result:** DEVICE RESETS ON ANY INPUT

**Analysis - Why It Failed:**

1. **select() Not Supported:** The ESP-IDF USB Serial JTAG VFS driver explicitly states in its source code (line 7-8 of `usb_serial_jtag_vfs.c`):
   > "This is a simple non-blocking (well, tx may spin for a bit if the buffer is full) USB-serial-jtag driver. **Select etc is not supported yet.**"

2. **VFS select() Registration:** For `select()` to work, the VFS driver must implement and register `start_select` and `end_select` callbacks. The USB Serial JTAG VFS has these but they may not function correctly for the secondary console configuration.

3. **File Descriptor Mapping:** `fileno(stdin)` returns an fd that maps through the VFS layer. The select() call goes to the VFS dispatcher which then calls the driver's select implementation - which doesn't work.

---

### 3.3 Attempt #3: USB Serial JTAG Driver API

**Implementation:**
```cpp
void begin(unsigned long) {
    usb_serial_jtag_driver_config_t cfg = {
        .tx_buffer_size = 256,
        .rx_buffer_size = 256,
    };
    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    // Accepted ESP_ERR_INVALID_STATE as "already installed"
}

int available() {
    // Buffer management for multi-byte reads
    if (m_rxBufHead != m_rxBufTail) return 1;
    
    int len = usb_serial_jtag_read_bytes(m_rxBuf, sizeof(m_rxBuf), 0);
    if (len > 0) {
        m_rxBufHead = 0;
        m_rxBufTail = len;
        return 1;
    }
    return 0;
}

int read() {
    // Return from buffer or read more
    if (m_rxBufHead < m_rxBufTail) {
        return m_rxBuf[m_rxBufHead++];
    }
    int len = usb_serial_jtag_read_bytes(m_rxBuf, sizeof(m_rxBuf), 0);
    // ...
}
```

**Result:** DEVICE RESETS ON ANY INPUT

**Analysis - Why It Failed:**

1. **Driver Already Installed:** The console subsystem has already installed the USB Serial JTAG driver during ESP-IDF startup. When we call `usb_serial_jtag_driver_install()`, it returns `ESP_ERR_INVALID_STATE`.

2. **We Ignored the Error:** We treated `ESP_ERR_INVALID_STATE` as acceptable ("already installed"). But this doesn't give us access to the driver's internal ring buffers.

3. **Buffer Ownership Conflict:** The console's VFS layer owns the RX ring buffer. When we call `usb_serial_jtag_read_bytes()`, we're either:
   - Reading from a buffer we didn't create (undefined behavior)
   - Conflicting with the console's reads (data corruption)
   - Triggering an assertion or null pointer access (crash)

4. **Interrupt Handler Conflict:** The driver uses interrupt-driven RX. There's only one interrupt handler registered. We can't have two consumers of the same interrupt.

---

### 3.4 Attempt #4: Low-Level HAL API (usb_serial_jtag_ll_*)

**Implementation:**
```cpp
int available() {
    return usb_serial_jtag_ll_rxfifo_data_available() ? 1 : 0;
}

int read() {
    uint8_t c;
    int len = usb_serial_jtag_ll_read_rxfifo(&c, 1);
    return (len > 0) ? static_cast<int>(c) : -1;
}
```

**Result:** DEVICE RESETS ON ANY INPUT

**Analysis - Why It Failed:**

1. **Shared Hardware Resource:** The USB Serial JTAG has a single hardware RX FIFO. The LL (low-level) functions read directly from this FIFO.

2. **Console VFS Also Reads FIFO:** The console's VFS driver (`usb_serial_jtag_vfs.c`) uses `usb_serial_jtag_ll_read_rxfifo()` internally (line 176):
   ```c
   static int usb_serial_jtag_rx_char_no_driver(int fd) {
       uint8_t c;
       int l = usb_serial_jtag_ll_read_rxfifo(&c, 1);
       if (l == 0) return NONE;
       return c;
   }
   ```

3. **Race Condition:** When a character arrives in the FIFO:
   - Our code calls `usb_serial_jtag_ll_rxfifo_data_available()` → returns true
   - Console interrupt fires, reads the byte from FIFO
   - Our code calls `usb_serial_jtag_ll_read_rxfifo()` → reads invalid data or triggers hardware fault
   
4. **No Synchronization:** The LL API has no locking. We're racing against the console's interrupt handler without any synchronization.

5. **Possible Watchdog:** If the LL read causes a hardware fault or infinite loop, the task watchdog triggers a reset.

---

## 4. Root Cause Analysis

### 4.1 Fundamental Architectural Conflict

The ESP32-P4's USB Serial JTAG peripheral is a **single hardware resource** that is being managed by **multiple software layers**:

```
┌─────────────────────────────────────────────────────────────┐
│                    USB Serial JTAG Hardware                  │
│  ┌─────────────────┐    ┌─────────────────┐                 │
│  │   TX FIFO       │    │   RX FIFO       │  ← SINGLE FIFO  │
│  └─────────────────┘    └─────────────────┘                 │
└─────────────────────────────────────────────────────────────┘
                              ↑
         ┌────────────────────┴────────────────────┐
         │                                          │
    ESP-IDF Console                          Our HardwareSerial
    ┌─────────────────┐                      ┌─────────────────┐
    │ VFS Driver      │                      │ Arduino Shim    │
    │ Interrupt Handler│                      │ Direct Access   │
    │ Ring Buffers    │                      │ No Buffers      │
    │ Locking         │                      │ No Locking      │
    └─────────────────┘                      └─────────────────┘
              │                                       │
              └───────────── CONFLICT ───────────────┘
```

### 4.2 The Console's Ownership

When ESP-IDF boots with `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED=y`:

1. **Early Initialization:** The USB Serial JTAG VFS driver is registered during system startup
2. **File Descriptor Binding:** stdin (fd 0) is mapped to `/dev/usbserjtag`
3. **Interrupt Registration:** An ISR is registered for USB Serial JTAG RX events
4. **printf/logging:** All ESP_LOG* macros and printf() go through this driver

### 4.3 Why Every Approach Failed

| Approach | Failure Mode |
|----------|--------------|
| stdio fgetc/ungetc | VFS layer conflict, ungetc buffer corruption |
| select() | Not implemented in USB Serial JTAG VFS |
| Driver API | Driver already installed by console, buffer ownership conflict |
| LL HAL API | Race condition with console ISR, no synchronization |

### 4.4 The Reset Mechanism

The actual reset is likely triggered by:

1. **Watchdog Timeout:** Task gets stuck in infinite loop or blocking call
2. **Assertion Failure:** Internal ESP-IDF assertion fails (e.g., null pointer, buffer overflow)
3. **Hardware Fault:** Invalid memory access, bus error, or similar
4. **Stack Overflow:** Recursive call or large stack allocation in interrupt context

Without crash dumps (which are output via the same serial port that's crashing), we cannot definitively identify which.

---

## 5. Evidence Supporting Root Cause

### 5.1 The Device Works Fine Without Input
- Audio pipeline runs at 99+ Hz with 100% success rate
- LED rendering at 120 FPS
- All FreeRTOS tasks operating normally
- Zero issues until a keystroke is received

### 5.2 Output Works, Input Doesn't
- `Serial.print()`, `Serial.printf()`, `ESP_LOGI()` all work fine
- `Serial.available()` and `Serial.read()` cause reset
- This is consistent with console owning TX path (output) while conflicting on RX path (input)

### 5.3 ESP-IDF Source Code Confirms Architecture
From `usb_serial_jtag_vfs.c`:
```c
// Line 7-8: "Select etc is not supported yet."
// Line 113-119: Static global context with peek buffer
// Line 173-180: Direct LL FIFO read in no-driver mode
// Line 231-247: Blocking vs non-blocking read handling
```

---

## 6. Potential Solutions (Not Yet Attempted)

### 6.1 Disable Console on USB Serial JTAG

**Approach:** Set `CONFIG_ESP_CONSOLE_NONE=y` or `CONFIG_ESP_CONSOLE_UART_CUSTOM=y` with different pins

**Pros:**
- Eliminates the conflict entirely
- We would own the USB Serial JTAG exclusively

**Cons:**
- Lose all boot logs and ESP_LOG output
- Lose crash dumps
- Debugging becomes extremely difficult

**Implementation:**
```
# In sdkconfig or menuconfig
CONFIG_ESP_CONSOLE_NONE=y
# OR
CONFIG_ESP_CONSOLE_UART_CUSTOM=y
CONFIG_ESP_CONSOLE_UART_TX_GPIO=XX
CONFIG_ESP_CONSOLE_UART_RX_GPIO=XX
```

### 6.2 Use Console's Input Mechanism

**Approach:** Instead of bypassing the console, work WITH it using `esp_console` APIs

**Implementation:**
```c
#include "esp_console.h"
#include "linenoise/linenoise.h"

// Register commands
esp_console_cmd_t cmd = {
    .command = "effect",
    .help = "Set effect by ID",
    .func = &effect_cmd_handler,
};
esp_console_cmd_register(&cmd);

// Use linenoise for input
char* line = linenoise(">");
esp_console_run(line, &ret);
```

**Pros:**
- Works with console architecture, not against it
- Line editing, history support
- Proper synchronization

**Cons:**
- Requires significant refactoring of command handling
- Blocking input model (need dedicated task)
- May not support single-keystroke commands

### 6.3 Dedicated UART for Commands

**Approach:** Use a separate UART peripheral with external USB-UART adapter

**Implementation:**
```cpp
// Use UART1 or UART2 with external pins
uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    // ...
};
uart_driver_install(UART_NUM_1, 256, 0, 0, NULL, 0);
uart_param_config(UART_NUM_1, &uart_config);
uart_set_pin(UART_NUM_1, TX_PIN, RX_PIN, -1, -1);

// Read from UART1 instead
uart_read_bytes(UART_NUM_1, buf, len, timeout);
```

**Pros:**
- Complete isolation from console
- No conflicts whatsoever
- Full control over the peripheral

**Cons:**
- Requires additional hardware (USB-UART adapter)
- Additional wiring
- Two serial ports to manage

### 6.4 Investigate VFS Non-Blocking Mode

**Approach:** Properly configure the VFS layer for non-blocking reads

**Implementation:**
```c
// Open with O_NONBLOCK
int fd = open("/dev/usbserjtag", O_RDONLY | O_NONBLOCK);

// Read with proper error handling
char c;
int n = read(fd, &c, 1);
if (n < 0 && errno == EAGAIN) {
    // No data available, this is fine
}
```

**Investigation Needed:**
- Does opening a second fd to the same VFS device work?
- Does O_NONBLOCK actually propagate to the driver?
- Can we read without conflicting with stdin?

### 6.5 Patch ESP-IDF USB Serial JTAG VFS

**Approach:** Modify ESP-IDF source to add proper non-blocking support or dual-consumer support

**This is a last resort and requires:**
- Deep understanding of VFS internals
- Maintaining a forked ESP-IDF
- Potential stability issues

---

## 7. Recommended Next Steps

### Immediate (Diagnostic):
1. **Add crash handler** to capture reset reason before it's lost
2. **Enable core dump to flash** to capture crash state
3. **Try CONFIG_ESP_CONSOLE_NONE** to confirm conflict theory

### Short-term (Workaround):
4. **Implement esp_console integration** for command handling
5. **Or use separate UART** with external adapter

### Long-term (Proper Fix):
6. **File ESP-IDF issue** with Espressif documenting this limitation
7. **Request dual-consumer support** or better non-blocking API

---

## 8. Files Involved

| File | Purpose |
|------|---------|
| `esp-idf-p4/components/arduino-compat/include/Arduino.h` | HardwareSerial class implementation |
| `esp-idf-p4/components/arduino-compat/CMakeLists.txt` | Component dependencies |
| `esp-idf-p4/main/main.cpp` | handleSerialCommands() function |
| `esp-idf-p4/sdkconfig` | Console and USB configuration |
| ESP-IDF `usb_serial_jtag_vfs.c` | VFS driver (read-only reference) |
| ESP-IDF `usb_serial_jtag.c` | High-level driver (read-only reference) |
| ESP-IDF `usb_serial_jtag_ll.h` | Low-level HAL (read-only reference) |

---

## 9. Conclusion

The ESP32-P4 USB Serial JTAG input reset issue is caused by a **fundamental architectural conflict** between the ESP-IDF console subsystem and our application's attempt to read serial input. The console "owns" the USB Serial JTAG peripheral and does not provide a safe, supported way for application code to read input without conflicting with its internal state management.

All four attempted solutions (stdio, select, driver API, LL HAL) failed because they all ultimately try to read from the same hardware RX FIFO that the console is also reading from, leading to race conditions, state corruption, or unsupported operations.

**The only viable solutions are:**
1. Disable the console on USB Serial JTAG entirely
2. Work within the console framework using esp_console APIs
3. Use a separate physical UART with external USB adapter

This is a significant limitation of the ESP32-P4 platform when using the built-in USB Serial JTAG for both logging output and interactive input.

---

## Appendix A: Current HardwareSerial Implementation (Latest Attempt)

```cpp
// Arduino.h lines 134-192
class HardwareSerial {
private:
    bool m_initialized = false;
    
public:
    void begin(unsigned long) {
        m_initialized = true;
    }
    void end() {}
    
    int available() {
        return usb_serial_jtag_ll_rxfifo_data_available() ? 1 : 0;
    }
    
    int read() {
        uint8_t c;
        int len = usb_serial_jtag_ll_read_rxfifo(&c, 1);
        return (len > 0) ? static_cast<int>(c) : -1;
    }
    
    // ... write/print methods use stdout (works fine)
};
```

## Appendix B: sdkconfig Console Settings

```
CONFIG_SOC_USB_SERIAL_JTAG_SUPPORTED=y
CONFIG_USJ_ENABLE_USB_SERIAL_JTAG=y
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=y
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED=y
CONFIG_ESP_CONSOLE_UART=y
CONFIG_ESP_CONSOLE_UART_NUM=0
CONFIG_ESP_CONSOLE_UART_BAUDRATE=115200
```

## Appendix C: Command Handler Entry Point

```cpp
// main.cpp lines 66-98
while (Serial.available()) {      // <-- RESET TRIGGERED HERE
    char c = Serial.read();       // <-- OR HERE
    
    if (serialCmdLen == 0) {
        bool isImmediate = false;
        switch (c) {
            case ' ':
            case '+': case '=':
            case '-': case '_':
            case '[': case ']':
            case ',': case '.':
                isImmediate = true;
                break;
        }
        // ... rest of command handling never reached
    }
}
```
