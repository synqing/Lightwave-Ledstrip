# Operator Runbook — Legacy Oracle Trace Capture + Golden Trace Replay

## 0) Preconditions (do this once)

### 0.1 Naming conventions

* **Oracle run** = last-known-good legacy behaviour
* **Candidate run** = new hub/node refactor build you’re validating
* **Trace IDs** are short and descriptive:
  `T01_boot_connect`, `T02_brightness_sweep`, etc.

### 0.2 Where traces live

Pick ONE canonical location and never deviate:

* **On Hub (Tab5)**: `littlefs:/traces/`
* **On Node (K1)**: `littlefs:/traces/`

File naming format (MUST):

```
{run_type}_{trace_id}_{device}_{YYYYMMDD_HHMMSS}.jsonl
```

Examples:

* `oracle_T02_brightness_sweep_hub_20260114_091205.jsonl`
* `oracle_T02_brightness_sweep_node_20260114_091205.jsonl`
* `candidate_T02_brightness_sweep_node_202601422.jsonl`

### 0.3 Build flag conventions

Your firmware MUST support these compile-time flags (or equivalents):

* `TRACE_ENABLE=1` (turns tracing on)
* `TRACE_MODE=FULL|HASHED` (FULL during bringup)
* `TRACE_SINK=LITTLEFS|SERIAL` (LITTLEFS preferred)
* `TRACE_TRACE_ID="Txx_name"` (bakes ID into header)
* `TRACE_RUN_TYPE=oracle|candidate` (bakes run type into header)

If you don’t have flags yet, the first “Phase 0” task is implementing them.

---

## 1) Setup: creating a clean test environment

### 1.1 Flash clean builds

For a true oracle/candidate comparison, you want reproducibility:

* Flash hub + node firmware for your chosen run type.
* **Wipe persisted state** once before each trace run *unless the trace explicitly tests persistence*.

Recommended wipe actions:

* clear NVS (Preferences) keys relevant to zones/state
* delete `/traces/*` in LittleFS
* optionally delete persisted `zones_v1` blob for “fresh boot” traces

### 1.2 Verify topology

Before capturing anything, confirm:

* Node id to Hub (WS connected)
* Session token (if used) is valid
* Hub sees node in registry
* Node reports READY (or expected initial state)

If any of those fail, do NOT capture—fix connectivity first.

---

## 2) Capture workflow (repeatable for every trace)

### 2.1 Start capture (Hub + Node)

You need both perspectives for useful diffs.

**Step A — Hub**

* Ensure `TRACE_ENABLE=1`
* Trigger “start trace” (via UI button, CLI command, or special WS endpoint)
* Hub writes header event to trace file:

  * trace_version
  * fw_versions
  * alias_table_version
  * capture_mode
  * run_type
  * trace_id

**Step B — Node**

* Same: start trace and write header
* Confirm Node trace is active (log line or status indicator)

**Operator check:** Both devices must confirm:

* “TRACE STARTED”
* file path printed

### 2.2 Inject the stimulus (the actual test)

Run the stimulus precisely (no improvisation). Use one of these methods:

**Method 1 (Preferred): scripted stimulus**

* Hub has a “Test Runner” morministic message sequences.
* Example: brightness steps at fixed intervals.

**Method 2: manual stimulus**

* Use the physical encoder but follow a strict script:

  * step count
  * pause duration
  * direction
* Example: “turn clockwise 10 detents, pause 500ms, repeat until max.”

**Method 3: external driver**

* Send WS messages from a host script with exact payloads and timestamps.

**Rule:** Don’t mix methods inside one trace.

### 2.3 Stop capture (Hub + Node)

* Trigger “stop trace” on hub and node.
* Both should flush and close the file.
* Each device should print:

  * final event count
  * filename
  * bytes written

**Operator check:** Ensure file sizes are non-trivial (not 1–2KB). If tiny, capture likely failed.

