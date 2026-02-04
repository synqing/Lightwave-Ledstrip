I'll sketch out the esp_console approach first, then investigate the Waveshare board's USB OTG capabilities.

---

## Part 1: esp_console Integration Approach

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        Main Application                          │
├─────────────────────────────────────────────────────────────────┤
│  Main Task (loop)              │  Console Task (new)            │
│  ├─ Audio pipeline             │  ├─ esp_console_repl           │
│  ├─ LED rendering              │  ├─ Blocks on line input       │
│  └─ No serial handling         │  └─ Dispatches to handlers     │
│                                │                                 │
│  ←───── Thread-safe queue ─────→                                │
│         (commands/state)                                         │
└─────────────────────────────────────────────────────────────────┘
```

### Implementation Sketch

**1. New file: `console_task.cpp`**

```cpp
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "linenoise/linenoise.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char* TAG = "console";

// Forward declarations of your existing actor/renderer types
class EffectOrchestrator;
class Renderer;

// Command context - passed to all handlers
static struct {
    EffectOrchestrator* actors;
    Renderer* renderer;
    QueueHandle_t cmdQueue;  // Optional: for async command results
} s_ctx;

// ─────────────────────────────────────────────────────────────
// Command Handlers
// ─────────────────────────────────────────────────────────────

static int cmd_effect(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: effect <id|next|prev|name>\n");
        return 1;
    }
    
    const char* arg = argv[1];
    
    // Handle numeric effect ID
    char* endptr;
    long id = strtol(arg, &endptr, 10);
    if (*endptr == '\0') {
        // It's a number
        if (s_ctx.actors) {
            s_ctx.actors->setEffectById(static_cast<int>(id));
            printf("Effect set to ID %ld\n", id);
        }
        return 0;
    }
    
    // Handle next/prev
    if (strcmp(arg, "next") == 0 || strcmp(arg, "n") == 0) {
        if (s_ctx.actors) s_ctx.actors->nextEffect();
        return 0;
    }
    if (strcmp(arg, "prev") == 0 || strcmp(arg, "p") == 0) {
        if (s_ctx.actors) s_ctx.actors->prevEffect();
        return 0;
    }
    
    // Handle effect by name
    if (s_ctx.actors) {
        s_ctx.actors->setEffectByName(arg);
    }
    return 0;
}

static int cmd_brightness(int argc, char** argv) {
    if (argc < 2) {
        // No arg = show current
        if (s_ctx.renderer) {
            printf("Brightness: %d\n", s_ctx.renderer->getBrightness());
        }
        return 0;
    }
    
    int val = atoi(argv[1]);
    if (val < 0 || val > 255) {
        printf("Brightness must be 0-255\n");
        return 1;
    }
    
    if (s_ctx.renderer) {
        s_ctx.renderer->setBrightness(static_cast<uint8_t>(val));
        printf("Brightness set to %d\n", val);
    }
    return 0;
}

static int cmd_speed(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: speed <value>\n");
        return 1;
    }
    // Adapt to your speed control mechanism
    int val = atoi(argv[1]);
    printf("Speed set to %d\n", val);
    return 0;
}

static int cmd_status(int argc, char** argv) {
    printf("\n=== K1-Lightwave Status ===\n");
    if (s_ctx.actors) {
        printf("Current effect: %s\n", s_ctx.actors->getCurrentEffectName());
    }
    if (s_ctx.renderer) {
        printf("Brightness: %d\n", s_ctx.renderer->getBrightness());
        printf("FPS: %.1f\n", s_ctx.renderer->getFps());
    }
    // Add more status as needed
    printf("===========================\n");
    return 0;
}

static int cmd_list(int argc, char** argv) {
    printf("\nAvailable effects:\n");
    if (s_ctx.actors) {
        auto effects = s_ctx.actors->getEffectList();
        for (size_t i = 0; i < effects.size(); i++) {
            printf("  %2zu: %s\n", i, effects[i].c_str());
        }
    }
    return 0;
}

