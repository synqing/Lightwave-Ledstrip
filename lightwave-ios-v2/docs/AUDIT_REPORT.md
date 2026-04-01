# LightwaveOS iOS v2 — Project Audit

Date: 3 February 2026
Scope: architecture, dependencies, performance, UX/accessibility, build/CI, tests, release risks.

## Executive Summary
The iOS v2 codebase is a clean SwiftUI-first foundation with strong visual direction and clear app-level state management. The main risks are a large single `AppViewModel` on `@MainActor`, heavy Canvas redraws, a lack of tests/CI, and incomplete accessibility coverage. Rive integration is viable but should be tightly scoped, version-pinned, and protected behind a runtime abstraction to manage churn.

## Architecture & State
**Strengths**
- Clear entry points and a single app root (`LightwaveOSApp` → `ContentView`).
- `@Observable` + `@MainActor` on ViewModels is consistent and easy to reason about.
- REST + WebSocket split is explicit and centralised.

**Risks**
- `AppViewModel` is a monolith: network, device discovery, WebSocket stream handling, logging, and child VMs in one class. This increases coupling and makes changes high risk.
- All state updates are on `@MainActor`, which can become a bottleneck as LED stream volume increases.
- Some UI components assume immediate data availability and do not guard against empty or transient states.

**Recommendations**
- Split `AppViewModel` responsibilities into focused services (connection manager, LED stream, discovery) and keep `AppViewModel` as orchestration only.
- Introduce throttled updates for high-frequency streams to avoid excessive SwiftUI invalidations.
- Add explicit “empty” and “loading” view states for user-facing screens.

## Performance Hotspots
**Findings**
- Canvas rendering in `HeroLEDPreview` and audio visualisations is the heaviest area.
- `HeroLEDPreview` currently renders only 160 LEDs (strip 1) and assumes 960 bytes are always available. This can be a correctness and performance issue as data volume increases.
- `TimelineView(.animation)` for audio canvases may run at high frequency without explicit throttling.

**Recommendations**
- Keep per-frame rendering under ~2 ms by reducing Canvas invalidations (batch updates, minimise state changes).
- Validate render ranges for LED data, and ensure strip pairing for the centre-origin constraint.
- Consider caching colour calculations or pre-mapping to reduce per-frame work.

## UX / Accessibility
**Findings**
- Some components honour `accessibilityReduceMotion` (e.g., `BeatPulse`, `HeroLEDPreview`), but coverage is inconsistent.
- Custom SwiftUI controls (e.g., sliders, pills, status indicators) lack explicit accessibility labels and values.
- No explicit VoiceOver hints or traits for key controls.

**Recommendations**
- Provide accessibility labels and values for all custom components.
- Ensure motion-heavy views provide reduced-motion fallback.
- Provide semantic grouping on cards and status elements.

## Build / CI
**Findings**
- No CI configuration or automated checks.
- XcodeGen is optional, but the project file can drift if manual edits occur.
- No automated linting or build validation.

**Recommendations**
- Add a minimal CI workflow to run `xcodebuild` on a simulator.
- Treat `project.yml` as the single source of truth and regenerate `xcodeproj` in CI.
- Document dependency pinning and update cadence for Rive.

## Testing
**Findings**
- `LightwaveOSTests` is empty.
- No coverage for network, view model state, or asset validation.

**Recommendations**
- Add unit tests for Rive asset loading and contract validation.
- Add a small set of ViewModel tests for critical state transitions (connection, audio beats).

## Release Risks
**High**
- Experimental Rive Apple runtime can introduce breaking changes.
- No automated tests means regressions can ship silently.

**Medium**
- Performance regressions under heavy WebSocket traffic.
- Accessibility regressions due to custom components.

**Low**
- Visual polish inconsistency between screens (mostly due to stubs).

## Immediate Priorities
1. Add Rive abstraction and pin the runtime version.
2. Add asset contract tests and a basic CI build step.
3. Address Canvas redraw cost and LED mapping correctness.

