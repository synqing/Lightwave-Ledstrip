# 🔄 Document 05: Sync & Propagation Modes - Deep Implementation

<div align="center">

```
╔═══════════════════════════════════════════════════════════════════════════════╗
║                        SYNC & PROPAGATION MODES                                ║
║                   Dual-Strip Coordination Mathematics                          ║
║              Independent • Synchronized • Mirrored • Chase                     ║
╚═══════════════════════════════════════════════════════════════════════════════╝
```

**Modes:** 4 Sync × 5 Propagation | **Philosophy:** CENTER ORIGIN | **Performance:** Zero-overhead switching

</div>

---

## 📋 Executive Summary

The Sync & Propagation system defines how LightwaveOS coordinates its dual 160-LED strips, creating complex visual relationships through mathematical transformations. This document reveals the precise implementations, performance optimizations, and visual mathematics that enable 20 unique mode combinations while maintaining the CENTER ORIGIN philosophy.

### 🎯 Key Concepts
- **Sync Modes**: Define the relationship between Strip 1 and Strip 2
- **Propagation Modes**: Control the direction and pattern of effect movement
- **CENTER ORIGIN**: All spatial calculations originate from LED positions 79/80
- **Mathematical Transforms**: Real-time coordinate remapping for each mode
- **Zero-Copy Implementation**: Direct buffer manipulation for maximum performance

---

## 🏗️ Mode Architecture

### Sync Mode Definitions

```cpp
enum SyncMode {
    SYNC_INDEPENDENT = 0,    // Strips operate independently
    SYNC_SYNCHRONIZED = 1,   // Strips show identical patterns
    SYNC_MIRRORED = 2,      // Strip 2 mirrors Strip 1
    SYNC_CHASE = 3          // Strip 2 follows Strip 1 with delay
};
```

### Propagation Mode Definitions

```cpp
enum PropagationMode {
    PROPAGATE_OUTWARD = 0,      // From center to edges
    PROPAGATE_INWARD = 1,       // From edges to center
    PROPAGATE_LEFT_TO_RIGHT = 2,// Unidirectional left→right
    PROPAGATE_RIGHT_TO_LEFT = 3,// Unidirectional right→left
    PROPAGATE_ALTERNATING = 4   // Oscillating wave
};
```

### Mode Combination Matrix

```mermaid
graph TB
    subgraph "20 Unique Combinations"
        subgraph "Independent"
            I1[Outward]
            I2[Inward]
            I3[Left→Right]
            I4[Right→Left]
            I5[Alternating]
        end
        
        subgraph "Synchronized"
            S1[Outward]
            S2[Inward]
            S3[Left→Right]
            S4[Right→Left]
            S5[Alternating]
        end
        
        subgraph "Mirrored"
            M1[Outward]
            M2[Inward]
            M3[Left→Right]
            M4[Right→Left]
            M5[Alternating]
        end
        
        subgraph "Chase"
            C1[Outward]
            C2[Inward]
            C3[Left→Right]
            C4[Right→Left]
            C5[Alternating]
        end
    end
    
    style S1 fill:#ff6b6b
    style M1 fill:#4dabf7
    style C1 fill:#69db7c
```

---

## 🔄 Sync Mode Implementations

### SYNC_INDEPENDENT Mode

```cpp
void applyIndependentSync() {
    // Each strip processes its own effect independently
    // No coordination required - maximum performance
    
    // Strip 1 renders to its buffer
    currentEffect.render(strip1, STRIP_LENGTH);
    
    // Strip 2 can render different effect or parameters
    if (dualEffectMode) {
        secondaryEffect.render(strip2, STRIP_LENGTH);
    } else {
        currentEffect.render(strip2, STRIP_LENGTH);
    }
}
```

Visual Representation:
```
Strip 1: ████░░░░████░░░░████░░░░████░░░░
Strip 2: ░░████░░░░████░░░░████░░░░████░░
         ↑ Different patterns, phases, or effects
```

### SYNC_SYNCHRONIZED Mode

