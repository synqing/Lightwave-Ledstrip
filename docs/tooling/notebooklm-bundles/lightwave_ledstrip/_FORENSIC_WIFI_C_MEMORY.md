---
abstract: Forensic memory + transcript archaeology of the 6+ K1 STA failures cited as canonical evidence for AP-only-EVER doctrine. Captain-directed evidence for strategic round-table on dual-mode reality. Reports failure chronology with per-incident symptom classification, '6+' count verification, and 'KNOWN BROKEN' string provenance. Crispy was OFF for this session — claude-mem L1/L2/L3 + file-based memory + git blame only.
---

# SSA-Forensic-C — WiFi-Mode Story: Memory & Transcript Archaeology

## Tooling availability

- **Crispy:** OFF. `$CRISPY_SOCK` and `$RECALL_CLI` both empty. Per project CLAUDE.md, no fabrication. Crispy recall workflow (Task 6) skipped.
- **claude-mem:** ON. L1 search → L2 timeline → L3 `get_observations` used.
- **File-based memory:** ON. Two relevant files read at `~/.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip/memory/`.
- **Git (out-of-scope but unavoidable for KNOWN-BROKEN provenance):** Used once (`git show 5d94dab0`, `git blame docs/protocol/k1-rest-contract.yaml`). This overlaps SSA-A's git scope but is necessary to date the doctrine string.

---

## Task 1 — claude-mem search for STA history

Searches run (project=Lightwave-Ledstrip, FTS5+semantic):

| Query | Hits | Notable IDs |
|---|---|---|
| `STA mode wifi failed` | 10 | #47125, #47140, #47152, #47156, #47157, #47158, #47159, #47179, #48568 |
| `WIFI_MODE_STA AUTH_EXPIRE` | 10 | #47125, #47140, #47152, #47156, #47157, #47158, #47159, #47179 |
| `AP_STA concurrent corrupt` | 10 | #47159, #48429, #48432, #48457, #48568 |
| `K1 connect router 802.11 driver` | 18 | (mostly soak-failure noise; no new STA-specific hits) |
| `KNOWN BROKEN apMode` | 10 | #47155, #47158, #47160, #47179, #48457 |
| `k1-rest-contract apMode disabled` | 14 | #48432, #48444 (STA-validation env), #48457 |
| `m_forceApOnly serial wifi connect escape hatch` | 10 | #47977, #48429 |
| `VX220 OPTUS BSSID PMF compatibility profile` | 8 | (no relevant hits — terms not present in primary observations) |

