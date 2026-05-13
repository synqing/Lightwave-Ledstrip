RBDO label: GROUNDED

Hard counters:
- show_skips
- failures
- rmt_errors
- underruns

Native test:
- `test_song_aware_health_gate_and_recovery_window_suppress_switches` passed.

Runtime final health:
- songAware health: `show_skips=0 failures=0 rmt_errors=0 underruns=0`.
- VP stack: `show_skips=0 failures=0 rmt_errors=0 underruns=0`.
- `songAware_health: degraded=false ... cleanWindowRemainingMs=0`.

No health-gated switch failure occurred.
