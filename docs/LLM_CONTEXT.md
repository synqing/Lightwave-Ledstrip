# LightwaveOS LLM Context

**Purpose**: Stable prefix file for LLM prompts. Include this instead of re-explaining the project.

**Version**: 1.0  
**Last Updated**: 2026-01-18

---

## Project Summary

LightwaveOS is ESP32-S3 firmware for a dual-strip Light Guide Plate (LGP) LED system. It drives 320 LEDs (2×160) with centre-origin effects, audio reactivity, and zone composition.

---

## Glossary

| Term | Definition |
|------|------------|
| **LGP** | Light Guide Plate — optical waveguide that creates interference patterns |
| **Centre origin** | Effects must originate from LEDs 79/80 and propagate outward |
| **IEffect** | Native effect interface (`init()`, `render()`, `cleanup()`, `getMetadata()`) |
| **Zone** | Configurable LED segment with independent effect/palette |
| **Actor** | Concurrent processing unit (RendererActor, AudioActor, etc.) |
| **TOON** | Token-Oriented Object Notation — compact format for LLM prompts |

---

## Architecture Map

```
firmware/v2/src/
├── core/actors/          # Actor system (Renderer, Audio, etc.)
├── effects/              # Legacy function effects
│   └── ieffect/          # Native IEffect classes
├── network/              # WebServer, WiFi, REST handlers
├── audio/                # Audio pipeline, beat detection
├── plugins/api/          # IEffect.h, EffectContext.h
└── palettes/             # Palette definitions
```

---

## Invariants (Non-Negotiable)

1. **Centre origin only**: All effects originate from LEDs 79/80, propagate outward (or converge inward)
2. **No rainbows**: No rainbow cycling or full hue-wheel sweeps
3. **No heap in render**: No `new`, `malloc`, `std::vector` growth, `String` concat in `render()` hot paths
4. **120 FPS target**: Keep per-frame work predictable
5. **British English**: Use British spelling (centre, colour, initialise)

---

## Constraints

| Constraint | Value |
|------------|-------|
| LED count | 320 (2×160) |
| Centre point | LEDs 79/80 |
| Target FPS | 120+ |
| ESP32-S3 SRAM | ~280KB available |
| Max effect state | Stack-allocated, no heap |

---

## Key Paths

| Purpose | Path |
|---------|------|
| Effects (IEffect) | `firmware/v2/src/effects/ieffect/` |
| Effect API | `firmware/v2/src/plugins/api/IEffect.h` |
| Renderer | `firmware/v2/src/core/actors/RendererActor.*` |
| WebServer | `firmware/v2/src/network/WebServer.*` |
| REST handlers | `firmware/v2/src/network/webserver/handlers/` |
| Dashboard | `lightwave-dashboard/src/` |
| Constraints | `constraints.md` |
| Architecture | `architecture.md` |

---

## Build Commands

```bash
# Build firmware (audio-enabled)
cd firmware/v2 && pio run -e esp32dev_audio

# Build + upload
cd firmware/v2 && pio run -e esp32dev_audio -t upload

# Serial monitor
pio device monitor -b 115200
```

---

## Serial Commands

| Command | Action |
|---------|--------|
| `s` | Print FPS/CPU status |
| `l` | List effects |
| `n/N` | Next/prev effect |
| `,/.` | Prev/next palette |
| `z` | Toggle zone mode |
| `validate <id>` | Run effect validation |

---

## Acceptance Check Rubric

When implementing or modifying effects, verify:

- [ ] **Centre origin**: Effect originates from LEDs 79/80
- [ ] **No rainbow**: No full hue-wheel cycling
- [ ] **No heap alloc**: No dynamic allocation in `render()`
- [ ] **Palette-aware**: Effect uses `ctx.palette`, not hardcoded colours
- [ ] **Zone-aware**: Effect respects `ctx.ledCount` and `ctx.zoneStart` if applicable
- [ ] **FPS maintained**: `validate <id>` shows no FPS regression
- [ ] **British English**: Comments use British spelling
- [ ] **Build passes**: `pio run -e esp32dev_audio` succeeds without errors

---

## See Also

- [AGENTS.md](../AGENTS.md) - Full agent workflow guide
- [CLAUDE.md](../CLAUDE.md) - Claude Code project instructions
- [constraints.md](../constraints.md) - Hard limits
- [architecture.md](../architecture.md) - Quick reference
- [docs/contextpack/README.md](contextpack/README.md) - Context Pack pipeline
