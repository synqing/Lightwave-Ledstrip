---
abstract: Forensic doctrine-drift archaeology — when did 'AP must work' transform into 'AP-only-EVER' across CLAUDE.md, AGENTS.md, MEMORY.md, BACKLOG.md, k1-rest-contract.yaml, et al. Captain-directed evidence for strategic round-table. Reports language-evolution timeline with verbatim quotes, drift inflection commits, and cross-file consistency. Verdict: DOCTRINE-DRIVEN single-commit jump from absence of constraint to maximal strength-D prohibition with no intermediate strength-A/B/C tiers, all on 2026-03-04 in commit 39d6dcee.
---

# SSA-Forensic-D — WiFi-Mode Doctrine-Drift Archaeology

**Scope:** Documentation drift only. Source code (Forensic-B), git code history (Forensic-A), and memory archaeology (Forensic-C) are out-of-scope and not duplicated here.

**Method:** Per-file `git log --follow` enumeration, `git show <hash>:<file> | grep` on each material revision, `git log -G/-S` pickaxe searches for STA/AP-only string introductions across all paths.

---

## 1. Doctrine file inventory (status of each file traced)

| File | First commit | First WiFi-mode wording | Total WiFi-doctrine-relevant revisions | Current state |
|---|---|---|---|---|
| `CLAUDE.md` (root) | 2025-12-12 (`48600216`) | 2026-03-04 (`39d6dcee`) | 4 (39d6dcee → 9bd56d07 → 80ba6ae8 → unchanged since) | Strength-D, prominent in Hard Constraints |
| `AGENTS.md` | 2025-12-24 (`cde3fbb7`) | NEVER | 0 | NO WiFi-mode language at any revision |
| `BACKLOG.md` | 2026-02-27 (`1cf1a4b0`) | NEVER | 0 (only oblique "WiFiManager dual-branch" at line 109 of current HEAD) | NO WiFi-mode doctrine — naming-audit reference only |
| `docs/WORKFLOW_ROUTING.md` | 2026-03-05 (`14cb2113`) | NEVER (current HEAD has no WiFi/STA mention) | 0 | NO WiFi-mode language |
| `docs/TOOLCHAIN_IMPLEMENTATION_GUIDE.md` | 2026-03-05 (`14cb2113`) | 2026-03-05 (`14cb2113`) | 1 | Strength-C/D ("K1 is AP-ONLY. Never enable STA mode on ESP32 firmware.") |
| `firmware-v3/CLAUDE.md` | 2026-01-09 (`94479036`) | NEVER | 0 | NO WiFi-mode language |
| `firmware-v3/docs/CLAUDE.md` | 2026-01-30 (`9df6ab6c`) | NEVER | 0 | Tiny stub, NO WiFi-mode language |
| `lightwave-ios-v2/docs/CLAUDE.md` | (recent) | NEVER | 0 | NO WiFi-mode language |
| `tab5-encoder/docs/PRODUCT_DECISION_PRINCIPLES.md` | 2026-03-22 (`6652640b`) | 2026-03-22 (`6652640b`) | 1 (born with strength-D narrative + AP-only as identity) | Strength-D with full retrospective rationalisation |
| `~/.claude/.../firmware_wifi_architecture.md` | 2026-04-26 (claude-mem cleanup) | 2026-04-26 | 1 (extracted from inline MEMORY.md content) | Strength-D, "DUPLICATE of CLAUDE.md hard constraint" |
| `~/.claude/.../feedback_never_change_network_architecture.md` | 2026-04-18 (origin session 24b94d09) | 2026-04-18 | 1 | Strength-D operational rule, cites CLAUDE.md |
| `docs/protocol/k1-rest-contract.yaml` | 2026-03-21 (`5d94dab0`) | 2026-03-21 (`5d94dab0`) | 1 | Strength-D ("KNOWN BROKEN on K1 — do NOT use") born at field creation |
| `docs/protocol/k1-ws-contract.yaml` | 2026-03-21 (`5d94dab0`) | NEVER | 0 | NO WiFi-mode commands |

