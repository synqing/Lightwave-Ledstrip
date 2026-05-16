---
abstract: "Source-level trace of K1 firmware 'frame drop' counter and the AP-VP contract between AudioActor (125 Hz publisher) and RendererActor (120 Hz consumer). Refutes Captain's coupling-cadence hypothesis: Drops measures budget overruns of rendered frames, not skip decisions. Every increment IS a rendered frame whose raw work-time exceeded 8333 us. Bench evidence of 82% / 99.5% drop ratios is an artefact of counter naming, not a defect. AP-VP contract has NO skip path; renderer always renders and always reads latest snapshot."
---

# AP-VP Contract and Frame Drop Counter — Forensic Investigation

**Investigation date:** 2026-05-14
**Investigator:** Claude Opus 4.7 (1M ctx), forensic subagent
**Subject hardware:** K1 v2 (ESP32-S3, MAC `b4:3a:45:a5:87:f8`)
**Bench session:** 2026-05-14, two builds compared back-to-back
**RBDO label:** `GROUNDED` — all source claims carry file:line citations; bench numbers are Captain-provided; design-rationale interpretation is labelled `[INFERENCE]` and grounded in the source trace, not symptom reading.

---

## TL;DR (decision-grade)

**The drop counter does NOT measure AP-VP skip decisions.** It measures the count of rendered frames whose raw (pre-pacing) work exceeded the 8333 us frame budget. Captain's coupling-invocation hypothesis is **refuted by source trace**:

- The renderer has exactly one counter increment site, in `updateStats()` at `firmware-v3/src/core/actors/RendererActor.cpp:2610`, gated by `rawFrameTimeUs > LedConfig::FRAME_TIME_US` (line 2609).
- `updateStats()` is called from exactly one site, `onTick()` line 1129, **after** every rendered frame. `framesRendered` increments at line 2606 alongside the drop check — every Drop **is also** a rendered Frame.
- There is no early-return-when-no-audio code path. `audioAvailable` (set at line 1892) is forwarded into effect context only; it does not cause `renderFrame()` to be skipped.
- The bench evidence (82% / 99.5% drop ratios on visually-correct output) is fully explained by this counter semantic: those are budget overruns, not skipped frames. Visual correctness is unaffected because pacing absorbs the overrun by simply shipping the frame late.

**Recommendation:** Rename the counter and re-label its serial output. The current name `frameDrops` and label `Drops:` mislead every reader who has not traced the source. No code-behaviour change is required.

---

## 1. Frame drop counter source-level trace (Deliverable 1)

### 1.1 Declaration

`[FACT]` `firmware-v3/src/core/actors/RendererActor.h:131` — field declaration:
```
uint32_t frameDrops;          // Frames that exceeded budget
```
Inline comment on the same line already states the true semantic — "Frames that exceeded budget", not "frames dropped from rendering".

`[FACT]` `firmware-v3/src/core/actors/RendererActor.h:130` — sibling counter:
```
uint32_t framesRendered;      // Total frames since start
```

`[FACT]` Initialisers at `RendererActor.h:139` and `:145`: both `framesRendered` and `frameDrops` start at 0 and are reset to 0 by `RenderStats::reset()`.

### 1.2 The single increment site

`[FACT]` `firmware-v3/src/core/actors/RendererActor.cpp:2604-2612`:
```
void RendererActor::updateStats(uint32_t frameTimeUs, uint32_t rawFrameTimeUs)
{
    m_stats.framesRendered++;

    // Check for frame drop (exceeded budget)
    if (rawFrameTimeUs > LedConfig::FRAME_TIME_US) {
        m_stats.frameDrops++;
        TRACE_INSTANT("frame_drop");
    }
```

`[FACT]` `LedConfig::FRAME_TIME_US` is defined at `RendererActor.h:115` as `1000000 / TARGET_FPS` where `TARGET_FPS = 120` (line 114). The numeric value is **8333 us**.

`[FACT]` Verified via `mcp__clangd__find_references` on `RendererActor::updateStats` (line 2604, column 22): exactly 2 references: the definition itself and one call site at `RendererActor.cpp:1128-1129` (column 4 is the prologue brace, the actual call is line 1129).

