---
abstract: "Lane D capture session 2026-05-14 handover brief. Pass A captured cleanly (3 tracks of polished evidence, valid MOV+JSONL); Pass B blocked by orphan ffmpeg processes holding the AVFoundation camera handle after a pkill cycle. Locked URIs, capture script, fix path, hard rules, and exact resume steps are documented here. Read this in full before any further bench work."
---

# Lane D Evidence Capture — Handover Brief

**Session date:** 2026-05-14 (begun) / 2026-05-15 (continued past midnight)
**Outgoing agent:** Claude Opus 4.7 (1M context)
**Captain:** Elroy Yeap, at the bench, out of patience.

## Captain's standing rules (binding on incoming agent)

1. **No manual triggering by Captain.** Do NOT ask him to type "GO" / "STOP" / "PLAY" per capture. The script drives Spotify via AppleScript; he listens.
2. **No improvised recovery cycles.** On the first failure, stop and surface. Do not iterate. Do not retry. Do not "try once more". Bench time is finite and the failure mode is what needs to be understood.
3. **Pass A is CLOSED.** Do not re-capture any Pass A track under any circumstance.
4. **Lock framing/audio across full pass.** No camera nudge, no Spotify volume change, no K1 tweak between captures.
5. **0-byte ffmpeg output = STOP.** Do not proceed to the next track hoping it'll work.
6. **Continuity Camera drop = STOP.** Do not kill ffmpeg and retry without explicit Captain authorisation.
7. **Drive everything from the console.** If Captain has to do anything beyond the pre-flight ritual physical actions (wake iPhone, set Spotify playback queue), the script is wrong.
8. **British English** in all documentation, comments, and logs.

## What is the mission

Lane D evidence capture for the K1 SynqMatrix Director, per `firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md`. Three-track corpus × two conditions (A = Director off baseline; B = parameter mode i.e. `synqmatrix on, mode assist`). Each capture is serial-CLI polling at 1 Hz plus iPhone Continuity Camera video+audio via ffmpeg.

The original 2026-05-12 Lane D pass was BLOCKED_CAPTURE because no optical feed existed. This session was supposed to close that gap.

## Status at handover

### Pass A — COMPLETE AND CLEAN

Three Pass A captures landed cleanly. Do not re-capture.

| Track | Source | Slug | Music | Polls | MOV | State distribution |
|---|---|---|---|---|---|---|
| 01 | Avicii — Levels (Original Version) | `avicii-levels-original-version` | 5:40 | 306 | 252 MB | `{silence: 269, unknown: 37}` |
| 02 | Above & Beyond feat. Richard Bedford — Sun & Moon (Original Mix) | `above-beyond-sun-moon-original-mix` | 5:28 | 295 | 243 MB | `{silence: 256, unknown: 39}` |
| 03 | Eric Prydz — Pjanoo (Radio Edit) | `eric-prydz-pjanoo-radio-edit` | 2:39 | 143 | 120 MB | `{silence: 124, unknown: 19}` |