**Observation:** Only 5 files (root `CLAUDE.md`, `TOOLCHAIN_IMPLEMENTATION_GUIDE.md`, `PRODUCT_DECISION_PRINCIPLES.md`, `k1-rest-contract.yaml`, and the two MEMORY.md descendants) carry the doctrine. AGENTS.md, BACKLOG.md, the WORKFLOW_ROUTING.md, all sub-CLAUDE.md files, and the WS contract are silent. The doctrine is concentrated, not pervasive.

---

## 2. Language-evolution timeline (verbatim quotes, chronological)

### Phase 0 — pre-doctrine (2025-12 → 2026-03-03): silence + STA actually worked

| Date | File | Hash | Quote | Tier |
|---|---|---|---|---|
| 2026-01-04 | (firmware code, not docs — included for triangulation) | `937c9abc` | Commit subject: "feat(network): add AP-only mode and STA enable/disable API". Body: "FORCE_AP_MODE build flag (default: true, AP-only) ... requestSTAEnable() - temporarily enable STA for OTA ... STA mode required for OTA (auto-enabled via API) ... No AP+STA dual-mode (historically unreliable)". | Tier A + Tier B mixed: AP-must-work + STA-as-OTA-mode + AP-first-preference, NO prohibition |
| 2026-02-05 | (firmware code) | `0b270a48` | "net: stabilise AP+STA and esv11 heap. Skip forced STA reconnect when WiFi runs in AP+STA mode (keeps AP clients stable)." | Tier A: AP+STA actively stabilised |
| 2026-02-27 | `CLAUDE.md` | `5ee8aa84` | Hard Constraints section listed centre-origin, no rainbows, no heap, 120 FPS, British English. **WiFi: ABSENT.** | NO doctrine |
| 2026-02-28 | `CLAUDE.md` | `2dd61ce5` | Hard Constraints unchanged. WiFi: ABSENT. | NO doctrine |
| 2026-03-02 | `CLAUDE.md` | `6a040b36` | Hard Constraints unchanged. WiFi: ABSENT. | NO doctrine |

### Phase 1 — doctrine slammed in (2026-03-04, single commit, strength-D)

| Date | File | Hash | Quote | Tier |
|---|---|---|---|---|
| **2026-03-04** | **`CLAUDE.md`** | **`39d6dcee`** (commit subject: "docs: add effect development standard, stimulus contract, CI check" — author SpectraSynq) | **"K1 is AP-ONLY. NEVER enable STA mode. The K1 device runs as a WiFi Access Point. Tab5 and iOS connect TO it. Do NOT attempt to connect K1 to external WiFi routers — STA authentication fails at the 802.11 driver level (AUTH_EXPIRE reason 2, AUTH_FAIL reason 202) and has NEVER been resolved despite 6+ mitigation attempts. This was architecturally resolved in Feb 2026. See `WiFiManager.h` for details. Do not modify WiFi mode, add STA connection logic, or change AP configuration without explicit user approval."** | **Tier D — born fully formed** |
| 2026-03-05 | `docs/TOOLCHAIN_IMPLEMENTATION_GUIDE.md` | `14cb2113` | "K1 is AP-ONLY. Never enable STA mode on ESP32 firmware." | Tier C/D (compact echo) |
| 2026-03-05 | `firmware-v3` deferred runtime hardening doc | `5ab1c627` | "K1 is AP-ONLY. No STA mode changes." | Tier C |
| 2026-03-05 | `CLAUDE.md` (no change to WiFi line) | `14cb2113` | (carries `39d6dcee` wording forward) | Tier D unchanged |

### Phase 2 — propagation and rationalisation (2026-03-21 → 2026-04-26)

