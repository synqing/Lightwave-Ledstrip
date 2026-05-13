# Lane D A/B Track Observations - Serial Capture

Capture window: 2026-05-12T09:44:45+0800 to 2026-05-12T10:13:35+0800  
Poll set during playback, at approximately 1 Hz: `songaware status`, `vp stack`, `s`, `adbg status`, `dbg status`  
Pre/end state set: `songaware status`, `vp stack`, `s`, `dbg memory`, `adbg status`, `dbg status`

## Track set

No user-selected tracks were requested. The run used the longest contrasting local audio files found on the machine.

| Track role | File | Duration | Limitation |
|---|---|---:|---|
| ambient / low-energy intro-heavy | `/System/Library/PrivateFrameworks/Slideshows.framework/Versions/A/Resources/Content/Audio/Reflections.m4a` | 243.9 s | Local system slideshow audio, not a known commercial music reference. |
| build/drop electronic | `/System/Library/PrivateFrameworks/Slideshows.framework/Versions/A/Resources/Content/Audio/SlidingPanels.m4a` | 257.6 s | Local system slideshow audio, slightly over the requested 2-4 minute window. |
| dense / high-energy | `/System/Library/PrivateFrameworks/Slideshows.framework/Versions/A/Resources/Content/Audio/Flipup.m4a` | 285.7 s | Local system slideshow audio, over the requested 2-4 minute window. |

## Condition A - baseline fixed effect

Runtime state: `songaware off`, fixed effect held, no automatic effect-id switching.

| Track | Polls | Poll rate | Pre state | End state | RMS avg/max | Flux avg/max | BPM range | Serial section notes |
|---|---:|---:|---|---|---:|---:|---:|---|
| ambient / low-energy intro-heavy | 245 | 1.00 Hz | `off`, `owner=none`, `suppressed=disabled` | `off`, `owner=none`, `suppressed=disabled` | 0.641 / 1.000 | 19.976 / 180.051 | 133-142 | Audio classifier stayed disabled by design; no song-aware state transitions applied. |
| build/drop electronic | 259 | 1.00 Hz | `off`, `owner=none`, `suppressed=disabled` | `off`, `owner=none`, `suppressed=disabled` | 0.574 / 1.000 | 19.003 / 159.298 | 78-142 | Baseline held fixed parameters through changing RMS/flux/BPM. |
| dense / high-energy | 287 | 1.00 Hz | `off`, `owner=none`, `suppressed=disabled` | `off`, `owner=none`, `suppressed=disabled` | 0.560 / 1.000 | 18.415 / 132.147 | 100-138 | Baseline held fixed parameters; no parameter update delta. |

## Condition B - song-aware parameter mode

Runtime state: `songaware on`, `songaware mode balanced`, family morphing off, constrained switching off, no automatic effect-id switching.

| Track | Polls | Poll rate | Pre state | End state | RMS avg/max | Flux avg/max | BPM range | Serial section notes |
|---|---:|---:|---|---|---:|---:|---:|---|
| ambient / low-energy intro-heavy | 245 | 1.00 Hz | `balanced`, `owner=director`, `state=steady`, `lastAction=parameter_update` | `balanced`, `owner=none`, `suppressed=low_confidence`, `state=silence` | 0.594 / 1.000 | 18.050 / 133.752 | 108-133 | Serial state moved through dense/drop/build/dense and ended low-confidence/silence after playback tail. |
| build/drop electronic | 259 | 1.00 Hz | `balanced`, `owner=none`, `suppressed=low_confidence`, `state=silence` | `balanced`, `owner=director`, `state=build`, `lastAction=parameter_update` | 0.570 / 1.000 | 18.581 / 186.666 | 102-140 | Serial state moved through silence/dense/drop/steady/build/dense/drop/build. |
| dense / high-energy | 287 | 1.00 Hz | `balanced`, `owner=none`, `suppressed=low_confidence`, `state=silence` | `balanced`, `owner=director`, `state=ambient`, `lastAction=parameter_update` | 0.601 / 1.000 | 18.188 / 166.174 | 90-137 | Serial state moved through dense/drop/build/dense/drop/ambient. |

## Visible section-change notes

No optical camera or human visual feed was available to this agent session. The notes above are serial-visible section-change proxies from `currentSongState`, confidence, RMS, flux, BPM, and `lastAction`. They do not prove visible subjective gain on the LEDs.