**Critical finding:** The `VX220` and `OPTUS_738CC0N` SSIDs and the `AUTH_EXPIRE`/`AUTH_FAIL`/`6+` numbers appear in claude-mem ONLY as a quoted retelling from `firmware_wifi_architecture.md`. The primary observations that introduced them are NOT in the indexed memory — only the file-based memory entry contains them, and downstream observations (#47125, #47159) cite that file rather than independent evidence.

L3 batch `get_observations([47125, 47140, 47152, 47156, 47157, 47158, 47159, 47179, 48568, 48429, 48432])` and a follow-up `get_observations([47155, 47160, 47977, 48444, 48457, 47168, 47285])` retrieved full details. Findings reported per-task below.

---

## Task 2 — the "6+ failures" count

**The "6+" number originates in a single secondary source: the file-based memory entry `firmware_wifi_architecture.md` (created 2026-04-26 during the cleanup pass that split MEMORY.md into topic files).**

Verbatim from the file:

> Reproduced across multiple routers (`VX220-013F`, `OPTUS_738CC0N`) and 6+ firmware-level mitigation attempts:
> - Compatibility profiles
> - BSSID scrubs
> - AP-stop-before-join
> - PMF optional
> - (and more)

The bullet list cites four named mitigations followed by "(and more)" — the explicit count of distinct mitigations in the file is **four**, with "6+" added editorially.

Every claude-mem hit referencing "6+" cites this memory file as its source:
- #47125 (2026-05-01 08:59): `"6+ firmware mitigation attempts"` — `files_read` includes `firmware_wifi_architecture.md`.
- #47159 (2026-05-01 09:11): `"6+ mitigation attempts: compatibility profiles, BSSID scrubs, AP-stop-before-join, PMF optional, and more"` — `files_read` includes `firmware_wifi_architecture.md`.
- #48568 (2026-05-04 18:32): does not cite a count, only restates the AP+STA-corruption claim.

**No primary observation in claude-mem documents 6+ distinct dated STA-failure incidents.** Captain-directed search for primary attempts located:
- One narrative event window: "Architectural decision finalised: Feb 6–16, 2026. Re-validated: March 3, 2026." (file-memory).
- The 2026-04-18 Wave-2 OTA debugging incident where an agent proposed flashing `tab5_sta` to bypass K1 (recorded in `feedback_never_change_network_architecture.md`) — this is about debugging architecture, NOT about a STA-on-K1 attempt.

Searches with `dateStart` constraints around Feb–Mar 2026 windows returned no per-attempt observation chains for STA mitigations (compatibility profiles, BSSID scrubs, AP-stop-before-join, PMF optional). Either those sessions predate the indexed memory (possible — Feb 2026 retention is sparse for this project) OR the count was constructed retrospectively without a per-incident audit trail.

---

## Task 3 — symptom classification per attempt

Only TWO concrete symptom strings are cited anywhere in the indexed memory or file-memory:

1. `AUTH_EXPIRE` (reason 2)
2. `AUTH_FAIL` (reason 202)

Both are 802.11 driver-level reason codes (ESP-IDF `wifi_err_reason_t`). Both come from the same upstream source: `firmware_wifi_architecture.md`. Every downstream observation (#47125, #47159) propagates the same two codes and the same two router SSIDs.

**Per-attempt classification:**

| Attempt | Symptom-class evidence | Confidence |
|---|---|---|
| Compatibility profiles | Driver-level (cited as failing with AUTH_EXPIRE/AUTH_FAIL) | LOW — no per-attempt log evidence in memory |
| BSSID scrubs | Driver-level (same as above) | LOW |
| AP-stop-before-join | Driver-level (same) | LOW |
| PMF optional | Driver-level (same) | LOW |
| "(and more)" | UNKNOWN | UNKNOWN |
| 2nd "(and more)" | UNKNOWN | UNKNOWN |

**Mixed-class evidence?** No primary observation in indexed claude-mem differentiates higher-layer (DHCP, captive portal, app-level) failures from 802.11 driver-level failures. The file-memory text aggregates all mitigations under a single `AUTH_EXPIRE/AUTH_FAIL` symptom umbrella. There is no per-incident split between e.g. PMF-mismatch (driver), BSSID-roaming (driver), captive-portal-hang (app), DHCP-timeout (lwIP). The original sessions that produced the count are not retrievable from current indexed memory; the symptom-uniformity claim cannot be independently corroborated.

**Adjacent unrelated wireless symptom (not STA on K1):** #48568 (2026-05-04, prompt #5) frames "AP+STA simultaneous mode causes memory/state corruption — known hardware bug" with the meta-acknowledgement that *"Existing agent-facing documentation may contain an exaggerated 'OTT scare warning' about STA mode"* and *"the actual severity and current fix status of the bug is unclear"*. This is Captain's own active-session reservation — recorded at 18:32 on the same day this forensic task was issued.

---

## Task 4 — "KNOWN BROKEN" provenance

The string `"Enable STA mode (KNOWN BROKEN on K1 — do NOT use)"` is at `docs/protocol/k1-rest-contract.yaml:683`. A second identical-tone string at line 699: `"Connect to a network (KNOWN BROKEN on K1)"`.

**Git blame attribution (verified):**

```
5d94dab0a (K1 Research Agent 2026-03-21 11:43:58 +0800 683)
    description: "Enable STA mode (KNOWN BROKEN on K1 — do NOT use)"
5d94dab0a (K1 Research Agent 2026-03-21 11:43:58 +0800 699)
    description: "Connect to a network (KNOWN BROKEN on K1)"
```

**Commit `5d94dab0`:**
- Date: 2026-03-21 11:43:58 +0800
- Author: `K1 Research Agent` (an agent identity, not Captain)
- Co-author: `Claude Opus 4.6 (1M context) <noreply@anthropic.com>`
- Subject: `docs: add K1↔Tab5 protocol contracts and LVGL component reference`
- This is the **single commit that introduced the entire `k1-rest-contract.yaml` file** (682 lines). The KNOWN BROKEN string was born in the contract's first revision and has never been updated.
- Commit message describes the contract as "agent harness documents — mandatory reading before modifying K1 network code". No reference to a hardware test session, soak run, or empirical reproduction.

**Memory cross-check:** No claude-mem observation around 2026-03-21 documents a STA hardware test that would justify the "KNOWN BROKEN" assertion. The closest in-memory STA-validation entries are:
- #47977 (2026-05-02): adds a stub `[wifi_sta]` section to fix CI. NOT a hardware test.
- #48444 (2026-05-03): adds a validation-only PlatformIO env. NOT yet flashed/tested at observation time.
- #48457 (2026-05-03): records that AP REST/WS validation was BLOCKED because the Codex host could not join the K1 AP, and STA validation was NOT USED. **No hardware STA test was run.**

**Verdict on KNOWN-BROKEN provenance:**
- Authored by `K1 Research Agent` + Claude Opus 4.6 in a documentation commit, 2026-03-21.
- Provenance trail: file-memory (`firmware_wifi_architecture.md`, created 2026-04-26 — *one month AFTER the KNOWN BROKEN string was committed*) cites Feb–Mar 2026 incidents to retroactively support the KNOWN BROKEN doctrine.
- The string in the contract was committed BEFORE the file-memory entry that supposedly justifies it.
- No claude-mem observation records a hardware-test session producing the KNOWN BROKEN verdict.

This matches Captain's own characterisation in #48568: *"exaggerated 'OTT scare warning' about STA mode to ensure agents never attempt to enable it."*

---

## Task 5 — file-based memory

### `firmware_wifi_architecture.md` (full body captured)

- Created 2026-04-26 during MEMORY.md cleanup pass (per Document Changelog).
- `originSessionId: 3226d895-0acd-4a85-86ee-eccd4964759d` — single-session origin.
- Type: `reference` (claims canonical-fact status, not lessons-learned).
- Claims (verbatim summary):
  - K1 boots `WIFI_MODE_AP`, not `WIFI_MODE_APSTA`.
  - STA fails with `AUTH_EXPIRE (reason 2)` and `AUTH_FAIL (reason 202)`.
  - Reproduced across `VX220-013F` and `OPTUS_738CC0N`.
  - "6+ firmware-level mitigation attempts" enumerated as 4 named + "(and more)".
  - "AP+STA concurrent corrupts STA auth state."
  - "lwIP TCP buffers (5744 bytes default) cause socket teardowns under concurrent load."
  - "AP uses an open network (no password) to eliminate WPA handshake as a failure point for clients."
  - Decision finalised "Feb 6–16, 2026", re-validated "March 3, 2026".
- `<system-reminder>` injected at read time: *"This memory is 8 days old. Memories are point-in-time observations, not live state — claims about code behavior or file:line citations may be outdated. Verify against current code before asserting as fact."*
- File-memory itself does not include link evidence to the original sessions, commits, or serial logs that produced the `AUTH_EXPIRE` reading. It is a synthesised note, not a primary log.

### `feedback_never_change_network_architecture.md` (full body captured)

- Created from session `24b94d09-db42-4ed0-94ad-80d31136707c`.
- Type: `feedback` (operational rule).
- Triggering event: 2026-04-18 Wave-2 OTA hardware verification. K1 internal heap was 15.9 KB (largest block 7.6 KB); K1 AP was failing to forward laptop→tab5 packets. Agent proposed flashing tab5 with `tab5_sta` env; Captain interpreted this as an unauthorised wireless-architecture pivot.
- This file documents an architecture-pivot rule, NOT a STA-on-K1 attempt. The actual K1-STA attempts it gestures at are referenced through CLAUDE.md and `firmware_wifi_architecture.md` — not first-hand here.

### `MEMORY.md` index entry

`firmware_wifi_architecture.md` description-line in the MEMORY.md index:

> "STA fails at 802.11 driver. AP+STA concurrent corrupts auth state. Decision Feb 2026, re-validated March 2026. CLAUDE.md is source of truth."

Index points back to CLAUDE.md as canonical, which in turn is a *constraint* (rule), not *evidence*.

---

## Task 6 — Crispy recall

Skipped — Crispy is OFF. `echo $CRISPY_SOCK` returned empty; `echo $RECALL_CLI` returned empty. Per project CLAUDE.md "Crispy is OFF" section, no fallback retry into stale paths.

---

## Task 7 — Captain's-correction transcript

Acknowledged. Captain's most recent statement reverses the doctrine ("dual-mode is the GOAL but current dev pipeline is AP-only"). #48568 (2026-05-04 18:32) is the earliest indexed observation explicitly capturing Captain's reservation about the OTT scare warning. The exact "dual-mode GOAL" wording is too recent to appear in indexed memory; not searched.

---

## Failure chronology table

| Date | Symptom-class (as recorded) | Attempt summary | Conclusion drawn at the time | Evidence quality |
|---|---|---|---|---|
| 2025-12-20 | n/a (origin) | Original network-stack commit `7700aba2`. Commit message describes "Soft-AP Fallback: STA+AP mode supported" + captive portal as designed architecture. | STA+AP designed-in. | HIGH (git history, #47155). |
| 2026-02-06 to 02-16 | Driver-level (AUTH_EXPIRE / AUTH_FAIL — *aggregate claim*) | "Compatibility profiles, BSSID scrubs, AP-stop-before-join, PMF optional, (and more)" against `VX220-013F` and `OPTUS_738CC0N`. | "Architectural decision to go AP-only finalised." | LOW. Cited only in file-memory (`firmware_wifi_architecture.md`, created 2026-04-26). No primary session observation, no serial logs, no commits per-attempt found in claude-mem. |
| 2026-02-27 | n/a | Commit `d943101a` ("integrate 95 commits from fix/stable-effect-ids") added the two `WIFI_MODE_APSTA` setter sites at WiFiManager.cpp:692 and 1233. | Portable Mode integration. | HIGH (git blame, #47155). |
| 2026-03-03 | Driver-level (per file-memory) | "Re-validated after identical failure recurrence." | AP-only re-confirmed. | LOW. No primary-observation citation. |
| 2026-03-21 | n/a (doctrine-codification) | `K1 Research Agent` + Claude Opus 4.6 commit `5d94dab0` introduces `docs/protocol/k1-rest-contract.yaml` containing the "KNOWN BROKEN on K1 — do NOT use" string. | KNOWN BROKEN entered the contract. | HIGH (git blame). No hardware test record cited in commit message or contemporaneous memory. |
| 2026-04-18 | Heap exhaustion (K1 internal heap 15.9 KB) | Agent proposed flashing `tab5_sta` to bypass K1 AP forwarding. NOT a STA-on-K1 attempt — a tab5-architecture pivot. | Captain forbade architectural pivots during debugging. | HIGH (`feedback_never_change_network_architecture.md`). |
| 2026-04-26 | n/a | `firmware_wifi_architecture.md` file created during MEMORY.md split. The "6+" count first appears here. | Doctrine canonicalised into file-memory. | HIGH (Document Changelog). |
| 2026-05-01 ~09:00 | n/a (audit) | Multi-subagent F3 investigation. Found `WIFI_MODE_APSTA` setters in source. After tracing call sites and `m_forceApOnly` flag, verdict: "default boot is AP-only; APSTA only via explicit serial `wifi connect`." | CLAUDE.md correct for default boot, doctrine incomplete re. operator escape hatch. | HIGH (#47125, #47140, #47152, #47155–47160, #47179 chain). |
| 2026-05-02 18:17 | CI breakage | Stub `[wifi_sta]` section added to `platformio.ini` to fix `firmware_build_check` red since 2026-03-14. | Doctrine had been actively degrading CI for 6 weeks. | HIGH (#47977). |
| 2026-05-03 18:24 | n/a (decision) | Captain approved temporary STA build profile for REST/WS validation. | First explicit Captain authorisation of a STA build env (validation-only, gitignored credentials, never-default). | HIGH (#48429, #48444). |
| 2026-05-03 19:21 | n/a (validation blocked) | AP REST/WS validation BLOCKED (Codex host can't join AP without losing internet); STA REST/WS NOT USED (Captain hadn't separately authorised this pass). | Neither AP nor STA was hardware-validated. | HIGH (#48457). |
| 2026-05-04 18:32 | n/a (Captain reservation) | Captain explicitly characterises agent-facing STA documentation as possibly an "exaggerated OTT scare warning". Forensic excavation directed. | (current task). | HIGH (#48568). |

---

## Verdict on the 6+ count

**INCONCLUSIVE — leaning toward OVER-COUNTED in primary evidence.**

- The "6+" number appears in exactly ONE primary source: `firmware_wifi_architecture.md`, a synthesised reference note created 2026-04-26 (one month *after* the KNOWN BROKEN doctrine string was committed).
- That source explicitly enumerates **four** named mitigations + "(and more)" — the explicit count is four, with "6+" editorial.
- Indexed claude-mem contains zero per-incident primary observations of distinct STA-failure sessions in Feb–Mar 2026.
- The originating sessions may pre-date current claude-mem retention (Feb 2026 is sparse for this project) OR may have been compressed into the synthesised file-memory without an audit trail.
- Without recovering Feb–Mar 2026 transcripts (Crispy is OFF; raw transcripts unavailable in this session), independent verification of distinct attempts is impossible.

---

## Verdict on symptom uniformity

**UNKNOWN.**

- Only two ESP-IDF reason codes are cited anywhere: `AUTH_EXPIRE (reason 2)` and `AUTH_FAIL (reason 202)`. Both are 802.11 driver-level codes from the synthesised file-memory.
- No per-attempt symptom log exists in indexed memory. Whether each of the named mitigations (compatibility profiles, BSSID scrubs, AP-stop-before-join, PMF optional) failed with the *same* code or with *different* codes cannot be determined.
- The synthesised file-memory presents a single uniform driver-level umbrella; this aggregation may itself be the source of doctrine over-correction. Captain's own May-4 reservation (#48568) directly anticipates this: *"the actual severity and current fix status of the bug is unclear."*
- One concrete anti-uniformity datum: #47159 attributes the failure to "ESP32 WiFi stack shares encryption key pools and radio time between AP and STA — AP operation corrupts STA auth state." This is a *concurrent-mode* hypothesis, not a *STA-only* failure mode. If the actual failure cause is AP+STA contention rather than STA-alone, then a STA-only build (such as `esp32dev_audio_esv11_k1v2_32khz_sta_validation` added 2026-05-03) might succeed where APSTA failed — but no hardware run has been performed to confirm or refute this.

---

## Verdict on KNOWN-BROKEN provenance

**DOCTRINE-ASSERTION — NOT HARDWARE-VALIDATED at the time of authorship.**

- Committed: 2026-03-21 11:43:58 +0800, commit `5d94dab0`, author `K1 Research Agent` + Claude Opus 4.6.
- Commit message scope: documentation-only — "agent harness documents — mandatory reading before modifying K1 network code". No reference to hardware test session, soak run, serial log, or empirical reproduction.
- The file-memory entry that supposedly justifies the KNOWN BROKEN string (`firmware_wifi_architecture.md`) was created **2026-04-26**, *one month AFTER* the KNOWN BROKEN string was committed. The doctrine could not have been derived from that file.
- No claude-mem observation contemporaneous with 2026-03-21 records a STA hardware test or reproduction.
- The string has never been updated, re-validated, or stress-tested across 134 commits to `k1-rest-contract.yaml` since.
- This matches the pattern Captain flagged in #48568 — an "exaggerated OTT scare warning" written to deter agents, not a hardware-grounded technical statement.

---

## What I did NOT verify

- **Feb 2026 raw transcripts** — Crispy was OFF; raw session transcripts for the original STA mitigation attempts are not retrievable from this session. The "6+" count cannot be independently confirmed without those.
- **Current source state of WiFiManager.cpp** — line numbers cited in observations (#47158, #47160) are from the May-1 sessions; current source may differ. Out of scope for SSA-C (SSA-B handles current source).
- **Whether `esp32dev_audio_esv11_k1v2_32khz_sta_validation` builds and connects on hardware** — created 2026-05-03 (#48444); no flash record found in indexed memory. SSA-B / SSA-A territory.
- **Episodic-memory fallback** — not invoked. Project CLAUDE.md prioritises claude-mem; episodic-memory is documented as fallback only when claude-mem is insufficient. claude-mem was sufficient for the doctrine-trail aspect of this task.
- **iOS / tab5-encoder STA references** — out of scope. SSA-D handles doctrine drift in docs.
- **The actual ESP-IDF version's known bug list for AP+STA** — would require Context7 / vendor docs; not in scope for memory archaeology.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:SSA-Forensic-C | Created. claude-mem L1/L2/L3 + file-memory + git blame for KNOWN-BROKEN provenance. Crispy OFF. |