`[FACT]` Verified via `mcp__clangd__get_call_hierarchy`: `updateStats` has exactly one incoming caller, `RendererActor::onTick` (line 990, call site 1128).

### 1.3 The single read sites

`[FACT]` Reads of `m_stats.frameDrops` (full inventory via `rg`):

| File:line | Purpose |
|---|---|
| `RendererActor.cpp:1174` | `onStop()` log line: "Stopping - rendered %lu frames, %lu drops" |
| `RendererActor.cpp:2610` | the increment itself (read-modify-write) |
| `core/actors/ActorSystem.cpp:844` | `dumpStatus()`: `printf("Frames: %lu, Drops: %lu\n", rs.framesRendered, rs.frameDrops)` |
| `serial/SerialCLI.cpp:460` | `dbg status` output: `drops=%lu` (lower-case label in `frame:` block, lines 457-465) |

`[FACT]` No JSON/REST/WebSocket exposure of `frameDrops`: `rg -n "framesRendered" src/` returns codec/network surfaces that copy `framesRendered` (e.g. `HttpDeviceCodec.h:53`, `WebServer.cpp:1063`) but **not** `frameDrops`. The counter is serial-only.

### 1.4 Reporting path through `s` and `dbg status`

`[FACT]` The output line that produced the bench evidence "Frames: 833,982 ... Drops: 1,016,792" form comes from `ActorSystem::dumpStatus()`, dispatched by the `s` serial command. Source: `firmware-v3/src/core/actors/ActorSystem.cpp:836-852`.

`[FACT]` The `dbg status` audio command also emits a frame block (lines 456-465 of `SerialCLI.cpp`) with `drops=` in lower-case alongside `target_fps`, `frames`, `fps`, `avg_us`, `min_us`, `max_us`, `cpu`.

### 1.5 Semantic meaning of an increment

`[FACT]` An increment occurs when a single rendered frame's **raw work time** — the wall-clock time from `frameStartUs = micros()` (line 993) until **after** the LED hardware show (`m_ledDriver.show()` at line 2596) returns and `frameEndUs = micros()` is sampled at line 1068 — exceeds 8333 us. This is established by:

- `rawFrameTimeUs` is computed at `RendererActor.cpp:1069`: `rawFrameTimeUs = frameEndUs - frameStartUs`.
- The pacing block at lines 1085-1106 runs AFTER `rawFrameTimeUs` is computed (so the timer-blocked pacing wait is NOT included in `rawFrameTimeUs`).
- `rawFrameTimeUs` is passed to `updateStats()` at line 1129; that is the value compared against `FRAME_TIME_US` at line 2609.

`[FACT]` Inline comment at `RendererActor.cpp:1118-1120` corroborates: "Update statistics (use raw time for drops, throttled time for FPS). Surface 1 Tier 1: log RAW pre-pacing work time so we measure actual CPU time spent in render rather than the post-throttle 8.33 ms cadence."

`[INFERENCE]` Therefore: a Drop means "this rendered frame's compute (renderFrame + colour correction + LED show) exceeded the 120 FPS budget". It does NOT mean "this frame was suppressed", "the renderer skipped this frame because no audio was available", or "the LEDs failed to update".

### 1.6 Expected firing frequency under normal operation

`[INFERENCE]` Pure function of effect-CPU cost. From `firmware-v3/CONSTRAINTS.md` § Timing Budget: baseline effects measured ~7.7 ms total frame time (so 7700 us, under 8333 us → no Drop). Heavier effects (Chimera Crown, Kuramoto Transport, Talbot Carpet — named in the comment at `RendererActor.cpp:1156-1159`) "regularly bust the budget".

`[FACT]` Prior bench evidence in `firmware-v3/docs/research/phase1b_runtime_evidence_2026-04-27/lean_trace_rerun/health/burnin_30min_idle_ap_no_client.txt`: "Frames: 58342, Drops: 530" — **0.91 % drop ratio** on a lighter effect set, idle AP, no client. Same firmware family. This is direct evidence that the counter's value depends on effect cost, not on AP-VP coupling.

