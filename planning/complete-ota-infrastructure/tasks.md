# Task List: Complete OTA Infrastructure for LightwaveOS

## Quick Reference (Extracted from PRD & Blueprint)

### Ownership Rules
*Source: PRD §7 + Blueprint §2.1*

| Artifact | Created By | App's Role | DO NOT |
|----------|------------|------------|--------|
| OTA server firmware binaries | **Developer** (manual upload via web UI) | **Observe** (Hub downloads) | ❌ Hub must NOT create firmware binaries |
| OTA server manifest JSON | **OTA Server** (auto-generated on upload) | **Observe** (Hub polls and parses) | ❌ Hub must NOT modify OTA server manifest |
| SHA256 checksums | **OTA Server** (auto-calculated) | **Observe** (Hub uses for verification) | ❌ Hub must NOT calculate SHA256 for server uploads |
| Hub LittleFS cached Node firmware | **Hub** (downloads from OTA server) | **Create** (Hub downloads and caches) | ✅ Hub creates local cache after verifying |
| Hub OTA partition (app1) | **Hub** (writes downloaded firmware) | **Create** (Hub writes to inactive partition) | ✅ Hub uses Update.h to write firmware |
| Node OTA partition | **Node** (writes downloaded firmware) | **Observe** (Hub monitors Node state) | ❌ Hub must NOT directly write to Node partitions |
| Dashboard event log entries | **Hub** (HubRegistry callback triggers) | **Create** (Hub generates log entries) | ✅ Hub creates log entries on OTA events |
| Node registry entries | **Hub** (on HELLO message) | **Create** (Hub registers nodes) | ✅ Hub creates node_entry_t on WebSocket HELLO |

### State Variables
*Source: PRD §8 + Blueprint §3*

| State Variable | Initial Value | Created When | Cleared When | Persists Across |
|----------------|---------------|--------------|--------------|------------------|
| `HubOtaDispatch::state_` | `OTA_DISPATCH_IDLE` | `startRollout()` → `IN_PROGRESS` | `abort()` or `processNextNode()` completes → `IDLE` | RAM only (lost on Hub reboot) |
| `HubOtaDispatch::nodeIds_` | Empty vector `[]` | `startRollout(nodeIds)` populates | `abort()` → clear | RAM only |
| `HubOtaDispatch::currentNodeId_` | `0` | `processNextNode()` assigns canary | `abort()` → `0` | RAM only |
| `HubOtaDispatch::completedCount_` | `0` | `startRollout()` initializes | `abort()` → `0` | RAM only |
| `node_entry_t::ota_state` | `"idle"` | Hub sends `ota_update` → Node reports "downloading" | Node reports "complete" or "error" → "idle" | RAM only |
| `node_entry_t::ota_pct` | `0` | Node reports download progress | Node reports "complete" or "error" → `0` | RAM only |
| `node_entry_t::ota_version` | `""` | Node reports OTA start with version | Node reconnects with new fw → cleared | RAM only |
| `node_entry_t::ota_error` | `""` | Node reports "error" state | Next OTA attempt → cleared | RAM only |
| Dashboard log entries | Empty (all zeroed) | Registry event callback → `addLogEntry()` | Never explicitly cleared (ring buffer overwrites oldest) | RAM only |
| Hub LittleFS cached firmware | Absent | Hub downloads from OTA server | Manual deletion or filesystem format | Flash (persists across reboots) |
| Hub LittleFS manifest | Initially manual upload | Hub updates after Node firmware cache | Manual deletion or filesystem format | Flash (persists across reboots) |
| OTA server firmware binaries | Absent | Developer uploads via web UI | Never (keep all versions forever) | Disk (persists indefinitely) |
| OTA server manifest JSON | Empty array or initial entry | First firmware upload | Never (keep all versions forever) | Disk (persists indefinitely) |

### Critical Boundaries
*Source: Blueprint §2.3*

- ❌ **DO NOT**: Hub must NOT create firmware binaries - Developer/PlatformIO creates them; Hub only downloads
- ❌ **DO NOT**: Hub must NOT modify OTA server manifest - OTA server owns manifest; Hub only reads
- ❌ **DO NOT**: Hub must NOT calculate SHA256 for OTA server uploads - OTA server calculates; Hub only verifies
- ❌ **DO NOT**: OTA server must NOT access Hub's LittleFS - Hub owns local filesystem
- ❌ **DO NOT**: Hub must NOT assume Node update succeeded without READY state verification
- ✅ **MUST**: Hub MUST wait for Node to report OTA status via WebSocket - Node owns its update process
- ✅ **MUST**: Hub MUST verify Node reached READY state after canary update - Node may fail to boot
- ✅ **MUST**: Hub MUST handle OTA server unavailability gracefully - Server may be down; continue LED operation
- ✅ **MUST**: Hub MUST trust ESP32 OTA library for rollback - ESP32 handles boot failure detection
- ✅ **MUST**: Hub MUST abort rollout if canary doesn't reconnect within 30s - Boot failure undetectable except via absence of HELLO

### User Visibility Rules
*Source: PRD §6*

| User Action | User Sees | User Does NOT See | Timing |
|-------------|-----------|-------------------|--------|
| Click "Check for Updates" | "Checking for updates..." loading spinner | HTTP GET request, JSON parsing, manifest comparison | Immediate (loading indicator) |
| Updates available | Event log: "Updates available: Hub 1.2.0, Node 0.9.5" (yellow) | Firmware sizes, SHA256 checksums, version comparison logic | After 1-2s (network latency) |
| No updates | Event log: "No updates available" (green) | Manifest parsing, version string comparison | After 1-2s |
| Update check fails | Event log: "Update check failed: Connection timeout" (red) | HTTP error codes, stack traces, retry logic | After 5-10s (timeout) |
| Click "START OTA" | Event log: "Canary OTA started: Node 2" (yellow); Node 2 highlighted | Canary selection algorithm, nodeIds_ vector, state machine | Immediate |
| Canary downloading | Progress bar: "Downloading to Node 2: 45%" (yellow) | 1KB chunk downloads, HTTP buffer, mbedtls SHA256 context | Real-time (every 32KB) |
| Canary verifying | "Verifying on Node 2..." (yellow) | SHA256 calculation, checksum comparison | 1-2s |
| Canary installing | "Installing on Node 2..." (yellow) | Flash write operations, partition switching | 2-5s |
| Canary verification wait | "Testing on Node 2 - waiting 30s" countdown (29, 28, 27...) (yellow) | Keepalive tracking, node state checks, timeout tracking | 30s countdown (updates every 1s) |
| Canary success | Event log: "Canary succeeded - updating remaining nodes" (green) | NODE_STATE_READY check, canary_phase_ flag clear | After 30s countdown completes |
| Canary failure | Event log: "Canary failed - rollout aborted" (red); Node 2 LOST/DEGRADED | HubOtaDispatch::abort(), nodeIds_.clear(), state machine reset | After 30s countdown (if not READY) |
| Hub self-update download | Progress bar: "Downloading Hub firmware: 45%" (yellow) | HTTPClient buffer, LittleFS write operations | Real-time (every 32KB) |
| Hub self-update verify | "Verifying Hub firmware..." (yellow) | mbedtls SHA256 calculation | 2-3s |
| Hub self-update install | "Installing Hub firmware..." (yellow) | ESP32 Update.h partition writes | 5-10s |
| Hub reboot countdown | "Hub will reboot in 5 seconds" countdown (5, 4, 3, 2, 1) (yellow) | Filesystem sync, pending operations | 5s countdown |
| Hub reboot | Screen goes blank, then reboots | ESP32 restart, bootloader, partition verification | 10-15s |

---

## Requirements Traceability

**Every requirement MUST map to at least one task. Nothing should be lost.**

