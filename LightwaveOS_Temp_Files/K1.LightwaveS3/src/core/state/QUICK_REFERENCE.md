# CQRS State Store - Quick Reference

## Cheat Sheet

### Include Files

```cpp
#include "core/state/StateStore.h"
#include "core/state/Commands.h"

using namespace lightwaveos::state;
```

### Create State Store

```cpp
StateStore stateStore;
```

### Query State (Lock-Free, Never Blocks)

```cpp
// Get entire state
const SystemState& state = stateStore.getState();

// Convenience methods
uint8_t effect = stateStore.getCurrentEffect();
uint8_t palette = stateStore.getCurrentPalette();
uint8_t brightness = stateStore.getBrightness();
uint8_t speed = stateStore.getSpeed();
bool zoneMode = stateStore.isZoneModeEnabled();
ZoneState zone = stateStore.getZoneConfig(0);
```

### Dispatch Commands (Thread-Safe Mutation)

```cpp
// Effect
SetEffectCommand cmd(5);
stateStore.dispatch(cmd);

// Brightness
stateStore.dispatch(SetBrightnessCommand(255));

// Palette
stateStore.dispatch(SetPaletteCommand(10));

// Speed
stateStore.dispatch(SetSpeedCommand(25));
```

### Zone Commands

```cpp
// Enable zone mode with 4 zones
stateStore.dispatch(SetZoneModeCommand(true, 4));

// Enable zone 0
stateStore.dispatch(ZoneEnableCommand(0, true));

// Set zone effect
stateStore.dispatch(ZoneSetEffectCommand(0, 5));

// Set zone brightness
stateStore.dispatch(ZoneSetBrightnessCommand(0, 200));
```

### Transition Commands

```cpp
// Start transition (type 3 = CrossFade)
stateStore.dispatch(TriggerTransitionCommand(3));

// Update progress
stateStore.dispatch(UpdateTransitionCommand(3, 128));

// Complete transition
stateStore.dispatch(CompleteTransitionCommand());
```

### Batch Commands (Atomic)

```cpp
SetEffectCommand effect(10);
SetBrightnessCommand brightness(255);
SetPaletteCommand palette(5);

const ICommand* commands[] = { &effect, &brightness, &palette };
stateStore.dispatchBatch(commands, 3);
```

### Subscribe to Changes

```cpp
void onStateChange(const SystemState& newState) {
    // Called after every state change
    // Keep FAST (< 100us recommended)
}

stateStore.subscribe(onStateChange);
stateStore.unsubscribe(onStateChange);
```

### Visual Parameters

```cpp
// Individual
stateStore.dispatch(SetIntensityCommand(200));
stateStore.dispatch(SetSaturationCommand(255));
stateStore.dispatch(SetComplexityCommand(100));
stateStore.dispatch(SetVariationCommand(150));

// All at once
stateStore.dispatch(SetVisualParamsCommand(200, 255, 100, 150));
```

### Hue Cycling

```cpp
// Auto-increment (for effects)
stateStore.dispatch(IncrementHueCommand());
```

### Custom Commands

```cpp
class MyCommand : public ICommand {
public:
    SystemState apply(const SystemState& current) const override {
        // Create new state from current
        return current.withEffect(newId);
    }

    const char* getName() const override {
        return "MyCommand";
    }

    bool validate(const SystemState& current) const override {
        // Optional validation
        return true;
    }
};

MyCommand cmd;
stateStore.dispatch(cmd);
```

## All Commands

| Command | Usage |
|---------|-------|
| `SetEffectCommand(id)` | Set current effect |
| `SetBrightnessCommand(val)` | Set global brightness (0-255) |
| `SetPaletteCommand(id)` | Set current palette |
| `SetSpeedCommand(val)` | Set speed (1-50) |
| `ZoneEnableCommand(zone, enabled)` | Enable/disable zone |
| `ZoneSetEffectCommand(zone, id)` | Set zone effect |
| `ZoneSetPaletteCommand(zone, id)` | Set zone palette |
| `ZoneSetBrightnessCommand(zone, val)` | Set zone brightness |
| `ZoneSetSpeedCommand(zone, val)` | Set zone speed |
| `SetZoneModeCommand(enabled, count)` | Enable zone mode |
| `TriggerTransitionCommand(type)` | Start transition |
| `UpdateTransitionCommand(type, progress)` | Update transition |
| `CompleteTransitionCommand()` | End transition |
| `IncrementHueCommand()` | Increment global hue |
| `SetVisualParamsCommand(i,s,c,v)` | Set all visual params |
| `SetIntensityCommand(val)` | Set intensity |
| `SetSaturationCommand(val)` | Set saturation |
| `SetComplexityCommand(val)` | Set complexity |
| `SetVariationCommand(val)` | Set variation |

## SystemState Fields

```cpp
struct SystemState {
    uint32_t version;           // State version

    // Global
    uint8_t currentEffectId;    // 0-63
    uint8_t currentPaletteId;   // 0-63
    uint8_t brightness;         // 0-255
    uint8_t speed;              // 1-50
    uint8_t gHue;               // 0-255

    // Visual params
    uint8_t intensity;          // 0-255
    uint8_t saturation;         // 0-255
    uint8_t complexity;         // 0-255
    uint8_t variation;          // 0-255

    // Zones
    bool zoneModeEnabled;
    uint8_t activeZoneCount;    // 1-4
    std::array<ZoneState, 4> zones;

    // Transitions
    bool transitionActive;
    uint8_t transitionType;     // 0-11
    uint8_t transitionProgress; // 0-255
};
```

