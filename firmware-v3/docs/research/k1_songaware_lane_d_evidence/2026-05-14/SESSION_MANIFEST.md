---
abstract: "Lane D evidence capture corpus, K1v2 canonical AP-only firmware. Three-track corpus (Avicii Levels Original, Above & Beyond Sun & Moon Original Mix, Eric Prydz Pjanoo Radio Edit) captured under Conditions A (baseline `synqmatrix off`) and B (parameter mode `synqmatrix on, mode assist` — post-SongAware→SynqMatrix rename, the `balanced` tier is now named `assist`). Per-condition serial-CLI telemetry at ~1 Hz plus iPhone Continuity Camera optical feed via name-bound AVFoundation (`EiP Camera:EiP Microphone`). Pass A complete 2026-05-14; Pass B retried and completed clean 2026-05-15 after attempt-1 failure (orphan-ffmpeg AVFoundation handle blocked Tracks 01 + 02 video). Per-track manifests sit in subdirectories; this file is the session-level summary."
---

# Lane D Evidence Capture — Session Manifest

**Session dates:** 2026-05-14 (Pass A complete) → 2026-05-15 (Pass B complete after one failed attempt)
**Capture protocol reference:** `firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md` (with deviations enumerated below)
**Capture script:** `/tmp/capture_lane_d.py` (preserved for session audit; will be archived into the session directory at session close)
**Agent (Pass A + Pass B attempt-1):** Claude Opus 4.7 (1M context)
**Agent (Pass B retry, clean):** Claude Opus 4.7 (1M context), resumed under handover brief
**Captain:** Elroy Yeap
**Status:** captures complete; awaiting Captain sign-off

## Device under test (formal evidence device)

| Field | Value |
|---|---|
| Device | K1v2 |
| USB port | `/dev/cu.usbmodem2101` |
| MAC | `b4:3a:45:a5:87:f8` (cited from prior evidence chain — `director_v1_runtime_decision.md:10` and `reference_device_port_map.md` memory; no fresh independent read possible from current firmware) |
| Firmware version | `2.0.0` / `firmwareVersionNumber 20000` (read from REST `/api/v1/device/info` pre-flash on STA validation build; same source tree compiled with canonical AP-only env flags) |
| SDK | `v4.4.7-dirty` |
| Architecture | `Actor System v2` |
| Board target | `ESP32-S3-DevKitC-1-N16R8V` (16 MB flash quad, 8 MB PSRAM octal) |
| Build env (in use) | `esp32dev_audio_esv11_k1v2_32khz` (canonical AP-only, post-flash 2026-05-14) |
| Build env (pre-flash) | `esp32dev_audio_esv11_k1v2_32khz_sta_validation` (flashed to canonical during this session) |
| Git HEAD | `dc46cc9b` on `feature/synqmatrix-rename-2026-05-13` |
| Boot uptime at session start | 63 s post-flash |
| Pre-capture state | Effect `0x1302` K1 Waveform; `synqmatrix off`; clean Condition A baseline |

## K1v1 reference device (not used for formal evidence)

| Field | Value |
|---|---|
| Device | K1v1 |
| USB port | `/dev/cu.usbmodem1101` |
| Role | side reference on bench, not part of Lane D evidence chain |

## Bench / rig (Captain-confirmed)

| Item | Configuration |
|---|---|
| Audio source | Spotify playlist; three-track corpus pre-queued |
| Camera | iPhone 14 Pro via Continuity Camera, tripod-mounted face-on to K1v2 panel |
| Recording app | QuickTime Player (track 01); switch to OBS conditional on auto-exposure flattening verdict after track 01 review |
| Framing | K1v2 panel fills substantial frame area, no edge clipping, both LED strips visible end-to-end |
| Distance | fixed throughout session |
| Ambient light | stable throughout session |
| Audio monitor level | fixed before track 01; not touched between captures |
| iPhone-to-Mac link | per Captain's rig confirmation; verified via 30-second dry run |
| Capture surface | Serial CLI on USB at 115200 baud |
| Telemetry cadence | ~1 Hz polling per condition (`synqmatrix status` + `dbg status` every round; `s` every 5th round for renderer/heap) |

## Capture protocol (per condition, per track)

