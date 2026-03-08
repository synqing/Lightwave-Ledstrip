# Multi-Worktree Parallel Testing Guide

## What We Discovered

On 2026-03-06, we deployed a workflow that allows **simultaneous side-by-side testing of two different firmware states on separate physical hardware**, with identical instrumentation injected into both codebases independently. This eliminates the need to merge, rebase, or sequentially test — you compare builds in real time, on real hardware, with real audio input.

This is not theoretical. We ran it. Two K1 devices, two firmware builds (main HEAD vs milestone branch), both streaming LED frame data with v2 metrics at 15 FPS simultaneously over serial. The captures produce animated GIFs that replay exactly what the strips showed, with beat detection, RMS, spectral bands, and heap telemetry embedded in every frame.

## Why This Matters

### The Old Way
1. Build firmware A, flash, test manually, take notes
2. Build firmware B, flash same device, test manually, take notes
3. Try to compare from memory or screenshots
4. Repeat when you realise you forgot to test something

**Problems:** You can never see both running at the same time. Manual observation is subjective. You lose the first build's state the moment you flash the second. Context switching destroys the comparison.

### The New Way
1. Main repo has firmware A checked out
2. Git worktree has firmware B checked out in a separate directory
3. Add identical measurement instrumentation to both
4. Build both, flash to separate hardware
5. Capture from both simultaneously
6. Compare with objective data

**What changes:** You see both builds running live, side by side. The comparison is objective (pixel data, beat correlation, timing metrics). You can re-run the comparison any time without reflashing. You can change what you're measuring without changing what you're testing.

## Quick-Start Checklist

Ten commands from zero to first comparison capture. Assumes two K1 devices connected, main repo at the project root, and a target commit or branch to compare against.

```bash
# 1. Create the worktree at the comparison commit
git worktree add /tmp/comparison-baseline <commit-or-branch>

# 2. Build primary firmware (K1v2, GPIO 6/7)
cd firmware-v3 && ./.venv-pio/bin/pio run -e esp32dev_audio_esv11_k1v2_32khz

# 3. Build comparison firmware (original K1, GPIO 4/5) — uses main repo's PlatformIO
cd /tmp/comparison-baseline/firmware-v3 && \
  /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/.venv-pio/bin/pio \
  run -e esp32dev_audio_esv11

# 4. Flash primary K1v2
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3 && \
  ./.venv-pio/bin/pio run -e esp32dev_audio_esv11_k1v2_32khz \
  -t upload --upload-port /dev/cu.usbmodem212401

# 5. Flash comparison K1
cd /tmp/comparison-baseline/firmware-v3 && \
  /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/.venv-pio/bin/pio \
  run -e esp32dev_audio_esv11 -t upload --upload-port /dev/cu.usbmodem21401

# 6. Wait for both boards to reboot (~5 seconds after flash completes)
sleep 5

# 7. Activate the capture tool venv
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/tools/led_capture && \
  source .venv/bin/activate

# 8. Run a 15-second side-by-side comparison capture
python led_capture.py \
  --serial /dev/cu.usbmodem212401 \
  --compare-serial /dev/cu.usbmodem21401 \
  --duration 15 \
  --label "Main HEAD" --compare-label "Baseline" \
  -o comparison.png

# 9. (Optional) Capture a GIF from the primary device
python led_capture.py --serial /dev/cu.usbmodem212401 --duration 15 -o primary.gif

# 10. Clean up the worktree when finished
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip && \
  git worktree remove /tmp/comparison-baseline
```

**Before you start:** Verify port assignments with `ls /dev/cu.usbmodem*`. If ports differ from the above, substitute throughout. K1v2 MAC: `b4:3a:45:a5:87:f8`, secondary K1 MAC: `b4:3a:45:a5:89:b4`.

## The Setup

### Prerequisites
- Two physical K1 devices (different USB ports)
- Git worktree checked out at the comparison commit
- `tools/led_capture/` Python tool with venv

### Device Mapping