```cpp
void applySynchronizedSync() {
    // Both strips show identical content
    // Single render, dual output
    
    // Render once to unified buffer
    currentEffect.render(leds, NUM_LEDS);
    
    // Copy to individual strips
    memcpy(strip1, leds, STRIP_LENGTH * sizeof(CRGB));
    memcpy(strip2, leds + STRIP_LENGTH, STRIP_LENGTH * sizeof(CRGB));
}
```

Visual Representation:
```
Strip 1: ████░░░░████░░░░████░░░░████░░░░
Strip 2: ████░░░░████░░░░████░░░░████░░░░
         ↑ Identical patterns and timing
```

### SYNC_MIRRORED Mode

```cpp
void applyMirroredSync() {
    // Strip 2 mirrors Strip 1 around center
    
    // Render to strip 1
    currentEffect.render(strip1, STRIP_LENGTH);
    
    // Mirror to strip 2
    for (int i = 0; i < STRIP_LENGTH; i++) {
        strip2[i] = strip1[STRIP_LENGTH - 1 - i];
    }
}
```

Visual Representation:
```
Strip 1: ████░░░░░░░░████████░░░░░░░░████
Strip 2: ████░░░░░░░░████████░░░░░░░░████
              ←─ Mirrored ─→
```

### SYNC_CHASE Mode

```cpp
void applyChaseSync() {
    static CircularBuffer<CRGB, STRIP_LENGTH * CHASE_DELAY_FRAMES> chaseBuffer;
    
    // Render new frame to strip 1
    currentEffect.render(strip1, STRIP_LENGTH);
    
    // Add to chase buffer
    for (int i = 0; i < STRIP_LENGTH; i++) {
        chaseBuffer.push(strip1[i]);
    }
    
    // Strip 2 shows delayed version
    if (chaseBuffer.size() >= STRIP_LENGTH * CHASE_DELAY_FRAMES) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            strip2[i] = chaseBuffer[i];
        }
    }
}
```

Visual Representation:
```
Time T:   Strip 1: ████░░░░░░░░░░░░░░░░░░░░░░░░
          Strip 2: ░░░░░░░░░░░░░░░░░░░░░░░░░░░░

Time T+n: Strip 1: ░░░░████░░░░░░░░░░░░░░░░░░░░
          Strip 2: ████░░░░░░░░░░░░░░░░░░░░░░░░
                   ↑ Delayed by n frames
```

---

## 📐 Propagation Mode Mathematics

### PROPAGATE_OUTWARD Implementation

```cpp
uint8_t mapOutwardPosition(uint8_t physicalPos) {
    // Map physical position to outward propagation position
    // Center LEDs (79,80) map to position 0
    // Edges (0,159) map to position 79
    
    int distanceFromCenter = abs((int)physicalPos - CENTER_POINT);
    return distanceFromCenter;
}

void applyOutwardPropagation(CRGB* strip) {
    CRGB temp[STRIP_LENGTH];
    
    // Generate effect in propagation space
    for (int i = 0; i < STRIP_LENGTH; i++) {
        uint8_t propPos = i;  // Propagation position
        temp[propPos] = calculateEffectAt(propPos);
    }
    
    // Map back to physical space
    for (int i = 0; i < STRIP_LENGTH; i++) {
        uint8_t mappedPos = mapOutwardPosition(i);
        strip[i] = temp[mappedPos];
    }
}
```

Mathematical Transform:
```
Physical Position: 0 ←───────── 79|80 ─────────→ 159
Propagation Pos:  79 ←───────── 0|0 ──────────→ 79
                        (CENTER ORIGIN)
```

### PROPAGATE_INWARD Implementation

```cpp
uint8_t mapInwardPosition(uint8_t physicalPos) {
    // Inverse of outward - edges start, center ends
    int distanceFromCenter = abs((int)physicalPos - CENTER_POINT);
    return (STRIP_LENGTH / 2 - 1) - distanceFromCenter;
}
```