---

## 3) Pull traces off devices

### 3.1 Transfer options

Pick whatever you already have:

* **HTTP endpoint** (recommended):
  `GET /traces` lists files; `GET /traces/<file>` downloads
* **Web UI “Download traces” button**
* **Serial dump** (fallback): stream file rial

### 3.2 Store in a canonical folder on host

Host folder structure:

```
traces/
  oracle/
    T01_boot_connect/
      hub.jsonl
      node.jsonl
  candidate/
    T01_boot_connect/
      hub.jsonl
      node.jsonl
```

You MUST keep oracle and candidate runs separate.

---

## 4) Run the comparator (golden trace replay)

### 4.1 Inputs required

For a given trace ID:

* oracle hub trace
* oracle node trace
* candidate hub trace
* candidate node trace
* alias table version used (should be recorded in header)
* comparator mode: TOLERANT or STRICT

### 4.2 Comparator run (operator command pattern)

Even if you haven’t written the comparator yet, standardize the interface now:

```
compare_traces \
  --trace_id T02_brightness_sweep \
  --oracle traces/oracle/T02_brightness_sweep \
  --candidate traces/candidate/T02_brightness_sweep \
  --mode TOLERANT \
  --out reports/T02_brightness_sweep_report.json
```

### 4.3 Expected outputs

Comparator MUST produce:

* `PASS/FAIL`
* counts: events compared, mismahes
* first divergence location:

  * side/stage/msg_type/seq/ts
* a minimal repro slice:

  * `repro_T02_brightness_sweep.jsonl` (±N events around divergence)
* a human-readable summary:

  * `report.md` with top-line verdict + divergence explanation

---

## 5) What counts as PASS

### 5.1 TOLERANT mode (migration)

PASS if:

* canonical msg_type sequences match for behaviour-critical messages
* node `apply` snapshots match on behaviour-critical fields
* renderer snapshots match (state proxy) within defined tolerances
* unknown fields and metadata differences do not affect apply/render state

### 5.2 STRICT mode (exit criteria)

PASS only if:

* payload hashes match (or full payload exact match)
* apply snapshots exact match
* render hash exact match (if implemented)

STRICT is required before you delete legacy aliases / dual discriminator support.

---

## 6) Troubleshooting (fast diagnosis)

### 6.1 “Connected but nothing happens”

Likely causes:

* schema mismatch (`t` vs `type`)
* command not ali(`effects.setCurrent` not mapped)
* ZoneComposer not initialized

Check:

* node recv.ws trace shows messages arriving
* node apply trace shows state changes (or not)
* unknown_type/error events

### 6.2 “Trace files empty/tiny”

* TRACE not enabled or sink not configured
* LittleFS full or not mounted
* start/stop triggers not called

Fix:

* verify mount
* print file path on start
* add “trace active” flag in status UI

### 6.3 “Comparator alignment is chaotic”

* seq not shared between hub/node
* no correlation IDs
* timestamps not comparable

Fix:

* include `hub_seq` in messages
* include `reqId`/`seq` in payload
* align using `(msg_type + nodeId + hub_seq)` as primary key

---

## 7) The canonical trace suite (minimum)

You must capture these as both oracle and candidate:

* T01_boot_connect
* T02_brightness_sweep
* T03_speed_sweep
* T04_zones_update
* T05_disconnect_reconnect
* T06_unknown_message_injection

---

## 8) Operator checklist (print this)

Before capture:

* [ ] Correct firmwaoracle/candidate)
* [ ] State wiped (unless persistence test)
* [ ] Node connected + READY
* [ ] Trace started on hub + node

During stimulus:

* [ ] Follow script exactly
* [ ] No extra knob twiddling

After capture:

* [ ] Trace stopped on hub + node
* [ ] Files downloaded and placed in correct folders
* [ ] Comparator run executed
* [ ] PASS/FAIL recorded + report saved
