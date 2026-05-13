# Lane D A/B Decision

Decision: `BLOCKED_CAPTURE`

## Why

The serial/runtime validation passed:

- Condition A held fixed effect `0x1302 K1 Waveform` with `parameterUpdates` delta 0 on all tracks.
- Condition B held the same effect id with autonomous parameter activity on all tracks.
- `automaticEffectSwitches=0` throughout.
- `show_skips=0`, `failures=0`, `rmt_errors=0`, and `underruns=0` throughout.
- B health was not worse than A on the hard counters.
- `songaware off` disabled cleanly after the run.

The full A/B decision is still blocked because the requested visible section-change notes could not be captured from this agent session. There was no optical camera or human visual feed of the LEDs, and current serial status does not expose post-overlay per-frame parameter output values. Serial telemetry proves runtime parameter activity; it does not prove visible gain.

## Non-decisions

- Not `PASS_PARAMETER_MODE_VALIDATION`: serial validation passed, but optical visible-gain capture is missing.
- Not `FAIL_PARAMETER_MODE_HEALTH`: hard health counters stayed zero and B was not worse than A.
- Not `FAIL_PARAMETER_MODE_NO_VISIBLE_GAIN`: no visual feed was available, so visible gain was not assessed.
- Not `FAIL_EFFECT_ID_STABILITY`: effect id was stable.
- Not `BLOCKED_RUNTIME_SUPPORT_REGRESSION`: serial runtime support worked; the remaining gap is capture visibility, not command support.

## Stop point

Stopped after A/B. Condition C and Condition D were not run.