---

## 2. AP-VP contract source-level trace (Deliverable 2)

### 2.1 Producer side — AudioActor

`[FACT]` AudioActor lives at `firmware-v3/src/audio/AudioActor.{h,cpp}` (not under `core/actors/` as the brief assumed). It runs on Core 0.

`[FACT]` Production cadence — **125 Hz** on the canonical K1 V2 build. Confirmed by inline comments at multiple sites in `AudioActor.cpp`:
- Line 715: "32kHz:   256/128 = 2 chunks → 125 Hz publish"
- Line 1153: "~6 events per hop @ 125 Hz = 750 events/sec; ~3.6 s ring fill"
- Line 1632: "vs 125 Hz target. Event-driven I2S (DMA interrupt → queue)"
- Line 1655: "ZERO_HOPS_RECOVERY_THRESHOLD = 250;  // ~2s at 125 Hz"
- Line 2856: "This achieves 125 Hz (= 16000 Hz / 128 samples)"
- Line 3148: "K1v2 32 kHz / HOP=256 = 125 Hz"

`[FACT]` AudioActor publishes a `ControlBusFrame` to a lock-free single-writer/single-reader `SnapshotBuffer` once per hop. Three publish sites: `AudioActor.cpp:1130`, `:2067`, `:3819` (different backends; one is active per build). Form: `m_controlBusBuffer->Publish(frameToPublish);`.

`[FACT]` Each published frame carries a monotonically increasing `hop_seq` (declared `ControlBus.h:120`). Renderer compares against its last-seen `m_lastControlBusSeq` to detect new frames.

### 2.2 Consumer side — RendererActor

`[FACT]` RendererActor lives at `src/core/actors/RendererActor.{h,cpp}`, runs on Core 1, priority 5 (highest). Tick configuration at `Actor.h:481-490`:
- tickInterval = 0 → **self-clocked**.
- Comment lines 475-476: "Self-clocked tick mode (interval 0): renderer paces itself at 120 FPS."

`[FACT]` Actor::run dispatch (`src/core/actors/Actor.cpp:275-386`): in self-clocked mode (line 356-359, 367), the loop polls the message queue with **zero timeout**, dispatches one message if any, and calls `onTick()` regardless. Effectively: `onTick()` fires every loop iteration of the renderer's task — **unconditionally**.

### 2.3 The AP-VP coupling point inside `onTick`

`[FACT]` `RendererActor.cpp:1018` — `renderFrame();` is called unconditionally near the top of `onTick`. Followed by colour correction (1032-1050), capture taps (1052-1055), LED show (1057-1065), pacing wait (1085-1106), watchdog feed (1112-1115), stats update (1129).

`[FACT]` Inside `renderFrame()` (definition at `RendererActor.cpp:1735`), the audio-snapshot read sequence is at lines 1820-1900:

- Line 1838: `uint32_t seq = activeBuffer->ReadLatest(m_lastControlBus);` — copies the **most recent** published frame, regardless of staleness.
- Lines 1850, 1862: sequence-number diffing against last-seen.
- Line 1866-1871: `if (seq != m_lastControlBusSeq) { … resync extrapolation base … }` — only updates the audio-time anchor when a new hop arrives.
- Lines 1873-1892: computes age of the current snapshot vs `now_us`, compares against `staleness_s` threshold (default 100 ms — see `AudioTuning.h:163`).
- Line 1892: `audioAvailable = sequence_changed || age_within_tolerance;`

`[FACT]` `audioAvailable` then propagates into:
- The `audioContextAvailable` flag forwarded into per-effect render context (`RendererActor.cpp:2029, 2076-2082, 2092, 2097`).
- The Independent-mode strip dispatch (`renderStripIndependent` arg at line 2383).
- A diagnostic LW_LOGW at line 1972 ("Audio unavailable: …") when audio debug is enabled and `audioAvailable` is false.