| Source | Requirement | Mapped To Task |
|--------|-------------|----------------|
| PRD §5.1.1 | OTA server binds to 192.168.x.x:8080 | Task 1.3 |
| PRD §5.1.2 | OTA server exposes REST API | Task 1.4, 1.5, 1.6 |
| PRD §5.2.2 | Web UI accepts device type, version, .bin file | Task 1.8 |
| PRD §5.2.3 | Server auto-calculates SHA256 | Task 1.9 |
| PRD §5.2.6 | Manifest follows Hub's LittleFS format | Task 1.10 |
| PRD §5.2.7 | All versions retained indefinitely | Task 1.10 (no auto-deletion logic) |
| PRD §5.3.1 | Hub polls every 5 minutes | Task 2.4 |
| PRD §5.3.2 | Polling runs on Core 0 | Task 2.4 |
| PRD §5.3.3 | Log new Hub firmware available | Task 2.5 |
| PRD §5.3.4 | Auto-download Node firmware | Task 2.6 |
| PRD §5.3.5 | Download in 1KB chunks | Task 2.6 |
| PRD §5.3.6 | Verify with SHA256 | Task 2.6 |
| PRD §5.4.1 | "Check for Updates" button | Task 2.7 |
| PRD §5.4.3 | Show "Checking..." loading indicator | Task 2.7 (User Sees field) |
| PRD §5.5.1 | Hub downloads Hub firmware in 1KB chunks | Task 3.2 |
| PRD §5.5.2 | Hub verifies SHA256 before applying | Task 3.2 |
| PRD §5.5.3 | Hub uses ESP32 dual-partition OTA | Task 3.2 |
| PRD §5.5.4 | Dashboard shows "Downloading Hub firmware: 45%" | Task 3.2 (User Sees field) |
| PRD §5.5.7 | Dashboard shows "Hub will reboot in 5 seconds" | Task 3.3 |
| PRD §5.5.10 | ESP32 rolls back if firmware fails to boot | Task 3.4 |
| PRD §5.6.1 | "START OTA" button in dashboard | Task 4.8 |
| PRD §5.6.2 | Button selects first READY node as canary | Task 4.3 |
| PRD §5.6.9 | Dashboard shows "Testing on Node 2 - waiting 30s" | Task 4.5 (User Sees field) |
| PRD §5.7.1 | Hub waits 30 seconds after Node reboot | Task 4.5 |
| PRD §5.7.3 | Hub checks Node state is READY | Task 4.5 |
| PRD §5.7.8 | If canary not READY, abort rollout | Task 4.6 |
| PRD §5.8.1 | Hub reboot during rollout loses RAM state | Task 4.7 |
| PRD §5.8.2 | After reboot, Hub does NOT resume rollout | Task 4.7 |
| PRD §6.1 V-1 | User sees "Checking for updates..." | Task 2.7 (User Sees) |
| PRD §6.1 V-6 | Progress bar: "Downloading to Node 2: 45%" | Task 4.4, 5.2 (User Sees) |
| PRD §6.1 V-9 | "Testing on Node 2 - waiting 30s" countdown | Task 4.5, 5.3 (User Sees) |
| PRD §6.1 V-14 | Progress bar: "Downloading Hub firmware: 45%" | Task 3.2, 5.2 (User Sees) |
| PRD §7.1 O-1 | OTA server firmware binaries created by Developer | Task 1.1 (Ownership) |
| PRD §7.1 O-2 | OTA server manifest created by OTA Server | Task 1.10 (Ownership) |
| PRD §7.1 O-4 | Hub LittleFS cached firmware created by Hub | Task 2.6 (Ownership) |
| PRD §8.1 SI-2 | Only one rollout at a time | Task 4.3 (State Change: check state == IDLE) |
| PRD §8.2 SL-1 | `HubOtaDispatch::state_` lifecycle | Task 4.3, 4.6, 4.7 (State Change) |
| PRD §8.2 SL-5 | `node_entry_t::ota_state` lifecycle | Task 4.4 (State Change) |
| Blueprint §2.1 | Ownership enforcement table | Quick Reference + relevant task Ownership fields |
| Blueprint §2.3 | Boundary rules (DO NOTs) | Quick Reference Critical Boundaries + IC tasks |
| Blueprint §3.1 | Transition: Start Canary Rollout | Task 4.3 Pre/Post conditions |
| Blueprint §3.2 | Transition: Canary Success | Task 4.5 Pre/Post conditions |
| Blueprint §3.3 | Transition: Canary Failure | Task 4.6 Pre/Post conditions |
| Blueprint §4.1 | Hub OTA Client → OTA Server Polling | IC-1 |
| Blueprint §4.2 | Hub OTA Client → Hub Self-Update | IC-2 |
| Blueprint §4.3 | Hub OTA Dispatch → Canary Rollout | IC-3 |

---

## Overview

This task list implements a complete Over-The-Air (OTA) update infrastructure for LightwaveOS, addressing the critical "who updates the Hub?" problem. The feature consists of an external Node.js OTA server that hosts firmware for both Hub (ESP32-P4) and Nodes (ESP32-S3), Hub OTA client that polls for updates and self-updates via ESP32 dual-partition OTA, canary rollout strategy that tests on 1 node before fleet-wide deployment, and full LVGL dashboard integration with progress bars and countdown timers.

**Key Outcomes:**
- Eliminate manual USB flashing for Hub updates (Hub self-updates remotely)
- Centralized version control and firmware distribution via OTA server
- Safe deployments with canary rollout (detect failures before fleet-wide deployment)
- Full observability via LVGL dashboard (detailed progress, countdown timers, color-coded status)

**Reference Documents:**
- PRD: `planning/complete-ota-infrastructure/prd.md`
- Technical Blueprint: `planning/complete-ota-infrastructure/technical_blueprint.md`

## Relevant Files

### OTA Server (New - to be created)
- `ota-server/server.js` – Main Express application
- `ota-server/routes/api.js` – REST API routes
- `ota-server/routes/upload.js` – Firmware upload route with multer
- `ota-server/views/upload.html` – Web UI for firmware upload
- `ota-server/storage/firmware/hub/manifest.json` – Hub firmware manifest
- `ota-server/storage/firmware/node/manifest.json` – Node firmware manifest
- `ota-server/package.json` – Dependencies (Express, Multer, etc.)
- `ota-server/README.md` – Setup and usage instructions

### Hub Firmware (New - to be created)
- `firmware/Tab5.encoder/src/hub/ota/hub_ota_client.h` – HubOtaClient class definition
- `firmware/Tab5.encoder/src/hub/ota/hub_ota_client.cpp` – Polling, downloading, caching, Hub self-update
- `firmware/Tab5.encoder/src/hub/ota/hub_ota_dispatch.h` – HubOtaDispatch class definition
- `firmware/Tab5.encoder/src/hub/ota/hub_ota_dispatch.cpp` – Canary rollout state machine

### Hub Firmware (Modified - existing files)
- `firmware/Tab5.encoder/src/hub/net/hub_registry.h` – Add `ota_state`, `ota_pct`, `ota_version`, `ota_error` to `node_entry_t`
- `firmware/Tab5.encoder/src/hub/net/hub_registry.cpp` – Add `setOtaState()` method
- `firmware/Tab5.encoder/src/hub/hub_main.h` – Add `HubOtaClient` and `HubOtaDispatch` instances
- `firmware/Tab5.encoder/src/hub/hub_main.cpp` – Initialize OTA client and dispatch, wire callbacks
- `firmware/Tab5.encoder/src/ui/HubDashboard.h` – Add OTA progress bars, countdown timer widgets
- `firmware/Tab5.encoder/src/ui/HubDashboard.cpp` – Implement OTA UI updates, button event handlers
- `firmware/Tab5.encoder/src/main.cpp` – Initialize OTA subsystems in `setup()`

### Integration Tests (New - to be created)
- `tools/test_ota_server.sh` – Bash script testing OTA server REST API with curl
- `tools/test_ws_protocol.js` – Node.js script testing WebSocket protocol
- `tools/test_ota_flow.sh` – End-to-end OTA flow test

---

## Tasks

### 1.0 OTA Server Setup (Node.js Express)

**Pre-condition:** Node.js >= 16.x installed on development machine

#### Sub-Tasks:

- [ ] 1.1 Review context: PRD §5.1, §5.2 and Blueprint §5.1, §7.1
  - **Relevant Sections:** OTA server setup requirements, REST API design, firmware upload workflow, SHA256 calculation
  - **Key Decisions:** 
    - Server binds to local network (192.168.x.x:8080) for Hub access
    - REST API follows `/api/v1/*` pattern (consistent with Hub API)
    - Error format matches Hub's JSON error format
    - SHA256 auto-calculated on upload (no manual entry)
    - Manifests stored in JSON (human-editable, no database)
    - All firmware versions kept forever (no auto-deletion)
  - **Watch Out For:** 
    - Must bind to network interface (0.0.0.0 or specific IP), not localhost
    - Manifest format must exactly match Hub's LittleFS manifest schema
    - SHA256 calculation must use same algorithm as Hub verification (mbedtls compatible)

- [ ] 1.2 Create project directory and initialize npm
  - **Input:** None (starting from scratch)
  - **Output:** `ota-server/` directory with `package.json`
  - **Ownership:** Developer creates OTA server project (PRD §7.1 O-1)
  - **Implements:** PRD §5.1.1
  ```bash
  mkdir -p ota-server
  cd ota-server
  npm init -y
  ```

- [ ] 1.3 Install dependencies
  - **Input:** `package.json`
  - **Output:** `node_modules/` with Express, Multer, etc.
  - **Implements:** PRD §5.1.1, Blueprint §11.1
  ```bash
  npm install express@^4.18.0 multer@^1.4.5 cors@^2.8.5
  ```