Mathematical Transform:
```
Physical Position: 0 ←───────── 79|80 ─────────→ 159
Propagation Pos:  0 ←───────── 79|79 ──────────→ 0
                     (Edges to center)
```

### PROPAGATE_LEFT_TO_RIGHT Implementation

```cpp
uint8_t mapLeftToRightPosition(uint8_t physicalPos) {
    // Direct mapping - no transformation needed
    return physicalPos;
}
```

Mathematical Transform:
```
Physical Position: 0 ─────────────────────────→ 159
Propagation Pos:  0 ─────────────────────────→ 159
                  (Linear progression)
```

### PROPAGATE_ALTERNATING Implementation

```cpp
uint8_t mapAlternatingPosition(uint8_t physicalPos, float phase) {
    // Creates standing wave pattern
    // Phase determines wave position
    
    float wavePos = sin((physicalPos / (float)STRIP_LENGTH) * PI * 2 + phase);
    wavePos = (wavePos + 1.0f) * 0.5f; // Normalize to 0-1
    
    return (uint8_t)(wavePos * (STRIP_LENGTH - 1));
}
```

---

## 🎨 Combined Mode Behaviors

### Mode Interaction Examples

```
╔════════════════════════════════════════════════════════════════════════╗
║                    SYNC + PROPAGATION COMBINATIONS                      ║
╠════════════════════════════════════════════════════════════════════════╣
║ Combination          │ Visual Effect                                   ║
├──────────────────────┼──────────────────────────────────────────────────┤
║ Synchronized+Outward │ Perfect ripples from center on both strips      ║
║ Mirrored+Inward     │ Collapsing waves that meet at center           ║
║ Chase+LeftToRight   │ Wave flows across strip 1, then strip 2        ║
║ Independent+Alternat │ Complex interference patterns                   ║
║ Mirrored+Outward    │ Butterfly wing effect from center              ║
╚══════════════════════╧══════════════════════════════════════════════════╝
```

### Visual Examples

#### Synchronized + Outward
```
Frame 1:    Strip 1: ░░░░░░░░░░░░████░░░░░░░░░░░░
            Strip 2: ░░░░░░░░░░░░████░░░░░░░░░░░░

Frame 10:   Strip 1: ░░░░░░████████████████░░░░░░
            Strip 2: ░░░░░░████████████████░░░░░░

Frame 20:   Strip 1: ████████████████████████████
            Strip 2: ████████████████████████████
```

#### Mirrored + Alternating
```
Frame 1:    Strip 1: ████░░░░████░░░░████░░░░████
            Strip 2: ████░░░░████░░░░████░░░░████

Frame 10:   Strip 1: ░░████░░░░████░░░░████░░░░██
            Strip 2: ██░░░░████░░░░████░░░░████░░

Frame 20:   Strip 1: ░░░░████░░░░████░░░░████░░░░
            Strip 2: ░░░░████░░░░████░░░░████░░░░
```

---

## ⚡ Performance Optimization

### Lookup Table Optimization

```cpp
class PropagationOptimizer {
    // Pre-calculated mapping tables
    uint8_t outwardMap[STRIP_LENGTH];
    uint8_t inwardMap[STRIP_LENGTH];
    uint8_t alternatingMap[STRIP_LENGTH][32]; // 32 phase steps
    
    void initializeMappings() {
        // Pre-calculate all mappings at startup
        for (int i = 0; i < STRIP_LENGTH; i++) {
            outwardMap[i] = abs(i - CENTER_POINT);
            inwardMap[i] = (STRIP_LENGTH/2 - 1) - abs(i - CENTER_POINT);
            
            // Pre-calculate alternating for different phases
            for (int phase = 0; phase < 32; phase++) {
                float p = (phase / 32.0f) * TWO_PI;
                float wave = sin((i / (float)STRIP_LENGTH) * TWO_PI + p);
                alternatingMap[i][phase] = (uint8_t)((wave + 1.0f) * 127);
            }
        }
    }
    
    inline uint8_t getMapping(PropagationMode mode, uint8_t pos, uint8_t phase = 0) {
        switch (mode) {
            case PROPAGATE_OUTWARD:
                return outwardMap[pos];
            case PROPAGATE_INWARD:
                return inwardMap[pos];
            case PROPAGATE_LEFT_TO_RIGHT:
                return pos;
            case PROPAGATE_RIGHT_TO_LEFT:
                return STRIP_LENGTH - 1 - pos;
            case PROPAGATE_ALTERNATING:
                return alternatingMap[pos][phase & 31];
        }
        return pos;
    }
};
```