| Role | Device | USB Port | Build Environment | GPIO |
|------|--------|----------|-------------------|------|
| Primary (current development) | K1v2 | `/dev/cu.usbmodem212401` | `esp32dev_audio_esv11_k1v2_32khz` | 6/7 |
| Comparison (baseline/milestone) | K1 | `/dev/cu.usbmodem21401` | `esp32dev_audio_esv11` | 4/5 |

**Critical:** The two boards use different GPIO pins. Always match the build environment to the hardware. K1v2 = `k1v2_32khz` (GPIO 6/7). Original K1 = generic `esv11` (GPIO 4/5). Wrong env = dark strips.

### Creating the Worktree

```bash
# From the main repo root
git worktree add /tmp/comparison-baseline <commit-or-branch>

# Example: compare against a specific tag
git worktree add /tmp/comparison-baseline stage1-d14-r3-stable-2026-03-05

# Example: compare against a feature branch
git worktree add /tmp/comparison-baseline milestone/deferred-runtime-hardening-2026-03
```

The worktree shares git history with the main repo (tags, branches, refs all work) but has its own independent working directory and staging area. Changes in one do not affect the other.

### Building Both Firmwares

```bash
# Primary (main repo)
cd firmware-v3
./pio run -e esp32dev_audio_esv11_k1v2_32khz

# Comparison (worktree) — use main repo's PlatformIO venv
cd /tmp/comparison-baseline/firmware-v3
/path/to/main/firmware-v3/.venv-pio/bin/pio run -e esp32dev_audio_esv11
```

**Gotcha:** The worktree may not have its own `.venv-pio`. Point to the main repo's PlatformIO installation rather than creating a duplicate.

### Flashing Both Devices

```bash
# Primary K1v2
./pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem212401

# Comparison K1
/path/to/main/.venv-pio/bin/pio run -e esp32dev_audio_esv11 -t upload --upload-port /dev/cu.usbmodem21401
```

**Note:** Opening the USB CDC serial port resets the ESP32-S3. After flashing, the board reboots automatically. The capture tool accounts for this with a 3-second stabilisation wait.

## The Instrumentation Layer

### Capture Streaming Protocol (v2)

Both firmware builds must have the `capture stream` command. This is a serial command handler addition in `main.cpp` — it lives entirely in the debug/command infrastructure and does not touch the rendering, effect, or audio paths being compared.

**Firmware serial commands:**
```
capture stream b 15    # Start streaming tap B at 15 FPS
capture stop           # Stop streaming
capture fps 20         # Change FPS mid-stream
capture tap c          # Switch to tap C (pre-WS2812)
capture format v1      # Switch to v1 (no metrics trailer)
capture status         # Show current capture state
```

**Three tap points available:**
- **Tap A** — Post-effect, pre-colour-correction. Raw effect output.
- **Tap B** — Post-correction. What the LGP "sees" after gamma/white balance. **Default and recommended.**
- **Tap C** — Pre-WS2812. Final data sent to the LED driver, including any per-strip reordering.

### Frame Format (v2, 1008 bytes)

```
Header (16 bytes):
  [0]     0xFD magic
  [1]     0x02 version
  [2]     tap point (0=A, 1=B, 2=C)
  [3]     effect ID
  [4]     palette ID
  [5]     brightness (0-255)
  [6]     speed (0-255)
  [7:11]  frame index (u32 LE) — monotonic, gaps indicate dropped frames
  [11:15] timestamp microseconds (u32 LE)
  [15:17] payload length (u16 LE, always 960)

RGB Payload (960 bytes):
  320 LEDs × 3 bytes (R, G, B) — strip 0 first (LEDs 0-159), then strip 1 (LEDs 160-319)

Metrics Trailer (32 bytes, v2 only):
  [0:2]   show time in microseconds (u16 LE) — FastLED.show() duration
  [2:4]   RMS audio level (u16 LE, 0-65535 maps to 0.0-1.0)
  [4:12]  octave bands[8] (u8 each, 0-255 maps to 0.0-1.0)
  [12]    beat tick (1 = beat detected this frame)
  [13]    onset tick (1 = snare or hihat onset this frame)
  [14:16] spectral flux (u16 LE, 0-65535)
  [16:20] free heap bytes (u32 LE)
  [20:22] show skips cumulative (u16 LE)
  [22:32] reserved (zeros)
```

