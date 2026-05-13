# K1 Song-Aware Lane D A/B Track Observations - Serial - 2026-05-12

## Run Status

Decision before playback: `BLOCKED_RUNTIME_SUPPORT`

No track was played and no A/B visual comparison was captured. The validation stopped at the Condition B activation gate because no existing runtime path exposed an autonomous song-aware parameter-only mode.

## Conditions

| Condition | Requested State | Runtime Result | Playback |
| --- | --- | --- | --- |
| A | Baseline fixed effect, `songAware.mode=off`, no family morphing, no constrained switching, no effect-ID switching | Baseline serial control appeared possible in principle through existing fixed controls, but was not run because B could not be activated | Not run |
| B | Song-aware parameter mode, family morphing off, constrained switching off, no effect-ID switching, parameter adaptation allowed | Could not activate or observe a song-aware parameter-only mode through existing serial/REST/WS surfaces | Not run |

## Track Matrix

| Track Role | Selected File/Title | Duration Evidence | Condition A Observation | Condition B Observation | Visible Section Change Notes |
| --- | --- | --- | --- | --- | --- |
| Ambient / low-energy intro-heavy | Not run | Not applicable | Not captured | Not captured | Blocked before playback |
| Build/drop electronic | Not run | Not applicable | Not captured | Not captured | Blocked before playback |
| Dense / high-energy | Not run | Not applicable | Not captured | Not captured | Blocked before playback |

## Required Polling Fields

The requested 1 Hz polling sequence was not started because Condition B failed activation before the first track. No full-track `vp stack`, `s`, `adbg status`, or `dbg status` polling logs exist for A or B.

## Perceived Fit

No perceived-fit judgement was made. Any visual judgement would be invalid without a working Condition B.