| Date | File | Hash | Quote | Tier |
|---|---|---|---|---|
| 2026-03-21 | `docs/protocol/k1-rest-contract.yaml` | `5d94dab0` | `/api/v1/network/sta/enable: ... description: "Enable STA mode (KNOWN BROKEN on K1 — do NOT use)"`; `/api/v1/network/connect: ... description: "Connect to a network (KNOWN BROKEN on K1)"` | Tier D — "KNOWN BROKEN" annotation born WITH the field, not added later |
| 2026-03-22 | `tab5-encoder/docs/PRODUCT_DECISION_PRINCIPLES.md` | `6652640b` | "K1 WiFi STA mode -- 6+ failed attempts over multiple weeks. The K1 team attempted to enable STA mode (connecting K1 to external WiFi routers) six times. Each attempt failed with AUTH_EXPIRE or AUTH_FAIL at the ESP-IDF 802.11 driver level. The constraint was real ... The correct decision was to accept the constraint early and design an AP-only architecture. Instead, six mitigation attempts ... were tried and failed." Plus: "AP-only networking means: design for local-first, zero-internet-required operation. This is actually a feature for musicians performing at venues with unreliable WiFi." Plus: "K1 is AP-only kills STA mode." | Tier D — full retrospective narrative; doctrine elevated to product-identity ("AP-only IS K1's identity") |
| 2026-03-25 | `b806b95e` (changelog/handoff) | `b806b95e` | "K1 is AP-ONLY — never enable STA mode" | Tier D (compact restatement) |
| 2026-04-18 | `~/.claude/.../feedback_never_change_network_architecture.md` | (memory file, not in git) | "During debugging, NEVER propose switching tab5 to the `tab5_sta` build env, or any other change that alters the wireless architecture between K1 and tab5. Do NOT wrap such a proposal in copy-paste command sequences ... K1-AP-ONLY is a CLAUDE.md hard constraint." | Tier D — escalates to "do not even propose alternatives" |
| 2026-04-26 | `~/.claude/.../firmware_wifi_architecture.md` | (memory file, not in git) | "K1 is AP-ONLY. NEVER add STA mode. K1 boots in `WIFI_MODE_AP` (not `WIFI_MODE_APSTA`) ... STA auth fails at the ESP-IDF 802.11 driver level with AUTH_EXPIRE (reason 2), AUTH_FAIL (reason 202). Reproduced across multiple routers (`VX220-013F`, `OPTUS_738CC0N`) and 6+ firmware-level mitigation attempts: Compatibility profiles, BSSID scrubs, AP-stop-before-join, PMF optional, (and more). NONE worked." | Tier D — claims "DUPLICATE of CLAUDE.md hard constraint" |
| 2026-04-26 | `CLAUDE.md` | `80ba6ae8` (compress 43.8k → 34.7k) | "K1 is AP-ONLY. NEVER enable STA mode. K1 runs as a WiFi Access Point. Tab5 and iOS connect TO it. STA has never worked (driver-level auth failures, 6+ failed mitigations). See MEMORY.md `firmware_wifi_architecture.md` for full history. Do not modify WiFi mode, add STA connection logic, or change AP configuration without explicit user approval." | Tier D — softened cosmetically (shorter), strength unchanged. **STA has never worked** — note the absolute past-perfect formulation: was "has NEVER been resolved despite 6+ mitigation attempts" in 39d6dcee, became "STA has never worked" in 80ba6ae8 — drift even within strength-D, towards more absolute formulation |

### Phase 3 — softening (2026-05-03, but ONLY in build config, not in docs)

| Date | File | Hash | Quote | Tier |
|---|---|---|---|---|
| 2026-05-03 | `firmware-v3/platformio.ini` (NOT a doctrine doc — included for context) | `11e040d6` | "K1v2 validation-only STA fallback for Portal REST/WS closeout. Production K1v2 remains AP-only via esp32dev_audio_esv11_k1v2_32khz. Use only after AP validation at 192.168.4.1 fails by the Captain-approved criteria. Credentials come from gitignored wifi_credentials.ini." | Tier C (preference-with-escape-hatch) — but only in build config; root `CLAUDE.md` and all other doctrine files STILL Tier D as of HEAD |