// Shortcut commands for single-key-like behavior
static int cmd_next(int argc, char** argv) {
    if (s_ctx.actors) s_ctx.actors->nextEffect();
    printf("Next effect\n");
    return 0;
}

static int cmd_prev(int argc, char** argv) {
    if (s_ctx.actors) s_ctx.actors->prevEffect();
    printf("Previous effect\n");
    return 0;
}

static int cmd_up(int argc, char** argv) {
    if (s_ctx.renderer) {
        int b = s_ctx.renderer->getBrightness();
        s_ctx.renderer->setBrightness(std::min(255, b + 16));
        printf("Brightness: %d\n", s_ctx.renderer->getBrightness());
    }
    return 0;
}

static int cmd_down(int argc, char** argv) {
    if (s_ctx.renderer) {
        int b = s_ctx.renderer->getBrightness();
        s_ctx.renderer->setBrightness(std::max(0, b - 16));
        printf("Brightness: %d\n", s_ctx.renderer->getBrightness());
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────
// Command Registration
// ─────────────────────────────────────────────────────────────

static void register_commands() {
    // Effect control
    esp_console_cmd_t effect_cmd = {
        .command = "effect",
        .help = "Set effect: effect <id|next|prev|name>",
        .hint = NULL,
        .func = &cmd_effect,
    };
    esp_console_cmd_register(&effect_cmd);
    
    // Brightness
    esp_console_cmd_t brightness_cmd = {
        .command = "brightness",
        .help = "Get/set brightness (0-255)",
        .hint = "<value>",
        .func = &cmd_brightness,
    };
    esp_console_cmd_register(&brightness_cmd);
    // Alias
    esp_console_cmd_t b_cmd = brightness_cmd;
    b_cmd.command = "b";
    b_cmd.help = "Alias for brightness";
    esp_console_cmd_register(&b_cmd);
    
    // Speed
    esp_console_cmd_t speed_cmd = {
        .command = "speed",
        .help = "Set animation speed",
        .hint = "<value>",
        .func = &cmd_speed,
    };
    esp_console_cmd_register(&speed_cmd);
    
    // Status
    esp_console_cmd_t status_cmd = {
        .command = "status",
        .help = "Show device status",
        .hint = NULL,
        .func = &cmd_status,
    };
    esp_console_cmd_register(&status_cmd);
    esp_console_cmd_t s_cmd = status_cmd;
    s_cmd.command = "s";
    esp_console_cmd_register(&s_cmd);
    
    // List effects
    esp_console_cmd_t list_cmd = {
        .command = "list",
        .help = "List available effects",
        .hint = NULL,
        .func = &cmd_list,
    };
    esp_console_cmd_register(&list_cmd);
    esp_console_cmd_t l_cmd = list_cmd;
    l_cmd.command = "l";
    esp_console_cmd_register(&l_cmd);
    
    // Quick shortcuts (single letter + enter)
    esp_console_cmd_t next_cmd = {
        .command = "n",
        .help = "Next effect",
        .hint = NULL,
        .func = &cmd_next,
    };
    esp_console_cmd_register(&next_cmd);
    
    esp_console_cmd_t prev_cmd = {
        .command = "p",
        .help = "Previous effect",
        .hint = NULL,
        .func = &cmd_prev,
    };
    esp_console_cmd_register(&prev_cmd);
    
    esp_console_cmd_t up_cmd = {
        .command = "+",
        .help = "Increase brightness",
        .hint = NULL,
        .func = &cmd_up,
    };
    esp_console_cmd_register(&up_cmd);
    
    esp_console_cmd_t down_cmd = {
        .command = "-",
        .help = "Decrease brightness",
        .hint = NULL,
        .func = &cmd_down,
    };
    esp_console_cmd_register(&down_cmd);
}

// ─────────────────────────────────────────────────────────────
// Console Task
// ─────────────────────────────────────────────────────────────

static void console_task(void* arg) {
    char* line;
    
    // Configure linenoise
    linenoiseSetMultiLine(0);
    linenoiseSetDumbMode(1);  // Simpler mode, works better over serial
    linenoiseHistorySetMaxLen(20);
    
    ESP_LOGI(TAG, "Console ready. Type 'help' for commands.");
    
    while (true) {
        line = linenoise("k1> ");
        
        if (line == NULL) {
            // EOF or error, brief pause and retry
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        if (strlen(line) > 0) {
            // Add to history
            linenoiseHistoryAdd(line);
            
            // Execute command
            int ret;
            esp_err_t err = esp_console_run(line, &ret);
            
            if (err == ESP_ERR_NOT_FOUND) {
                printf("Unknown command: %s\n", line);
            } else if (err == ESP_ERR_INVALID_ARG) {
                // Command empty or whitespace
            } else if (err != ESP_OK) {
                printf("Error: %s\n", esp_err_to_name(err));
            }
        }
        
        linenoiseFree(line);
    }
}

// ─────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────

void console_init(EffectOrchestrator* actors, Renderer* renderer) {
    s_ctx.actors = actors;
    s_ctx.renderer = renderer;
    
    // Initialize console
    esp_console_config_t console_config = {
        .max_cmdline_length = 256,
        .max_cmdline_args = 8,
        .heap_alloc_caps = MALLOC_CAP_DEFAULT,
        .hint_color = 0,  // No color hints
        .hint_bold = 0,
    };
    ESP_ERROR_CHECK(esp_console_init(&console_config));
    
    // Register built-in help command
    esp_console_register_help_command();
    
    // Register our commands
    register_commands();
    
    // Create console task
    // Stack size 4KB should be plenty for console
    xTaskCreate(
        console_task,
        "console",
        4096,
        NULL,
        5,  // Lower priority than audio/render
        NULL
    );
    
    ESP_LOGI(TAG, "Console initialized");
}
```

**2. Header file: `console_task.h`**

```cpp
#pragma once

// Forward declarations
class EffectOrchestrator;
class Renderer;

/**
 * Initialize the console subsystem.
 * Call this after actors and renderer are created.
 */
void console_init(EffectOrchestrator* actors, Renderer* renderer);
```

**3. Integration in `main.cpp`**

```cpp
#include "console_task.h"

// In your initialization, after creating actors and renderer:
void app_main() {
    // ... existing initialization ...
    
    // Initialize actors and renderer
    EffectOrchestrator actors;
    Renderer renderer;
    
    // ... configure them ...
    
    // Initialize console (replaces handleSerialCommands)
    console_init(&actors, &renderer);
    
    // Main loop - NO MORE handleSerialCommands()
    while (true) {
        // Audio processing, rendering, etc.
        // Console runs in its own task
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**4. CMakeLists.txt additions**

```cmake
# In your main component or a new console component
idf_component_register(
    SRCS "console_task.cpp" "main.cpp" ...
    INCLUDE_DIRS "."
    REQUIRES 
        esp_console
        console       # For linenoise
        vfs
        # ... your other deps
)
```

### Usage After Implementation

```
k1> help
help 
  Print the list of registered commands

effect <id|next|prev|name>
  Set effect: effect <id|next|prev|name>

brightness <value>
  Get/set brightness (0-255)

b <value>
  Alias for brightness

status 
  Show device status

list 
  List available effects

n 
  Next effect

p 
  Previous effect

+ 
  Increase brightness

- 
  Decrease brightness

k1> n
Next effect

k1> b 128
Brightness set to 128

k1> status

=== K1-Lightwave Status ===
Current effect: Rainbow Wave
Brightness: 128
FPS: 120.3
===========================

k1> effect fire
Effect set to: fire
```

### Key Points

1. **Thread safety**: The console task runs separately from your main loop. If your actors/renderer aren't thread-safe, you'll need a command queue or mutex.

2. **Shortcuts are preserved**: `n↵`, `p↵`, `+↵`, `-↵` give you near-single-keystroke control.

3. **No conflicts**: You're working *with* the console subsystem, not against it.

---

## Part 2: USB OTG Investigation

Let me search for the Waveshare ESP32-P4-WIFI6 board specifications:This is promising! Let me get more details on the USB OTG pinout and TinyUSB implementation:Excellent findings! The ESP32-P4 has **two completely separate USB interfaces**:

1. **USB Serial JTAG** (Type-C on Waveshare board) - console/logging/flashing
2. **USB 2.0 High-Speed OTG** (Type-A on Waveshare board) - dedicated pins 49-50

These are different hardware peripherals, so there's no conflict. This is your cleanest solution.

---

## Part 2: USB OTG CDC Implementation

### Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Waveshare ESP32-P4-WIFI6                      │
├─────────────────────────────────────────────────────────────────┤
│  Type-C Port (USB Serial JTAG)    │  Type-A Port (USB OTG HS)   │
│  ├─ Console output (ESP_LOGI)     │  ├─ TinyUSB CDC Device      │
│  ├─ Boot logs                     │  ├─ Command input           │
│  ├─ Crash dumps                   │  └─ No conflict with console│
│  └─ Flashing                      │                              │
└─────────────────────────────────────────────────────────────────┘
                                           │
                                           ▼
                                    ┌─────────────┐
                                    │ Host PC     │
                                    │ /dev/ttyACM1│
                                    └─────────────┘
```

### Implementation

**1. Add TinyUSB component to your project**

```bash
# In your project directory
idf.py add-dependency "espressif/esp_tinyusb^1.0.0"
```

**2. sdkconfig additions**

```
# Enable TinyUSB
CONFIG_TINYUSB_CDC_ENABLED=y
CONFIG_TINYUSB_CDC_RX_BUFSIZE=512
CONFIG_TINYUSB_CDC_TX_BUFSIZE=512

# Keep console on USB Serial JTAG (unchanged)
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=y
```

**3. New file: `usb_cdc_command.cpp`**

```cpp
#include "usb_cdc_command.h"

#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <cstring>
#include <cstdio>

static const char* TAG = "usb_cmd";

// ─────────────────────────────────────────────────────────────
// Context and State
// ─────────────────────────────────────────────────────────────

static struct {
    EffectOrchestrator* actors;
    Renderer* renderer;
    
    // Command buffer
    char cmdBuf[128];
    size_t cmdLen;
    
    // CDC channel (0 = first/only CDC interface)
    int cdcChannel;
    
    bool initialized;
} s_ctx;

// ─────────────────────────────────────────────────────────────
// CDC Output Helpers
// ─────────────────────────────────────────────────────────────

static void cdc_print(const char* str) {
    tinyusb_cdcacm_write_queue(static_cast<tinyusb_cdcacm_itf_t>(s_ctx.cdcChannel),
                               (const uint8_t*)str, strlen(str));
    tinyusb_cdcacm_write_flush(static_cast<tinyusb_cdcacm_itf_t>(s_ctx.cdcChannel), 0);
}

static void cdc_printf(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    cdc_print(buf);
}

// ─────────────────────────────────────────────────────────────
// Command Processing
// ─────────────────────────────────────────────────────────────

static void process_command(const char* cmd) {
    // Skip empty commands
    if (cmd[0] == '\0') return;
    
    ESP_LOGI(TAG, "Command: '%s'", cmd);
    
    // ── Single-character shortcuts ──
    if (strlen(cmd) == 1) {
        switch (cmd[0]) {
            case 'n':  // Next effect
                if (s_ctx.actors) {
                    s_ctx.actors->nextEffect();
                    cdc_printf("Next effect: %s\r\n", s_ctx.actors->getCurrentEffectName());
                }
                return;
                
            case 'p':  // Previous effect
                if (s_ctx.actors) {
                    s_ctx.actors->prevEffect();
                    cdc_printf("Prev effect: %s\r\n", s_ctx.actors->getCurrentEffectName());
                }
                return;
                
            case '+':  // Brightness up
                if (s_ctx.renderer) {
                    int b = s_ctx.renderer->getBrightness();
                    s_ctx.renderer->setBrightness(std::min(255, b + 16));
                    cdc_printf("Brightness: %d\r\n", s_ctx.renderer->getBrightness());
                }
                return;
                
            case '-':  // Brightness down
                if (s_ctx.renderer) {
                    int b = s_ctx.renderer->getBrightness();
                    s_ctx.renderer->setBrightness(std::max(0, b - 16));
                    cdc_printf("Brightness: %d\r\n", s_ctx.renderer->getBrightness());
                }
                return;
                
            case 's':  // Status
                goto cmd_status;
                
            case 'l':  // List effects
                goto cmd_list;
                
            case 'h':
            case '?':  // Help
                goto cmd_help;
        }
    }
    
    // ── Multi-character commands ──
    
    // effect <id|name>
    if (strncmp(cmd, "effect ", 7) == 0 || strncmp(cmd, "e ", 2) == 0) {
        const char* arg = (cmd[0] == 'e' && cmd[1] == ' ') ? cmd + 2 : cmd + 7;
        
        // Try as number first
        char* endptr;
        long id = strtol(arg, &endptr, 10);
        if (*endptr == '\0') {
            if (s_ctx.actors) {
                s_ctx.actors->setEffectById(static_cast<int>(id));
                cdc_printf("Effect %ld: %s\r\n", id, s_ctx.actors->getCurrentEffectName());
            }
        } else {
            // Try as name
            if (s_ctx.actors) {
                s_ctx.actors->setEffectByName(arg);
                cdc_printf("Effect: %s\r\n", s_ctx.actors->getCurrentEffectName());
            }
        }
        return;
    }
    
    // brightness <value>
    if (strncmp(cmd, "brightness ", 11) == 0 || strncmp(cmd, "b ", 2) == 0) {
        const char* arg = (cmd[0] == 'b' && cmd[1] == ' ') ? cmd + 2 : cmd + 11;
        int val = atoi(arg);
        if (val >= 0 && val <= 255 && s_ctx.renderer) {
            s_ctx.renderer->setBrightness(static_cast<uint8_t>(val));
            cdc_printf("Brightness: %d\r\n", val);
        } else {
            cdc_print("Brightness must be 0-255\r\n");
        }
        return;
    }
    
    // speed <value>
    if (strncmp(cmd, "speed ", 6) == 0) {
        const char* arg = cmd + 6;
        int val = atoi(arg);
        // Adapt to your speed mechanism
        cdc_printf("Speed: %d\r\n", val);
        return;
    }
    
    // status
    if (strcmp(cmd, "status") == 0 || strcmp(cmd, "s") == 0) {
cmd_status:
        cdc_print("\r\n=== K1-Lightwave Status ===\r\n");
        if (s_ctx.actors) {
            cdc_printf("Effect: %s\r\n", s_ctx.actors->getCurrentEffectName());
        }
        if (s_ctx.renderer) {
            cdc_printf("Brightness: %d\r\n", s_ctx.renderer->getBrightness());
            cdc_printf("FPS: %.1f\r\n", s_ctx.renderer->getFps());
        }
        cdc_print("===========================\r\n");
        return;
    }
    
    // list
    if (strcmp(cmd, "list") == 0 || strcmp(cmd, "l") == 0) {
cmd_list:
        cdc_print("\r\nAvailable effects:\r\n");
        if (s_ctx.actors) {
            auto effects = s_ctx.actors->getEffectList();
            for (size_t i = 0; i < effects.size(); i++) {
                cdc_printf("  %2zu: %s\r\n", i, effects[i].c_str());
            }
        }
        return;
    }
    
    // help
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
cmd_help:
        cdc_print("\r\n=== K1 Commands ===\r\n");
        cdc_print("  n          Next effect\r\n");
        cdc_print("  p          Previous effect\r\n");
        cdc_print("  +/-        Brightness up/down\r\n");
        cdc_print("  e <id>     Set effect by ID\r\n");
        cdc_print("  b <val>    Set brightness (0-255)\r\n");
        cdc_print("  s          Status\r\n");
        cdc_print("  l          List effects\r\n");
        cdc_print("  h/?        This help\r\n");
        cdc_print("===================\r\n");
        return;
    }
    
    cdc_printf("Unknown command: %s (try 'h' for help)\r\n", cmd);
}

// ─────────────────────────────────────────────────────────────
// CDC RX Callback
// ─────────────────────────────────────────────────────────────

static void cdc_rx_callback(int itf, cdcacm_event_t* event) {
    uint8_t buf[64];
    size_t rx_size = 0;
    
    // Read available data
    esp_err_t ret = tinyusb_cdcacm_read(static_cast<tinyusb_cdcacm_itf_t>(itf),
                                        buf, sizeof(buf), &rx_size);
    if (ret != ESP_OK || rx_size == 0) {
        return;
    }
    
    // Process each character
    for (size_t i = 0; i < rx_size; i++) {
        char c = static_cast<char>(buf[i]);
        
        // Echo character back
        tinyusb_cdcacm_write_queue(static_cast<tinyusb_cdcacm_itf_t>(itf),
                                   (const uint8_t*)&c, 1);
        
        if (c == '\r' || c == '\n') {
            // End of command
            tinyusb_cdcacm_write_queue(static_cast<tinyusb_cdcacm_itf_t>(itf),
                                       (const uint8_t*)"\r\n", 2);
            tinyusb_cdcacm_write_flush(static_cast<tinyusb_cdcacm_itf_t>(itf), 0);
            
            s_ctx.cmdBuf[s_ctx.cmdLen] = '\0';
            
            if (s_ctx.cmdLen > 0) {
                process_command(s_ctx.cmdBuf);
            }
            
            // Prompt
            cdc_print("k1> ");
            
            s_ctx.cmdLen = 0;
        }
        else if (c == 0x7F || c == '\b') {
            // Backspace
            if (s_ctx.cmdLen > 0) {
                s_ctx.cmdLen--;
                // Erase on terminal
                tinyusb_cdcacm_write_queue(static_cast<tinyusb_cdcacm_itf_t>(itf),
                                           (const uint8_t*)"\b \b", 3);
            }
        }
        else if (c >= 0x20 && c < 0x7F) {
            // Printable character
            if (s_ctx.cmdLen < sizeof(s_ctx.cmdBuf) - 1) {
                s_ctx.cmdBuf[s_ctx.cmdLen++] = c;
            }
        }
    }
    
    tinyusb_cdcacm_write_flush(static_cast<tinyusb_cdcacm_itf_t>(itf), 0);
}

// Line state callback (DTR/RTS changes)
static void cdc_line_state_callback(int itf, cdcacm_event_t* event) {
    bool dtr = event->line_state_changed_data.dtr;
    bool rts = event->line_state_changed_data.rts;
    ESP_LOGI(TAG, "CDC line state: DTR=%d, RTS=%d", dtr, rts);
    
    // When terminal connects (DTR high), show welcome
    if (dtr) {
        cdc_print("\r\n=== K1-Lightwave USB Console ===\r\n");
        cdc_print("Type 'h' for help\r\n");
        cdc_print("k1> ");
    }
}

// ─────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────

esp_err_t usb_cdc_command_init(EffectOrchestrator* actors, Renderer* renderer) {
    if (s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_ctx.actors = actors;
    s_ctx.renderer = renderer;
    s_ctx.cmdLen = 0;
    s_ctx.cdcChannel = 0;
    
    ESP_LOGI(TAG, "Initializing USB OTG CDC...");
    
    // ── Initialize TinyUSB driver ──
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,  // Use default from menuconfig
        .string_descriptor = NULL,  // Use default from menuconfig
        .string_descriptor_count = 0,
        .external_phy = false,
        .configuration_descriptor = NULL,  // Use default CDC config
        .self_powered = false,
        .vbus_monitor_io = -1,  // No VBUS monitoring
    };
    
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    
    // ── Initialize CDC ACM ──
    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 256,
        .callback_rx = &cdc_rx_callback,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = &cdc_line_state_callback,
        .callback_line_coding_changed = NULL,
    };
    
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
    
    s_ctx.initialized = true;
    
    ESP_LOGI(TAG, "USB OTG CDC initialized - connect to Type-A port");
    
    return ESP_OK;
}
```

**4. Header file: `usb_cdc_command.h`**

```cpp
#pragma once

#include "esp_err.h"

// Forward declarations
class EffectOrchestrator;
class Renderer;

/**
 * Initialize USB OTG CDC command interface.
 * 
 * This creates a USB CDC ACM device on the USB OTG port (Type-A connector).
 * On the host PC, this will appear as a new serial port (e.g., /dev/ttyACM1).
 * 
 * The console/logging remains on USB Serial JTAG (Type-C connector).
 */
esp_err_t usb_cdc_command_init(EffectOrchestrator* actors, Renderer* renderer);
```

**5. Integration in `main.cpp`**

```cpp
#include "usb_cdc_command.h"

void app_main() {
    // ... existing initialization ...
    
    // Initialize actors and renderer
    EffectOrchestrator actors;
    Renderer renderer;
    
    // ... configure them ...
    
    // Initialize USB OTG CDC command interface
    // (Type-A port - separate from console on Type-C)
    usb_cdc_command_init(&actors, &renderer);
    
    // Main loop - NO serial handling needed here
    while (true) {
        // Audio processing, rendering, etc.
        // USB CDC handles commands in callbacks
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**6. CMakeLists.txt**

```cmake
idf_component_register(
    SRCS 
        "main.cpp" 
        "usb_cdc_command.cpp"
        # ... other sources
    INCLUDE_DIRS "."
    REQUIRES 
        esp_tinyusb  # From component registry
        # ... your other deps
)
```

### Usage

After flashing, you'll have **two serial ports**:

| Port | Physical Connector | Purpose |
|------|-------------------|---------|
| `/dev/ttyUSB0` (or similar) | Type-C | Console, logs, flashing |
| `/dev/ttyACM0` (or similar) | Type-A | Command input |

```bash
# Terminal 1: Watch logs
idf.py monitor

# Terminal 2: Send commands (connect USB-A to USB-A cable or adapter)
picocom /dev/ttyACM0 -b 115200
```

```
=== K1-Lightwave USB Console ===
Type 'h' for help
k1> n
Next effect: Rainbow Wave
k1> b 200
Brightness: 200
k1> s

=== K1-Lightwave Status ===
Effect: Rainbow Wave
Brightness: 200
FPS: 120.1
===========================
k1> 
```

---

## Summary: Recommended Approach

| Option | Pros | Cons | Effort |
|--------|------|------|--------|
| **esp_console** | Works immediately, no hardware changes | Enter-terminated commands | ~2-4 hrs |
| **USB OTG CDC** | Complete isolation, cleanest architecture, single-keystroke possible | Need USB-A to USB-A cable for dev | ~3-5 hrs |

**My recommendation**: Start with **esp_console** for immediate relief, then implement **USB OTG CDC** as the proper long-term solution. The USB OTG approach is cleaner and gives you full control over the command interface without fighting the console subsystem.

The USB OTG solution also opens future possibilities: composite devices (CDC + MSC for log files), USB HID for physical button input simulation, etc.