RBDO label: GROUNDED

Final decision:

PASS_SONGAWARE_DIRECTOR_FULL_BUILD

Basis:
- Source audit completed.
- Full mode contract implemented.
- Boot/default safety enforced.
- Audio intake works and degrades explicitly.
- Classifier implemented.
- Visual-language policy implemented.
- Effect allowlist validated at runtime.
- Selection scoring and gates implemented.
- Anti-thrash/rate-limit/cooldown/dwell implemented.
- Director-specific transition handling implemented.
- Parameter envelopes implemented as support layer.
- Ownership and health gates implemented and tested.
- Restore/rollback implemented.
- Telemetry explains switch and no-switch decisions.
- Serial commands work.
- REST/WS updated where cleanly supported; not used for validation.
- Native tests and native matrix pass.
- K1 build passes.
- Upload passes to MAC b4:3a:45:a5:87:f8.
- Runtime smoke proves at least two allowlisted visual-language decisions.
- Suppression proof captured.
- Restore returns K1 to 0x1302 fixed controls, SongAware off, hard counters zero.
- No NVS save path in SongAware files.
- No production default enablement.
