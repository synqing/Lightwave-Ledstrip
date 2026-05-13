RBDO label: GROUNDED

Commands:
- `pio test -e native_test_song_aware_director`
- `python3 scripts/native_harness_matrix.py`
- YAML parse for REST/WS contracts
- `git diff --check` for touched files

Results:
- SongAware Director native tests: 19/19 passed.
- Native harness matrix: all listed environments passed.
- YAML contracts parsed OK.
- `git diff --check` produced no output.

Notable SongAware test coverage:
- defaults/reset/off
- mode parsing
- classifier states
- boot/enable grace
- switching disabled
- successful switch/counters
- same effect
- dwell
- cooldown
- rate limit
- anti-thrash
- manual/show owner
- health/recovery
- parameter-only mode
- transition telemetry
