RBDO label: GROUNDED

Director-specific transition handling:
- Renderer queues Director switch request.
- Renderer snapshots source LED buffer.
- Renderer brackets target effect lifecycle with existing `handleSetEffect`.
- Renderer renders target frame under Director guard.
- Renderer starts existing TransitionEngine.
- Existing manual/show transition path was not refactored.

Runtime transition logs:
- 0x1302 -> 0x0407: `Started: Pulsewave (2000ms)`.
- 0x0407 -> 0x1B01: `Started: Fade (800ms)`.
- 0x1B01 -> 0x0204: `Started: Wipe Out (1200ms)`.

Telemetry:
- transitionActive false after completion.
- transition progress reached 1.000.