- [ ] 1.4 Create Express server skeleton (`server.js`)
  - **Input:** None
  - **Output:** `ota-server/server.js` with basic Express setup
  - **Ownership:** OTA Server creates server instance
  - **Implements:** PRD §5.1.1, FR-1
  - Bind to `0.0.0.0:8080` (all network interfaces)
  - Enable CORS for Hub access
  - Add request logging middleware

- [ ] 1.5 Implement GET `/api/v1/versions` endpoint
  - **Input:** None (reads manifest files from disk)
  - **Output:** JSON: `{ "hub": ["1.2.0", ...], "node": ["0.9.5", ...] }`
  - **Implements:** PRD §5.1.2, FR-2, Blueprint §7.1
  - Read both `hub/manifest.json` and `node/manifest.json`
  - Extract version strings from each manifest
  - Return consolidated list

- [ ] 1.6 Implement GET `/api/v1/manifest/:device` endpoint
  - **Input:** URL parameter `:device` ("hub" or "node")
  - **Output:** Full manifest JSON (see Blueprint §6.1 schema)
  - **Ownership:** OTA Server creates manifest (PRD §7.1 O-2)
  - **Implements:** PRD §5.1.2, FR-2, FR-4
  - Validate `:device` is "hub" or "node"
  - Read `storage/firmware/:device/manifest.json`
  - Return 404 if device invalid or manifest missing

- [ ] 1.7 Implement GET `/api/v1/firmware/:device/:version` endpoint
  - **Input:** URL parameters `:device` and `:version`
  - **Output:** Binary firmware file (`.bin`)
  - **Implements:** PRD §5.1.2, FR-2
  - Validate device and version
  - Stream file from `storage/firmware/:device/v:version.bin`
  - Set `Content-Type: application/octet-stream`
  - Set `Content-Disposition: attachment; filename=...`
  - Return 404 if firmware file not found

- [ ] 1.8 Create web UI (`views/upload.html`)
  - **Input:** None
  - **Output:** HTML form for firmware upload
  - **User Sees:** Web form with device dropdown (Hub/Node), version input, file picker, description textarea, "Upload" button
  - **Implements:** PRD §5.2.2, FR-5
  - Form fields: device (select), version (text), description (textarea), file (file input)
  - Submit button triggers POST to `/api/v1/firmware/upload`
  - Display upload progress (if possible)
  - Show success message with firmware details (version, size, SHA256)

- [ ] 1.9 Implement POST `/api/v1/firmware/upload` endpoint with multer
  - **Input:** `multipart/form-data` with `device`, `version`, `description`, `file` fields
  - **Output:** JSON: `{ "success": true, "device": "hub", "version": "1.2.0", "sha256": "...", "size": 500000 }`
  - **Ownership:** OTA Server creates firmware binary storage (PRD §7.1 O-1), calculates SHA256 (PRD §7.1 O-3)
  - **Implements:** PRD §5.2.3, FR-3, FR-5
  - Use multer to handle file upload
  - Save file as `storage/firmware/:device/v:version.bin`
  - Calculate SHA256 using Node.js `crypto.createHash('sha256')`
  - Update manifest JSON (next task)
  - Return firmware details

