# Serial Interface Engineer Agent

> Specialist for SerialMenu command interface, telemetry output, zone commands, configuration persistence, and user interaction patterns in LightwaveOS.

---

## Overview

The Serial Interface is the primary debug and control interface for LightwaveOS. It provides:
- **Interactive REPL** for real-time parameter tuning
- **Zone Composer control** (`zone on/off`, `zone preset <n>`)
- **Performance telemetry** (FPS, CPU%, timing breakdown)
- **Configuration persistence** via NVS/Preferences
- **ANSI terminal support** for clear screen and formatting

**Key Files:**
- `src/utils/SerialMenu.h` - Core menu system (566 LOC)
- `src/main.cpp` - Command dispatch (lines 1100-1400)
- `src/hardware/PerformanceMonitor.h` - Telemetry data (340 LOC)

---

## Architecture

### Command Processing Flow

```
Serial.available()
        │
        ▼
    readStringUntil('\n')
        │
        ▼
    cmd.trim()
        │
        ▼
┌───────┴───────┐
│   Dispatch    │
├───────────────┤
│ zone *        │ → main.cpp zone handlers
│ effect *      │ → SerialMenu::setEffect()
│ palette *     │ → SerialMenu::setPalette()
│ perf *        │ → PerformanceMonitor
│ menu commands │ → SerialMenu::show*()
└───────────────┘
```

### SerialMenu Class Structure

```cpp
// src/utils/SerialMenu.h
class SerialMenu {
public:
    void begin(unsigned long baudRate = 115200);
    void processCommand(const String& command);

    // Menu display functions
    void showHelp();           // h, help
    void showMainMenu();       // m, menu
    void showStatus();         // s, status
    void showEffectsMenu();    // e, effects
    void showPalettesMenu();   // p, palettes
    void showConfigMenu();     // c, config
    void showWaveMenu();       // wave
    void showPipelineMenu();   // pipe, pipeline

    // Performance commands
    void showPerformanceInfo();      // perf
    void showDetailedPerformance();  // pd, perfdetail
    void showPerformanceGraph();     // pg, perfgraph
    void resetPerformanceMetrics();  // perfreset

    // Parameter setters
    void setEffect(String value);
    void setPalette(String value);
    void setPaletteInterval(String value);
    void setBrightness(String value);
    void setFade(String value);
    void setSpeed(String value);
    void setFPS(String value);
    void setTransition(String value);
    void resetToDefaults();
    void clearScreen();

private:
    // References to global state
    FxEngine& fxEngine;
    PerformanceMonitor& perfMon;
    uint8_t& currentPaletteIndex;
    uint8_t& brightnessVal;
    uint8_t& fadeAmount;
    uint8_t& paletteSpeed;
    uint8_t& fps;
    bool& paletteAutoCycle;
    unsigned long& paletteCycleInterval;
};
```

---

## Complete Command Reference

### Navigation Commands

| Command | Aliases | Description |
|---------|---------|-------------|
| `help` | `h` | Show all available commands |
| `menu` | `m` | Show main menu |
| `status` | `s` | Show current system status |
| `clear` | - | Clear terminal (ANSI escape) |

### Effects Commands

| Command | Range | Description |
|---------|-------|-------------|
| `effects` | - | Show effects menu with list |
| `effect <n>` | 0-45 | Select effect by index |
| `next` | - | Next effect (with transition) |
| `prev` | - | Previous effect (with transition) |
| `transition <n>` | 0-2 | Set transition type (Fade/Wipe/Blend) |

### Palette Commands

| Command | Range | Description |
|---------|-------|-------------|
| `palettes` | - | Show palette menu |
| `palette <n>` | 0-32 | Select palette by index |
| `pnext` | - | Next palette |
| `pprev` | - | Previous palette |
| `pauto` | - | Toggle palette auto-cycle |
| `pinterval <ms>` | 500-60000 | Set auto-cycle interval |

### Configuration Commands

| Command | Range | Description |
|---------|-------|-------------|
| `config` | - | Show configuration menu |
| `brightness <n>` | 0-255 | Set global brightness |
| `fade <n>` | 0-255 | Set fade amount (trails) |
| `speed <n>` | 1-50 | Set palette speed |
| `fps <n>` | 10-120 | Set target frame rate |
| `reset` | - | Reset all to defaults |

### Performance Commands

| Command | Aliases | Description |
|---------|---------|-------------|
| `perf` | - | Show performance summary |
| `perfdetail` | `pd` | Show detailed performance breakdown |
| `perfgraph` | `pg` | Draw ASCII performance graph |
| `perfreset` | - | Reset peak metrics |

### Menu Navigation