**Critical observation:** As of HEAD (commit 827ab713), the build system has a STA-validation env (`11e040d6`, 2026-05-03), yet root `CLAUDE.md` line 236 STILL reads "K1 is AP-ONLY. NEVER enable STA mode." The documentation has NOT caught up to the engineering. This is the live doctrine drift Captain described: the build system softened the rule 24 hours ago; the docs still assert maximal prohibition.

---

## 3. Drift inflection points

### A → B inflection
**Did not occur in documentation.** The strength-A/B state existed only in code commits (`937c9abc` 2026-01-04, `0b270a48` 2026-02-05). The root `CLAUDE.md` Hard Constraints section pre-39d6dcee listed only effects/render rules — WiFi was NEVER mentioned in strength-A or strength-B form in any doctrine doc.

### B → C inflection
**Did not occur in documentation.** No "K1 prefers AP" or "default to AP" wording exists in any traced doc revision. The doctrine never passed through a preference tier.

### C → D inflection (the only one that occurred)
- **Hash:** `39d6dcee`
- **Date:** 2026-03-04 00:06:38 +0800
- **Author:** SpectraSynq <spectrasynq@example.com>
- **Commit subject:** "docs: add effect development standard, stimulus contract, CI check"
- **Diff to root `CLAUDE.md`:** **+2 lines, -0 lines.** A single bullet inserted in the Hard Constraints list. The full strength-D wording — including the specific failure codes (AUTH_EXPIRE reason 2, AUTH_FAIL reason 202), the count ("6+ mitigation attempts"), the date claim ("architecturally resolved in Feb 2026"), and the prohibition ("Do not modify WiFi mode ... without explicit user approval") — appeared **all at once, in a commit nominally about effect development**.

**There was no A → B nor B → C step in the documentation.** The doctrine jumped directly from total absence (Phase 0) to maximal strength-D (Phase 1) in a single commit, on the same day as a bare "AP auth fix" commit `86abcb12` (which simplified `softAP()` for client compatibility, not STA).

---

## 4. Cross-file consistency analysis

| Metric | Value |
|---|---|
| Window from first doctrine appearance to last propagation | 2026-03-04 → 2026-04-26 (54 days) |
| Files updated within 24 hours of `39d6dcee` | 1 (root `CLAUDE.md`); 1 supporting commit `5ab1c627` 2026-03-05 (deferred runtime hardening doc) |
| Files updated 1–7 days later | `docs/TOOLCHAIN_IMPLEMENTATION_GUIDE.md` (`14cb2113`, +1 day) |
| Files updated 17 days later | `docs/protocol/k1-rest-contract.yaml` (`5d94dab0`, 2026-03-21) — born with the doctrine annotation, not retrofitted |
| Files updated 18 days later | `tab5-encoder/docs/PRODUCT_DECISION_PRINCIPLES.md` (`6652640b`, 2026-03-22) — born with full retrospective narrative |
| Files updated 45+ days later | Memory files (2026-04-18, 2026-04-26) |

**Pattern:** **Staggered, not synchronised.** The doctrine originated in `39d6dcee` and propagated organically over 54 days as agents/Captain wrote new docs. New documents (k1-rest-contract.yaml, PRODUCT_DECISION_PRINCIPLES.md, MEMORY.md descendants) were born already carrying the doctrine — they did not transition through earlier strength tiers in their own histories. This indicates the strength-D wording crystallised once in `39d6dcee` and was then copy-evolved into derivative docs. The doctrine was carried, not re-derived.

**No central doctrine-update commit exists.** There is no commit that simultaneously updated CLAUDE.md, BACKLOG.md, AGENTS.md, and the contract YAML to align them. AGENTS.md and BACKLOG.md were never updated — they remain doctrine-silent at HEAD. This means the "K1 is AP-only" worldview lives in 5 places in the repo and 2 memory files; an agent reading only AGENTS.md or only BACKLOG.md would not encounter it.

---

## 5. Current canonical wording (verbatim quotes of HEAD state)