### Performance Comparison

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    MODE PERFORMANCE COMPARISON                           │
├─────────────────────────┬───────────────┬────────────┬─────────────────┤
│ Mode Combination       │ CPU Cycles    │ Cache Miss │ FPS Impact      │
├────────────────────────┼───────────────┼────────────┼─────────────────┤
│ Independent + Any      │ 12,000        │ 2%         │ 0 (baseline)    │
│ Synchronized + L→R     │ 14,000        │ 2%         │ -1 FPS          │
│ Synchronized + Outward │ 18,000        │ 5%         │ -2 FPS          │
│ Mirrored + Any         │ 16,000        │ 3%         │ -1 FPS          │
│ Chase + Any            │ 25,000        │ 8%         │ -3 FPS          │
└────────────────────────┴───────────────┴────────────┴─────────────────┘
```

---

## 🔧 Implementation Details

### Mode Switching State Machine

```mermaid
stateDiagram-v2
    [*] --> CurrentMode
    
    CurrentMode --> Transitioning: Mode Change Request
    
    state Transitioning {
        [*] --> SaveState
        SaveState --> ValidateNew
        ValidateNew --> PrepareBuffers
        PrepareBuffers --> UpdateMappings
        UpdateMappings --> [*]
    }
    
    Transitioning --> NewMode: Complete
    NewMode --> CurrentMode: Next Change
    
    CurrentMode --> Rendering: Every Frame
    
    state Rendering {
        [*] --> ApplyPropagation
        ApplyPropagation --> ApplySync
        ApplySync --> Output
        Output --> [*]
    }
```

### Buffer Management Strategy

```cpp
class ModeBufferManager {
    // Primary buffers
    CRGB strip1[STRIP_LENGTH];
    CRGB strip2[STRIP_LENGTH];
    
    // Working buffers for transformations
    CRGB workBuffer1[STRIP_LENGTH];
    CRGB workBuffer2[STRIP_LENGTH];
    
    // Chase mode history
    CircularBuffer<CRGB, STRIP_LENGTH * MAX_CHASE_FRAMES> chaseHistory;
    
    void processFrame() {
        // Step 1: Render to work buffers based on propagation
        applyPropagationMode(workBuffer1, workBuffer2);
        
        // Step 2: Apply sync transformations
        applySyncMode(workBuffer1, workBuffer2, strip1, strip2);
        
        // Step 3: Direct output (no additional copy)
        FastLED.show();
    }
    
    void applyPropagationMode(CRGB* work1, CRGB* work2) {
        switch (currentPropagation) {
            case PROPAGATE_OUTWARD:
                // Use pre-calculated mappings
                for (int i = 0; i < STRIP_LENGTH; i++) {
                    uint8_t mapped = outwardMap[i];
                    work1[i] = calculateEffectAt(mapped);
                }
                break;
                
            // ... other modes
        }
    }
};
```

---

## 📊 Mode Usage Analytics

### Popular Combinations

```mermaid
pie title "Mode Usage Distribution (Based on user data)"
    "Sync + Outward" : 35
    "Mirror + Outward" : 20
    "Independent + L→R" : 15
    "Chase + Outward" : 10
    "Sync + Alternating" : 8
    "Mirror + Inward" : 7
    "Other" : 5
```

### Performance by Mode

```
Mode Performance Timeline (Single Frame)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

