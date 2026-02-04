# Rive Integration — LightwaveOS iOS v2

## Summary
Rive is approved for **UI animation assets only**. It is not a general UI framework for layout, navigation, or core app logic. All Rive usage must bind explicit app data and emit explicit events where needed.

## Runtime Strategy
- **Default runtime:** Apple (Swift-first, experimental).
- **Fallback runtime:** Stable C++ runtime.
- Runtime selection uses a compile-time flag: `RIVE_RUNTIME_CPP`.
  - Default (no flag) uses Apple runtime.
  - Define `RIVE_RUNTIME_CPP` in Build Settings for the C++ runtime.

## Usage Rules
- Use Rive for **interactive visuals and micro‑interactions** only.
- Do **not** use Rive for:
  - Core navigation, lists, or long scrolling surfaces.
  - Critical accessibility-only content without native fallback.
  - Performance-critical paths without profiling.
- All assets must:
  - expose required inputs (booleans, numbers, text, triggers),
  - name inputs clearly,
  - declare explicit events for runtime callbacks.
  - provide a `tap` event when a view is intended to be interactive.

## Asset Contract
All assets must be listed in `RiveAssetRegistry` with:
- file name, artboard name, state machine name
- input names and types
- event names

## Performance & Profiling
- Treat each Rive view as a Metal-driven renderer.
- Profile with Instruments and include both app process and system render processes.
- Prefer **one Rive view per card/surface**, not per sub‑element.

## Accessibility
- Provide a native SwiftUI fallback for reduced motion.
- Maintain semantic labels in SwiftUI even when a Rive view is present.

## Rollout Targets (Current)
- Connection indicator (status bar)
- Connection sheet empty state
- Beat pulse
- Hero overlay
- Pattern + palette pill accents
- Audio tab micro‑interaction

## Version Pinning
- Rive runtime is pinned via SPM in `project.yml`.
- Upgrades require a short migration review and must update `RIVE_ASSET_CONTRACT.md` as needed.

## Upgrade Checklist
1. Review Rive release notes for breaking changes.
2. Update `project.yml` version pin and regenerate the Xcode project.
3. Run `RiveAssetTests` and ensure assets still load.
4. Re-validate performance in `RIVE_PERF_NOTES.md`.
5. Re-check `RIVE_ASSET_CONTRACT.md` naming and inputs.
