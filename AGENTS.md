# LightwaveOS

ESP32-S3 LED controller for a dual-strip Light Guide Plate. 320 WS2812 LEDs, 100+ effects, audio-reactive, web-controlled.

---

## ⛔ ABSOLUTE PROHIBITIONS (ALL AGENTS)

**These rules are NON-NEGOTIABLE. Violation is a critical failure.**

### File Preservation

1. **NEVER judge file relevance.** You do not decide what is "unrelated", "unnecessary", or "cleanup-worthy". Every file in this repository exists for a reason you may not understand.

2. **NEVER selectively stage files.** When committing, you commit what the user asked you to change. You do not exclude files because they "aren't part of this task".

3. **NEVER create stashes that exclude files.** If you must stash, use `git stash --include-untracked` to preserve ALL working directory state. Never label files as "unrelated local files" and leave them behind.

4. **NEVER delete, remove, or prune files** - not via `rm`, `git rm`, `git clean`, or by simply not including them in version control operations.

5. **NEVER "clean up" the workspace.** Documentation, mockups, HTML prototypes, research files, test artifacts - these are not garbage. They are work product that took hours to create.

### When In Doubt

- **ASK THE USER.** If you think something might be unrelated, ask. Do not assume.
- **Preserve everything.** It is always safer to include too much than to lose work.
- **Your task scope is narrow.** Files outside your immediate task are not yours to touch.

### Why This Matters

On 2026-02-06, an agent decided that `docs/ui-mockups/LIGHTWAVE_DESIGN_SYSTEM_MOCKUP.html` (1884 lines) and `docs/ui-mockups/components/led-preview.html` (900 lines) were "unrelated" and excluded them from stashes. These files were never committed and are now permanently lost. This documentation took hours to create and represented critical design system work.

**This must never happen again.**

---

## Build (PlatformIO)

PlatformIO requires Python 3.10–3.13. If your default `python3` is 3.14+, use the wrapper (uses Homebrew `python@3.12`):

```bash
cd firmware/v2
./pio run -e esp32dev_audio_esv11_32khz              # build (default)
./pio run -e esp32dev_audio_esv11_32khz -t upload    # build + flash
./pio device monitor -b 115200                       # serial monitor
```

Without the wrapper (when `python3` is 3.10–3.13):

```bash
cd firmware/v2
pio run -e esp32dev_audio_esv11_32khz
pio run -e esp32dev_audio_esv11_32khz -t upload
pio device monitor -b 115200
```

**Default build environment: `esp32dev_audio_esv11_32khz`** (32kHz/125Hz DSP pipeline).
The legacy `esp32dev_audio_esv11` (12.8kHz/50Hz) is retained for regression testing only.

## Hard Constraints

- **PSRAM MANDATORY**: All build environments MUST have PSRAM enabled (`-D BOARD_HAS_PSRAM`). The firmware requires 8MB PSRAM for DSP buffers, network stack, and effects. Internal SRAM (~320KB) is insufficient. Builds without PSRAM will crash with `ESP_ERR_NO_MEM`.
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
| **Memory Allocation** (MANDATORY) | [firmware/v2/docs/MEMORY_ALLOCATION.md](firmware/v2/docs/MEMORY_ALLOCATION.md) |
| System facts & data flow | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Audio-reactive protocol | [docs/audio-visual/audio-visual-semantic-mapping.md](docs/audio-visual/audio-visual-semantic-mapping.md) |
| Full REST API reference | [docs/api/api-v1.md](docs/api/api-v1.md) |
| Harness worker mode | [.claude/harness/HARNESS_RULES.md](.claude/harness/HARNESS_RULES.md) |