### `CLAUDE.md` (root) — line 117 and line 236
> Line 117 (Constraint readback table): `| Network / WiFi | K1 is AP-ONLY — NEVER enable STA mode |`
> Line 227 (Pre-Commit Confidence Gate): `WiFi? Confirm AP-only preserved.`
> Line 236 (Hard Constraints): `**K1 is AP-ONLY. NEVER enable STA mode.** K1 runs as a WiFi Access Point. Tab5 and iOS connect TO it. STA has never worked (driver-level auth failures, 6+ failed mitigations). See MEMORY.md \`firmware_wifi_architecture.md\` for full history. **Do not modify WiFi mode, add STA connection logic, or change AP configuration without explicit user approval.**`
> Line 538 (superthink trigger conditions): `Network protocol changes (WebSocket commands, REST endpoints, k1-ws-contract.yaml, k1-rest-contract.yaml, WiFi mode)`

### `docs/TOOLCHAIN_IMPLEMENTATION_GUIDE.md` — line 45, 560, 818
> Line 45: `**K1 is AP-ONLY.** Never enable STA mode on ESP32 firmware.`
> Line 560: `firmware_agent.remember("K1 is AP-ONLY. Never enable STA mode.", critical=True)`
> Line 818: `Any STA mode WiFi configuration on K1` (in a "what to refuse" list)

### `tab5-encoder/docs/PRODUCT_DECISION_PRINCIPLES.md` — lines 305, 314–316, 325, 330, 413
> Line 305: `**Technical constraints:** "We have 6 months" kills certain architectures. "ESP32-S3 has 2MB PSRAM" kills certain features. "K1 is AP-only" kills STA mode.`
> Line 314: `**K1 WiFi STA mode -- 6+ failed attempts over multiple weeks.** The K1 team attempted to enable STA mode (connecting K1 to external WiFi routers) six times. Each attempt failed with AUTH_EXPIRE or AUTH_FAIL at the ESP-IDF 802.11 driver level. The constraint was real: the ESP32 WiFi stack shares encryption key pools between AP and STA, and AP operation corrupts STA auth state. The correct decision was to accept the constraint early and design an AP-only architecture. Instead, six mitigation attempts (compatibility profiles, BSSID scrubs, AP-stop-before-join, PMF optional) were tried and failed. Each attempt cost engineering time that could have been spent on features. The constraint was right. Fighting it was wrong.`
> Line 316: `This is the product equivalent of Principle 11: "K1 is AP-only" is not a limitation to work around. It is a filter that eliminates an entire class of architectural options and frees the team to optimise within the viable space.`
> Line 325: `**AP-only networking** means: design for local-first, zero-internet-required operation. This is actually a feature for musicians performing at venues with unreliable WiFi.`
> Line 330: `Each constraint eliminates a class of options and defines K1's identity. A product that tried to be all things (STA + AP, linear + centre-origin, high-compute effects + fast rendering) would be nothing.`
> Line 413: `**Principle 11: Let constraints kill options.** (Rank: 3rd) K1's constraints ARE its identity. AP-only networking means local-first reliability.`

### `docs/protocol/k1-rest-contract.yaml` — line 683, 699
> Line 683: `description: "Enable STA mode (KNOWN BROKEN on K1 — do NOT use)"`
> Line 699: `description: "Connect to a network (KNOWN BROKEN on K1)"`

### `~/.claude/.../firmware_wifi_architecture.md` (memory)
> "**K1 is AP-ONLY. NEVER add STA mode.** K1 boots in `WIFI_MODE_AP` (not `WIFI_MODE_APSTA`) ... If an agent attempts to add STA connectivity, WiFi mode changes, or router connection logic to K1 firmware, it is WRONG. Stop and check `CLAUDE.md`."

### `~/.claude/.../feedback_never_change_network_architecture.md` (memory)
> "Rule: During debugging, NEVER propose switching tab5 to the `tab5_sta` build env, or any other change that alters the wireless architecture between K1 and tab5. Do NOT wrap such a proposal in copy-paste command sequences — that makes an unauthorised architectural pivot look routine."