- [ ] 1.10 Implement manifest update logic
  - **Input:** Uploaded firmware metadata (device, version, sha256, size, description)
  - **Output:** Updated `manifest.json` with new version entry
  - **Ownership:** OTA Server creates and updates manifest (PRD §7.1 O-2)
  - **State Change:** Adds new version entry to `versions[]` array, updates `latest` pointer
  - **Implements:** PRD §5.2.6, FR-4, FR-8
  - Read existing `storage/firmware/:device/manifest.json` (if exists)
  - Append new version entry (don't remove old versions - PRD FR-8)
  - Update `latest` field to new version
  - Write updated manifest back to disk
  - Ensure manifest format matches Blueprint §6.1 schema exactly

- [ ] 1.11 Implement error handling with Hub-compatible format
  - **Input:** Error conditions (404, 400, 500)
  - **Output:** JSON: `{ "error": true, "message": "...", "code": "..." }`
  - **Implements:** PRD §5.1.5, FR-6, Blueprint §7.1
  - Return 404 for missing firmware/manifest
  - Return 400 for invalid request parameters
  - Return 500 for server errors
  - All errors in same format as Hub API (consistent error schema)

- [ ] 1.12 Create storage directories
  - **Input:** None
  - **Output:** Directory structure: `storage/firmware/hub/`, `storage/firmware/node/`
  - **Implements:** PRD §5.1.6
  ```bash
  mkdir -p storage/firmware/hub storage/firmware/node
  ```

- [ ] 1.13 Initialize empty manifest files
  - **Input:** None
  - **Output:** `hub/manifest.json`, `node/manifest.json` with initial empty structure
  - **Implements:** PRD §5.1.6
  - Create manifests with `{"device": "hub", "versions": [], "latest": null}` structure

**Post-condition:** OTA server running on `http://192.168.x.x:8080`, serving REST API, web UI accessible, ready to accept firmware uploads

**Verification:**
- [ ] Server starts without errors: `npm start`
- [ ] Web UI accessible at `http://192.168.x.x:8080/`
- [ ] GET `/api/v1/versions` returns 200 with JSON array
- [ ] GET `/api/v1/manifest/hub` returns 200 with empty manifest
- [ ] Upload firmware via web UI, verify SHA256 calculated correctly
- [ ] GET `/api/v1/firmware/hub/1.0.0` returns binary file (after upload)
- [ ] Hub can reach OTA server from same WiFi network

---

### 2.0 Hub OTA Client (Polling and Caching)

**Pre-condition:** OTA server running and accessible on local network; Hub has WiFi connectivity

#### Sub-Tasks:

- [ ] 2.1 Review context: PRD §5.3, §5.4 and Blueprint §4.1, §5.2
  - **Relevant Sections:** Hub polling requirements, firmware download workflow, caching in LittleFS, performance constraints
  - **Key Decisions:**
    - Polling every 5 minutes (fixed interval) on Core 0 (network core)
    - Download in 1KB chunks (safe for Hub's 704KB SRAM)
    - Verify SHA256 before caching
    - Auto-download Node firmware, don't block LED rendering
  - **Watch Out For:**
    - Must run on Core 0 (not Core 1 which handles rendering @ 120 FPS)
    - Must not block main loop (use FreeRTOS timer or task)
    - Graceful failure if OTA server unavailable (don't crash)

- [ ] 2.2 Create `HubOtaClient` class header (`src/hub/ota/hub_ota_client.h`)
  - **Input:** None (new file)
  - **Output:** `HubOtaClient` class definition with public methods
  - **Implements:** PRD §5.3, Blueprint §5.2
  ```cpp
  class HubOtaClient {
  public:
      HubOtaClient(const char* serverUrl, HubRegistry* registry);
      void begin();
      bool checkForUpdates();  // Manual polling
      void startPeriodicPolling();  // 5-minute auto-polling
      bool downloadFirmware(const char* device, const char* version, const char* sha256);
      
  private:
      char serverUrl_[128];
      HubRegistry* registry_;
      TimerHandle_t pollTimer_;
      bool parseManifest(const String& json, String& version, String& url, String& sha256);
      bool verifySha256(const uint8_t* data, size_t len, const char* expected);
  };
  ```

- [ ] 2.3 Implement `HubOtaClient::begin()` initialization
  - **Input:** Server URL (from constructor), registry pointer
  - **Output:** Initialized HTTP client, timer created
  - **Implements:** PRD §5.3.1
  - Validate server URL format
  - Create FreeRTOS timer for periodic polling (not started yet)
  - Log "OTA Client initialized" to serial

- [ ] 2.4 Implement periodic polling with FreeRTOS timer (Core 0)
  - **Input:** None (timer callback)
  - **Output:** Automatic polling every 5 minutes
  - **State Change:** Sets `checking_updates = true` during poll, `false` after
  - **Implements:** PRD §5.3.1, §5.3.2, NFR-1
  - Create FreeRTOS timer with 5-minute period (300,000 ms)
  - Pin timer callback to Core 0 (network core, not rendering core)
  - Timer callback calls `checkForUpdates()` internally
  - Verify LED rendering FPS stays >120 during polling (use PerformanceMonitor)

- [ ] 2.5 Implement `checkForUpdates()` for Hub firmware
  - **Input:** None (polls OTA server)
  - **Output:** Returns `true` if updates available, `false` otherwise
  - **User Sees:** (If called manually) "Checking for updates..." loading indicator
  - **Implements:** PRD §5.3.3, Blueprint §4.1
  - HTTP GET to `http://OTA_SERVER/api/v1/manifest/hub`
  - Parse JSON manifest (ArduinoJson)
  - Compare `manifest["latest"]` with current Hub firmware version
  - If new version available:
    - Log "New Hub firmware available: vX.Y.Z" to serial
    - Dashboard event log: "Updates available: Hub X.Y.Z" (yellow)
  - If no updates:
    - (Manual check only) Dashboard event log: "No updates available" (green)
  - If error (timeout, 404, parse failure):
    - Log error to serial
    - (Manual check only) Dashboard event log: "Update check failed: [error]" (red)
    - Return `false` (graceful degradation per PRD §7.2 E-1)

- [ ] 2.6 Implement `downloadFirmware()` for Node firmware caching
  - **Input:** Device ("node"), version string, expected SHA256
  - **Output:** Returns `true` if downloaded and cached successfully, `false` on error
  - **Ownership:** Hub creates cached Node firmware in LittleFS (PRD §7.1 O-4)
  - **State Change:** Creates `/ota/node_vX.Y.Z.bin` in LittleFS
  - **Implements:** PRD §5.3.4, §5.3.5, §5.3.6, FR-11, FR-12
  - HTTP GET to `http://OTA_SERVER/api/v1/firmware/node/:version`
  - Download in 1KB chunks (loop with `client.readBytes(buf, 1024)`)
  - Calculate SHA256 during download (`mbedtls_sha256_update()`)
  - Write chunks directly to LittleFS file (`/ota/node_vX.Y.Z.bin`)
  - After download complete, verify SHA256 matches expected
  - If mismatch: delete incomplete file, return `false`
  - If success: update LittleFS manifest (`/ota/manifest.json`)
  - Log "Node firmware cached: vX.Y.Z" to serial

- [ ] 2.7 Wire "Check for Updates" button in dashboard
  - **Input:** Button touch event
  - **Output:** Calls `otaClient_->checkForUpdates()`, shows loading indicator
  - **User Sees:** "Checking for updates..." loading indicator (yellow), then "Updates available" or "No updates" or "Check failed"
  - **Implements:** PRD §5.4.1, §5.4.3, §6.1 V-1
  - Add button event handler in `HubDashboard.cpp`
  - Display loading indicator (spinner or message)
  - Call `HubMain::otaClient_->checkForUpdates()`
  - Update event log based on result
  - Hide loading indicator after result

**Post-condition:** Hub polls OTA server every 5 minutes (Core 0), auto-downloads Node firmware to LittleFS cache, "Check for Updates" button triggers manual poll

**Verification:**
- [ ] Hub serial logs show "Polling OTA server..." every 5 minutes
- [ ] Upload new Node firmware to OTA server, verify Hub auto-downloads within 5 minutes
- [ ] Verify `/ota/node_vX.Y.Z.bin` exists in LittleFS after auto-download
- [ ] Click "Check for Updates" button, verify dashboard shows loading indicator
- [ ] Upload new Hub firmware to OTA server, verify "Updates available: Hub X.Y.Z" appears in event log
- [ ] Disconnect OTA server, verify Hub continues operating (LED rendering unaffected, logs "Poll failed" but doesn't crash)
- [ ] Monitor FPS during polling, verify stays >120 FPS

---

### 3.0 Hub Self-Update (ESP32 Dual-Partition OTA)

**Pre-condition:** HubOtaClient implemented, Hub firmware update available on OTA server, Hub has stable WiFi

#### Sub-Tasks:

- [ ] 3.1 Review context: PRD §5.5 and Blueprint §4.2, §5.2
  - **Relevant Sections:** Hub self-update workflow, ESP32 dual-partition OTA, SHA256 verification, user feedback
  - **Key Decisions:**
    - Use Arduino `Update.h` library for ESP32 OTA
    - Download in 1KB chunks, verify SHA256 before applying
    - Display detailed progress (Downloading, Verifying, Installing, Rebooting)
    - ESP32 bootloader handles automatic rollback on boot failure
  - **Watch Out For:**
    - Must call `Update.begin()` before `Update.write()`
    - Must call `Update.end(true)` to finalize (mark partition valid)
    - If verification fails, must call `Update.abort()`
    - Dashboard must show reboot countdown (user awareness)

- [ ] 3.2 Implement `HubOtaClient::performHubUpdate(version, url, sha256)`
  - **Input:** Hub firmware version, download URL, expected SHA256
  - **Output:** Returns `true` if update applied and Hub about to reboot, `false` on error
  - **Ownership:** Hub creates Hub OTA partition (PRD §7.1 O-6)
  - **User Sees:** 
    - Progress bar: "Downloading Hub firmware: 0%" → "45%" → "100%" (yellow)
    - "Verifying Hub firmware..." (yellow)
    - "Installing Hub firmware..." (yellow)
    - "Hub will reboot in 5 seconds" with countdown (yellow)
  - **State Change:** Writes new firmware to inactive ESP32 partition (app1)
  - **Integration:** Calls ESP32 `Update.h` API (per PRD §7.2 E-3)
  - **Implements:** PRD §5.5.1 to §5.5.7, FR-13, Blueprint §4.2
  - HTTP GET to download URL
  - Dashboard: `logEvent("Downloading Hub firmware: 0%")`
  - Call `Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)`
  - Loop: read 1KB chunks, write with `Update.write()`, update SHA256, update dashboard progress every 32KB
  - After download: `mbedtls_sha256_finish()` to get calculated checksum
  - Dashboard: `logEvent("Verifying Hub firmware...")`
  - Compare calculated SHA256 with expected
  - If mismatch: `Update.abort()`, dashboard: "Hub update failed: checksum mismatch" (red), return `false`
  - If match: Dashboard: `logEvent("Installing Hub firmware...")`
  - Call `Update.end(true)` to finalize and mark partition valid
  - Dashboard: "Hub will reboot in 5 seconds" with countdown (see next task)

- [ ] 3.3 Implement reboot countdown with dashboard display
  - **Input:** None (called after successful Update.end())
  - **Output:** Hub reboots into new firmware partition
  - **User Sees:** "Hub will reboot in 5 seconds" countdown: 5, 4, 3, 2, 1 (yellow)
  - **Implements:** PRD §5.5.7, §6.1 V-17
  - Display countdown on dashboard (update every 1 second)
  - Give time for user to see message
  - Call `ESP.restart()` after countdown completes
  - Hub will reboot into new partition (app1)

- [ ] 3.4 Verify ESP32 automatic rollback works
  - **Input:** Intentionally corrupted or non-bootable firmware
  - **Output:** Hub rolls back to previous partition after 3 failed boots
  - **User Sees:** (After rollback) Dashboard event log: "Hub update failed - rolled back to v1.1.0" (red)
  - **Integration:** ESP32 bootloader handles rollback automatically (per PRD §7.2 E-3)
  - **Implements:** PRD §5.5.10, NFR-5, Blueprint §9 Risk mitigation
  - Test 1: Deploy firmware with syntax error (won't compile - catch before upload)
  - Test 2: Deploy firmware that crashes in `setup()` immediately
  - Verify: Hub reboots 3 times (~10s each = 30s total)
  - Verify: Hub boots into previous partition (app0) after 3rd failure
  - Verify: Dashboard shows "Hub update failed - rolled back" message
  - Verify: Hub operates normally on previous firmware

**Post-condition:** Hub can self-update remotely via ESP32 dual-partition OTA with SHA256 verification and automatic rollback on boot failure

**Verification:**
- [ ] Upload new Hub firmware to OTA server
- [ ] Manually trigger Hub update (via serial command or future dashboard button)
- [ ] Verify dashboard shows progress: "Downloading 45%", "Verifying...", "Installing...", "Rebooting in 5s"
- [ ] Verify Hub reboots successfully with new firmware
- [ ] Check `fw` field in Hub status (should show new version)
- [ ] Test rollback: Deploy bad firmware, verify Hub recovers to previous version after ~30s
- [ ] Verify no data loss (LittleFS, NVS persist across reboot)

---

### 4.0 Canary Rollout State Machine

**Pre-condition:** HubOtaClient implemented, Node firmware cached in LittleFS, Hub has 1+ READY nodes

#### Sub-Tasks:

- [ ] 4.1 Review context: PRD §5.6, §5.7, §5.8 and Blueprint §3, §4.3
  - **Relevant Sections:** Canary rollout workflow, state transitions, 30s verification, abort conditions
  - **Key Decisions:**
    - Update 1 READY node first, wait 30s (fixed), check state == READY, then proceed or abort
    - If canary fails (not READY after 30s), abort entire rollout
    - If Hub reboots, rollout state lost (RAM only, no persistence)
    - Sequential fleet rollout (one node at a time)
  - **Watch Out For:**
    - Must check `state_ == IDLE` before starting rollout (prevent concurrent rollouts)
    - Must select canary from READY nodes only (not PENDING/DEGRADED/LOST)
    - Countdown timer must update every 1 second for user awareness
    - Node may report "complete" but fail to reconnect → still abort

- [ ] 4.2 Create `HubOtaDispatch` class header (`src/hub/ota/hub_ota_dispatch.h`)
  - **Input:** None (new file)
  - **Output:** `HubOtaDispatch` class definition
  - **Implements:** PRD §5.6, Blueprint §5.2, §6.3
  ```cpp
  enum ota_dispatch_state_t {
      OTA_DISPATCH_IDLE,
      OTA_DISPATCH_IN_PROGRESS,
      OTA_DISPATCH_COMPLETE,
      OTA_DISPATCH_ABORTED
  };
  
  class HubOtaDispatch {
  public:
      HubOtaDispatch(HubRegistry* registry, HubOtaRepo* repo);
      bool startRollout(const std::vector<uint8_t>& nodeIds);
      void abort();
      void tick();  // Called in main loop, manages state machine
      void onNodeOtaStatus(uint8_t nodeId, const char* state, uint8_t pct);
      ota_dispatch_state_t getState() const { return state_; }
      uint8_t getCurrentNodeId() const { return currentNodeId_; }
      
  private:
      ota_dispatch_state_t state_;
      std::vector<uint8_t> nodeIds_;
      uint8_t currentNodeId_;
      uint8_t completedCount_;
      uint32_t canaryStartTime_;
      bool canary_phase_;
      HubRegistry* registry_;
      HubOtaRepo* repo_;
      
      void processNextNode();
      void sendUpdateToNode(uint8_t nodeId);
  };
  ```

- [ ] 4.3 Implement `startRollout(nodeIds)` with pre-condition check
  - **Input:** Vector of READY node IDs (e.g., [2, 4, 7, 10])
  - **Output:** Returns `true` if rollout started, `false` if already in progress or no nodes
  - **State Change:** 
    - `state_` = `OTA_DISPATCH_IN_PROGRESS` (PRD §8.2 SL-1)
    - `nodeIds_` = input vector (PRD §8.2 SL-2)
    - `currentNodeId_` = first node ID (PRD §8.2 SL-3)
    - `completedCount_` = 0 (PRD §8.2 SL-4)
    - `canary_phase_` = true
  - **Implements:** PRD §5.6.1, §5.6.2, Blueprint §3.1, §4.3
  - **Check pre-condition:** `if (state_ != OTA_DISPATCH_IDLE) return false;` (PRD §8.1 SI-2 - only one rollout at a time)
  - Validate `nodeIds` not empty
  - Select canary node: `currentNodeId_ = nodeIds_[0]` (first node in list)
  - Set state variables as above
  - Dashboard: `logEvent("Canary OTA started: Node X")` (yellow)
  - Call `sendUpdateToNode(currentNodeId_)` to start canary update
  - Dashboard: Highlight canary node in grid (yellow border or background)
  - Return `true`

- [ ] 4.4 Implement `onNodeOtaStatus(nodeId, state, pct)` callback
  - **Input:** Node ID, OTA state string ("downloading", "verifying", "installing", "complete", "error"), progress percentage (0-100)
  - **Output:** Updates `node_entry_t` in registry, updates dashboard
  - **State Change:** 
    - `node_entry_t::ota_state` = state string (PRD §8.2 SL-5)
    - `node_entry_t::ota_pct` = pct (PRD §8.2 SL-6)
  - **User Sees:** Real-time progress updates:
    - "Downloading to Node X: 45%" (yellow, progress bar)
    - "Verifying on Node X..." (yellow)
    - "Installing on Node X..." (yellow)
  - **Implements:** PRD §5.6.6 to §5.6.8, Blueprint §4.3, §6.2
  - Call `registry_->setOtaState(nodeId, state, pct)`
  - Update dashboard progress bar for this node
  - If `state == "complete"`:
    - Node has finished flashing and will reboot
    - Start 30s countdown for canary verification (if this is canary node)

- [ ] 4.5 Implement `tick()` for canary 30s verification
  - **Input:** None (called in main loop)
  - **Output:** Checks canary status after 30s, proceeds or aborts
  - **User Sees:** "Testing on Node X - waiting 30s" with countdown: 30, 29, 28, ..., 1 (yellow)
  - **State Change:** After 30s:
    - If READY: `completedCount_++` (PRD §8.2 SL-4), proceed to next node
    - If not READY: call `abort()` (PRD §3.3 transition)
  - **Implements:** PRD §5.6.9, §5.7.1 to §5.7.4, §5.7.8, FR-14, Blueprint §3.2, §3.3
  - Check if `canary_phase_ == true` and `currentNodeId_` has completed flashing
  - If not in canary phase or node not complete yet, return early
  - Calculate remaining time: `remaining = 30 - ((millis() - canaryStartTime_) / 1000)`
  - Update dashboard countdown every 1 second: "Testing on Node X - waiting Ys" (where Y = remaining)
  - After 30s:
    - Get node state: `node_entry_t* node = registry_->getNode(currentNodeId_)`
    - Check `node->state == NODE_STATE_READY` (per PRD §7.1 O-7 - Hub observes, Node creates)
    - **If READY** (canary success):
      - Dashboard: `logEvent("Canary succeeded - updating remaining nodes")` (green)
      - `completedCount_++`
      - `canary_phase_ = false`
      - Call `processNextNode()` to start fleet rollout
    - **If not READY** (canary failure - LOST or DEGRADED):
      - Dashboard: `logEvent("Canary failed - rollout aborted")` (red)
      - Call `abort()`

- [ ] 4.6 Implement `abort()` to stop rollout
  - **Input:** None (called on canary failure or Hub reboot)
  - **Output:** Clears rollout state, logs error
  - **State Change:** (PRD §3.3 transition, §8.2 SL-1 to SL-4)
    - `state_` = `OTA_DISPATCH_ABORTED`
    - `nodeIds_.clear()`
    - `currentNodeId_` = 0
    - `completedCount_` = 0
    - `canary_phase_` = false
  - **User Sees:** Event log: "Canary failed - rollout aborted" (red) OR "Hub rebooted - rollout aborted" (yellow)
  - **Implements:** PRD §5.7.8, §5.7.9, FR-15, NFR-7, Blueprint §3.3
  - Clear all rollout state variables (as above)
  - Log "Rollout aborted" to serial with reason
  - Dashboard: Update node grid to show failed node as LOST/DEGRADED (red)
  - **DO NOT** proceed with fleet rollout - all remaining nodes stay on old firmware

- [ ] 4.7 Handle Hub reboot during rollout (state loss)
  - **Input:** Hub reboot event (power cycle, crash, self-update)
  - **Output:** All rollout state lost (RAM only), Dashboard shows "Hub rebooted - rollout aborted"
  - **State Change:** (PRD §3.4 transition, §8.2 SL-1 to SL-4, SL-9)
    - All RAM state cleared (constructor resets to defaults)
    - `state_` = `OTA_DISPATCH_IDLE`
    - `nodeIds_` = empty
    - `currentNodeId_` = 0
    - `completedCount_` = 0
    - Dashboard log entries = empty (RAM cleared)
  - **User Sees:** (After reboot) Event log: "Hub rebooted - rollout aborted" (yellow)
  - **Implements:** PRD §5.8.1 to §5.8.4, FR-16, Blueprint §3.4
  - This is **automatic** - no code needed (RAM cleared on reboot)
  - After reboot, Hub initializes with default state (IDLE)
  - Dashboard detects Hub rebooted (uptime reset or boot message) and displays abort message
  - Operator must manually click "START OTA" to retry rollout (PRD §5.8.4)

- [ ] 4.8 Implement `processNextNode()` for fleet rollout
  - **Input:** None (uses internal `nodeIds_` vector)
  - **Output:** Sends `ota_update` to next node in sequence
  - **User Sees:** Progress bar: "Updating Node Y: 65%" (yellow) for each node
  - **Implements:** PRD §5.7.5, §5.7.6, Blueprint §4.3
  - Check if all nodes completed: `if (completedCount_ >= nodeIds_.size())`
    - If yes: `state_ = OTA_DISPATCH_COMPLETE`, dashboard: "Rollout complete - X nodes updated" (green), return
  - Get next node: `currentNodeId_ = nodeIds_[completedCount_]`
  - Call `sendUpdateToNode(currentNodeId_)`
  - Wait for node to complete (via `onNodeOtaStatus()` callbacks and `tick()` monitoring)
  - When node reaches READY: `completedCount_++`, call `processNextNode()` again (recursive for next node)

- [ ] 4.9 Wire "START OTA" button in dashboard
  - **Input:** Button touch event
  - **Output:** Calls `otaDispatch_->startRollout(readyNodeIds)`
  - **User Sees:** Event log: "Canary OTA started: Node X" (yellow), Node X highlighted in grid
  - **Implements:** PRD §5.6.1, FR-17
  - Add button event handler in `HubDashboard.cpp`
  - Query registry for READY nodes: `registry_->getReadyNodes()`
  - If no READY nodes: Dashboard: "No nodes ready for OTA" (yellow), return
  - Call `HubMain::otaDispatch_->startRollout(readyNodeIds)`
  - Dashboard updates handled by `onNodeOtaStatus()` and `tick()` callbacks

**Post-condition:** Canary rollout state machine implemented, "START OTA" button triggers canary-then-fleet deployment, 30s verification, abort on canary failure or Hub reboot

**Verification:**
- [ ] Connect 3+ Nodes to Hub, ensure all READY
- [ ] Upload new Node firmware to OTA server, verify Hub caches it
- [ ] Click "START OTA" button
- [ ] Verify dashboard shows "Canary OTA started: Node X" (yellow)
- [ ] Verify first node (canary) updates: downloading → verifying → installing → reboot
- [ ] Verify 30s countdown displays: "Testing on Node X - waiting 30s" → "...29s" → ... → "...1s"
- [ ] After 30s, verify canary node shows READY (green in node grid)
- [ ] Verify dashboard shows "Canary succeeded - updating remaining nodes" (green)
- [ ] Verify remaining nodes update sequentially (one at a time)
- [ ] Verify dashboard shows "Rollout complete - 3 nodes updated" (green) after all nodes finish
- [ ] **Test canary failure:** Disconnect canary node after flashing (simulate boot failure), verify rollout aborts, dashboard shows "Canary failed - rollout aborted" (red), remaining nodes NOT updated
- [ ] **Test Hub reboot:** Start rollout, reboot Hub during canary wait, verify rollout aborted, dashboard shows "Hub rebooted - rollout aborted" (yellow) after reboot

---

### 5.0 Dashboard Integration (Progress and UI)

**Pre-condition:** HubOtaClient and HubOtaDispatch implemented, LVGL dashboard initialized

#### Sub-Tasks:

- [ ] 5.1 Review context: PRD §4.1 FR-17 to FR-22 and Blueprint §5.2
  - **Relevant Sections:** Dashboard UI requirements, progress bars, countdown timers, color scheme, button styles
  - **Key Decisions:**
    - Progress bars match existing health panel bar style
    - Countdown timer updates every 1 second
    - Color-coded event log: Green=success, Yellow=in-progress, Red=error
    - Single-touch interactions (no double-tap or long-press)
  - **Watch Out For:**
    - LVGL widgets must be created on same core as LVGL update loop (Core 1)
    - Don't block rendering loop with network operations (those are on Core 0)
    - Progress bars must show 0-100% range, not raw byte counts

- [ ] 5.2 Add OTA progress bars to dashboard
  - **Input:** OTA progress percentage (0-100) from `node_entry_t::ota_pct` or Hub download progress
  - **Output:** LVGL bar widget showing progress
  - **User Sees:** 
    - Progress bar filling left-to-right (0% → 45% → 100%)
    - Label above bar: "Downloading to Node X: 45%" or "Downloading Hub firmware: 45%"
  - **Implements:** PRD §5.6.6, §5.5.4, FR-18, FR-21
  - Create `lv_bar` widget in `HubDashboard.cpp` (matches health panel bar style per PR D FR-21)
  - Set bar range: `lv_bar_set_range(bar, 0, 100)`
  - Update bar value: `lv_bar_set_value(bar, pct, LV_ANIM_ON)`
  - Create label above bar: `lv_label_set_text_fmt(label, "Downloading to Node %d: %d%%", nodeId, pct)`
  - Update bar and label in real-time when `onNodeOtaStatus()` called

- [ ] 5.3 Add countdown timer widget for canary verification
  - **Input:** Remaining seconds (30, 29, 28, ..., 1)
  - **Output:** LVGL label showing countdown
  - **User Sees:** "Testing on Node X - waiting 30s" → "...29s" → ... → "...1s" (yellow)
  - **Implements:** PRD §5.6.9, FR-19, §6.1 V-9
  - Create `lv_label` widget
  - Update label every 1 second from `HubOtaDispatch::tick()`
  - Text format: `lv_label_set_text_fmt(label, "Testing on Node %d - waiting %ds", nodeId, remaining)`
  - Color: yellow (in-progress per PRD FR-20)

- [ ] 5.4 Implement color-coded event log
  - **Input:** Event type (success, in-progress, error), event message
  - **Output:** Event log entry with appropriate color
  - **User Sees:** 
    - Green text: "Rollout complete", "Canary succeeded", "No updates available"
    - Yellow text: "Canary OTA started", "Checking for updates...", "Hub rebooted"
    - Red text: "Canary failed", "Update check failed", "Hub update failed"
  - **Implements:** PRD §5.7.4, §5.7.8, FR-20
  - Modify `HubDashboard::logEvent(message, type)` to accept event type enum
  - Set label color based on type:
    - `LV_COLOR_MAKE(0x4C, 0xAF, 0x50)` for success (green)
    - `LV_COLOR_MAKE(0xFF, 0xEB, 0x3B)` for in-progress (yellow)
    - `LV_COLOR_MAKE(0xF4, 0x43, 0x36)` for error (red)
  - Update all OTA event log calls to specify type

- [ ] 5.5 Add loading indicator for "Check for Updates"
  - **Input:** Button click event
  - **Output:** Loading spinner or animated label
  - **User Sees:** "Checking for updates..." with spinner (yellow) while polling
  - **Implements:** PRD §5.4.3, FR-22, §6.1 V-1
  - Create `lv_spinner` widget (or animated label with "...")
  - Show spinner when button clicked
  - Hide spinner when `checkForUpdates()` returns (success or failure)
  - Duration: 1-2s (network latency per PRD §6.2 T-1)

- [ ] 5.6 Verify all dashboard OTA buttons use single-touch
  - **Input:** Touch event
  - **Output:** Action triggered immediately (no long-press or double-tap)
  - **Implements:** PRD §5.6.1, FR-17, NFR-10, §6.1 V-5
  - Review button event handlers in `HubDashboard.cpp`
  - Ensure all use `LV_EVENT_CLICKED` (not `LV_EVENT_LONG_PRESSED` or multiple clicks)
  - "START OTA" button: single touch triggers `startRollout()`
  - "Check for Updates" button: single touch triggers `checkForUpdates()`

**Post-condition:** Dashboard displays real-time OTA progress (progress bars, countdown timers), color-coded event log (green/yellow/red), loading indicators, single-touch buttons

**Verification:**
- [ ] Trigger Hub self-update, verify progress bar shows 0% → 100% as download progresses
- [ ] Verify progress bar label updates: "Downloading Hub firmware: 45%"
- [ ] Trigger Node rollout, verify per-node progress bars display
- [ ] Verify canary countdown timer displays: "Testing on Node 2 - waiting 30s" → "...1s"
- [ ] Verify event log colors:
  - Green: "Rollout complete - 3 nodes updated"
  - Yellow: "Canary OTA started: Node 2", "Checking for updates..."
  - Red: "Canary failed - rollout aborted"
- [ ] Click "Check for Updates", verify loading spinner appears and disappears after 1-2s
- [ ] Verify all buttons respond to single touch (no long-press or double-tap required)

---

### 6.0 Integration Testing

**Pre-condition:** OTA server running, Hub firmware with OTA client deployed, test scripts written

#### Sub-Tasks:

- [ ] 6.1 Review context: PRD §5.9 (Epic 6) and Blueprint §10
  - **Relevant Sections:** Integration test requirements, REST API validation, WebSocket protocol testing, end-to-end OTA flow
  - **Key Decisions:**
    - Bash script tests OTA server REST API with curl
    - Node.js script tests WebSocket protocol (HELLO/WELCOME/AUTH/READY/ota_update/ota_status)
    - Bash script tests full OTA flow end-to-end
    - All tests exit with code 0 on success, non-zero on failure
    - Generate timestamped log files with colored output (green ✓ / red ✗)
  - **Watch Out For:**
    - Must validate JSON schemas (not just 200 status codes)
    - Must verify SHA256 checksums actually calculated correctly
    - Must grep Hub/Node serial logs for expected messages
    - Tests must be deterministic (idempotent, repeatable)

- [ ] 6.2 Write `tools/test_ota_server.sh` (Bash script)
  - **Input:** OTA server URL (default: `http://192.168.1.100:8080`)
  - **Output:** Exit code 0 on success, non-zero on failure; colored output (green ✓ / red ✗)
  - **Implements:** PRD §5.9.1 to §5.9.5
  ```bash
  #!/bin/bash
  SERVER_URL="${1:-http://192.168.1.100:8080}"
  LOG_FILE="test_ota_server_$(date +%Y%m%d_%H%M%S).log"
  
  # Test 1: GET /api/v1/versions returns 200 with JSON array
  # Test 2: GET /api/v1/manifest/hub returns valid manifest JSON
  # Test 3: GET /api/v1/manifest/node returns valid manifest JSON
  # Test 4: GET /api/v1/firmware/hub/1.0.0 returns binary data (after upload)
  # Test 5: POST /api/v1/firmware/upload with mock .bin file
  # Test 6: Verify SHA256 auto-calculated (check manifest after upload)
  # Test 7: Verify error format (GET non-existent firmware → JSON error)
  
  # Use curl to test each endpoint
  # Use jq to validate JSON schemas
  # Print green ✓ for pass, red ✗ for fail
  # Exit 0 if all tests pass, exit 1 if any fail
  # Save all output to LOG_FILE
  ```

- [ ] 6.3 Write `tools/test_ws_protocol.js` (Node.js script)
  - **Input:** Hub WebSocket URL (default: `ws://192.168.1.100:80`)
  - **Output:** Exit code 0 on success, non-zero on failure; colored output
  - **Implements:** PRD §5.9.6 to §5.9.9
  ```javascript
  const WebSocket = require('ws');
  
  // Test 1: Connect to Hub WebSocket
  // Test 2: Send HELLO message → expect WELCOME
  // Test 3: Send AUTH message → expect node registered
  // Test 4: Send keepalive → expect state = READY
  // Test 5: Simulate ota_update message format (validate schema)
  // Test 6: Simulate ota_status messages (downloading, verifying, installing, complete)
  // Test 7: Validate JSON schemas for all messages
  
  // Use WebSocket client library
  // Print green ✓ for pass, red ✗ for fail
  // Exit 0 if all tests pass, exit 1 if any fail
  ```

- [ ] 6.4 Write `tools/test_ota_flow.sh` (End-to-end test)
  - **Input:** OTA server URL, Hub serial port, Node serial port
  - **Output:** Exit code 0 on success, non-zero on failure; colored output
  - **Implements:** PRD §5.9.10, §5.9.11, Blueprint §10.2
  ```bash
  #!/bin/bash
  # Test full OTA flow:
  # 1. Upload new Node firmware to OTA server (POST /api/v1/firmware/upload)
  # 2. Wait for Hub to detect update (grep serial log for "New Node firmware available")
  # 3. Trigger "START OTA" via REST API or serial command
  # 4. Monitor Hub serial log for "Canary OTA started"
  # 5. Monitor Node serial log for "OTA_START", "Downloading", "Verifying", "Installing"
  # 6. Wait 30s for canary verification
  # 7. Grep Hub log for "Canary succeeded"
  # 8. Monitor remaining nodes update
  # 9. Grep Hub log for "Rollout complete"
  # 10. Verify all nodes show new firmware version (grep serial or REST API)
  
  # Use curl for REST API calls
  # Use serial monitor or `cat /dev/ttyUSB0` to monitor logs
  # Print green ✓ for each step, red ✗ on failure
  # Exit 0 if all steps pass, exit 1 if any fail
  # Generate timestamped log file
  ```

- [ ] 6.5 Run all integration tests and verify pass rate
  - **Input:** All test scripts
  - **Output:** Test results with pass/fail counts
  - **Implements:** PRD §10 M-10 (100% integration test pass rate)
  - Run `tools/test_ota_server.sh` → expect all tests pass
  - Run `tools/test_ws_protocol.js` → expect all tests pass
  - Run `tools/test_ota_flow.sh` → expect full OTA flow success
  - Fix any failing tests before proceeding
  - Document test results in `INTEGRATION_TEST_RESULTS.md`

**Post-condition:** Integration test suite validates OTA server REST API, WebSocket protocol, and full OTA flow; all tests pass; tests are repeatable and deterministic

**Verification:**
- [ ] Run `./tools/test_ota_server.sh`, verify exit code 0, all tests show green ✓
- [ ] Run `node tools/test_ws_protocol.js`, verify exit code 0, all tests show green ✓
- [ ] Run `./tools/test_ota_flow.sh`, verify exit code 0, full OTA flow completes successfully
- [ ] Run all tests 3 times to verify repeatability
- [ ] Review timestamped log files, verify detailed output captured

---

## Integration-Critical Tasks
*Source: Blueprint §4 - Integration Wiring*

These tasks have specific wiring requirements that must be followed exactly. Deviating from the specified sequence can cause bugs.

### IC-1: Hub OTA Client → OTA Server Polling
*Maps to: Blueprint §4.1*

**Critical Sequence:**
```
1. HTTPClient.begin("http://192.168.x.x:8080/api/v1/manifest/hub") 
   // REQUIRED: Must use full URL with IP (not hostname) - local network
2. HTTPClient.GET() 
   // Returns manifest JSON with version, sha256, size, url
3. ArduinoJson::deserializeJson(doc, payload) 
   // Parse manifest (from PRD §7.1 O-2 - Hub observes, does not create)
4. compareVersions(doc["version"], currentVersion) 
   // Version comparison logic (hidden from user per PRD §6.1 V-1)
5. dashboard_.logEvent("Updates available: Hub X.Y.Z") 
   // (message to dashboard: per PRD §6.1 V-2 - visible to user)
```

**Ownership Rules (from PRD §7):**
- OTA server manifest JSON is created by **OTA Server** (auto-generated on upload) — Hub must NOT create or modify it
- Hub creates event log entries — Hub is responsible

**User Visibility (from PRD §6):**
- User sees: "Checking for updates..." loading indicator, then "Updates available: Hub X.Y.Z" (yellow) or "No updates available" (green)
- User does NOT see: HTTP GET request, JSON parsing, manifest comparison, version comparison logic, firmware sizes, SHA256 checksums

**State Changes (from Blueprint §3):**
- Before: `checking_updates = false`
- During: `checking_updates = true` (while polling)
- After: `checking_updates = false`, event log updated

**Common Mistakes to Avoid (from Blueprint §2.3):**
- ❌ **Mistake 1**: Hub modifies OTA server manifest - **Why wrong:** OTA server owns manifest; Hub only reads (violates ownership boundary PRD §7.1 O-2)
- ❌ **Mistake 2**: Using `localhost` instead of network IP - **Why wrong:** Hub is on different device than OTA server; must use network-accessible IP
- ❌ **Mistake 3**: Blocking Core 1 (rendering) with HTTP request - **Why wrong:** Causes LED FPS drop below 120 target (violates PRD NFR-1)

**Verification:**
- [ ] Verify HTTP GET uses network IP (192.168.x.x), not localhost
- [ ] Verify polling runs on Core 0 (network core), not Core 1 (rendering core)
- [ ] Verify LED FPS stays >120 during polling (use PerformanceMonitor)
- [ ] Verify dashboard shows loading indicator during poll (user sees per PRD §6.1 V-1)
- [ ] Verify manifest parsing validates JSON schema (don't accept malformed JSON)

---

### IC-2: Hub OTA Client → Hub Self-Update
*Maps to: Blueprint §4.2*

**Critical Sequence:**
```
1. dashboard_.logEvent("Downloading Hub firmware: 0%") 
   // REQUIRED: User must see immediate feedback (per PRD §6.1 V-14)
2. Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH) 
   // REQUIRED: Must call before Update.write() - initializes partition write
3. while (client.available()) { Update.write(buf, 1024); mbedtls_sha256_update(...); } 
   // Download in 1KB chunks, verify while downloading (per PRD §4.1 FR-11, §4.2 NFR-4)
4. mbedtls_sha256_finish(&sha_ctx, calculated_sha); if (mismatch) { Update.abort(); } 
   // Verify before applying (per PRD §4.2 NFR-4)
5. Update.end(true); 
   // Finalize and mark partition valid
6. ESP.restart(); 
   // Reboot into new firmware (ESP32 handles rollback if boot fails per PRD §7.2 E-3)
```

**Ownership Rules (from PRD §7):**
- Hub OTA partition (inactive partition, e.g., app1) is created by **Hub** (writes downloaded firmware) — Hub is responsible
- ESP32 bootloader creates rollback (autonomous action) — Hub must trust ESP32 OTA library

**User Visibility (from PRD §6):**
- User sees: 
  - Progress bar: "Downloading Hub firmware: 45%" (real-time updates every 32KB per PRD §6.2 T-8)
  - "Verifying Hub firmware..." (1-2s per PRD §6.2 T-9)
  - "Installing Hub firmware..." (5-10s per PRD §6.2 T-10)
  - "Hub will reboot in 5 seconds" countdown (5, 4, 3, 2, 1 per PRD §6.1 V-17)
- User does NOT see: HTTPClient buffer, LittleFS operations, mbedtls SHA256 context, ESP32 Update.h partition writes, bootloader operations

**State Changes (from Blueprint §3):**
- Before: `hub_updating = false`, `ota_pct = 0`
- During: `hub_updating = true`, `ota_pct = 0-100` (real-time updates)
- After reboot: `hub_updating = false` (RAM cleared), new firmware version active

**Common Mistakes to Avoid (from Blueprint §2.3):**
- ❌ **Mistake 1**: Calling `Update.write()` before `Update.begin()` - **Why wrong:** ESP32 OTA API requires `begin()` first to initialize partition; causes crash or flash corruption
- ❌ **Mistake 2**: Not calling `Update.end(true)` after successful download - **Why wrong:** Partition not marked valid; ESP32 won't boot into new firmware
- ❌ **Mistake 3**: Applying update without SHA256 verification - **Why wrong:** Corrupted firmware bricks Hub; violates PRD §4.2 NFR-4 safety requirement
- ❌ **Mistake 4**: Buffering entire firmware in RAM before writing - **Why wrong:** Hub only has 704KB SRAM; firmware is ~500KB; causes out-of-memory crash (violates PRD §4.2 NFR-2)

**Verification:**
- [ ] Verify `Update.begin()` called before first `Update.write()`
- [ ] Verify download uses 1KB chunks (not larger)
- [ ] Verify SHA256 calculated during download (not after)
- [ ] Verify SHA256 mismatch triggers `Update.abort()` (not `Update.end()`)
- [ ] Verify `Update.end(true)` called after successful verification
- [ ] Verify dashboard shows reboot countdown (5s) before `ESP.restart()`
- [ ] Test rollback: Deploy bad firmware, verify Hub recovers to previous version after ~30s

---

### IC-3: Hub OTA Dispatch → Canary Rollout
*Maps to: Blueprint §4.3*

**Critical Sequence:**
```
1. if (state_ != OTA_DISPATCH_IDLE) return ERROR_ALREADY_IN_PROGRESS; 
   // REQUIRED: Check pre-condition (per PRD §8.1 SI-2 - only one rollout at a time)
2. state_ = OTA_DISPATCH_IN_PROGRESS; nodeIds_ = nodeIds; completedCount_ = 0; 
   // Set state variables (per PRD §8.2 SL-1, SL-2, SL-4)
3. dashboard_.logEvent("Canary OTA started: Node X") 
   // User sees rollout begin (per PRD §6.1 V-5)
4. processNextNode(); sendUpdateToNode(currentNodeId_); 
   // Start canary update (WebSocket ota_update message per PRD §9.2 Integration 3)
5. Wait for Node to report ota_state = "complete" (via onNodeOtaStatus callbacks per PRD §8.2 SL-5)
6. Wait 30s countdown (per PRD §4.1 FR-14, §6.1 V-9)
7. Check node state = READY (per PRD §7.1 O-7 - Hub observes, Node creates)
8. If READY: completedCount_++; processNextNode(); (continue with fleet)
   OR If not READY: abort(); (stop rollout per PRD §4.1 FR-15)
```

**Ownership Rules (from PRD §7):**
- Node OTA partition is created by **Node** (writes downloaded firmware from Hub) — Hub must NOT directly write to Node partitions
- HubOtaDispatch state variables are created by **Hub** — Hub is responsible
- Dashboard event log entries are created by **Hub** — Hub is responsible

**User Visibility (from PRD §6):**
- User sees:
  - "Canary OTA started: Node 2" (yellow) - immediate
  - Progress bar: "Downloading to Node 2: 45%" (yellow) - real-time updates
  - "Testing on Node 2 - waiting 30s" countdown (29, 28, ..., 1) (yellow) - 30s countdown
  - "Canary succeeded - updating remaining nodes" (green) - after 30s if READY
  - OR "Canary failed - rollout aborted" (red) - after 30s if not READY
- User does NOT see: nodeIds_ vector manipulation, HubOtaDispatch state machine, NODE_STATE_READY check, canary_phase_ flag, processNextNode() iterations

**State Changes (from Blueprint §3):**
- Before: `state_ = IDLE`, `nodeIds_ = []`, `currentNodeId_ = 0`, `completedCount_ = 0`
- After startRollout: `state_ = IN_PROGRESS`, `nodeIds_ = [2, 4, 7, ...]`, `currentNodeId_ = 2`, `completedCount_ = 0`
- After canary success: `completedCount_ = 1`, `currentNodeId_ = 4` (next node)
- After canary failure: `state_ = ABORTED`, `nodeIds_ = []`, `currentNodeId_ = 0`, `completedCount_ = 0`
- After all nodes complete: `state_ = COMPLETE`, `completedCount_ = nodeIds_.size()`

**Common Mistakes to Avoid (from Blueprint §2.3):**
- ❌ **Mistake 1**: Not checking `state_ == IDLE` before starting rollout - **Why wrong:** Allows concurrent rollouts; causes race conditions and state corruption (violates PRD §8.1 SI-2)
- ❌ **Mistake 2**: Assuming canary succeeded without checking READY state - **Why wrong:** Node may report "complete" but fail to boot; only HELLO message with keepalives confirms success (violates PRD §7.1 O-7)
- ❌ **Mistake 3**: Proceeding with fleet rollout after canary failure - **Why wrong:** Bad firmware deployed to entire fleet; violates PRD §4.1 FR-15, NFR-7 safety requirements
- ❌ **Mistake 4**: Waiting less than 30s for canary verification - **Why wrong:** Some boot failures take 20-25s to manifest; 30s is minimum to catch slow-developing problems (violates PRD §4.1 FR-14, NFR-6)
- ❌ **Mistake 5**: Hub directly writes to Node OTA partition - **Why wrong:** Violates ownership boundary; Node owns its partition; Hub can only send WebSocket message (violates PRD §7.1 O-7, Blueprint §2.3)

**Verification:**
- [ ] Verify `state_ == IDLE` check prevents concurrent rollouts (try clicking "START OTA" twice rapidly)
- [ ] Verify 30s countdown displays and completes before checking READY state
- [ ] Verify canary node must be READY (not just "complete") before proceeding
- [ ] Verify rollout aborts if canary not READY after 30s
- [ ] Verify remaining nodes NOT updated if canary fails
- [ ] Test canary failure: Disconnect canary node after flashing, verify rollout aborts
- [ ] Test Hub reboot: Start rollout, reboot Hub during canary wait, verify rollout aborted, operator must manually restart

---

## Validation Checklist

Before implementation, verify 1:1 mapping is complete:

### PRD Coverage
- [x] Every §5 acceptance criterion has a corresponding subtask (48+ criteria → tasks)
- [x] Every §6 visibility rule has "User Sees" in relevant subtask (22 visibility specs → User Sees fields)
- [x] Every §7 ownership rule is in Quick Reference AND relevant subtask "Ownership" field (10 artifacts → Quick Reference + task Ownership)
- [x] Every §8 state requirement has "State Change" in relevant subtask (13 state variables → State Change fields)

### Blueprint Coverage
- [x] Every §2 boundary rule is in Critical Boundaries AND enforced in tasks (9 derived rules → Quick Reference DO NOTs + IC tasks)
- [x] Every §3 state transition maps to Pre/Post conditions (4 transitions → Pre/Post in tasks 4.3, 4.5, 4.6, 4.7)
- [x] Every §4 integration wiring maps to an Integration-Critical Task (3 integrations → IC-1, IC-2, IC-3)

### Task Quality
- [x] First subtask of each parent references relevant docs (Tasks 1.1, 2.1, 3.1, 4.1, 5.1, 6.1)
- [x] All subtasks are specific and actionable (not vague - each has Input/Output/Implements)
- [x] All "Implements" fields trace back to PRD/Blueprint sections (every subtask has PRD/Blueprint references)

---

## Notes

- **Unit tests**: Not explicitly required in PRD, but recommended for critical logic (version comparison, SHA256 calculation, canary selection)
- **Run all tests**: `cd ota-server && npm test` (after writing unit tests), `./tools/test_ota_server.sh`, `node tools/test_ws_protocol.js`, `./tools/test_ota_flow.sh`
- **Lint**: Follow existing lint and format scripts in Hub and Node firmware
- **Serial monitoring**: `pio device monitor -b 115200` for Hub/Node logs
- **OTA server startup**: `cd ota-server && npm start` (server listening on http://192.168.x.x:8080)
- **Hub build**: `cd firmware/Tab5.encoder && PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin" pio run -e tab5 -t upload -d .`
- **Node build**: `cd firmware/K1.LightwaveS3 && pio run -t upload`
- **Ensure all tasks are completed before moving to next parent task**
- **Pay special attention to Integration-Critical Tasks – follow sequences exactly**
- **Verify pre-conditions are met before starting each parent task**
- **Confirm post-conditions after completing each parent task**
- **Reference the Quick Reference section frequently during implementation**
- **When in doubt, check the original PRD §X or Blueprint §Y cited in "Implements"**

---

*End of Task List*
