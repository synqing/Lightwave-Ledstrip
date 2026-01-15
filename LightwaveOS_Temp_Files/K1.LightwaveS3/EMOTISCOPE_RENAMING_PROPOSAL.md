# Emotiscope Renaming Proposal

## Overview
Remove ALL references to "Emotiscope" from the beat tracking system and replace with LightwaveOS-native naming.

## Proposed Naming Convention

### Core Classes & Files
- **EmotiscopeEngine** → **TempoEngine**
- **EmotiscopeEngine.h** → **TempoEngine.h**
- **EmotiscopeEngine.cpp** → **TempoEngine.cpp**
- **EmotiscopeBin** → **TempoBin**

### Constants
- **EMOTISCOPE_NUM_TEMPI** → **TEMPO_ENGINE_NUM_TEMPI**
- **EMOTISCOPE_TEMPO_LOW** → **TEMPO_ENGINE_TEMPO_LOW**
- **EMOTISCOPE_TEMPO_HIGH** → **TEMPO_ENGINE_TEMPO_HIGH**
- **EMOTISCOPE_HISTORY_LENGTH** → **TEMPO_ENGINE_HISTORY_LENGTH**
- **EMOTISCOPE_NOVELTY_DECAY** → **TEMPO_ENGINE_NOVELTY_DECAY**

### Variables
- **m_emotiscope** → **m_tempoEngine**
- **emotiscope** (parameter names) → **tempoEngine**

### Methods
- **getEmotiscopeMut()** → **getTempoEngineMut()**
- **getEmotiscope()** → **getTempoEngine()**
- **setEmotiscope()** → **setTempoEngine()**

### Comments & Documentation
- All "Emotiscope" references → "TempoEngine" or "tempo engine"
- "Emotiscope v2.0" → "TempoEngine" or "tempo tracking engine"
- "Emotiscope-style" → "tempo-driven" or "beat-synchronized"

## Files Requiring Changes

### Core Beat Tracking Files
1. **firmware/v2/src/audio/tempo/EmotiscopeEngine.h** → Rename to TempoEngine.h
2. **firmware/v2/src/audio/tempo/EmotiscopeEngine.cpp** → Rename to TempoEngine.cpp

### Integration Files
3. **firmware/v2/src/audio/AudioNode.h** - Class member, includes, methods
4. **firmware/v2/src/audio/AudioNode.cpp** - All references
5. **firmware/v2/src/core/actors/RendererNode.h** - Includes, method signatures
6. **firmware/v2/src/core/actors/RendererNode.cpp** - Includes, comments
7. **firmware/v2/src/core/actors/NodeOrchestrator.cpp** - Method calls
8. **firmware/v2/src/effects/zones/ZoneComposer.h** - Forward declarations, methods
9. **firmware/v2/src/effects/zones/ZoneComposer.cpp** - Includes, variable references

### Configuration & Documentation
10. **firmware/v2/src/config/features.h** - Comments
11. **firmware/v2/src/main.cpp** - Debug commands, comments
12. **firmware/v2/src/audio/contracts/ControlBus.h** - Comments
13. **firmware/v2/src/audio/contracts/TempoOutput.h** - Comments

### Effect Files (Perlin noise - separate concern)
14. **firmware/v2/src/effects/ieffect/LGPPerlinBackendEmotiscopeFullEffect.h** - Consider renaming
15. **firmware/v2/src/effects/ieffect/LGPPerlinBackendEmotiscopeFullEffect.cpp** - Consider renaming
16. **firmware/v2/src/effects/ieffect/LGPPerlinBackendEmotiscopeQuarterEffect.h** - Consider renaming
17. **firmware/v2/src/effects/ieffect/LGPPerlinBackendEmotiscopeQuarterEffect.cpp** - Consider renaming
18. **firmware/v2/src/effects/ieffect/PerlinNoiseTypes.h** - Comments
19. **firmware/v2/src/effects/ieffect/LGPPerlinShocklinesEffect.h** - Comments
20. **firmware/v2/src/effects/CoreEffects.cpp** - Includes, comments

## Detailed Change List

### 1. File Renames
```
firmware/v2/src/audio/tempo/EmotiscopeEngine.h → TempoEngine.h
firmware/v2/src/audio/tempo/EmotiscopeEngine.cpp → TempoEngine.cpp
```