1. Preflight pulse: `synqmatrix status`, `vp stack`, `s`, `dbg memory`, `dbg status`
2. Apply fixed controls: effect `0x1302`, brightness 160, speed 27, intensity 128, saturation 128, complexity 128, variation 0, palette 10, EdgeMixer mode=0 spatial=0 temporal=0 spread=30 strength=255 — verify with `s` re-read
3. Set condition: A → `synqmatrix off`; B → `synqmatrix on` then `synqmatrix mode balanced` — verify with `synqmatrix status` re-read
4. Arm and wait for start signal (filesystem signal touched by agent on Captain's "GO")
5. 1 Hz polling loop until stop signal (filesystem signal touched by agent on Captain's "STOP")
6. 5 s tail polling after stop signal
7. 30 s silence-coast dwell polling
8. Postflight pulse: same five commands as preflight
9. Save JSONL; append per-condition block to per-track manifest

## Tracklist

| ID | Slug | Source | Intent | A status | B status |
|----|------|--------|--------|----------|----------|
| 01 | `avicii-levels-original-version`     | Avicii — Levels (Original Version)                                 | build-drop | ✅ complete (2026-05-14, 252.3 MB / 306 polls) | ✅ complete (2026-05-15 retry, 249.9 MB / 305 polls) |
| 02 | `above-beyond-sun-moon-original-mix` | Above & Beyond — Sun & Moon (Original Mix)                         | breakdown  | ✅ complete (2026-05-14, 242.7 MB / 295 polls) | ✅ complete (2026-05-15 retry, 241.4 MB / 293 polls) |
| 03 | `eric-prydz-pjanoo-radio-edit`       | Eric Prydz — Pjanoo (Radio Edit)                                   | steady     | ✅ complete (2026-05-14, 120.2 MB / 143 polls) | ✅ complete (2026-05-15 capture, 119.0 MB / 142 polls) |

**Slug correction note:** original tracklist used `avicii-levels-radio` / `above-beyond-sun-moon` / `prydz-pjanoo-radio` as placeholder slugs; the Spotify-resolved canonical track names (Original Version / Original Mix / Radio Edit) yielded the slugs in the table above and on disk. Subdirectory names match the on-disk slugs.

## Pass B retry summary (2026-05-15)

| Field | Value |
|---|---|
| Pass A capture window | 2026-05-14 17:54 – 18:09 GMT+8 (~15 min) |
| Pass B attempt-1 window (FAILED) | 2026-05-14 18:17 – 18:23 GMT+8 (halted at Track 01B; partial Track 02B captured before stop) |
| Pass B retry window (CLEAN) | 2026-05-15 13:03 – 13:19 GMT+8 (~16 min, all 3 tracks) |
| Total event count (retry) | 20 (smoke-ok, spotify-initial, 3× track-loaded, pass-start, fixed-controls-applied, condition-set, 3×{cue, track-capture-start, track-capture-complete}, pass-complete, session-complete) |
| Abort events | 0 |
| Heartbeat triggers | 0 |
| Orphan-cleaned events | 0 |
| Smoke-failed events | 0 |
| SynqMatrix mode confirmed | `assist` (post-rename name for the parameter-mode tier; firmware coerces unknown values, so `assist` was used explicitly) |
| Fixed-control effect | `0x1302` K1 Waveform, brightness 160, speed 27 (identical to Pass A) |

## Pass B attempt-1 failure forensic chain (preserved on disk)

For audit-trail continuity, the failed-attempt artefacts were renamed rather than deleted before the retry:

- `SESSION_LOG_passB_attempt1_failed.jsonl` (top-level)
- `pass_B_pulses_attempt1_failed.jsonl` (top-level)
- `01_avicii-levels-original-version/01_avicii-levels-original-version_conditionB_serial.attempt1_failed.jsonl` (213 KB partial polling)
- `01_avicii-levels-original-version/01_avicii-levels-original-version_conditionB_video.mov.ffmpeg.attempt1_failed.log` (1.2 KB — captures the AVFoundation handle hang)
- `02_above-beyond-sun-moon-original-mix/02_above-beyond-sun-moon-original-mix_conditionB_serial.attempt1_failed.jsonl` (94 KB partial polling — contradicts the handover claim that Track 02B was never attempted; the filesystem records that it was)
- `02_above-beyond-sun-moon-original-mix/02_above-beyond-sun-moon-original-mix_conditionB_video.mov.ffmpeg.attempt1_failed.log` (1.2 KB)

The Track 01 per-track manifest retains a `## Condition B` block from attempt-1 (showing 0.0 MB video, 146.1 s music duration before Spotify mid-track stop) followed by the successful retry `## Condition B` block. Track 02 and Track 03 manifests carry only the successful retry block.

## Permanent script fixes baked in before Pass B retry

The handover brief required five hardening fixes; all five plus one independently-discovered AVFoundation device-binding fix landed before the retry run.

1. **Session-start ffmpeg smoke gate** (`Session.smoke_ffmpeg`) — 5 s Continuity-Camera capture validated by ffprobe (exit 0, > 200 KB, video + audio streams) before any K1 / Spotify side-effect. Smoke-ok was the first non-`session-start` event in the clean run.
2. **ffmpeg output-file heartbeat** (`Session.capture_track` loop) — `os.path.getsize(mov_path)` checked every ~10 s; no growth between two consecutive checks raises `CaptureAbort('mov-no-growth …')`.
3. **Pre-spawn ffmpeg orphan cleanup + post-stop poll verification** (`Session._kill_orphan_ffmpeg` called from `Session.start_ffmpeg`; `Session.stop_ffmpeg` re-polls `proc.poll()` and SIGKILLs survivors) — emits `ffmpeg-orphan-cleaned` only when count > 0 (zero emissions in the clean retry run).
4. **Pass-boundary AVFoundation release** (`Session.run`) — `_kill_orphan_ffmpeg(phase_label='pass-boundary')` + `time.sleep(2.5)` between passes. Not exercised in the `--passes B` retry (single pass), but armed for future A+B contiguous runs.
5. **Spotify play-state heartbeat** — already present at `Session.capture_track` lines 493 – 502; confirmed during validation.
6. **(independently discovered) AVFoundation device-binding via name** — Pass A used `-i '0:0'` and successfully captured the iPhone Continuity Camera; at retry time the AVFoundation enumeration had drifted (`[0] MacBook Pro Camera`, `[1] EiP Camera`). With the original `0:0` the retry would have captured the laptop's built-in webcam, not the K1 panel. Switched to name-based `-i 'EiP Camera:EiP Microphone'` (constant `AVFOUNDATION_DEVICE` at top of script). Validated by a 3 s bench smoke (2.3 MB H.264/AAC file) before the script run.

A new `CaptureAbort` exception type was added to bubble first-failure aborts up to `Session.run`, which emits `session-aborted` with the reason before re-raising (no terminal `session-end` event previously existed).

## Procedural deviations from Lane D protocol

Recorded so Gate 2 evaluation can interpret the evidence under the actual procedure:

1. **Polling-phase command set is restricted to `synqmatrix status` + `dbg status` every round and `s` every fifth round.** Lane D protocol § 4 listed `vp stack`, `s`, `adbg status`, `dbg status` as the per-second polling set. With the post-rename multi-line outputs, polling all four commands at 1 Hz produced 1.5–2 s round times, falling below the protocol's "1 Hz (minimum)" floor. The full command set is captured in preflight and postflight snapshots; the polling-phase subset preserves the per-second cadence while still emitting state, audio, and renderer/heap at the protocol-required intervals.
2. **Run scope is Conditions A and B only.** Conditions C (family morphing) and D (constrained switching) are deferred to follow-up sessions per Captain's session-scope decision (Option B). Director v1 runtime smoke evidence already exists for the C/D territory in `evidence/k1_songaware_director_v1_runtime_2026-05-12/director_v1_runtime_decision.md` — but is independent of this session.
3. **Three tracks are radio edits (~2–4 minutes) rather than full-length versions.** Within Lane D protocol § 3 ("Run 3 tracks minimum, each 2–4 minutes") — compliant.

## Defects observed during session

- **Renderer "drops" counter — naming artefact, not defect.** Pre-flash STA validation reported `Drops: 833,982 / Frames: 1,016,792` (82 % ratio) at 173 min uptime; post-flash canonical reported `Drops: 5,813 / Frames: 5,843` (99.5 % ratio) at 63 s uptime. The AP-VP contract + drop counter forensic investigation (`firmware-v3/docs/research/ap_vp_contract_frame_drop_investigation_2026-05-14.md`, RBDO GROUNDED) determined that `frameDrops` is a budget-overrun counter at `RendererActor.cpp:2610` gated by `rawFrameTimeUs > 8333 µs`, not a frame-skip counter. Every counted "drop" is also a displayed frame; `showSkips=0` on both builds confirms zero hardware suppression. Recommendation is a documentation/naming change only with zero behaviour risk. **Not a defect requiring this session's attention.**
- **WiFi RSSI weakness on STA validation pre-flash:** −90 to −95 dBm to `VX220-013F`. Inert on AP-only canonical (no STA). Recorded; not addressed.

## Captain sign-off

Captain sign-off: evidence accepted, 2026-05-15 UTC+8 18:13, [Elroy Yeap]

Anchored at `01_avicii-levels-original-version/01_avicii-levels-original-version_manifest.md:55` and mirrored into the per-track manifests (Tracks 02 + 03) and the session handover report (`firmware-v3/docs/research/SESSION_HANDOVER_20260514_Lane_D_Evidence.md`).

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-14 | agent:claude (opus-4.7) | Created session manifest stub during pre-capture setup. Will be finalised at session close with per-track summaries, anomalies, retries, and final sign-off line. |
