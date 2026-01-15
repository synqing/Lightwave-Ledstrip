# Audio-Visual Processing Documentation

**LightwaveOS v2 Audio-Visual Pipeline Reference**

This documentation suite provides comprehensive coverage of the audio-visual processing system in LightwaveOS v2, designed for developers creating audio-reactive LED visualization effects.

---

## Quick Start

If you're new to the codebase:
1. Start with [IMPLEMENTATION_PATTERNS.md](IMPLEMENTATION_PATTERNS.md) - copy-paste ready code
2. Reference [TROUBLESHOOTING.md](TROUBLESHOOTING.md) when issues arise
3. Deep-dive into specifications as needed

---

## Document Index

### Core Specifications

| Document | Purpose | When to Read |
|----------|---------|--------------|
| [AUDIO_OUTPUT_SPECIFICATIONS.md](AUDIO_OUTPUT_SPECIFICATIONS.md) | Complete audio pipeline reference | Understanding audio data flow, accessors, timing |
| [VISUAL_PIPELINE_MECHANICS.md](VISUAL_PIPELINE_MECHANICS.md) | Render architecture and timing | Understanding frame timing, buffers, smoothing |
| [COLOR_PALETTE_SYSTEM.md](COLOR_PALETTE_SYSTEM.md) | 75-palette library reference | Color selection, dual-strip strategies |

### Practical Guides

| Document | Purpose | When to Read |
|----------|---------|--------------|
| [IMPLEMENTATION_PATTERNS.md](IMPLEMENTATION_PATTERNS.md) | Working code examples | Starting a new effect, need proven patterns |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Common issues and fixes | Debugging problems, unexpected behavior |

---

## Key Concepts Summary

### Audio Pipeline (8 Stages)

```
I2S Input → Clamping → Spike Detection → Zone AGC → Band Smoothing
     → RMS/Flux → Chord Detection → Saliency → Silence Detection → Output
```

**Critical Accessor:** `ctx.audio.controlBus.heavy_bands[]` - pre-smoothed, use for speed modulation

### Visual Pipeline (120 FPS)

```
Effect.render() → m_leds[320] → Zone Compositor → Hardware Buffers → FastLED
```

**Critical Formula:** `m_phase += speedNorm * 240.0f * smoothedSpeed * dt`

### CENTER ORIGIN (MANDATORY)

ALL effects originate from LEDs 79/80 (center) and propagate outward:
- `centerPairDistance(i)` - returns 0 at center, 79 at edges
- Outward motion: `sin(k*dist - phase)`
- Inward motion: `sin(k*dist + phase)`

### Smoothing Architecture

| For Speed | For Brightness |
|-----------|----------------|
| `heavy_bands` → Spring | AsymmetricFollower |
| ~200ms response | 50ms rise / 300ms fall |
| No extra smoothing! | MOOD-adaptive |

---

## File Locations

```
v2/
├── docs/audio-visual/              # This documentation
│   ├── README.md                   # You are here
│   ├── AUDIO_OUTPUT_SPECIFICATIONS.md
│   ├── VISUAL_PIPELINE_MECHANICS.md
│   ├── COLOR_PALETTE_SYSTEM.md
│   ├── IMPLEMENTATION_PATTERNS.md
│   └── TROUBLESHOOTING.md
├── src/audio/
│   ├── contracts/ControlBus.h      # Audio data structures
│   ├── contracts/ControlBus.cpp    # 8-stage processing pipeline
│   └── AudioTuning.h               # Tuning parameters
├── src/effects/
│   ├── enhancement/SmoothingEngine.h  # Smoothing primitives
│   ├── CoreEffects.h               # CENTER_ORIGIN helpers
│   ├── PatternRegistry.h           # Pattern taxonomy
│   └── ieffect/                    # Individual effects
├── src/palettes/
│   ├── Palettes_Master.h           # Palette flags and helpers
│   └── Palettes_MasterData.cpp     # 75 palette definitions
└── src/plugins/api/
    └── EffectContext.h             # ctx.* interface
```

---

## Contributing

When updating these documents:
1. Keep code examples copy-paste ready
2. Update version numbers
3. Maintain cross-references between documents
4. Add new troubleshooting entries as bugs are fixed
5. Include file:line references for source code

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2025-12-31 | Initial comprehensive documentation suite |

---

## See Also

- [docs/api/API_V1.md](../api/API_V1.md) - REST/WebSocket API documentation
- [CLAUDE.md](../../CLAUDE.md) - Development guidelines