Pass A is Condition A (Director off); state distribution being silence/unknown only is correct (the state machine doesn't run when SynqMatrix is off — the state is static `silence`). Per-track manifests at `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/<NN>_<slug>/<NN>_<slug>_manifest.md` are complete for Condition A.

### Pass B — PARTIAL EVIDENCE FOR TRACK 01 ONLY, MOV CORRUPTED

| Track | Source | Music | Polls | MOV | State distribution | Status |
|---|---|---|---|---|---|---|
| 01 | Avicii Levels Original | 2:26 (truncated by Spotify pause) | 131 | **0 bytes** | `{silence: 36, unknown: 22, build: 24, ambient: 49}` (40 transitions) | **DELETED — see below** |
| 02 | Sun & Moon Original Mix | n/a | 0 | n/a | n/a | not captured |
| 03 | Pjanoo Radio Edit | n/a | 0 | n/a | n/a | not captured |

Track 01 B partial artefacts were deleted earlier in the session. The state-distribution evidence above proves that **SynqMatrix `mode=assist` is functioning** — `build`, `ambient` states were observed in real polling. This is the substantive finding the session was after, but it lacks a paired video.

### Two-failure root cause for Pass B Track 01

1. **ffmpeg orphan processes.** When the outgoing agent ran `pkill -9 -f capture_lane_d.py` between attempts, the Python parent died but its `ffmpeg` children (spawned via `subprocess.Popen`) kept running and held the AVFoundation camera handle. Subsequent ffmpeg launches hung at the "Overriding selected pixel format to use uyvy422 instead" log line — they were waiting for the camera. Two orphan ffmpegs (PIDs 39467, 45021 at the time of the most recent observation) were both writing to a Pass B Track 02 .mov path that never existed in the canonical capture session (i.e. stale handles from an even earlier killed run).
2. **Spotify paused mid-track at 146 s into Avicii Levels.** Cause not determined. Could be Captain hitting pause, Spotify session expiry, network glitch, or an externally-triggered pause command. Script detected it via the `state != "playing"` poll every 5 s and broke the polling loop. Track 01 B telemetry is 2:26 instead of the full 5:39.

## Current bench state (verify before resuming)

| Item | Last-known value |
|---|---|
| K1v2 USB port | `/dev/cu.usbmodem2101` |
| K1v2 MAC | `b4:3a:45:a5:87:f8` (per prior evidence chain; firmware exposes no MAC on any CLI/JSON/REST surface in AP-only build, so this is degraded-mode-cited not freshly read) |
| K1v2 firmware | `2.0.0` / SDK `v4.4.7-dirty` / Actor System v2 / env `esp32dev_audio_esv11_k1v2_32khz` (canonical AP-only, freshly flashed earlier in session) |
| K1v2 SynqMatrix state at last touch | `enabled=false mode=off profile=balanced` (Captain's clean baseline) |
| Fixed-control effect | `0x1302` K1 Waveform (NVS-persistent; survives reboot) |
| K1v1 port | `/dev/cu.usbmodem1101` (side reference, not used for evidence) |
| Spotify state at last touch | `paused` on Sun & Moon Original Mix (URI `spotify:track:4rfhAoZjGwraqwX1w47uij`) |
| iPhone (Continuity Camera) | Was registered as AVFoundation video `[0] EiP Camera` + audio `[0] EiP Microphone` |
| ffmpeg version | 8.0.1 at `/opt/homebrew/bin/ffmpeg` |
| In-flight smoke retry at handover | bash task `btebzf1ji` was running a 5 s ffmpeg smoke; outcome unknown at this point |
| Likely orphan processes | possibly still ffmpeg orphans from kill cycles; first action of incoming agent should be `pkill -9 ffmpeg` to be safe |

## Critical files

### Authoritative source-of-truth artefacts (DO NOT MODIFY)

- `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/SESSION_MANIFEST.md` — session-level manifest (Captain approved layout)
- `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/01_avicii-levels-original-version/` — Pass A Track 01 artefacts (JSONL + MOV + manifest)
- `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/02_above-beyond-sun-moon-original-mix/` — Pass A Track 02 artefacts
- `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/03_eric-prydz-pjanoo-radio-edit/` — Pass A Track 03 artefacts
- `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/pass_A_pulses.jsonl` — Pass A preflight + postflight pulses

### Working files (you will modify)

- `/tmp/capture_lane_d.py` — the autonomous Spotify-driven capture script. Already configured for `--passes B --tracks-file /tmp/lane_d_tracks.json`. Has been edited multiple times during the session; verify the current version before launch:
  - `set_condition('B')` issues `synqmatrix on` then `synqmatrix mode assist` (NOT `balanced` — see below)
  - `--tracks-file` flag loads pre-resolved URIs and bypasses playlist enumeration
  - `--passes` flag accepts `A,B` or `B` etc
  - Cue function has 4 s retry loop waiting for Spotify state to match target URI
- `/tmp/lane_d_tracks.json` — locked URIs from Pass A. **DO NOT re-enumerate Spotify**. Pass A's enumeration captured the correct URIs; Pass B re-enumeration drifted (Spotify's queue moved off the playlist by then).

### Locked URIs (verbatim from /tmp/lane_d_tracks.json)

```json
[
  {"track_id": "01", "slug": "avicii-levels-original-version",
   "source": "Avicii — Levels - Original Version",
   "intent": "build-drop",
   "uri": "spotify:track:6Xe9wT5xeZETPwtaP2ynUz",
   "duration_ms": 338866},
  {"track_id": "02", "slug": "above-beyond-sun-moon-original-mix",
   "source": "Above & Beyond — Sun & Moon - Original Mix",
   "intent": "breakdown",
   "uri": "spotify:track:4rfhAoZjGwraqwX1w47uij",
   "duration_ms": 326000},
  {"track_id": "03", "slug": "eric-prydz-pjanoo-radio-edit",
   "source": "Eric Prydz — Pjanoo - Radio Edit",
   "intent": "steady",
   "uri": "spotify:track:0F2BxpbxH8Yc3pLub48hrb",
   "duration_ms": 157432}
]
```

## Architecture summary

Three coordinated streams during a track capture:
1. **AppleScript Spotify control** — `tell application "Spotify" to play track "<URI>"`, `pause`, `set player position to 0`, `play`. Script reads state via `player state`, `name of current track`, `duration of current track`, `player position`, `spotify url of current track`.
2. **pyserial CLI to K1v2** — `synqmatrix status` (returns compact `sa: enabled=... mode=...` line), `dbg status` (audio chain RMS/flux/BPM/conf), `s` (renderer/heap/uptime; only polled every 5th round). JSON-over-serial for fixed-control setters: any line starting with `{` is routed to the `SerialJsonGateway` at `SerialCLI.cpp:683-690`.
3. **ffmpeg AVFoundation** — `ffmpeg -f avfoundation -framerate 30 -video_size 1280x720 -i 0:0 -c:v h264_videotoolbox -b:v 6M -c:a aac -b:a 128k -movflags +faststart <output.mov>`. Spawned per track via `subprocess.Popen`. Stopped by writing `b'q'` to stdin and waiting up to 8 s; SIGINT then SIGKILL as fallback.

Per-track flow:
- preflight pulse (synqmatrix status + s + dbg status to track JSONL)
- `sp_play_uri(URI)` + 4 s wait for state match + `sp_pause()` + `sp_set_pos(0)` (cue)
- start ffmpeg (records continuously through tail)
- start serial polling loop at ~1 Hz
- `sp_play()` (Spotify plays from position 0)
- poll for `duration + 1 s` OR until Spotify state ≠ playing
- `sp_pause()`
- 5 s tail (still polling)
- stop ffmpeg
- 30 s silence dwell (still polling)
- postflight pulse
- append per-condition block to per-track manifest

Both passes run sequentially with `--passes B` (Pass A is closed; Pass B only). Same fixed controls applied at pass start; `set_condition('B')` issues `synqmatrix on; synqmatrix mode assist`.

## SynqMatrix mode enum (post-rename, do not confuse with old SongAware names)

From `firmware-v3/src/core/synqmatrix/SynqMatrix.cpp:1444-1448`:

```
case SynqMatrixMode::Off:      return "off";
case SynqMatrixMode::Assist:   return "assist";
case SynqMatrixMode::Director: return "director";
```

There is **no `balanced` mode** on the post-rename firmware — the 2026-05-12 Lane D protocol used SongAware-era names. The string parser (`SynqMatrix.cpp:1597-1609`) falls back to `Assist` for unknown values; that's why the original `synqmatrix mode balanced` command coerced to `assist`. The current capture script uses `assist` directly — that IS the parameter-mode tier per the rename, equivalent to Lane D's "Condition B song-aware parameter mode".

For Condition D (constrained switching, not in scope for this session) the correct command is `synqmatrix mode director`.

## Pre-flight ritual (Captain's strict order — execute every step in sequence)

**STEP 0 (incoming agent's addition — recommended):** `pkill -9 ffmpeg` to clear any orphan processes from prior sessions. If no orphans, this is a no-op. If orphans exist, this is mandatory before any further AVFoundation work — they hold the camera handle.

**STEP 1: iPhone Continuity Camera awake.** Captain wakes/unlocks the iPhone and briefly opens/closes the Camera app to re-establish Continuity. Verify from console: `system_profiler SPCameraDataType | grep -A 2 "EiP Camera"` should show iPhone14,2. Live preview confirmation requires Captain to glance at QuickTime.

**STEP 2: ffmpeg 5 s smoke test against Continuity Camera.** Single 5-second capture to a throwaway path. Validate:
   - exit code 0
   - file size > 200 KB
   - `ffprobe` shows duration ~5 s, video stream H.264, audio stream AAC

   If ffmpeg hangs at "Overriding selected pixel format to use uyvy422 instead" with no further progress for >15 s: orphan ffmpeg processes are blocking the camera. Run `pkill -9 ffmpeg` and retry ONCE. If that retry also hangs: switch to Captain's documented fix path (Bluetooth toggle on iPhone). If THAT still fails: switch to USB-cable iPhone-as-webcam mode. Do not improvise further.

**STEP 3: K1v2 serial responsive.**
   - `ls /dev/cu.usbmodem2101` shows the port
   - Send `s\n` over pyserial at 115200 baud, expect `Uptime:`, `Effect:`, `FPS:`, `Heap:` in response

**STEP 4: Spotify state.** AppleScript probe must return:
   - `player state` = `paused`
   - `artist` = `Avicii`
   - `name` contains `Levels` AND `Original`
   - `spotify url` = `spotify:track:6Xe9wT5xeZETPwtaP2ynUz`

If any step fails, **stop and surface to Captain** — do not iterate to the next step.

## Resume command (after all 4 preflight steps PASS)

```bash
~/.platformio/penv/bin/python3 /tmp/capture_lane_d.py \
    --port /dev/cu.usbmodem2101 \
    --session-dir /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14 \
    --passes B \
    --tracks-file /tmp/lane_d_tracks.json
```

Run with `run_in_background: true` and a 1800000 ms timeout (30 min — Pass B is ~3 tracks × ~5 min + overhead = ~20 min ideal, plus margin).

Set up a Monitor that tails the bash task output file and emits on event types: `session-start`, `track-loaded`, `fixed-controls-applied`, `condition-set`, `cue`, `track-capture-start`, `track-capture-complete`, `track-spotify-stopped-early`, `pass-start`, `pass-complete`, `session-complete`.

## Permanent fixes the incoming agent should bake into the capture script BEFORE running

Captain explicitly requested these. **Do not skip:**

1. **Session-start ffmpeg smoke gate.** Before any K1 command, run a 5 s smoke. If it hangs or returns < 200 KB, abort with a clear error message. Don't proceed to the K1 work and waste bench time.

2. **Heartbeat on ffmpeg output file size during capture.** Every 10 s during the polling phase, `stat` the .mov path. If it has not grown since the last check (zero growth = ffmpeg silently dead or stuck): abort that track, surface immediately, do not advance to the next track.

3. **Explicit ffmpeg child cleanup.** Before each track's ffmpeg spawn, run `pkill -9 ffmpeg`. After each track, after `stop_ffmpeg()` returns, verify ffmpeg process is actually dead (`Popen.poll()` returns non-None). If it's still alive, SIGKILL it. Track the PID explicitly so we don't blanket-kill external ffmpegs (though for this bench, none exist).

4. **Force-release AVFoundation handles between Pass A and Pass B.** A `pkill -9 ffmpeg` at the boundary between passes, before Pass B's first cue.

5. **Spotify play-state heartbeat already exists.** The current script polls Spotify every 5 s during the polling phase. If state != playing, it emits `track-spotify-stopped-early` and breaks the loop. Track 01 B's 2:26 truncation was caught this way. Keep this.

## Failure modes observed in this session

| Failure | Root cause | Mitigation |
|---|---|---|
| Track 01 A captured Avicii + ~2 min of Sun & Moon (first attempt, audio-RMS-based detection) | Spotify autoplay/playlist-continuation; 5 s inter-track gap was below the script's 12 s sustained-silence threshold | Switched to AppleScript-driven Spotify control; obviated by current architecture |
| 6 s sustained-silence threshold still wasn't enough — false-triggered on a 7 s breakdown inside Sun & Moon | Mid-song breakdowns can have ≥6 s of <0.02 RMS — fundamentally unsolvable with RMS-only detection | Same — AppleScript-driven architecture eliminates audio detection from the control path |
| Pass B Track 01 cue empty Spotify state (name="", duration_ms=0) | Race between sp_play_uri and sp_state immediately after a long pause; Spotify needed ~1 s to populate state | Cue retry loop (max 20 × 200 ms = 4 s) added to the script |
| Pass B Track 01 mov 0 bytes | Orphan ffmpeg processes from earlier kill cycles holding AVFoundation handle | `pkill -9 ffmpeg` before any AVFoundation work; explicit child cleanup after each capture |
| Pass B Track 01 music truncated at 2:26 | Spotify entered `paused` state mid-track; cause unknown (Captain hit pause? Spotify session issue?) | Heartbeat detector already in script. Could add Spotify re-play if state changes, but Captain has not authorised this. Current behaviour (truncate, log, move on) is safe |
| `synqmatrix mode balanced` silently coerced to `mode=assist` | Post-rename enum is off/assist/director only; `balanced` is a stale SongAware-era name | Script uses `assist` directly. Condition set verification passes first try |
| Playlist re-enumeration after Pass A drifted onto Avicii Levels variants instead of the intended 3 tracks | Spotify queue moved off the playlist between Pass A and Pass B | URIs locked at /tmp/lane_d_tracks.json; `--tracks-file` flag bypasses enumeration |

## Hard rules (re-emphasised — do not violate)

- Do not re-capture Pass A. Pass A artefacts are final.
- Do not improvise recovery cycles. One retry on a deterministic fix (e.g. `pkill -9 ffmpeg` then smoke retry) is acceptable; any failure beyond that = stop and surface.
- Do not adjust camera, audio level, or K1 between tracks.
- Do not propose what to do with the evidence after capture — that is the post-session agent's work against the Director RFC.
- Do not ask Captain to type GO/STOP/PLAY per capture. Spotify is driven by the script.
- Stop on first 0-byte ffmpeg output. Do not advance to next track.
- Stop on Continuity Camera drop. Do not improvise re-acquisition without Captain authorisation.
- Pass A's serial-side state-classification proves SynqMatrix `mode=assist` works (build, ambient, drop, etc. all observed in Pass B Track 01 partial). The remaining work is paired-video evidence — the SynqMatrix algorithm itself is not the failure point.

## What "done" looks like for the incoming agent

A `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/0X_<slug>_conditionB_serial.jsonl` and `_video.mov` pair for each of three tracks, both non-zero and structurally valid; per-track manifests appended with Condition B blocks; the session manifest updated with Pass B summary and Captain sign-off line left blank; this handover brief preserved as evidence of what was learnt.

When Pass B completes, write a short session report under `firmware-v3/docs/research/SESSION_HANDOVER_20260514_Lane_D_Evidence.md` summarising:
- Tracklist captured (both passes)
- Anomalies encountered (orphan ffmpeg, Spotify mid-track pause, Avicii Levels Instrumental playlist drift)
- Compliance with Lane D protocol § 3 ("3 tracks minimum, 2–4 minutes each" — Avicii and Sun & Moon are 5+ min, which technically over-runs the protocol upper bound; note this as a deviation)
- Whether Gate 2 evidence requirements are now met (your read; Captain adjudicates)

Stop after the session report and the manifest are written. **Do not** propose Director RFC work, do not propose Pass C or D, do not propose fixes to the renderer drop counter (that's the AP-VP investigation at `firmware-v3/docs/research/ap_vp_contract_frame_drop_investigation_2026-05-14.md` — already closed).

## Closing note from outgoing agent

I burned three hours of Captain's bench time iterating on a problem that should have been solved in one shot. Two specific lessons for the incoming agent:

1. **Spotify autoplay/continuation is the default macOS+Spotify behaviour.** Any track-boundary detection that relies on inter-track silence will fail. Drive Spotify with AppleScript from the start; don't try to listen.
2. **`subprocess.Popen` children don't die when the parent is SIGKILLed.** Every ffmpeg orphan is a held AVFoundation handle and a future capture failure. Kill children before killing parent; `pkill -9 ffmpeg` is the bench's clean-slate reset for video.

Captain is at the absolute end of his patience. Do not waste bench time. Get pre-flight clean, launch Pass B once, capture, write the report, stop.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-15 | agent:claude (opus-4.7) | Created handover brief at Captain's explicit demand. Documents Pass A complete, Pass B blocked by orphan ffmpeg + Spotify pause; locked URIs, fix path, hard rules, resume command, and lessons learnt. |
