# Integrating Tuned Effects

## Quick Integration Steps

### 1. Add Include to main.cpp

Add this include after the other effect includes (around line 119):

```cpp
// Tuned effects with enhanced encoder control
#include "effects/strip/TunedEffects.h"
```

### 2. Add Effect Declarations

Add these external declarations after the other effect declarations (around line 140):

```cpp
// Tuned effects
extern void fireTuned();
extern void stripBPMTuned();
extern void sinelonTuned();
extern void gravityWellTuned();
```

### 3. Add to Effects Array

You can either:

**Option A: Replace existing effects** (recommended for testing)
```cpp
// Replace these lines:
{"Fire", fire, EFFECT_TYPE_STANDARD},
{"Strip BPM", stripBPM, EFFECT_TYPE_STANDARD},
{"Sinelon", sinelon, EFFECT_TYPE_STANDARD},
{"Gravity Well", gravityWellEffect, EFFECT_TYPE_STANDARD},

// With:
{"Fire", fireTuned, EFFECT_TYPE_STANDARD},
{"Strip BPM", stripBPMTuned, EFFECT_TYPE_STANDARD},
{"Sinelon", sinelonTuned, EFFECT_TYPE_STANDARD},
{"Gravity Well", gravityWellTuned, EFFECT_TYPE_STANDARD},
```

**Option B: Add as new effects**
```cpp
// Add after existing effects:
{"Fire Tuned", fireTuned, EFFECT_TYPE_STANDARD},
{"BPM Tuned", stripBPMTuned, EFFECT_TYPE_STANDARD},
{"Sinelon Tuned", sinelonTuned, EFFECT_TYPE_STANDARD},
{"Gravity Tuned", gravityWellTuned, EFFECT_TYPE_STANDARD},
```

### 4. Update platformio.ini (if needed)

Make sure the new source file is included in the build:

```ini
[env:esp32-s3-devkitm-1]
; ... existing config ...
build_src_filter = 
    +<*>
    +<effects/strip/TunedEffects.cpp>
```

## Testing the Tuned Effects

1. **Build and Upload**
   ```bash
   pio run -t clean
   pio run -t upload
   ```

2. **Test Each Effect**
   - Use encoder 0 to select each tuned effect
   - Experiment with encoders 3-7 to see the enhanced parameter control

3. **Monitor Serial Output**
   ```bash
   pio device monitor -b 115200
   ```

## Encoder Reference for Testing

| Encoder | Function |
|---------|----------|
| 0 | Effect selection |
| 1 | Brightness |
| 2 | Palette |
| 3 | Speed/BPM/Gravity |
| 4 | Intensity |
| 5 | Saturation |
| 6 | Complexity |
| 7 | Variation |

## Troubleshooting

If effects don't appear:
1. Check serial output for initialization messages
2. Verify TunedEffects.cpp is being compiled
3. Check that effect count matches (NUM_EFFECTS)
4. Ensure encoders are properly initialized

## Performance Notes

The tuned effects are optimized but use more CPU due to:
- Additional parameter calculations
- More complex animations
- Enhanced particle systems (Gravity Well)
- Multi-layer rendering (Fire)

Monitor FPS with serial output to ensure performance remains above 120 FPS.