`[FACT]` `audioAvailable` is **never** used to skip a render. There is no `if (!audioAvailable) return;` anywhere in `onTick` or `renderFrame`. Verified by inspection of all 25 `return;` statements in `RendererActor.cpp` (lines 1238, 1440, 1500, 1534, 1538, 1588, 1596, 1720, 1795, 1812, 2104, 2127, 2154, 2172, 2388, 2390, 2397, 2466, 2651, 2663, 2727, 2890, 2894, 2924, 2988). The early-return at line 1812 ("Skip all effect rendering") is gated on `m_renderingDisabled`, an operator control surface, **not** on audio availability. The early-return at 1795 is for SynqMatrix director transitions, **not** audio availability.

### 2.4 The actual synchronisation primitive

`[INFERENCE]` The contract is **best-effort latest-snapshot polling**:

| Property | Mechanism |
|---|---|
| Producer cadence | 125 Hz (ESV11 32 kHz, hop 256) |
| Consumer cadence | 120 FPS target (8333 us frame budget) |
| Transport | Lock-free single-writer/single-reader `SnapshotBuffer<ControlBusFrame>` |
| Synchronisation | `ReadLatest(out)` — atomic copy of the most recent published frame |
| Freshness gating | sequence-number diff + age vs `audioStalenessMs` (default 100 ms) |
| Skip-on-no-data behaviour | **None.** Renderer always renders, with the last-seen audio frame (or the default-constructed initial `ControlBusFrame` if nothing has been published yet). The `audioAvailable` flag advises effects only. |
| Skip-on-stale behaviour | **None.** `audioAvailable` becomes false but the renderer still calls every effect's render path; effects either use the stale audio or fall back to non-reactive behaviour at their own discretion. |

`[INFERENCE]` This is intentional design. The alternative (block on new audio frame, accumulate latency) would couple LED frame rate to audio hop rate (125 Hz upper bound vs the 120 Hz visual target), introduce queue / wait primitives, and on missed audio hops would cause visible LED stutter. The lock-free latest-snapshot pattern is canonical for hard-real-time AV pipelines: the visual pipeline drifts in/out of phase with the audio pipeline by design, and corrects extrapolation on each newly-arrived hop.

### 2.5 Original design-doc context

`[FACT]` `git log --all --oneline -S "frameDrops" -- firmware-v3/src/core/actors/RendererActor.cpp firmware-v3/src/core/actors/RendererActor.h` returns exactly one commit: `5ee8aa84` "refactor: complete repository restructure (Phases 0-7)". The counter survived the firmware-v2 → firmware-v3 reorganisation; its original introduction commit is **not** reachable from current HEAD because pre-restructure history was archived externally (see commit body: "Removed from tracking (archived externally): docs/ (200+ files of accumulated analysis, architecture, planning)").

`[INFERENCE]` Captain's "the original investigative context for this design decision has degraded and cannot be reconstructed from memory" is corroborated by git — the pre-`5ee8aa84` history that would have carried the counter's introduction discussion is no longer in-tree. Counter survives, design discussion does not. This is consistent with the brief's framing of the problem.

`[FACT]` Adjacent surviving design statement — `firmware-v3/CONSTRAINTS.md:9-19`: "Frame total (raw) | 8.33ms (120 FPS target) | ~7.7ms on baseline effects" and "Rule: Effect code must complete in < 2ms to maintain 120 FPS." This document treats the 8.33 ms frame budget as the load-bearing invariant — exactly the condition the counter measures violations of.

---

## 3. Definitive characterisation of observed counter values (Deliverable 3)

### 3.1 Bench evidence (captured verbatim from brief)

| Build | Uptime | FPS / target | Drops | Frames | Drop ratio | Frame time avg/max (us) | LED show skips |
|---|---:|---:|---:|---:|---:|---:|---:|
| `..._sta_validation` (pre-flash) | 173 min | 110 / 120 | 833,982 | 1,016,792 | 82.0 % | 9041 / 83055 | 0 |
| `...` canonical AP-only (post-flash) | 63 s | 106 / 120 | 5813 | 5843 | 99.5 % | 9736 / 33067 | 0 |

### 3.2 Characterisation

