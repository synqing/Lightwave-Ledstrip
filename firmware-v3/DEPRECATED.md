# ⛔ DEPRECATED — DO NOT WORK HERE

**This firmware lineage is DEAD.** It was the product until the Captain pivoted and rebuilt
the firmware from the ground up on the Sensory Bridge AP/VP architecture.

## The canonical product firmware is:
### `/Users/spectrasynq/SpectraSynq_K1_Firmware`

Anything you change here ships to nobody. Known traps in this tree:
- The good `OnsetDetector` is demoted to telemetry (NOT published to ControlBus).
- `GoertzelAnalyzer` / `ChromaAnalyzer` / `TempoTracker` are EXCLUDED from the build.
- Downbeat is a fake modulo-4 counter.
- `BeatTracker.cpp` is intact but dead code — `update()` is never called.

_Deprecated 2026-07-13._