| Command | Aliases | Description |
|---------|---------|-------------|
| `wave` | - | Show wave effects submenu |
| `pipe` | `pipeline` | Show pipeline effects submenu |

---

## Zone Composer Commands

The Zone Composer enables multi-zone rendering with independent effects per zone. These commands are handled in `main.cpp` (lines 1218-1314).

### Global Zone Control

| Command | Description |
|---------|-------------|
| `zone on` | Enable Zone Composer (multi-zone mode) |
| `zone off` | Disable Zone Composer (single-effect mode) |
| `zone status` | Print current zone configuration |
| `zone count <n>` | Set active zone count (1-4) |

### Zone-Specific Control

| Command | Description |
|---------|-------------|
| `zone <id> effect <n>` | Assign effect to zone (id: 0-3, n: 0-45) |
| `zone <id> enable` | Enable specific zone |
| `zone <id> disable` | Disable specific zone |

### Zone Presets

| Command | Description |
|---------|-------------|
| `zone presets` | List all available presets |
| `zone preset <n>` | Load preset configuration (n: 0-4) |

**Built-in Presets:**
- **0:** Unified - Single zone, all LEDs one effect
- **1:** Dual Split - Two concentric zones (inner/outer)
- **2:** Triple Rings - **DEFAULT** 3-zone (30+90+40 LEDs, AURA spec)
- **3:** Quad Active - Four equal 40-LED concentric zones
- **4:** LGP Showcase - LGP physics effects across all 4 zones

### Zone Persistence

| Command | Description |
|---------|-------------|
| `zone save` | Save current zone config to NVS |
| `zone load` | Load zone config from NVS |

### Zone Command Examples

```bash
# Enable 3-zone mode
zone on

# Check current configuration
zone status

# Assign Gradient effect (0) to zone 1
zone 1 effect 0

# Assign Kaleidoscope (3) to zone 2
zone 2 effect 3

# Load Triple Rings preset
zone preset 2

# Save configuration to flash
zone save
```

---

## Output Formats

### Status Output Format

```
=== CURRENT STATUS ===
Effect: 3 - Kaleidoscope
Palette: 5 - Ocean Breeze
Brightness: 180
Fade Amount: 20
Palette Speed: 10
Target FPS: 120
Actual FPS: 119.8
====================
```

### Performance Output Format

```
=== PERFORMANCE INFO ===
FPS: 119.8 / 120 (99%)
CPU Usage: 67.2%

Timing Breakdown:
  Effect: 45.3% (3780μs)
  FastLED: 21.8% (1820μs)
  Serial: 0.2%
  Idle: 32.7%

Dropped Frames: 0
Free Heap: 182456 / Min: 178230
======================
```

### Zone Status Output Format

```
=== ZONE COMPOSER STATUS ===
Enabled: YES
Zone Count: 3
Total LEDs: 160

Zone 0: LEDs 0-29 (30 LEDs)
  Effect: 0 - Gradient
  Brightness: 255
  Enabled: YES

Zone 1: LEDs 30-119 (90 LEDs)
  Effect: 3 - Kaleidoscope
  Brightness: 255
  Enabled: YES

Zone 2: LEDs 120-159 (40 LEDs)
  Effect: 7 - FxWave Orbital
  Brightness: 255
  Enabled: YES
===========================
```

---

## Implementation Patterns

### Parameter Validation Pattern

```cpp
void setBrightness(String value) {
    int brightness = value.toInt();
    if (brightness >= 0 && brightness <= 255) {
        brightnessVal = brightness;
        FastLED.setBrightness(brightnessVal);
        Serial.print(F("Set brightness to: "));
        Serial.println(brightness);
    } else {
        Serial.println(F("Invalid brightness (0-255)"));
    }
}
```

**Key Points:**
- Always validate range before applying
- Use `toInt()` for numeric conversion (returns 0 on failure)
- Apply to both local variable AND hardware (FastLED)
- Provide clear feedback with actual value set

### F() Macro Usage

**CRITICAL:** Always wrap static strings in `F()` macro to store in PROGMEM:

```cpp
// CORRECT - string stored in flash
Serial.println(F("=== MENU ==="));

// WRONG - wastes ~2KB RAM for menu system
Serial.println("=== MENU ===");
```

**Exception:** Dynamic strings (like effect names) cannot use `F()`:
```cpp
Serial.println(fxEngine.getCurrentEffectName());  // OK - dynamic
```

### ANSI Escape Sequences

The terminal supports ANSI escape codes for formatting:

```cpp
void clearScreen() {
    Serial.print(F("\033[2J\033[H"));  // Clear + home cursor
    Serial.println(F("=== LightwaveOS Control System ==="));
}
```