**Verdict: counter-reporting artefact (by-design, not a defect).** Both numbers are correct measurements of the thing the counter actually measures — number of rendered frames whose pre-pacing work exceeded 8333 us. They are misleading only because the counter's name (`frameDrops`) and its serial label (`Drops:`) imply that those frames were suppressed or lost, when in fact every Drop is also a rendered, displayed frame.

### 3.3 Sub-question (a): why does the drop ratio rise on the leaner canonical build?

Captain's a-priori hypothesis ("more CPU headroom on the leaner build → more skip decisions per second → counter measures coupling-invocation cadence") is **refuted by source trace**, because there is no skip path. The counter increments per **rendered frame**, not per coupling invocation. With no skip path to invoke at any cadence, the headroom argument does not apply.

The actual reason for the ratio difference is visible directly in the bench numbers:

`[FACT]` Both builds report `avgFrameTimeUs` (the rolling-averaged post-pacing frame time tracked at `RendererActor.cpp:2622-2628`) at **above 8333 us**:
- sta_validation: 9041 us avg (which is ~120 % of budget — every frame on average is over budget)
- canonical: 9736 us avg (which is ~117 % of budget)

`[INFERENCE]` Both builds were running effects whose typical work-time exceeded the 8333 us budget. The drop ratio is therefore the fraction of those frames whose **raw** work-time also exceeded 8333 us. Because the avg is already > 8333 us in both cases, the ratio is dominated by variance, not mean.

`[INFERENCE]` The canonical build's higher ratio (99.5 % vs 82 %) is consistent with: (i) different effect active at sample time (the bench captures one moment, not a controlled corpus), (ii) different runtime maturity (63 s uptime vs 173 min — early uptime has not warmed up the rolling-average enough to smooth low-variance windows), (iii) different active SynqMatrix director / song-aware transitions whose work cost varies. The numbers are **not** evidence of a regression introduced by the canonical build; they are evidence that a single bench point per build is insufficient to compare drop ratios fairly.

`[FACT]` This is corroborated by prior bench evidence in `firmware-v3/docs/research/phase1b_runtime_evidence_2026-04-27/lean_trace_rerun/health/burnin_30min_idle_ap_no_client.txt`: "Frames: 58342, Drops: 530, Frame time avg=8375 min=8242 max=27402 us" — **0.91 % drop ratio on the same firmware family**, baseline effect, 30 min idle. When `avgFrameTimeUs` sits at 8375 us (barely over 8333), only the variance-tail crosses the threshold; the ratio collapses to under 1 %. The 82-99 % current readings reflect heavier effects active during the 2026-05-14 bench, not a property of the AP-only build.

`[HYPOTHESIS]` The fastest test to falsify any residual concern is to capture both ratios with the same effect locked, the same SynqMatrix mode locked, and ≥5 minutes uptime per build. Expected outcome: the ratios converge within ±5 percentage points, and both move with effect cost in lock-step with the avgFrameTimeUs reading. Not a deliverable here — flagged as a Findings Appendix candidate.

### 3.4 Sub-question (b): why is panel output visually correct despite 99.5 % drops?

Because every Drop is also a **rendered, shown** frame. The drop counter does not suppress LED output. Source-level proof:

`[FACT]` In `onTick`, the order is: `renderFrame()` (line 1018) → colour correction (1032-1050) → `showLeds()` (1061) → frame-time measurement (1067-1079) → pacing wait (1085-1106) → `updateStats()` (1129). The Drop check at `updateStats` (line 2609) runs **after** `m_ledDriver.show()` has already pushed the frame to the WS2812 strips.

`[FACT]` `LedDriver_S3::show()` (`src/hal/esp32s3/LedDriver_S3.cpp:147`) has its own separate skip counter (`m_stats.showSkips++` at line 151), incremented only when the RMT mutex acquisition times out (mutex-contention safety guard). Bench evidence: `LED show: ... skips=0` on both builds — there is **zero** hardware-level skip. Every frame the renderer prepares actually reaches the LEDs.