### Adding Instrumentation to a New Build

When comparing against a build that doesn't have `capture stream`, you need to add it. The change is confined to `main.cpp`:

1. **Add state variables** (7 static globals for streaming control)
2. **Add streaming tick** in `loop()` after `uint32_t now = millis()` — reads existing capture buffers and writes binary to serial
3. **Add command handlers** for `stream`, `stop`, `fps`, `tap`, `format` in the capture command block

The v2 metrics trailer reads from:
- `renderer->getLedDriverStats()` — show time, frame count, skip count
- `renderer->getBandsDebugSnapshot()` — bands[8], RMS, flux (lock-free double-buffered)
- `renderer->copyCachedAudioFrame()` — beat tick, onset triggers
- `esp_get_free_heap_size()` — heap (register read, NOT heap walk)

**None of these affect the renderer.** They read cached/snapshotted data via existing thread-safe accessors.

## Running Captures

### Python Capture Tool

```bash
cd tools/led_capture
source .venv/bin/activate

# Single device, 15-second GIF at 15 FPS
python led_capture.py --serial /dev/cu.usbmodem212401 --duration 15 -o capture.gif

# Side-by-side comparison PNG (both devices simultaneously)
python led_capture.py \
    --serial /dev/cu.usbmodem212401 \
    --compare-serial /dev/cu.usbmodem21401 \
    --duration 30 \
    --label "Main HEAD" --compare-label "Milestone" \
    -o comparison.png

# MP4 video with real-time playback
python led_capture.py --serial /dev/cu.usbmodem212401 --duration 30 -o capture.mp4
```

### What the GIF Shows

Each frame renders the two LED strips as they appear physically — 160 coloured rectangles per strip, top and bottom, animated at real capture timing. Time flows forward through the GIF. Beat events, audio reactivity, effect transitions, and colour changes are all visible as they happened on the actual hardware.

### Interpreting the Output

**Visual quality indicators:**
- Smooth colour gradients = healthy rendering pipeline
- Symmetric patterns around centre (LED 79/80) = centre-origin constraint respected
- Responsive brightness changes to audio = good audio-reactive coupling
- Consistent frame timing (no visible stutter) = stable render loop

**Regression indicators:**
- Frozen or repeated frames = render stall or frame drops (check `frame_idx` gaps)
- Asymmetric patterns = centre-origin violation
- Sluggish response to audio = audio pipeline latency
- Sudden brightness drops to black = silence gate triggering incorrectly

## When to Use This Workflow

### High-Value Scenarios

1. **Before merging a feature branch.** Flash the branch to one K1, main to the other. Capture side-by-side. Objective visual comparison before the merge.

2. **Regression hunting.** Flash the known-good tag to one device, suspect commit to the other. Capture simultaneously. The diff is visual and immediate.

3. **Effect tuning.** Same effect, two different parameter sets (or two versions of the effect code). Compare the visual output directly rather than A/B testing from memory.

4. **Audio backend comparison.** PipelineCore vs ESV11 on the same effect, side by side. Do beats land differently? Is the spectrum response tighter?

5. **Performance validation after refactoring.** The v2 metrics trailer includes show time, heap, and skip count. Compare these between the old and new code while the visual output confirms nothing regressed.

### When NOT to Use This

- Trivial changes (typo fixes, comment updates)
- Changes that don't affect visual output or performance
- When you only have one device available (use sequential testing with tagged builds instead)

## Constraints and Gotchas

1. **Both K1s broadcast as APs at 192.168.4.1.** You cannot connect to both via WiFi simultaneously. You cannot use WebSocket capture from both at once. Serial is the only viable dual-device transport.

2. **Connecting to a K1's AP drops internet.** This kills Claude sessions, package managers, and anything requiring connectivity. Never attempt WebSocket capture during an interactive Claude session.

3. **Serial port opening resets ESP32-S3.** The DTR/RTS toggle on USB CDC causes a board reset. The capture tool handles this (3-second stabilisation wait), but be aware the device reboots when you start capturing.