### `AGENTS.md`, `BACKLOG.md`, `docs/WORKFLOW_ROUTING.md`, `firmware-v3/CLAUDE.md`, `firmware-v3/docs/CLAUDE.md`, `lightwave-ios-v2/docs/CLAUDE.md`, `docs/protocol/k1-ws-contract.yaml`
> **No WiFi-mode language whatsoever.** Doctrine-silent.

---

## 6. apMode YAML field provenance

**Field name:** `/api/v1/network/sta/enable` (not literally `apMode` — Captain's reference was approximate). Companion field `/api/v1/network/connect` is similarly annotated.

| Question | Answer |
|---|---|
| Commit that ADDED the `/api/v1/network/sta/enable` field | `5d94dab0` (2026-03-21) — first commit of the entire k1-rest-contract.yaml file |
| Commit that ADDED the "KNOWN BROKEN" annotation | `5d94dab0` — **same commit, born with the annotation in place** |
| Was the annotation hardware-validated by a referenced test or commit? | **No.** Commit message: "docs: add K1↔Tab5 protocol contracts and LVGL component reference". The contract was assembled as documentation; the "KNOWN BROKEN" string is a doctrine-assertion drawn from the `39d6dcee` doctrine landed 17 days earlier, not from a fresh hardware test cited in the commit |
| Has the annotation been edited since? | **No.** A pickaxe search (`git log -S 'KNOWN BROKEN' -- docs/protocol/k1-rest-contract.yaml`) returns only `5d94dab0`. The string is unchanged in the 7 subsequent revisions of the file (`6f06bd7c`, `8914eea6`, `5e37daa0`, `eca30206`, `d53092ad`, `71d1fd1d`, etc.). |
| Does the annotation match firmware reality? | **No.** As of 2026-05-03, build env `esp32dev_audio_esv11_k1v2_32khz_sta_validation` (commit `11e040d6`) un-defines `WIFI_AP_ONLY` and enables STA — explicitly for "REST/WS validation only after the approved AP failure criteria are met". The contract YAML still says "KNOWN BROKEN — do NOT use." |

**Provenance verdict for apMode/STA contract field:** **Doctrine-asserted at birth, never hardware-revalidated, contradicted by HEAD build configuration as of 24 hours before this excavation.**

---

## 7. Verdict — was the drift evidence-driven or doctrine-driven?

**DOCTRINE-DRIVEN.** The case is unambiguous on the documentation side.

Three independent indicators:

1. **Single-commit jump from absence to strength-D.** The transition was not gradual, not evidence-paced, not tied to an incident postmortem committed alongside it. Commit `39d6dcee` (2026-03-04) added the entire strength-D bullet — including specific failure codes, the count "6+ mitigation attempts", and an absolute prohibition — in a 2-line diff inside a commit nominally about effect development. There was no companion "incident report" or "STA-failure investigation log" commit on or near 2026-03-04.

2. **Strength-A and strength-B existed in code but never in docs.** As of 2026-01-04 (`937c9abc`), STA was a working OTA-mode escape hatch with the comment "No AP+STA dual-mode (historically unreliable)" — i.e. strength-A acceptance with a strength-B preference. As of 2026-02-05 (`0b270a48`), the team was actively stabilising AP+STA, not abandoning it. The documentation skipped these intermediate states and crystallised at strength-D 27 days later.

3. **Propagation was carrier-style, not evidence-style.** Each later doc that mentions the doctrine (TOOLCHAIN_IMPLEMENTATION_GUIDE.md, PRODUCT_DECISION_PRINCIPLES.md, k1-rest-contract.yaml, the two memory files) restates the doctrine without citing fresh evidence. The PRODUCT_DECISION_PRINCIPLES.md narrative (2026-03-22) elevates AP-only to a product-identity virtue ("AP-only ... is actually a feature for musicians performing at venues with unreliable WiFi") — that is rationalisation language, not evidence language. The string "6+ mitigation attempts" appears in 4 docs verbatim; none of them cite a commit, log, or test artifact for the count of 6.

**This matches Captain's hypothesis exactly:** an original requirement that AP must work (which it does, and is the production path) was, in retelling on 2026-03-04 in a single doc edit, transformed into a stronger restriction "AP-only-EVER, never STA." The stronger formulation then propagated organically into 4 derivative docs and 2 memory files over 54 days.

**Mixed-verdict caveat:** The build-system softening of 2026-05-03 (`11e040d6`, STA validation env) shows the engineering reality has begun to diverge from the doctrine reality. The drift is now bidirectional: docs still say "NEVER", build config says "validation-only STA fallback exists". This is the live drift Captain wants the round-table to address.

---

## 8. What I did NOT verify (scope limits)

1. **Source code state.** I did not audit `WiFiManager.cpp`/`WiFiManager.h` or related source for current STA-related logic, the actual `m_forceApOnly` semantics, or whether `WIFI_AP_ONLY` build flag enforcement matches the documented prohibition. That belongs to Forensic-B. (I did surface incidental greps showing the source still has a serial `wifi connect` escape hatch — the documentation never mentions this.)
2. **Git code-history beyond commit messages.** I read commit subjects/bodies but did not diff the actual `WiFiManager` source across the inflection window. The "6+ mitigation attempts" count is repeated in docs but I did not attempt to enumerate 6 specific commits or branches that match the count. That belongs to Forensic-A.
3. **Memory archaeology.** I read the two memory files surfaced in the task brief; I did not search claude-mem observations for sessions where the AP-only doctrine was discussed, debated, or reinforced. That belongs to Forensic-C.
4. **Hardware test evidence.** I did not verify whether AUTH_EXPIRE reason 2 / AUTH_FAIL reason 202 were actually observed on K1 hardware, or whether the count "6+ mitigation attempts" matches the historical record. The doctrine claims this; I traced the claim's appearance in docs but cannot adjudicate its accuracy.
5. **The K1 v1 / K1 v2 split.** I did not separately trace whether the doctrine ever differentiated between K1 v1 and K1 v2 hardware revisions. The 2026-05-03 STA-validation env (`11e040d6`) is K1v2-specific; this may be material to the round-table.
6. **Tab5 and iOS doctrine state.** I checked that `lightwave-ios-v2/docs/CLAUDE.md` and `firmware-v3/docs/CLAUDE.md` do not carry the doctrine, but I did not exhaustively grep the iOS or Tab5 trees for STA/AP-only mentions in non-CLAUDE files.
7. **Decision-author identity for the inflection commit.** The author of `39d6dcee` is `SpectraSynq <spectrasynq@example.com>` (Captain). I did not investigate whether the doctrine wording was drafted by an agent and committed by Captain, or written by Captain directly. The git record shows Captain as committer.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:SSA-Forensic-D (Claude Opus 4.7) | Created. Forensic doctrine-drift archaeology for K1 WiFi-mode story. Traced 13 candidate doctrine files via `git log --follow` + `git show <hash>:<file>`. Built per-commit verbatim-quote timeline showing strength-D doctrine landed in single commit `39d6dcee` (2026-03-04, 2-line diff inside an effects-docs commit) with NO prior strength-A/B/C tier in any doc. Cross-file analysis: staggered carrier-style propagation over 54 days into 4 derivative docs + 2 memory files. AGENTS.md, BACKLOG.md, WORKFLOW_ROUTING.md, sub-CLAUDE.md files, and k1-ws-contract.yaml are doctrine-silent at HEAD. apMode YAML field annotation born with "KNOWN BROKEN" string in `5d94dab0` (2026-03-21), never hardware-revalidated, now contradicted by build-config STA-validation env `11e040d6` (2026-05-03) that the docs have not caught up to. Verdict: DOCTRINE-DRIVEN with a Mixed caveat for the 24-hour-old build softening. |
