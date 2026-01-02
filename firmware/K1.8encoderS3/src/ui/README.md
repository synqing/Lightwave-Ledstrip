# DisplayUI - Neon Cyberpunk Display Interface

Full-screen 128x128px display UI for K1.8encoderS3 with glowing neon cyberpunk aesthetic.

## Design Overview

**Layout**: 2x4 grid (8 cells total), each cell 64x32 pixels

**Features**:
- Full parameter names (no abbreviations)
- Glowing colored borders per parameter
- Progress bars with glow effect
- Scanline overlay for retro CRT aesthetic
- Touch swipe detection for page navigation
- Highlight system for last-changed parameter

## Parameter Colors

Each parameter has a dedicated neon color:

| Parameter   | Color       | Hex Code | RGB565  |
|-------------|-------------|----------|---------|
| Effect      | Hot Pink    | #ff0080  | 0xF810  |
| Brightness  | Yellow      | #ffff00  | 0xFFE0  |
| Palette     | Cyan        | #00ffff  | 0x07FF  |
| Speed       | Orange      | #ff4400  | 0xFA20  |
| Intensity   | Magenta     | #ff00ff  | 0xF81F  |
| Saturation  | Green       | #00ff88  | 0x07F1  |
| Complexity  | Purple      | #8800ff  | 0x901F  |
| Variation   | Blue        | #0088ff  | 0x047F  |

Background: Dark Navy (#0a0a14, RGB565: 0x0841)

## Cell Layout

Each 64x32px cell contains:

```
┌─────────────────────────────┐ ← Glowing border (1-3px)
│ Effect            255       │ ← Name + Value
│                             │
│ ▓▓▓▓▓▓▓▓▓▓▓░░░░░░░          │ ← Progress bar
└─────────────────────────────┘
```

**Row 1** (top): Parameter name (left) + value (right)
**Row 2** (bottom): Progress bar with glow effect

## API Reference

### Constructor

```cpp
DisplayUI(M5GFX& display)
```

Pass reference to M5GFX display object (e.g., `AtomS3.Display`).

### Methods

#### `void begin()`
Initialize display, clear screen, draw initial grid.

**Usage**:
```cpp
DisplayUI ui(AtomS3.Display);
void setup() {
    AtomS3.begin();
    ui.begin();
}
```

#### `void update(uint8_t paramIndex, uint8_t value)`
Update a single parameter cell.

**Parameters**:
- `paramIndex`: 0-7 (parameter index)
- `value`: 0-255 (parameter value)

**Usage**:
```cpp
ui.update(0, 128); // Set Effect to 128
```

#### `void updateAll(const uint8_t values[8])`
Update all 8 parameters at once.

**Parameters**:
- `values`: Array of 8 values (0-255)

**Usage**:
```cpp
uint8_t params[8] = {255, 128, 64, 32, 16, 8, 4, 2};
ui.updateAll(params);
```

#### `void setHighlight(uint8_t paramIndex)`
Highlight a specific parameter with brighter glow.

**Parameters**:
- `paramIndex`: 0-7 to highlight, 255 to clear highlight

**Usage**:
```cpp
ui.setHighlight(2); // Highlight Palette
ui.setHighlight(255); // Clear highlight
```

#### `void drawScanlines()`
Redraw scanline overlay effect (called automatically after updates).

**Usage**:
```cpp
ui.drawScanlines(); // Manually refresh scanlines
```

#### `bool handleTouch()`
Process touch input for swipe detection. Call in `loop()`.

**Returns**: `true` if valid swipe detected (30px+ horizontal movement)

**Usage**:
```cpp
void loop() {
    if (ui.handleTouch()) {
        // Handle page switch
    }
}
```

## Example Usage

### Basic Demo

```cpp
#include <M5AtomS3.h>
#include "ui/DisplayUI.h"

DisplayUI ui(AtomS3.Display);
uint8_t values[8] = {0};

void setup() {
    AtomS3.begin(false);
    ui.begin();
}

void loop() {
    // Increment Effect parameter
    values[0]++;
    ui.update(0, values[0]);
    ui.setHighlight(0);

    delay(50);
}
```

### Integration with Encoder

```cpp
#include <M5AtomS3.h>
#include <M5ROTATE8.h>
#include "ui/DisplayUI.h"

M5ROTATE8 encoder;
DisplayUI ui(AtomS3.Display);
uint8_t paramValues[8] = {127, 255, 0, 64, 128, 192, 32, 96};

void setup() {
    AtomS3.begin(false);
    Wire.begin();
    encoder.begin();
    encoder.resetAll();

    ui.begin();
    ui.updateAll(paramValues);
}

void loop() {
    // Read encoder changes (V2 firmware)
    uint8_t changeMask = encoder.getEncoderChangeMask();

    for (uint8_t i = 0; i < 8; i++) {
        if (changeMask & (1 << i)) {
            int32_t delta = encoder.getRelativeCounter(i);
            paramValues[i] = constrain(paramValues[i] + delta, 0, 255);

            ui.update(i, paramValues[i]);
            ui.setHighlight(i);
        }
    }

    delay(10);
}
```

## Implementation Details

### Grid Positioning

Cells arranged in 2 columns x 4 rows:

```
[0 Effect     ][1 Brightness]
[2 Palette    ][3 Speed     ]
[4 Intensity  ][5 Saturation]
[6 Complexity ][7 Variation ]
```

Position formula:
- Column: `paramIndex % 2`
- Row: `paramIndex / 2`
- X: `col * 64`
- Y: `row * 32`

### Glow Effect

Multi-layer borders create glow:
1. Outer layer: 30% brightness
2. Middle layer: 50% brightness
3. Inner layer: 60-100% brightness (100% when highlighted)

### Color Dimming

RGB565 color components extracted and scaled:
```cpp
uint16_t dimColor(uint16_t color, float factor)
```

### Scanlines

Horizontal lines drawn every 2 pixels across entire display for CRT effect.

## Touch Detection

Swipe threshold: 30 pixels horizontal movement

**States**:
- Touch start: Record X/Y coordinates
- Touch active: Track movement
- Touch end: Calculate delta, check threshold

## Performance

- Single cell update: ~10-15ms
- Full screen update: ~60-80ms
- Scanline overlay: ~5-10ms

Optimizations:
- `startWrite()`/`endWrite()` batching
- Only redraw changed cells
- Scanlines drawn after updates

## Files

- `DisplayUI.h` - Class definition and API
- `DisplayUI.cpp` - Implementation
- `README.md` - This documentation
- `../examples/DisplayUI_Demo/DisplayUI_Demo.ino` - Demo sketch

## Dependencies

- **M5GFX**: Graphics library (required)
- **M5AtomS3**: Hardware abstraction (AtomS3.Display)

## Notes

- Display resolution: 128x128px
- Color format: RGB565
- Font: Built-in M5GFX fonts (Font0, Font2)
- No tab bar (full-screen grid)
- Swipe detection ready for multi-page navigation