4. **USB port assignments can change.** After a board reset or USB reconnect, the port may shift. Always verify which port maps to which device before flashing. K1v2 MAC: `b4:3a:45:a5:87:f8`, secondary K1 MAC: `b4:3a:45:a5:89:b4`.

5. **Worktrees share the git object database.** Running `git gc` or `git prune` from either directory affects both. Tags and branches created in one are visible from the other.

6. **PlatformIO build directories are per-worktree.** Each worktree gets its own `.pio/build/`. This is correct — the builds should be independent. But the toolchain (`~/.platformio/`) is shared.

## Troubleshooting

Every failure mode encountered so far, with fixes.

### Port busy — "Resource busy" or "Permission denied"

**Symptom:** Flash or capture fails with `Resource busy` or the serial port cannot be opened.

**Cause:** Another process is holding the TTY. Common culprits are Cursor's built-in serial monitor, `screen`, `minicom`, or a previous capture tool instance that did not exit cleanly.

**Fix:**
```bash
# Find what is holding the port
lsof /dev/tty.usbmodem*

# Kill the offending process, or close the serial monitor tab in your IDE
kill <pid>
```

### Dark strips — wrong GPIO mapping

**Symptom:** Firmware flashes and boots (serial output looks normal), but one or both LED strips stay dark.

**Cause:** Build environment does not match the hardware. K1v2 uses GPIO 6/7 for LED data; original K1 uses GPIO 4/5. Flashing the wrong env sends data to pins that are not connected to strips.

**Fix:** Verify the build environment matches the device:
- K1v2 hardware → `esp32dev_audio_esv11_k1v2_32khz`
- Original K1 hardware → `esp32dev_audio_esv11`

Rebuild with the correct environment and reflash. There is no runtime override — GPIO assignment is a compile-time constant.

### PEP 668 — pip refuses to install packages

**Symptom:** `pip install` fails with `externally-managed-environment` error on Python 3.12+.

**Cause:** macOS and Homebrew Python enforce PEP 668, which prevents installing packages into the system Python.

**Fix:** Always use a virtual environment:
```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

The capture tool's venv already exists at `tools/led_capture/.venv/`. Activate it rather than installing globally.

### Missing PlatformIO in worktree

**Symptom:** `pio: command not found` or `No such file or directory` when building in the worktree.

**Cause:** The worktree does not have its own `.venv-pio/` directory. PlatformIO was installed in the main repo's venv, not globally.

**Fix:** Point to the main repo's PlatformIO binary directly:
```bash
/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/.venv-pio/bin/pio \
  run -e esp32dev_audio_esv11