`[INFERENCE]` The drop counter is reporting a budget overrun on a frame that was nevertheless displayed. The pacing block at lines 1085-1106 absorbs the overrun by simply not waiting (when `frameTimeUs >= FRAME_TIME_US`, `pacingWaitUs` stays 0 and the timer is not armed; the next frame starts immediately). The observed FPS drops below the 120 target (to 110 / 106 in the bench) — this is the **only** visible effect of high Drop ratios, and it is captured by the `currentFPS` reading, not by `frameDrops`.

### 3.5 Reconciliation

Bench evidence and source trace are **fully consistent** once the counter is understood as a budget-overrun counter rather than a skip counter. The brief's hypothesis that the counter measures coupling-invocation skip decisions is refuted; the alternative hypothesis that it is a counter-reporting artefact is confirmed by source. The 82 % / 99.5 % numbers are accurate measurements of an effect-cost / budget-tightness condition, not of any AP-VP defect or skip pathology.

---

## 4. Recommendation (Deliverable 4)

**By-design. Rename the counter. Re-label the serial output. No behaviour change.**

### 4.1 Required changes (documentation + naming only)

`[INFERENCE]` Specific edits, each in scope and each verifiable without hardware:

1. **Rename the field.** `RendererActor.h:131` — change `uint32_t frameDrops` to `uint32_t framesOverBudget`. Propagate to:
   - `RendererActor.h:139, :145` (constructor initialiser and `reset()`)
   - `RendererActor.cpp:1174` (onStop log)
   - `RendererActor.cpp:2610` (increment site)
   - `ActorSystem.cpp:844` (s-command output)
   - `SerialCLI.cpp:460` (dbg-status output)
   - Total touch points: 6 in C++ source.

2. **Re-label the serial output.**
   - `ActorSystem.cpp:844`: change `"Frames: %lu, Drops: %lu\n"` to `"Frames: %lu, OverBudget: %lu (%.1f%%)\n"` with the ratio computed in-place — agents reading the line will then see the budget-overrun framing rather than the skip framing.
   - `SerialCLI.cpp:457`: change `drops=%lu` to `over_budget=%lu` in the `frame:` block.
   - `RendererActor.cpp:1174`: change "%lu drops" to "%lu over-budget frames".

3. **Update CONSTRAINTS.md** at `firmware-v3/CONSTRAINTS.md` to add an explanatory row under § Timing Budget: "Budget overrun is reported by the renderer as `framesOverBudget`. Every increment is a rendered, displayed frame whose pre-pacing work exceeded the 8333 us target. It does not indicate a skipped frame, suppressed output, or AP-VP coupling fault."

4. **Optional but recommended:** add a 2-line note to `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md` so effect authors interpret `framesOverBudget` readings correctly when profiling their effects.

### 4.2 What this accomplishes

- Future agents who see a high "Drops" value will not waste cycles investigating an imagined AP-VP defect.
- Captain's bench reports no longer hide the actual semantic behind a misleading label.
- Existing effect-development discipline (the 2 ms render contract in `CLAUDE.md`) is reinforced — over-budget is a real signal that an effect is too heavy, but it is a **performance** signal, not a **correctness** signal.

### 4.3 Risk

Minimal. Single-direction rename, serial-only consumer, no protocol consumer (verified above: `frameDrops` is not in the REST/WS surface). The rename will appear in commit diffs and the new label will appear in serial logs from the next flash onward; no field workflow depends on the old label.

### 4.4 Out-of-scope but adjacent

- The fact that both builds in the 2026-05-14 bench show `avgFrameTimeUs > 8333` (i.e. effect work routinely exceeds budget) is a separate performance concern unrelated to the counter. The renderer is healthy, the panel is healthy, but per-effect optimisation work remains valuable. **Not** this investigation's scope per the brief; flagged in the Findings Appendix.
- The FPS readings (110, 106) being below the 120 target is a direct consequence of avgFrameTimeUs being above budget. Not a defect, also flagged in the Appendix.

---

## 5. Findings Appendix (separate workstream candidates)

These surfaced during the trace and are recorded for accountability. They are **not** investigated here.

