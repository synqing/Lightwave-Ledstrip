# Commit Context — 12 Mar 2026

This note is a developer-facing companion to the commit series on branch `feature/voice-control-exploration`.
It is intended to serve as both a historical record and a quick onboarding reference for future contributors.

## Overview (what changed)

- **Repo hygiene**: ignore local media/office artefacts so they are never committed by accident.
- **Docs/process**: hard gate rules for clangd/QMD/Context7, session-start checklist, and tool failure protocol.
- **Firmware**:
  - New **AMOLED 2.41" test rig** PlatformIO environments and board pin map (`BOARD_AMOLED241`).
  - New **DisplayActor** and RM690B0 QSPI driver for a 5-panel diagnostic display (no LVGL).
  - New **motion-semantics scaffolding**: ControlBus extension fields + a MotionSemanticEngine and effect accessors.
  - Temporary **tempo ground-truth log** gated by `LW_TEMPO_GT_LOG`.
- **iOS**: rename “Pattern” UI pill to “Effect” (and update Rive registry symbol).

## Commit-by-commit rationale

### `chore(repo): ignore local media and office artefacts`

- **Problem**: Large local videos/spreadsheets and office lockfiles existed as untracked work artefacts and were at high risk of accidental commit.
- **Solution**: Add explicit `.gitignore` patterns for known local artefacts (K1 media + tracker spreadsheet + lock file + local folder with a space).
- **Alternatives**: blanket `*.mp4`/`*.mov` ignores were rejected to avoid hiding real product assets.
- **Gotchas**:
  - Paths with spaces require escaping in `.gitignore` (e.g., `CN\ 3/`).

### `docs(tooling): tighten workflow gates and status`

- **Problem**: Investigations were slower and lower quality when done via grep-first and ad-hoc doc reading; toolchain status was not clearly communicated.
- **Solution**: Codify hard routing rules and operational status in `CLAUDE.md` and `docs/WORKFLOW_ROUTING.md`.
- **Maintenance**:
  - Update the “Status” blocks when clangd/QMD indexing changes (entry counts, scripts, prerequisites).
  - Keep constraints text aligned with firmware invariants (AP-only, centre origin, no heap alloc in render).

### `feat(firmware-board): add amoled241 test rig env`

- **Problem**: Need a repeatable, headless target to run full firmware + audio pipeline + diagnostics without LEDs.
- **Solution**:
  - Add PlatformIO environments `amoled_testrig` and `amoled_testrig_esv11_32khz`.
  - Introduce `BOARD_AMOLED241` and route `chip_config.h` to `chip_amoled241.h`.
- **Performance implications**:
  - Pinout and clocks influence I2S timing and DSP cadence; verify hop rates in serial telemetry.
- **Security considerations**:
  - Explicitly preserves **AP-only** behaviour (no STA mode).

### `feat(firmware-display): add amoled diagnostic actor`

- **Problem**: Need on-device diagnostics for rendering/audio health without LED hardware.
- **Solution design**:
  - `DisplayActor` (Core 0, lowest priority) reads renderer snapshots and draws a 5-panel view at ~30 FPS.
  - `RM690B0Driver` is a minimal, single-task QSPI driver using ESP-IDF SPI master (no LVGL).
- **Key technical choices**:
  - **Single-task SPI**: driver is intentionally not thread-safe; the actor owns all transactions.
  - **Render pacing**: 33ms tick to keep the 120 FPS renderer budget stable.
- **Known limitations / follow-ups**:
  - Confirm memory placement for the heatmap buffer. Comments imply PSRAM, but large member arrays may still land in internal RAM depending on toolchain/attributes. If internal RAM pressure is observed, refactor to PSRAM allocation via `heap_caps_malloc(MALLOC_CAP_SPIRAM)` for the heatmap storage.
- **Testing methodology**:
  - `pio run -e amoled_testrig`
  - Runtime smoke: actor start/stop logs; verify metrics panel updates and no renderer cadence regressions.

### `feat(audio): add motion-semantics plumbing`

- **Problem**: Effects need a stable semantic interface (Layer 2) rather than consuming raw DSP directly.
- **Solution design**:
  - Extend `ControlBusFrame` with `timing_jitter`, `syncopation_level`, `pitch_contour_dir`.
  - Compute derivations inside `ControlBus::applyDerivedFeatures()`.
  - Provide `MotionSemanticEngine` that maps ControlBus data to Laban Effort axes + modifiers with smoothing.
  - Add effect-facing accessors in `EffectContext::AudioContext`.
- **Alternative approaches considered**:
  - Doing inference inside each effect (rejected; duplicates logic and increases cost).
  - Pulling in a heavyweight UI/ML framework (rejected; must remain deterministic and MCU-friendly).
- **Performance implications**:
  - Must remain heap-free on render paths; smoothing uses fixed objects.
- **Known limitations / pitfalls**:
  - FLOW is currently neutral until a validated correlate exists.
  - Accessors exist, but downstream wiring may still be required to populate `AudioContext.motionFrame` during render; verify in `RendererActor` integration points.

### `chore(audio): gate temporary tempo ground-truth log`

- **Problem**: Need periodic raw tempo dumps for validation; always-on Serial output is noisy.
- **Solution**: Guard with `LW_TEMPO_GT_LOG` compile-time flag.
- **Troubleshooting**:
  - If you don’t see `[TEMPO_GT]`, confirm the env includes `-D LW_TEMPO_GT_LOG=1` and you are on-device (not `NATIVE_BUILD`).
- **Removal plan**:
  - Delete this once tempo validation is complete (keep the instrumentation out of release builds).

### `refactor(play): rename pattern pill to effect pill`

- **Problem**: UI terminology mismatch (“Pattern” vs “Effect”) causes confusion and drift.
- **Solution**:
  - Replace `PatternPill` with `EffectPill`, update `PlayTab`, update Rive registry symbol.
- **Testing methodology**:
  - Xcode build; confirm `EffectSelectorView` opens and Rive accent renders.
- **Pitfalls**:
  - Keep registry symbols and call-sites aligned (`effectPillAccent`).

### `docs(ios): tighten BeatPulse demo labels`

- **Problem**: Demo title overly verbose.
- **Solution**: Rename string only (no functional impact).

## Cross-cutting lessons learned

- **Spaces in paths**: use escaped patterns in `.gitignore` and avoid quoting.
- **Diagnostic code**: when in doubt, make debug output opt-in behind compile-time flags and isolate it in its own commit.
- **Display + render timing**: keep diagnostic rendering decoupled from the 120 FPS renderer cadence; low-priority, fixed-rate actors are safer than adding work to the render loop.
- **Semantic layers**: keep DSP-to-semantics mapping centralised (ControlBus + engine) to reduce duplicated computation and drift across effects.

## Improvement areas (future work)

- **Display memory placement**: verify PSRAM usage for large buffers; add attributes/allocations as needed.
- **Motion semantics integration**: ensure `AudioContext.motionFrame` is populated deterministically per render cycle (likely inside `RendererActor`), and add a minimal validation harness that compares derived fields over known stimuli.