**Supported Sequences:**
- `\033[2J` - Clear entire screen
- `\033[H` - Move cursor to home (0,0)
- `\033[K` - Clear to end of line

---

## Command Dispatch Implementation

### In main.cpp (lines 1100-1400)

```cpp
void processSerialCommand() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd.length() == 0) return;

        // Zone commands (highest priority)
        if (cmd == "zone status") {
            zoneComposer.printStatus();
        }
        else if (cmd == "zone on") {
            zoneComposer.enable();
            Serial.println("Zone Composer enabled");
        }
        else if (cmd == "zone off") {
            zoneComposer.disable();
            Serial.println("Zone Composer disabled (single-effect mode)");
        }
        else if (cmd.startsWith("zone preset ")) {
            uint8_t presetId = cmd.substring(12).toInt();
            if (presetId <= 4) {
                zoneComposer.loadPreset(presetId);
                Serial.printf("Loaded preset %d: %s\n",
                    presetId, zoneComposer.getPresetName(presetId));
            }
        }
        // ... more zone commands ...

        // Delegate to SerialMenu for other commands
        else {
            serialMenu.processCommand(cmd);
        }
    }
}
```

### Priority Order

1. **Zone commands** - Handled directly in `main.cpp`
2. **Performance commands** - Handled in `main.cpp` → `PerformanceMonitor`
3. **Menu commands** - Delegated to `SerialMenu`
4. **Unknown commands** - Show help message

---

## NVS/Preferences Persistence

Zone configurations and user settings persist across reboots using ESP32 Preferences library.

### Save Operation

```cpp
// In ZoneComposer::saveConfig()
bool saveConfig() {
    Preferences prefs;
    prefs.begin("zones", false);  // RW mode

    prefs.putBool("enabled", _enabled);
    prefs.putUChar("count", _zoneCount);

    for (int i = 0; i < MAX_ZONES; i++) {
        String key = "zone" + String(i);
        prefs.putUChar((key + "_effect").c_str(), _zones[i].effectIndex);
        prefs.putUChar((key + "_bright").c_str(), _zones[i].brightness);
        prefs.putBool((key + "_on").c_str(), _zones[i].enabled);
    }

    prefs.end();
    return true;
}
```

### Load Operation

```cpp
// In ZoneComposer::loadConfig()
bool loadConfig() {
    Preferences prefs;
    prefs.begin("zones", true);  // Read-only mode

    if (!prefs.isKey("enabled")) {
        prefs.end();
        return false;  // No saved config
    }

    _enabled = prefs.getBool("enabled", false);
    _zoneCount = prefs.getUChar("count", 1);

    // ... load per-zone settings ...

    prefs.end();
    return true;
}
```

### Persistence Commands

| Command | NVS Namespace | Description |
|---------|---------------|-------------|
| `zone save` | `zones` | Save zone configuration |
| `zone load` | `zones` | Load zone configuration |
| `save` | `settings` | Save global settings (future) |
| `load` | `settings` | Load global settings (future) |

---

## Telemetry Integration

### PerformanceMonitor Data Access

```cpp
// Available metrics from PerformanceMonitor
float getCurrentFPS();           // Actual frames per second
float getCPUUsage();             // CPU utilization percentage
uint32_t getEffectTime();        // Effect render time (μs)
uint32_t getFastLEDTime();       // LED output time (μs)
uint32_t getDroppedFrames();     // Frames that exceeded budget
uint32_t getMinFreeHeap();       // Lowest heap during session

// Timing breakdown percentages
void getTimingPercentages(float& effect, float& led,
                          float& serial, float& idle);
```

### Real-Time Telemetry Output

```cpp
void showPerformanceInfo() {
    float effectPct, ledPct, serialPct, idlePct;
    perfMon.getTimingPercentages(effectPct, ledPct, serialPct, idlePct);

    Serial.print(F("FPS: "));
    Serial.print(perfMon.getCurrentFPS(), 1);
    Serial.print(F(" / "));
    Serial.println(fps);

    Serial.print(F("CPU Usage: "));
    Serial.print(perfMon.getCPUUsage(), 1);
    Serial.println(F("%"));

    Serial.print(F("\nEffect: "));
    Serial.print(effectPct, 1);
    Serial.print(F("% ("));
    Serial.print(perfMon.getEffectTime());
    Serial.println(F("μs)"));
}
```

---

## Anti-Patterns

### DO NOT: Block in Serial Processing

```cpp
// WRONG - blocks render loop
void processCommand(String cmd) {
    delay(100);  // Never delay in command processing!
    // ...
}
```