### 2. Class & Struct Renames
```cpp
// OLD
class EmotiscopeEngine { ... };
struct EmotiscopeBin { ... };

// NEW
class TempoEngine { ... };
struct TempoBin { ... };
```

### 3. Constant Renames
```cpp
// OLD
constexpr uint16_t EMOTISCOPE_NUM_TEMPI = 96;
constexpr float EMOTISCOPE_TEMPO_LOW = 48.0f;
constexpr float EMOTISCOPE_TEMPO_HIGH = 144.0f;
constexpr uint16_t EMOTISCOPE_HISTORY_LENGTH = 512;
constexpr float EMOTISCOPE_NOVELTY_DECAY = 0.999f;

// NEW
constexpr uint16_t TEMPO_ENGINE_NUM_TEMPI = 96;
constexpr float TEMPO_ENGINE_TEMPO_LOW = 48.0f;
constexpr float TEMPO_ENGINE_TEMPO_HIGH = 144.0f;
constexpr uint16_t TEMPO_ENGINE_HISTORY_LENGTH = 512;
constexpr float TEMPO_ENGINE_NOVELTY_DECAY = 0.999f;
```

### 4. Variable Renames
```cpp
// OLD
EmotiscopeEngine m_emotiscope;
EmotiscopeBin m_bins[EMOTISCOPE_NUM_TEMPI];
EmotiscopeEngine* m_emotiscope = nullptr;

// NEW
TempoEngine m_tempoEngine;
TempoBin m_bins[TEMPO_ENGINE_NUM_TEMPI];
TempoEngine* m_tempoEngine = nullptr;
```

### 5. Method Renames
```cpp
// OLD
EmotiscopeEngine& getEmotiscopeMut() { return m_emotiscope; }
EmotiscopeEngine* getEmotiscope() { return &m_emotiscope; }
void setEmotiscope(audio::EmotiscopeEngine* emotiscope) { m_emotiscope = emotiscope; }

// NEW
TempoEngine& getTempoEngineMut() { return m_tempoEngine; }
TempoEngine* getTempoEngine() { return &m_tempoEngine; }
void setTempoEngine(audio::TempoEngine* tempoEngine) { m_tempoEngine = tempoEngine; }
```

### 6. Include Updates
```cpp
// OLD
#include "tempo/EmotiscopeEngine.h"
#include "../../audio/tempo/EmotiscopeEngine.h"

// NEW
#include "tempo/TempoEngine.h"
#include "../../audio/tempo/TempoEngine.h"
```

### 7. Forward Declaration Updates
```cpp
// OLD
namespace lightwaveos { namespace audio { class EmotiscopeEngine; } }

// NEW
namespace lightwaveos { namespace audio { class TempoEngine; } }
```

## Implementation Strategy

1. **Phase 1: Core Engine Files**
   - Rename EmotiscopeEngine.h/cpp to TempoEngine.h/cpp
   - Update class name, struct name, constants
   - Update all internal references

2. **Phase 2: Integration Points**
   - Update AudioNode.h/cpp
   - Update RendererNode.h/cpp
   - Update NodeOrchestrator.cpp
   - Update ZoneComposer.h/cpp

3. **Phase 3: Configuration & Documentation**
   - Update comments in all files
   - Update debug output
   - Update feature flags

4. **Phase 4: Effect Files (Optional)**
   - Consider renaming Perlin effect files if they're beat-tracking related
   - Update comments in effect files

## Notes

- **Perlin Noise Effects**: The Perlin noise effects (LGPPerlinBackendEmotiscope*) may be separate from beat tracking. Evaluate if these should be renamed or if they're just using similar algorithms.
- **Documentation Files**: The docs/ folder contains Emotiscope analysis files - these can remain as historical reference or be archived.
- **Backward Compatibility**: No API compatibility needed - this is internal refactoring.

## Verification Checklist

- [ ] All class names updated
- [ ] All file names updated
- [ ] All constants updated
- [ ] All variable names updated
- [ ] All method names updated
- [ ] All includes updated
- [ ] All forward declarations updated
- [ ] All comments updated
- [ ] All debug output updated
- [ ] Code compiles without errors
- [ ] No remaining "Emotiscope" references in beat tracking code

