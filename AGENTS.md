# LightwaveOS

ESP32-S3 LED controller for a dual-strip Light Guide Plate. 320 WS2812 LEDs, 100+ effects, audio-reactive, web-controlled.

## Build (PlatformIO)

```bash
cd firmware/v2
pio run -e esp32dev_audio              # build
pio run -e esp32dev_audio -t upload    # build + flash
pio device monitor -b 115200           # serial monitor
```

## Hard Constraints

- **Centre origin**: All effects originate from LED 79/80 outward (or inward to 79/80). No linear sweeps.
- **No rainbows**: No rainbow cycling or full hue-wheel sweeps.
- **No heap alloc in render**: No `new`/`malloc`/`String` in `render()` paths. Use static buffers.
- **120 FPS target**: Keep per-frame effect code under ~2 ms.
- **British English** in comments and docs (centre, colour, initialise).

## Further Docs

Read these when the task requires it:

| Topic | File |
|-------|------|
| Architecture & key files | [docs/agent/ARCHITECTURE.md](docs/agent/ARCHITECTURE.md) |
| Effects & IEffect API | [docs/agent/EFFECTS.md](docs/agent/EFFECTS.md) |
| Build variants & OTA | [docs/agent/BUILD.md](docs/agent/BUILD.md) |
| Network, API & serial | [docs/agent/NETWORK.md](docs/agent/NETWORK.md) |
| Protected files (landmines) | [docs/agent/PROTECTED_FILES.md](docs/agent/PROTECTED_FILES.md) |
| Agents, skills & harness | [docs/agent/AGENTS_AND_TOOLS.md](docs/agent/AGENTS_AND_TOOLS.md) |
| Timing & memory budgets | [CONSTRAINTS.md](CONSTRAINTS.md) |
| System facts & data flow | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Audio-reactive protocol | [docs/audio-visual/audio-visual-semantic-mapping.md](docs/audio-visual/audio-visual-semantic-mapping.md) |
| Full REST API reference | [docs/api/api-v1.md](docs/api/api-v1.md) |
| Harness worker mode | [.claude/harness/HARNESS_RULES.md](.claude/harness/HARNESS_RULES.md) |
