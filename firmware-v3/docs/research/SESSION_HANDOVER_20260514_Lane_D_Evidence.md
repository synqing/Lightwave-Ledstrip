---
abstract: "Lane D evidence capture session handover, 2026-05-14 → 2026-05-15. Three-track corpus × two conditions (A baseline `synqmatrix off` / B parameter-mode `synqmatrix on, mode assist`) captured against K1v2 canonical AP-only firmware (HEAD `dc46cc9b` on `feature/synqmatrix-rename-2026-05-13`). Pass A complete 2026-05-14; Pass B attempt-1 failed (orphan-ffmpeg AVFoundation handle blocked Tracks 01 + 02 video); Pass B retry completed clean 2026-05-15 after five permanent fixes plus an AVFoundation device-binding correction were baked into `/tmp/capture_lane_d.py`. All six Condition A + B videos pass ffprobe (1280x720 H.264 + 48 kHz AAC, durations matching native track length ± tail). Awaiting Captain sign-off."
---

# Lane D Evidence Capture — Session Handover

**RBDO:** GROUNDED — every claim below is traceable to the SESSION_LOG.jsonl event stream, the per-track manifests, ffprobe metadata on the six final MOVs, the renamed `_attempt1_failed` forensic chain, and the `/tmp/capture_lane_d.py` HEAD that drove the clean retry.

## Status at handover