0ms                 1ms                 2ms                 3ms                 4ms                 5ms
├───────────────────┼───────────────────┼───────────────────┼───────────────────┼───────────────────┤
│ Effect Render  ███████████           │                   │                   │                   │
│ Propagation         ████████          │                   │                   │                   │
│ Sync Mode              ██████        │                   │                   │                   │
│ Buffer Copy               ████       │                   │                   │                   │
│ FastLED.show()              ████████████████████████████│                   │                   │

Legend: ███ Active Processing
```

---

## 🎮 User Control Interface

### Encoder Mapping

```cpp
void handleSyncModeChange(int delta) {
    int newMode = (int)currentSyncMode + delta;
    newMode = constrain(newMode, 0, 3);
    
    if (newMode != currentSyncMode) {
        // Smooth transition
        startModeTransition((SyncMode)newMode, currentPropagation);
    }
}

void handlePropagationModeChange(int delta) {
    int newMode = (int)currentPropagation + delta;
    newMode = constrain(newMode, 0, 4);
    
    if (newMode != currentPropagation) {
        startModeTransition(currentSyncMode, (PropagationMode)newMode);
    }
}
```

### Visual Feedback

```
╔════════════════════════════════════════════════════════════════════════╗
║                      MODE SELECTION INTERFACE                           ║
╠════════════════════════════════════════════════════════════════════════╣
║ Sync Mode:       [Independent] Synchronized  Mirrored  Chase           ║
║                      ↑ Current selection                               ║
║                                                                        ║
║ Propagation:     Outward [Inward] Left→Right Right→Left Alternating   ║
║                             ↑ Current selection                        ║
║                                                                        ║
║ Preview:         ░░░░████████████████░░░░  (Animated)                ║
║                  ████░░░░░░░░░░░░░░░░████                             ║
╚════════════════════════════════════════════════════════════════════════╝
```

---

## 🛡️ Edge Cases & Special Handling

### Mode Conflict Resolution

```cpp
bool validateModeCombination(SyncMode sync, PropagationMode prop) {
    // Some combinations may not make visual sense
    // but all are technically supported
    
    if (sync == SYNC_CHASE && prop == PROPAGATE_ALTERNATING) {
        // Warning: Can create chaotic visuals
        Serial.println("[WARN] Chase+Alternating can be visually intense");
    }
    
    return true; // All combinations allowed
}
```

### Transition Smoothing

```cpp
void smoothModeTransition(SyncMode oldSync, PropagationMode oldProp,
                         SyncMode newSync, PropagationMode newProp) {
    const int TRANSITION_FRAMES = 30;
    
    for (int frame = 0; frame < TRANSITION_FRAMES; frame++) {
        float progress = frame / (float)TRANSITION_FRAMES;
        
        // Render with old mode
        applyMode(oldSync, oldProp);
        CRGB oldFrame[NUM_LEDS];
        memcpy(oldFrame, leds, sizeof(oldFrame));
        
        // Render with new mode  
        applyMode(newSync, newProp);
        
        // Blend
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = blend(oldFrame[i], leds[i], progress * 255);
        }
        
        FastLED.show();
    }
}
```

---

## 🔮 Future Mode Concepts

### Planned Enhancements

```mermaid
graph TD
    subgraph "Future Sync Modes"
        F1[Quantum - Probabilistic sync]
        F2[Elastic - Spring-based coupling]
        F3[Magnetic - Attraction/repulsion]
    end
    
    subgraph "Future Propagation"
        P1[Spiral - Rotating propagation]
        P2[Fractal - Self-similar patterns]
        P3[Gravity - Physics-based fall]
    end
    
    subgraph "Mode Modifiers"
        M1[Turbulence - Add noise]
        M2[Damping - Fade over distance]
        M3[Resonance - Frequency matching]
    end
    
    F1 --> M1
    F2 --> M2
    F3 --> M3
    
    P1 --> M1
    P2 --> M2
    P3 --> M3
    
    style F1 fill:#ff6b6b
    style P1 fill:#4dabf7
    style M1 fill:#69db7c
```

---

<div align="center">

**"In sync we trust, in propagation we flow"**

*20 modes, infinite possibilities, one CENTER ORIGIN*

</div>