1. **Both builds show avgFrameTimeUs above the 8333 us frame budget.** Captain bench captured both builds running effects whose typical work exceeds budget. Per `CLAUDE.md` § Hard Constraints, effect render code must complete in < 2 ms; that is the per-effect ceiling, not the full per-frame ceiling. The full per-frame ceiling is 8333 us (includes effect + colour correction + showLeds). When `avgFrameTimeUs` sustains above 8333 us, observed FPS drops below 120. Worth a separate audit to identify which effects in the current registry routinely bust budget and whether their cost is intrinsic or optimisable.

2. **Comparable drop ratios across builds require controlled-effect benchmarking.** The 2026-05-14 bench measured two different runtime conditions, not two configurations of the same condition. A fair comparison would lock effect, lock SynqMatrix director state, run both builds for ≥5 minutes after warm-up, then compare. Recommended for any future regression-detection workstream.

3. **MabuTrace `TRACE_INSTANT("frame_drop")` is already wired** at `RendererActor.cpp:2611`. Capturing a Perfetto trace with the canonical-`_trace` env (per `firmware-v3/docs/debugging/MABUTRACE_GUIDE.md`) will show the per-effect histogram of over-budget frames and pinpoint the worst offenders without further source archaeology.

4. **`hop_seq` lag instrumentation exists but is consumer-side only.** `RendererActor.cpp:1862-1864` traces `audio_snapshot_hop_seq_lag` (renderer view of how many hops accumulated between two consecutive renderer reads). This is useful debug telemetry but is unrelated to the Drop counter; flagged so future agents do not conflate the two.

5. **The pre-restructure git history is no longer reachable.** Commit `5ee8aa84` archived 200+ docs externally. The original frameDrops introduction commit (and its design discussion) is not in the current repo. Captain's note that the design context has "degraded" is structural, not a memory failure on Captain's side. If this matters for future audits, the external archive may need to be located.

---

## 6. Evidence trail and verification commands

```
# Counter increment site
rg -n "framesDropped|frame_drops|framedrop|frameDrops|drops_|m_drops|skipCount" src/
  → RendererActor.cpp:1174, :2610, :131, :139, :145, ActorSystem.cpp:844, SerialCLI.cpp:460

# Single-caller verification (clangd)
mcp__clangd__get_call_hierarchy(RendererActor::updateStats, line 2603, col 22)
  → incoming_count: 1 (onTick, line 990, call_site 1128)

# Reference inventory (clangd)
mcp__clangd__find_references(RendererActor::updateStats, line 2603, col 22)
  → 2 locations (definition + 1 call)

# AP-side publish rate
rg -n "FRAME_HZ|125\\s*Hz|HOP_SIZE|publishRate" src/audio/
  → 17 corroborating sites in AudioActor.cpp, including line 715 "256/128 = 2 chunks → 125 Hz publish"

# Renderer tick mode
rg -n "Renderer\\(\\)" src/core/actors/Actor.h
  → line 481-490: tickInterval = 0 (self-clocked), priority 5, Core 1

# Skip-on-no-audio path search (negative result)
rg -n "audioAvailable" src/core/actors/RendererActor.cpp
  → 10 sites; none gate renderFrame() entry or onTick() early-return

# Prior burn-in bench (0.91 % drop ratio on lighter effect)
docs/research/phase1b_runtime_evidence_2026-04-27/lean_trace_rerun/health/burnin_30min_idle_ap_no_client.txt
  → "Frames: 58342, Drops: 530"

# Git history (pre-restructure unreachable)
git log --all --oneline -S "frameDrops" -- firmware-v3/src/core/actors/RendererActor.cpp
  → single commit 5ee8aa84 (the restructure itself)
```

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-14 | agent:claude-opus-4-7-1m-forensic | Created. Traced frameDrops counter to single increment site (budget-overrun guard, not AP-VP skip decision); refuted Captain's coupling-cadence hypothesis with source evidence; characterised the 82 % / 99.5 % bench evidence as counter-naming artefact; recommended rename to `framesOverBudget` with no behaviour change. |