- **Pass A:** ✅ complete (2026-05-14, 3 / 3 tracks clean baseline)
- **Pass B:** ✅ complete (2026-05-15 retry, 3 / 3 tracks clean under SynqMatrix `mode=assist`)
- **Captain sign-off:** (awaiting)
- **Out-of-scope deliverables intentionally not produced** (per Captain's standing instruction): Director RFC verdict, Lane D Pass C (family morphing), Lane D Pass D (constrained switching), renderer drop-counter behaviour change (closed at `firmware-v3/docs/research/ap_vp_contract_frame_drop_investigation_2026-05-14.md`).

## Evidence locations (canonical)

Top-level session directory: `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/`.

| Artefact | Path (relative to session directory) | Size |
|---|---|---|
| Session event log (clean retry) | `SESSION_LOG.jsonl` | 4.9 KB / 20 events |
| Pass A pulse pulse-log | `pass_A_pulses.jsonl` | 9.6 KB |
| Pass B pulse pulse-log (clean retry) | `pass_B_pulses.jsonl` | 9.7 KB |
| Pass B attempt-1 forensic session log | `SESSION_LOG_passB_attempt1_failed.jsonl` | 3.3 KB |
| Pass B attempt-1 forensic pulse-log | `pass_B_pulses_attempt1_failed.jsonl` | 4.7 KB |
| Track 01 (Avicii Levels Original) MOVs | `01_avicii-levels-original-version/01_…_conditionA_video.mov` (252.3 MB) + `…_conditionB_video.mov` (249.9 MB) |  |
| Track 01 serial JSONLs | `01_…_conditionA_serial.jsonl` (426 KB) + `…_conditionB_serial.jsonl` (451 KB) |  |
| Track 02 (Sun & Moon Original Mix) MOVs | `02_above-beyond-sun-moon-original-mix/…_conditionA_video.mov` (242.7 MB) + `…_conditionB_video.mov` (241.4 MB) |  |
| Track 02 serial JSONLs | `…_conditionA_serial.jsonl` (414 KB) + `…_conditionB_serial.jsonl` (436 KB) |  |
| Track 03 (Pjanoo Radio Edit) MOVs | `03_eric-prydz-pjanoo-radio-edit/…_conditionA_video.mov` (120.2 MB) + `…_conditionB_video.mov` (119.0 MB) |  |
| Track 03 serial JSONLs | `…_conditionA_serial.jsonl` (217 KB) + `…_conditionB_serial.jsonl` (232 KB) |  |
| Per-track manifests | `0X_<slug>/0X_<slug>_manifest.md` (3 files) |  |
| Updated session manifest | `SESSION_MANIFEST.md` |  |
| Handover brief from outgoing agent | `HANDOVER_BRIEF.md` | 20.2 KB |
| Capture script HEAD that drove the clean retry | `/tmp/capture_lane_d.py` (839 lines) — see "Permanent script fixes" below |

## Pass B clean retry — execution facts

- **Launch:** 2026-05-15 13:03:44 GMT+8
- **Completion:** 2026-05-15 13:19:18 GMT+8
- **Duration:** ~15 min 34 s
- **Command:** `~/.platformio/penv/bin/python3 /tmp/capture_lane_d.py --port /dev/cu.usbmodem2101 --session-dir <session-dir> --passes B --tracks-file /tmp/lane_d_tracks.json`
- **Background bash ID:** `bhpv79vjb` (exit 0)
- **Event sequence (verbatim, 20 events):** `session-start → smoke-ok → spotify-initial → track-loaded × 3 → pass-start → fixed-controls-applied → condition-set → (cue → track-capture-start → track-capture-complete) × 3 → pass-complete → session-complete`
- **Abort events:** 0. Heartbeat triggers: 0. Orphan-cleaned events: 0. Smoke-failed: 0. ffmpeg-stop-failed: 0.

### Per-track Condition B results

| Track | Music duration | Polling rows | Video size | ffprobe duration | State distribution (Condition B) | Transitions |
|---|---|---|---|---|---|---|
| 01 Avicii Levels (build-drop) | 340.4 s | 305 | 249.9 MB | 343.3 s | `silence: 44, unknown: 33, build: 174, drop: 26, ambient: 18, breakdown: 10` | 67 |
| 02 Above & Beyond Sun & Moon (breakdown) | 327.6 s | 293 | 241.4 MB | 331.9 s | `silence: 92, unknown: 39, build: 156, breakdown: 4, steady: 2` | 60 |
| 03 Eric Prydz Pjanoo (steady) | 159.1 s | 142 | 119.0 MB | 163.6 s | `silence: 5, unknown: 17, ambient: 1, breakdown: 10, dense: 29, build: 62, drop: 18` | 41 |

Every Condition B video is non-zero, structurally valid, and the audio + video streams open in ffprobe without warning. Durations match `music_duration_s + 5 s tail` to within < 4 s.

### A versus B state-distribution observation (no interpretation)

Pass A under Condition A reports state distributions dominated by `silence` + `unknown` for all three tracks. Pass B under Condition B reports a broader state vocabulary including `build`, `drop`, `breakdown`, `dense`, `ambient`, `steady`. The Director-assist state classifier is firing on Condition B. This report does **not** interpret these distributions against the SynqMatrix Director RFC, the Lane D Gate 2 evaluation, or any downstream verdict — that work is out of scope here, and the brief explicitly forbids it.

## Pass B attempt-1 failure — what happened (2026-05-14 18:17 – 18:23)

The handover brief documented two distinct root causes for the attempt-1 failure on Track 01B; the filesystem shows that Track 02B was also attempted (94 KB of partial serial polling + 1.2 KB ffmpeg.log) before the session was stopped manually.

1. **Orphan ffmpeg children retained AVFoundation Continuity Camera handle.** The pre-retry sweep on 2026-05-15 12:55 showed no live ffmpeg processes (a transient PID 52708 had already exited), but the attempt-1 ffmpeg.log stops at `Overriding selected pixel format to use uyvy422 instead.` with no following `Input #0, avfoundation, …` line, confirming the AVFoundation input never opened. This matches the brief's diagnosis.
2. **Spotify paused mid-track at ~146 s into Avicii Levels.** Root cause unresolved in evidence (could be hand, session expiry, or external trigger). The script's existing 5 s play-state heartbeat caught it and broke the polling loop, but with no MOV growing, the resulting artefact was zero-byte.

Both failure surfaces are now armoured against in the retry script — see Permanent script fixes below.

## AVFoundation device-binding correction (independent discovery)

A third potential failure mode was discovered during the pre-launch ffmpeg `-list_devices` probe on 2026-05-15 12:56 and would have invalidated Pass B silently:

- Pass A `start_ffmpeg` used `-i '0:0'`. Pass A succeeded because the iPhone Continuity Camera was at AVFoundation video index 0 at capture time.
- On 2026-05-15 the enumeration had drifted: `[0] MacBook Pro Camera`, `[1] EiP Camera`, `[2] EiP Desk View Camera`, `[3] MacBook Pro Desk View Camera`, `[4]/[5]` screen captures. Audio: `[0] EiP Microphone`, `[1] MacBook Pro Microphone`.
- Index-based `0:0` on 2026-05-15 would have captured the MacBook Pro built-in webcam (Captain's face / desk view) plus the EiP Microphone — wrong framing for the K1-panel evidence chain.
- Switched the script to name-based binding via a new module constant `AVFOUNDATION_DEVICE = 'EiP Camera:EiP Microphone'` referenced by both `start_ffmpeg` and the new `smoke_ffmpeg` gate. Bench-validated with a 3 s smoke (2.3 MB H.264/AAC, `Input #0, avfoundation, from 'EiP Camera:EiP Microphone'`) before the script run, then validated again at script session-start (`smoke-ok` event reporting 4.0 MB / video + audio streams).

This is a defensive change beyond the brief's five fixes; it is the only reason the Pass B retry video shows the K1 panel rather than the laptop webcam.

## Permanent script fixes (5 + 1)

`/tmp/capture_lane_d.py` grew from 666 lines to 839 lines (`+173 LOC, 0 deletions other than the `-i '0:0'` → `-i AVFOUNDATION_DEVICE` rewrite). Every fix has a unique announce-event so the SESSION_LOG.jsonl alone confirms whether it was exercised on any given run.

| # | Fix | Surface | Exercised on retry? |
|---|---|---|---|
| 1 | Session-start ffmpeg smoke gate | new `Session.smoke_ffmpeg`, called first from `Session.run` | YES — `smoke-ok` event emitted with 4 032 645 byte file + `["video", "audio"]` streams |
| 2 | ffmpeg output-file 10 s heartbeat | new gate block in `Session.capture_track` main loop, raises `CaptureAbort('mov-no-growth …')` | NO (no abort fired across 3 tracks) |
| 3a | Pre-spawn `pkill -9 -x ffmpeg` | new `Session._kill_orphan_ffmpeg('pre-spawn')` called from `Session.start_ffmpeg` | NO (no orphans found, no emission) |
| 3b | Post-stop `proc.poll()` + SIGKILL fallback | new tail block in `Session.stop_ffmpeg`, emits `ffmpeg-stop-failed` if survivor | NO (all stops clean) |
| 4 | Pass-boundary AVFoundation release | `Session._kill_orphan_ffmpeg('pass-boundary')` + `time.sleep(2.5)` between passes in `Session.run` | NOT EXERCISED (single-pass retry; armed for future A+B contiguous runs) |
| 5 | Spotify play-state heartbeat | existing — `Session.capture_track` lines 493 – 502 — confirmed | NO trigger (no early stops in retry) |
| 6 | Name-based AVFoundation device binding | new constant `AVFOUNDATION_DEVICE = 'EiP Camera:EiP Microphone'`, used by `start_ffmpeg` and `smoke_ffmpeg` | YES — every capture |

A new `CaptureAbort` exception type was added to bubble first-failure aborts up to `Session.run`, which emits a terminal `session-aborted` event with the reason before re-raising. (Previously the `finally` block silently swallowed exceptions; a crashed session had no terminal log entry.)

## Procedural deviations from Lane D protocol (2026-05-12)

The protocol-vs-execution deviations recorded in `SESSION_MANIFEST.md` from 2026-05-14 still apply unchanged for the Pass B retry:

1. Polling-phase command set is `synqmatrix status` + `dbg status` every round; `s` every fifth round only. Full preflight + postflight pulse retains `vp stack`, `s`, `dbg memory`, `dbg status` at the boundaries.
2. Run scope is Conditions A and B only — Conditions C (family morphing) and D (constrained switching) explicitly deferred per Captain's session-scope decision (Option B).
3. Three tracks are within the protocol's 2 – 4 min minimum bound (Track 01 native 5:39, Track 02 5:26, Track 03 2:37 — all ≥ 2 min).
4. **(new on retry)** Lane D protocol used the pre-rename SongAware enum (`off / balanced / hyperreactive`). Post-rename SynqMatrix enum is `off / assist / director`. Condition B uses `assist` explicitly; the firmware parser coerces unknown values to `assist` but the script does not rely on that fallback.
5. **(new on retry)** AVFoundation device binding moved from index to name (see "AVFoundation device-binding correction" above).

## Known recorded-not-fixed defects (out of scope for Lane D)

These are noted only so they are not confused with Pass B failure modes. None of them require Lane D attention:

- K1v2 frame-budget overrun counter (`frameDrops`) — naming artefact, not a defect. Closed at `firmware-v3/docs/research/ap_vp_contract_frame_drop_investigation_2026-05-14.md`. Counter values appear in per-track manifests as `Drops counter delta (budget-overrun, see AP-VP investigation)` for evidence completeness.
- K1v2 STA-validation WiFi RSSI weakness (−90 to −95 dBm to `VX220-013F`). Inert on canonical AP-only firmware; the Lane D capture ran with WiFi unused.

## Captain sign-off

Captain sign-off: evidence accepted, 2026-05-15 UTC+8 18:13, [Elroy Yeap]

Lane D evidence chain CLOSED. Anchored at `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/01_avicii-levels-original-version/01_avicii-levels-original-version_manifest.md:55`.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-15 | agent:claude (opus-4.7) | Created session handover after Pass B retry completed cleanly. Documents Pass A baseline + Pass B attempt-1 failure + Pass B retry success + the 5+1 permanent script fixes + the AVFoundation device-binding correction. Awaiting Captain sign-off; out-of-scope items (Director RFC, Pass C/D, drop-counter rework) explicitly not proposed per Captain instruction. |