### DO NOT: Use Dynamic Allocation for Commands

```cpp
// WRONG - fragments heap
char* cmdBuffer = new char[64];

// CORRECT - use String (with reserve) or stack allocation
String cmd;
cmd.reserve(64);
```

### DO NOT: Skip F() for Static Strings

```cpp
// WRONG - wastes 500+ bytes RAM
Serial.println("=== PERFORMANCE INFO ===");
Serial.println("FPS: ");
Serial.println(" / ");
// ... more strings ...

// CORRECT - uses flash storage
Serial.println(F("=== PERFORMANCE INFO ==="));
```

### DO NOT: Forget to trim() Input

```cpp
// WRONG - whitespace causes command mismatch
String cmd = Serial.readStringUntil('\n');
if (cmd == "help") { ... }  // Fails if cmd == "help\r"

// CORRECT - always trim
String cmd = Serial.readStringUntil('\n');
cmd.trim();
if (cmd == "help") { ... }
```

### DO NOT: Use printf Without F()

```cpp
// WRONG - format string in RAM
Serial.printf("Zone %d: %s\n", id, name);

// BETTER - but still format string in RAM
Serial.printf("Zone %d: %s\n", id, name);  // Acceptable for dynamic data

// BEST - use print chain for simple cases
Serial.print(F("Zone "));
Serial.print(id);
Serial.print(F(": "));
Serial.println(name);
```

---

## Adding New Commands

### Step 1: Add to Help Menu

```cpp
// In SerialMenu::showHelp()
Serial.println(F("  newcmd <arg>   - Description of new command"));
```

### Step 2: Add Handler Function

```cpp
// In SerialMenu class
void setNewParam(String value) {
    int param = value.toInt();
    if (param >= MIN_VAL && param <= MAX_VAL) {
        newParam = param;
        Serial.print(F("Set new param to: "));
        Serial.println(param);
    } else {
        Serial.printf("Invalid value (%d-%d)\n", MIN_VAL, MAX_VAL);
    }
}
```

### Step 3: Add to Dispatcher

```cpp
// In processCommand() or main.cpp
if (cmd.startsWith("newcmd ")) {
    setNewParam(cmd.substring(7));  // "newcmd " = 7 chars
}
```

### Step 4: Add to Status Display (if applicable)

```cpp
// In showStatus()
Serial.print(F("New Param: "));
Serial.println(newParam);
```

---

## Thread Safety Notes

Serial command processing runs in the **main loop on Core 1**. This means:

1. **Direct parameter access is safe** - No mutex needed for globals
2. **FxEngine calls are safe** - Same core as render loop
3. **ZoneComposer calls are safe** - Same core as render loop
4. **WiFi/Network access needs care** - WebServer on Core 0

### Safe Pattern for Cross-Core Communication

```cpp
// If a serial command needs to trigger something on Core 0:
// Use a flag that Core 0 checks periodically
volatile bool pendingWiFiRestart = false;

// In serial handler (Core 1)
if (cmd == "wifi restart") {
    pendingWiFiRestart = true;
    Serial.println(F("WiFi restart scheduled"));
}

// In Core 0 task
void wifiTask(void* param) {
    while (true) {
        if (pendingWiFiRestart) {
            pendingWiFiRestart = false;
            WiFi.disconnect();
            WiFi.begin(ssid, password);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

---

## Quick Reference Card

```
=== NAVIGATION ===
h/help        Show all commands
m/menu        Main menu
s/status      Current status
clear         Clear screen

=== EFFECTS ===
e/effects     List effects
effect <n>    Select effect (0-45)
next/prev     Cycle effects

=== PALETTES ===
p/palettes    List palettes
palette <n>   Select palette (0-32)
pnext/pprev   Cycle palettes
pauto         Toggle auto-cycle

=== CONFIG ===
c/config      Show settings
brightness <0-255>
fade <0-255>
speed <1-50>
fps <10-120>
reset         Reset defaults

=== ZONES ===
zone on/off   Enable/disable zones
zone status   Show zone config
zone count <1-4>
zone <id> effect <n>
zone preset <0-4>
zone presets  List presets
zone save/load

=== PERFORMANCE ===
perf          Summary
pd/perfdetail Detailed
pg/perfgraph  ASCII graph
perfreset     Clear peaks
```

---

## Related Agents

| Topic | See Agent |
|-------|-----------|
| FxEngine effects | `visual-fx-architect.md` |
| Zone Composer details | `visual-fx-architect.md` |
| REST API alternatives | `network-api-engineer.md` |
| Hardware configuration | `embedded-system-engineer.md` |
| FreeRTOS threading | `embedded-system-engineer.md` |