```

Do not create a second PlatformIO venv in the worktree — it wastes disk and diverges from the known-working toolchain.

### Flash completes but "no serial data received"

**Symptom:** PlatformIO reports successful upload, but serial monitor shows nothing, or the board appears unresponsive.

**Cause:** The ESP32-S3 USB CDC interface occasionally fails to re-enumerate after flash. The board is running but the USB link is stale.

**Fix:**
1. Press the physical **RST** button on the board.
2. If that does not help, unplug the USB cable, wait 2 seconds, and replug.
3. Verify the port reappears: `ls /dev/cu.usbmodem*`

### Board resets when serial port is opened

**This is expected behaviour**, not a bug. Opening the USB CDC serial port toggles DTR/RTS, which the ESP32-S3 interprets as a reset signal. The capture tool accounts for this with a 3-second stabilisation wait after opening the port. If you open the port manually (e.g., `screen`), expect a reboot and a brief delay before output appears.

### WebSocket capture drops internet connection

**Symptom:** Running a WebSocket-based capture causes your Mac to lose internet access.

**Cause:** Both K1 devices broadcast as WiFi access points at `192.168.4.1`. Connecting to a K1's AP makes your Mac route traffic through an AP that has no upstream internet. macOS may also auto-join the open K1 network.

**Fix:** Do not use WebSocket capture for dual-device comparison. Serial is the only viable dual-device transport. If your Mac auto-joins the K1 AP, remove it from the known networks list in System Settings > Wi-Fi.

### Python 3.14+ breaks PlatformIO

**Symptom:** PlatformIO crashes or fails to import modules on Python 3.14 or later.

**Cause:** PlatformIO has not yet updated for Python 3.14 API changes.

**Fix:** Use the `./pio` wrapper script in the firmware-v3 directory, which selects a compatible Python version. Alternatively, ensure your PlatformIO venv was created with Python 3.12 or 3.13.

### "No frames captured" — capture tool exits with zero frames

**Symptom:** The capture tool runs for the specified duration but reports no frames received.

**Checklist:**
1. **Correct port?** Verify with `ls /dev/cu.usbmodem*` and cross-check MAC addresses.
2. **Board powered and running?** Check for serial output: `screen /dev/cu.usbmodem212401 115200` (then `Ctrl-A K` to exit).
3. **Capture streaming firmware?** The build must include the `capture stream` command handler. If comparing against an older build that lacks it, you need to add the instrumentation layer (see "Adding Instrumentation to a New Build" above).
4. **Streaming started?** The capture tool sends `capture stream b 15` automatically. If the board rebooted mid-capture (DTR/RTS), the stream command may not have been received. Restart the capture.
5. **Baud rate mismatch?** The capture tool defaults to 115200. If the firmware uses a different rate, pass `--baud`.

## The Principle

**Isolate the code under test. Share the measurement infrastructure.**

The worktree gives you isolation — two complete, independent firmware states that cannot interfere with each other. The capture streaming protocol gives you shared measurement — identical instrumentation producing identical data formats from both builds. The Python tool gives you comparison — objective, repeatable, visual.

This pattern is not specific to firmware. Any time you need to compare two code states with confidence, the structure is the same: separate working directories, shared measurement tools, simultaneous execution, objective comparison.

## Onwards: What to Build Next

### Immediate (Next Session)

1. ~~**Quick-start checklist.**~~ **DONE** — see "Quick-Start Checklist" section above.

2. ~~**Troubleshooting section.**~~ **DONE** — see "Troubleshooting" section above.

3. **Beat-brightness correlation score.** The v2 metrics trailer already streams beat flags and RMS per frame. Build a Python analyser that computes a single scalar: "how tightly does brightness respond to beats?" This is the killer metric for audio-reactive quality comparison. A correlation coefficient between beat events and brightness spikes across the capture window. Higher = tighter response. Compare the score between builds.

### Short-Term

4. **Frame drop detector.** The `frameIndex` field is monotonic. Gaps mean the renderer dropped frames. Build a simple report: total drops, longest gap, drops-per-minute. This catches stutter regressions that are invisible in a GIF at 15 FPS but felt at 120 FPS on the physical hardware.

5. **Metrics dashboard PNG.** Alongside the strip-view GIF, generate a static PNG with time-series sparklines: RMS, bands heatmap, beat markers, heap free, show time. One image that summarises the entire capture session's health. Side-by-side dashboards for comparison captures.

6. **Automated regression gate.** Define pass/fail thresholds: frame drop rate < 0.1%, heap never below X, show time never above Y, beat correlation above Z. Run captures, compute scores, emit pass/fail. This is the foundation of a CI-testable visual quality gate — even without CI, it gives objective go/no-go for merges.

### Medium-Term

7. **Three-way comparison.** Add a third worktree. Compare main HEAD vs feature branch vs known-good baseline simultaneously. Requires a third K1 or sequential capture with tagging, but the Python tooling already supports N-device comparison by design.

8. **Effect-specific capture profiles.** Automate: switch to effect X via serial, wait 2 seconds for stabilisation, capture 10 seconds, switch to effect Y, repeat. Build a standardised "effect gallery" capture that exercises the full effect library across both builds. Diff the galleries.

9. **Temporal alignment.** Both devices hear the same audio (same room, same speaker). The beat flags in the v2 trailer can be used to time-align the two capture streams post-hoc, even if serial opening caused slightly different start times. This makes frame-by-frame comparison meaningful for audio-reactive effects.

10. **iOS companion integration.** The capture tool produces GIFs and PNGs. The iOS app could display these as "capture reports" — flash both K1s from the app, trigger a capture, view the comparison on your phone. This moves the workflow from developer tooling to product QA.