## ZoneState Fields

```cpp
struct ZoneState {
    uint8_t effectId;       // Zone effect (0-63)
    uint8_t paletteId;      // Zone palette (0-63)
    uint8_t brightness;     // Zone brightness (0-255)
    uint8_t speed;          // Zone speed (1-50)
    bool enabled;           // Zone active
};
```

## Common Patterns

### Render Loop (120 FPS)

```cpp
void renderLoop() {
    const SystemState& state = stateStore.getState();

    // Use state for rendering
    uint8_t effect = state.currentEffectId;
    uint8_t brightness = state.brightness;

    // Render...
}
```

### Web API Handler

```cpp
void handleSetEffect(uint8_t id) {
    SetEffectCommand cmd(id);
    if (stateStore.dispatch(cmd)) {
        sendSuccess();
    } else {
        sendError("Invalid effect");
    }
}
```

### NVS Persistence

```cpp
void saveToNVS() {
    const SystemState& state = stateStore.getState();
    nvs.setUInt8("effect", state.currentEffectId);
    nvs.setUInt8("brightness", state.brightness);
}

void loadFromNVS() {
    uint8_t effect = nvs.getUInt8("effect", 0);
    stateStore.dispatch(SetEffectCommand(effect));
}
```

### State Logging

```cpp
void onStateChange(const SystemState& state) {
    Serial.printf("State v%d: effect=%d, brightness=%d\n",
                  state.version, state.currentEffectId, state.brightness);
}

stateStore.subscribe(onStateChange);
```

## Performance Tips

1. **Lock-free reads** - Use `getState()` freely, it never blocks
2. **Batch commands** - Use `dispatchBatch()` for multiple updates
3. **Fast subscribers** - Keep callbacks < 100us (called in write lock)
4. **Immutable state** - Never modify returned state, always dispatch commands
5. **Version checking** - Use `version` field for optimistic concurrency

## Common Mistakes

### ❌ DON'T: Modify state directly

```cpp
const SystemState& state = stateStore.getState();
state.brightness = 255;  // COMPILE ERROR - const reference
```

### ✅ DO: Dispatch commands

```cpp
SetBrightnessCommand cmd(255);
stateStore.dispatch(cmd);
```

### ❌ DON'T: Store state references

```cpp
const SystemState& myState = stateStore.getState();
// ... later ...
uint8_t brightness = myState.brightness;  // May be stale!
```

### ✅ DO: Query state when needed

```cpp
// Query state immediately before use
const SystemState& state = stateStore.getState();
uint8_t brightness = state.brightness;
```

### ❌ DON'T: Slow subscribers

```cpp
void onStateChange(const SystemState& state) {
    delay(100);  // Blocks all state changes!
    saveToNVS();  // Slow operation in subscriber
}
```

### ✅ DO: Fast subscribers, defer slow work

```cpp
volatile bool needsSave = false;

void onStateChange(const SystemState& state) {
    needsSave = true;  // Fast flag set
}

void loop() {
    if (needsSave) {
        saveToNVS();  // Slow work in main loop
        needsSave = false;
    }
}
```

## Debugging

### Check State

```cpp
const SystemState& state = stateStore.getState();
Serial.printf("State v%d: effect=%d, brightness=%d\n",
              state.version, state.currentEffectId, state.brightness);
```

### Get Statistics

```cpp
uint32_t commandCount, lastDuration;
stateStore.getStats(commandCount, lastDuration);
Serial.printf("Commands: %d, Last: %d us\n", commandCount, lastDuration);
```

### Subscriber Count

```cpp
uint8_t count = stateStore.getSubscriberCount();
Serial.printf("Subscribers: %d\n", count);
```

### Version Tracking

```cpp
uint32_t beforeVersion = stateStore.getVersion();
stateStore.dispatch(SetEffectCommand(5));
uint32_t afterVersion = stateStore.getVersion();
// afterVersion == beforeVersion + 1
```

## Example: Complete Integration

```cpp
#include "core/state/StateStore.h"
#include "core/state/Commands.h"

using namespace lightwaveos::state;

StateStore g_stateStore;

void onStateChange(const SystemState& state) {
    Serial.printf("State changed: v%d\n", state.version);
}

void setup() {
    Serial.begin(115200);

    // Subscribe
    g_stateStore.subscribe(onStateChange);

    // Set initial state
    g_stateStore.dispatch(SetEffectCommand(0));
    g_stateStore.dispatch(SetBrightnessCommand(128));
}

void loop() {
    // Render loop
    const SystemState& state = g_stateStore.getState();
    uint8_t effect = state.currentEffectId;
    uint8_t brightness = state.brightness;

    // Render using state...

    // Auto-cycle hue
    static uint32_t lastHue = 0;
    if (millis() - lastHue > 100) {
        g_stateStore.dispatch(IncrementHueCommand());
        lastHue = millis();
    }
}
```

## Links

- Full documentation: `v2/src/core/state/README.md`
- Architecture guide: `v2/docs/CQRS_STATE_ARCHITECTURE.md`
- Example usage: `v2/src/core/state/example_usage.cpp`
- Integration test: `v2/src/main.cpp`
