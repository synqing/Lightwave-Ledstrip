# Product Requirements Document: Complete OTA Infrastructure for LightwaveOS

> **Traceability Note:** This PRD uses section numbers (§1-§10) that are referenced by the Technical Blueprint and Task List. All 24 user answers have been mapped to specific sections.

## §1. Overview & Vision

LightwaveOS is a distributed LED control system where a Hub (ESP32-P4 Tab5) coordinates 1-12 Nodes (ESP32-S3 controllers). Currently, the Hub can push firmware updates to Nodes via WebSocket, but has no mechanism to update itself - requiring manual USB flashing. This creates an asymmetric update architecture and a single point of failure.

This feature implements a complete Over-The-Air (OTA) update infrastructure with an external Node.js server that hosts firmware for both Hub and Nodes. The Hub will poll the server for updates (every 5 minutes + manual trigger), download and cache Node firmware for distribution, and self-update using ESP32 dual-partition OTA. A canary rollout strategy (update 1 node first, wait 30s, verify, then update remaining nodes) minimizes deployment risk.

**Key Outcomes:**
- **Eliminate manual Hub updates** - Hub can self-update remotely via ESP32 dual-partition OTA with automatic rollback
- **Unified firmware distribution** - Single source of truth (OTA server) for all firmware versions
- **Safe deployments** - Canary rollout tests on 1 node before fleet-wide deployment
- **Developer productivity** - Upload firmware once via web UI, system handles distribution automatically
- **Full observability** - LVGL dashboard shows detailed progress, countdown timers, and color-coded status

## §2. Problem Statement

**Current Pain Points:**
- **Hub requires USB flashing** - No remote update capability; requires physical access for firmware updates
- **Asymmetric architecture** - Hub can update Nodes but cannot update itself ("who watches the watchmen?")
- **Manual firmware management** - Developer must manually upload firmware to Hub's LittleFS
- **No version control** - No centralized tracking of firmware versions or rollback capability
- **High deployment risk** - No staged rollout; all nodes update simultaneously (all-or-nothing)
- **Limited observability** - Dashboard buttons exist but aren't wired; no progress visibility

**Impact:**
- Development friction: Every Hub firmware change requires physical USB access
- Deployment risk: Bad Hub firmware requires manual recovery; bad Node firmware affects entire fleet instantly
- Operational overhead: No automation; every update is manual multi-step process

## §3. Target Users

| User Type | Needs | Key Actions |
|-----------|-------|-------------|
| **Firmware Developer** | Safe, automated firmware deployment; version control; rollback capability | Upload firmware via web UI; trigger OTA rollouts; monitor progress; roll back if needed |
| **System Operator** | Remote Hub updates without USB; visibility into update status; control over update timing | Click "START OTA" to deploy; click "Check for Updates" to poll; observe countdown timers and progress bars |
| **LED Installation Technician** | Reliable, self-healing system; minimal physical intervention | Rely on automatic updates; only intervene for disaster recovery via USB |

## §4. Core Requirements

### §4.1 Functional Requirements

| ID | Requirement | Priority | Source |
|----|-------------|----------|--------|
| FR-1 | OTA server (Node.js Express) serves firmware on local network (192.168.x.x:8080) | High | Q1 |
| FR-2 | OTA server exposes REST API at `/api/v1/versions`, `/api/v1/manifest/:device`, `/api/v1/firmware/:device/:version`, POST `/api/v1/firmware/upload` | High | Q5 |
| FR-3 | OTA server auto-calculates SHA256 checksums on firmware upload | High | Q2, Q19 |
| FR-4 | OTA server stores version metadata in JSON files (manifest format matching Hub's LittleFS manifest) | High | Q8, Q17, Q18 |
| FR-5 | OTA server provides web UI for manual firmware upload (developer uploads .bin files) | High | Q4 |
| FR-6 | OTA server returns errors in same JSON format as Hub API | High | Q6 |
| FR-7 | OTA server has no authentication (open on local network like Hub API) | Medium | Q7 |
| FR-8 | OTA server retains all firmware versions indefinitely (no auto-deletion) | Medium | Q20 |
| FR-9 | Hub polls OTA server every 5 minutes (fixed interval) for updates | High | Q21 |
| FR-10 | Hub exposes "Check for Updates" button for manual polling | High | Q12 |
| FR-11 | Hub downloads firmware in 1KB chunks (matches NodeOtaClient pattern) | High | Q22 |
| FR-12 | Hub caches Node firmware in LittleFS before distribution | High | Feature description |
| FR-13 | Hub self-updates using ESP32 dual-partition OTA (app0/app1 with rollback) | High | Feature description |
| FR-14 | Canary rollout: Update 1 READY node first, wait 30s (fixed), verify stability, then update remaining nodes | High | Q10, Q23 |
| FR-15 | If canary node fails, abort entire rollout and log error | High | Feature description |
| FR-16 | If Hub reboots during rollout, abort rollout (require manual "START OTA" restart) | High | Q3, Q11 |
| FR-17 | Dashboard "START OTA" button triggers canary rollout | High | Q15 |
| FR-18 | Dashboard shows detailed progress: "Downloading 45%", "Verifying...", "Installing..." | High | Q9 |
| FR-19 | Dashboard shows "Testing on Node X - waiting 30s" with countdown timer during canary verification | High | Q10 |
| FR-20 | Dashboard uses existing color scheme: Green=success, Yellow=in-progress, Red=error | High | Q14 |
| FR-21 | Dashboard OTA controls match existing button style/colors and use same bars/labels for progress | High | Q13, Q16 |
| FR-22 | Dashboard shows "Checking for updates..." loading indicator when polling server | High | Q12 |
| FR-23 | OTA server prioritizes small API requests over large firmware downloads (dashboard stays responsive) | Medium | Q24 |
| FR-24 | Integration test suite validates REST API, WebSocket HELLO/WELCOME/AUTH/READY flow, and full OTA flow | High | Feature description |

### §4.2 Non-Functional Requirements

| ID | Category | Requirement | Source |
|----|----------|-------------|--------|
| NFR-1 | Performance | Hub polling OTA server every 5 minutes must not impact LED rendering (120 FPS target on Core 1) | Q21, Codebase |
| NFR-2 | Performance | Firmware downloads (1KB chunks) must not exhaust Hub's 704KB SRAM | Q22, Constraints |
| NFR-3 | Performance | Small API requests prioritized over firmware downloads to keep dashboard responsive | Q24 |
| NFR-4 | Reliability | SHA256 verification must catch corrupted downloads before applying updates | Q2, Q19 |
| NFR-5 | Reliability | ESP32 dual-partition OTA with automatic rollback if Hub fails to boot after self-update | Feature description |
| NFR-6 | Safety | Canary rollout 30s verification delay must catch boot loops and crashes before fleet-wide deployment | Q23 |
| NFR-7 | Safety | If canary fails, entire rollout aborts (no partial deployments) | FR-15 |
| NFR-8 | Consistency | OTA server API patterns (`/api/v1/*`, JSON responses, error format) match Hub API | Q5, Q6 |
| NFR-9 | Consistency | Dashboard UI (buttons, colors, progress bars) consistent with existing LVGL widgets | Q13, Q14, Q16 |
| NFR-10 | Usability | All dashboard interactions use single-touch (no double-tap or long-press) | Q15 |
| NFR-11 | Usability | All user actions provide immediate visual feedback (loading indicators, progress, countdowns) | Q9, Q10, Q12 |
| NFR-12 | Scalability | OTA server must handle 1-12 concurrent Node downloads without blocking | Q24 |
| NFR-13 | Maintainability | Firmware metadata stored in JSON (human-editable, no database required) | Q8, Q18 |

## §5. User Stories & Acceptance Criteria

### Epic 1: OTA Server Setup

#### Story §5.1: Developer Sets Up OTA Server
**As a** firmware developer  
**I want** to run the OTA server on my local network  
**So that** the Hub and my development machine can both access it

**Acceptance Criteria:**
- [ ] §5.1.1: OTA server binds to 192.168.x.x:8080 (local network interface)
- [ ] §5.1.2: OTA server serves REST API at `/api/v1/versions`, `/api/v1/manifest/:device`, `/api/v1/firmware/:device/:version`
- [ ] §5.1.3: Hub can reach OTA server from same WiFi network
- [ ] §5.1.4: OTA server has no authentication (open access on local network)
- [ ] §5.1.5: OTA server logs all requests to console (for debugging)

#### Story §5.2: Developer Uploads Firmware
**As a** firmware developer  
**I want** to upload compiled .bin files via web UI  
**So that** I don't need to manually SSH or copy files

**Acceptance Criteria:**
- [ ] §5.2.1: Web UI accessible at `http://192.168.x.x:8080/` with upload form
- [ ] §5.2.2: Form accepts device type (Hub/Node), version string (e.g., "1.2.0"), and .bin file
- [ ] §5.2.3: Server auto-calculates SHA256 checksum on upload
- [ ] §5.2.4: Server stores firmware in `ota-server/firmware/{device}/v{version}.bin`
- [ ] §5.2.5: Server updates manifest JSON with new version, size, SHA256
- [ ] §5.2.6: Manifest follows same structure as Hub's LittleFS manifest
- [ ] §5.2.7: All firmware versions retained indefinitely (no auto-deletion)
- [ ] §5.2.8: Upload success shows firmware details (version, size, SHA256)

### Epic 2: Hub OTA Client

#### Story §5.3: Hub Periodically Checks for Updates
**As a** system operator  
**I want** the Hub to automatically check for updates  
**So that** I'm notified of new firmware without manual intervention

**Acceptance Criteria:**
- [ ] §5.3.1: Hub polls `/api/v1/manifest/hub` every 5 minutes (fixed interval)
- [ ] §5.3.2: Polling runs on Core 0 (network core) without blocking Core 1 (rendering)
- [ ] §5.3.3: If new Hub firmware available, log to serial and dashboard event log (yellow)
- [ ] §5.3.4: If new Node firmware available, download to LittleFS cache automatically
- [ ] §5.3.5: Node firmware download uses 1KB chunks to avoid exhausting 704KB SRAM
- [ ] §5.3.6: Downloaded firmware verified with SHA256 before caching
- [ ] §5.3.7: Polling errors logged but don't crash Hub (graceful degradation)

#### Story §5.4: Operator Manually Checks for Updates
**As a** system operator  
**I want** to click "Check for Updates" to immediately poll the server  
**So that** I don't have to wait up to 5 minutes after uploading new firmware

**Acceptance Criteria:**
- [ ] §5.4.1: "Check for Updates" button in dashboard action bar (matches existing button style)
- [ ] §5.4.2: Single touch triggers immediate poll (no double-tap or long-press)
- [ ] §5.4.3: Dashboard shows "Checking for updates..." loading indicator while polling
- [ ] §5.4.4: If updates available, dashboard event log shows "Updates available: Hub 1.2.0, Node 0.9.5" (yellow)
- [ ] §5.4.5: If no updates, dashboard shows "No updates available" (green)
- [ ] §5.4.6: If poll fails, dashboard shows "Update check failed: [error]" (red)

### Epic 3: Hub Self-Update

#### Story §5.5: Hub Self-Updates via ESP32 OTA
**As a** firmware developer  
**I want** the Hub to self-update without USB flashing  
**So that** I can deploy Hub firmware remotely

**Acceptance Criteria:**
- [ ] §5.5.1: Hub downloads Hub firmware from `/api/v1/firmware/hub/{version}` in 1KB chunks
- [ ] §5.5.2: Hub verifies SHA256 checksum before applying
- [ ] §5.5.3: Hub uses ESP32 dual-partition OTA (writes to inactive partition)
- [ ] §5.5.4: Dashboard shows "Downloading Hub firmware: 45%" (yellow, progress bar)
- [ ] §5.5.5: Dashboard shows "Verifying Hub firmware..." (yellow)
- [ ] §5.5.6: Dashboard shows "Installing Hub firmware..." (yellow)
- [ ] §5.5.7: Dashboard shows "Hub will reboot in 5 seconds" (yellow, countdown)
- [ ] §5.5.8: Hub reboots into new firmware partition
- [ ] §5.5.9: If new firmware boots successfully, mark partition as valid (no rollback)
- [ ] §5.5.10: If new firmware fails to boot (3 attempts), ESP32 rolls back to previous partition
- [ ] §5.5.11: After rollback, dashboard shows "Hub update failed - rolled back to v1.1.0" (red)

### Epic 4: Canary Rollout for Nodes

#### Story §5.6: Operator Triggers Canary Rollout
**As a** system operator  
**I want** to click "START OTA" to deploy Node firmware safely  
**So that** bad firmware is tested on 1 node before the entire fleet

**Acceptance Criteria:**
- [ ] §5.6.1: "START OTA" button in dashboard action bar (matches existing button style, single-touch)
- [ ] §5.6.2: Button selects first READY node as canary (based on node ID order)
- [ ] §5.6.3: If no READY nodes, dashboard shows "No nodes ready for OTA" (yellow)
- [ ] §5.6.4: Dashboard event log shows "Canary OTA started: Node 2" (yellow)
- [ ] §5.6.5: Node grid highlights canary node (yellow border or background)
- [ ] §5.6.6: Dashboard shows "Downloading to Node 2: 45%" (yellow, progress bar matching health panel style)
- [ ] §5.6.7: Dashboard shows "Verifying on Node 2..." (yellow)
- [ ] §5.6.8: Dashboard shows "Installing on Node 2..." (yellow)
- [ ] §5.6.9: Dashboard shows "Testing on Node 2 - waiting 30s" with countdown timer (yellow)

#### Story §5.7: Canary Verification and Fleet Rollout
**As a** system operator  
**I want** the system to verify canary success before updating remaining nodes  
**So that** I catch boot loops or crashes before they affect the entire fleet

**Acceptance Criteria:**
- [ ] §5.7.1: After Node 2 reboots, Hub waits 30 seconds (fixed) before continuing
- [ ] §5.7.2: Dashboard countdown timer decrements every second (30, 29, 28, ...)
- [ ] §5.7.3: After 30s, Hub checks Node 2 state is READY (connected, keepalives received)
- [ ] §5.7.4: If Node 2 is READY, dashboard shows "Canary succeeded - updating remaining nodes" (green)
- [ ] §5.7.5: Hub proceeds to update remaining READY nodes sequentially
- [ ] §5.7.6: Dashboard shows "Updating Node 4: 65%" (yellow) for each node
- [ ] §5.7.7: After all nodes complete, dashboard shows "Rollout complete - X nodes updated" (green)
- [ ] §5.7.8: If Node 2 is not READY after 30s (LOST or DEGRADED), dashboard shows "Canary failed - rollout aborted" (red)
- [ ] §5.7.9: If canary fails, remaining nodes are not updated (abort entire rollout)

### Epic 5: Rollout Interruption Handling

#### Story §5.8: Hub Reboot During Rollout
**As a** system operator  
**I want** rollouts to abort if Hub reboots  
**So that** I don't have inconsistent firmware versions across the fleet

**Acceptance Criteria:**
- [ ] §5.8.1: If Hub reboots during Node rollout, all rollout state in RAM is lost
- [ ] §5.8.2: After reboot, Hub does NOT attempt to resume rollout
- [ ] §5.8.3: Dashboard event log shows "Hub rebooted - rollout aborted" (yellow)
- [ ] §5.8.4: Operator must manually click "START OTA" to retry rollout
- [ ] §5.8.5: Nodes that already completed update remain on new firmware
- [ ] §5.8.6: Nodes that didn't complete remain on old firmware (operator can see version mismatch in node grid)

### Epic 6: Integration Testing

#### Story §5.9: Automated Integration Tests
**As a** firmware developer  
**I want** automated tests to validate the OTA pipeline  
**So that** I catch regressions before deployment

**Acceptance Criteria:**
- [ ] §5.9.1: Bash script `tools/test_integration.sh` tests OTA server REST API with curl
- [ ] §5.9.2: Script tests `GET /api/v1/versions` returns 200 with JSON array
- [ ] §5.9.3: Script tests `GET /api/v1/manifest/hub` returns valid manifest JSON
- [ ] §5.9.4: Script tests `GET /api/v1/firmware/hub/1.0.0` returns binary data
- [ ] §5.9.5: Script tests `POST /api/v1/firmware/upload` with mock .bin file
- [ ] §5.9.6: Node.js script `tools/test_ws_protocol.js` tests WebSocket handshake
- [ ] §5.9.7: Script simulates HELLO → WELCOME → AUTH → READY flow
- [ ] §5.9.8: Script validates OTA_UPDATE WebSocket message format
- [ ] §5.9.9: Script greps serial logs for "OTA_START", "OTA_COMPLETE" events
- [ ] §5.9.10: All tests exit with code 0 on success, non-zero on failure
- [ ] §5.9.11: Tests output colored status (green ✓ / red ✗) and generate timestamped log file

## §6. User Experience Contract
*→ Extracted into Blueprint §1.5/1.6 User Journey diagrams*

### §6.1 User Visibility Specification

| ID | User Action | User Sees (Exact) | User Does NOT See | Timing |
|----|-------------|-------------------|-------------------|--------|
| V-1 | Click "Check for Updates" | "Checking for updates..." loading spinner | HTTP GET request, JSON parsing, manifest comparison | Immediate |
| V-2 | Updates available | Event log: "Updates available: Hub 1.2.0, Node 0.9.5" (yellow) | Firmware file sizes, SHA256 checksums, internal version comparison logic | After 1-2s (network latency) |
| V-3 | No updates available | Event log: "No updates available" (green) | Manifest parsing, version string comparison | After 1-2s |
| V-4 | Update check fails | Event log: "Update check failed: Connection timeout" (red) | HTTP error codes, stack traces, retry logic | After 5-10s (timeout) |
| V-5 | Click "START OTA" | Event log: "Canary OTA started: Node 2" (yellow); Node 2 highlighted in grid | Canary selection algorithm, nodeIds_ vector manipulation, HubOtaDispatch state machine | Immediate |
| V-6 | Canary downloading | Progress bar: "Downloading to Node 2: 45%" (yellow, matching health panel style) | 1KB chunk downloads, HTTP stream buffer, mbedtls SHA256 context | Real-time updates every 32KB |
| V-7 | Canary verifying | "Verifying on Node 2..." (yellow) | SHA256 calculation, checksum comparison | 1-2s |
| V-8 | Canary installing | "Installing on Node 2..." (yellow) | Flash write operations, partition switching | 2-5s |
| V-9 | Canary verification wait | "Testing on Node 2 - waiting 30s" with countdown (29, 28, 27...) (yellow) | Keepalive message tracking, node state checks, timeout tracking | 30s countdown (updates every 1s) |
| V-10 | Canary success | Event log: "Canary succeeded - updating remaining nodes" (green) | NODE_STATE_READY check, canary_phase_ flag clear | After 30s countdown completes |
| V-11 | Canary failure | Event log: "Canary failed - rollout aborted" (red); Node 2 shows LOST/DEGRADED state | HubOtaDispatch::abort(), nodeIds_.clear(), state machine reset | After 30s countdown (if node not READY) |
| V-12 | Fleet rollout progress | Progress bars for each node: "Updating Node 4: 65%" (yellow) | processNextNode() calls, sequential iteration through nodeIds_ | Real-time updates per node |
| V-13 | Rollout complete | Event log: "Rollout complete - 8 nodes updated" (green); all nodes show green in grid | completedCount comparison, state = OTA_DISPATCH_COMPLETE | After last node completes |
| V-14 | Hub self-update download | Progress bar: "Downloading Hub firmware: 45%" (yellow) | HTTPClient buffer, LittleFS write operations | Real-time updates every 32KB |
| V-15 | Hub self-update verify | "Verifying Hub firmware..." (yellow) | mbedtls SHA256 calculation | 2-3s |
| V-16 | Hub self-update install | "Installing Hub firmware..." (yellow) | ESP32 Update.h partition writes | 5-10s |
| V-17 | Hub reboot countdown | "Hub will reboot in 5 seconds" (yellow, countdown: 5, 4, 3, 2, 1) | Filesystem sync, pending operations | 5s countdown |
| V-18 | Hub reboot | Screen goes blank, then reboots | ESP32 restart, bootloader, partition verification | 10-15s |
| V-19 | Hub update success | Dashboard reappears; event log: "Hub updated to v1.2.0" (green) | Partition marked valid, rollback disabled | After reboot |
| V-20 | Hub update failed (rollback) | Event log: "Hub update failed - rolled back to v1.1.0" (red) | ESP32 boot failure detection, automatic partition rollback | After 3 failed boot attempts (~30s) |
| V-21 | Automatic 5min poll (success) | Event log: "New Node firmware available: v0.9.5" (yellow) | Timer interrupt, HTTP GET, manifest parsing, LittleFS write | Background (no immediate UI change) |
| V-22 | Automatic 5min poll (failure) | No visible change (poll errors silent unless debug enabled) | HTTP timeout, JSON parse error, retry backoff | Background |

### §6.2 Timing & Feedback Expectations

| ID | Event | Expected Timing | User Feedback | Failure Feedback |
|----|-------|-----------------|---------------|------------------|
| T-1 | Click "Check for Updates" | < 2s (local network) | Loading indicator → "Updates available" (yellow) or "No updates" (green) | "Update check failed: [error]" (red) after 5-10s timeout |
| T-2 | Click "START OTA" | Immediate | "Canary OTA started: Node X" (yellow), node highlighted | "No nodes ready for OTA" (yellow) if no READY nodes |
| T-3 | Canary firmware download | 10-30s (depends on firmware size ~300KB) | Progress bar updates every 32KB chunk | "Download failed: [error]" (red), rollout aborted |
| T-4 | Canary firmware verify | 1-2s | "Verifying on Node X..." (yellow) | "Verification failed: checksum mismatch" (red), rollout aborted |
| T-5 | Canary firmware install | 2-5s | "Installing on Node X..." (yellow) | "Install failed: [error]" (red), rollout aborted |
| T-6 | Canary verification wait | Exactly 30s (fixed) | Countdown timer (30, 29, 28, ...) | "Canary failed - rollout aborted" (red) if node not READY after 30s |
| T-7 | Per-node rollout | 15-40s per node | Progress bar per node with real-time % updates | "Node X update failed: [error]" (red), rollout aborted |
| T-8 | Hub self-update download | 20-60s (depends on firmware size ~500KB) | Progress bar updates every 32KB chunk | "Hub download failed: [error]" (red), update aborted |
| T-9 | Hub self-update verify | 2-3s | "Verifying Hub firmware..." (yellow) | "Hub verification failed" (red), update aborted |
| T-10 | Hub self-update install | 5-10s | "Installing Hub firmware..." (yellow) | "Hub install failed" (red), update aborted |
| T-11 | Hub reboot countdown | Exactly 5s | Countdown (5, 4, 3, 2, 1) | N/A (countdown always runs if install succeeds) |
| T-12 | Hub reboot | 10-15s | Blank screen → Dashboard reappears | "Hub update failed - rolled back" (red) after 3 failed boots (~30s) |
| T-13 | Automatic 5min poll | ~100ms (local network HTTP GET) | Event log entry if updates found (yellow) | Silent failure (no user feedback unless debug enabled) |

## §7. Artifact Ownership
*→ Extracted into Blueprint §2 System Boundaries*

### §7.1 Creation Responsibility

| ID | Artifact | Created By | When | App's Role |
|----|----------|------------|------|------------|
| O-1 | OTA server firmware binaries (`.bin` files in `ota-server/firmware/{device}/`) | **Developer** (manual upload via web UI) | Developer uploads after PlatformIO build | **Observe** (Hub downloads when available) |
| O-2 | OTA server manifest JSON (`ota-server/firmware/{device}/manifest.json`) | **OTA Server** (auto-generated on upload) | Firmware upload triggers manifest update | **Observe** (Hub polls and parses) |
| O-3 | SHA256 checksums in manifest | **OTA Server** (auto-calculated on upload) | During firmware upload processing | **Observe** (Hub uses for verification) |
| O-4 | Hub LittleFS cached Node firmware (`/ota/node_vX.Y.Z.bin`) | **Hub** (downloads from OTA server) | After Hub polls and detects new Node firmware | **Create** (Hub downloads and caches) |
| O-5 | Hub LittleFS manifest (`/ota/manifest.json`) | **Hub** (initially manual upload; updated by Hub OTA client) | Hub OTA client updates after successful Node firmware cache | **Create** (Hub maintains local manifest) |
| O-6 | Hub OTA partition (inactive partition, e.g., `app1`) | **Hub** (writes downloaded Hub firmware) | Hub self-update triggered by operator or automatic poll | **Create** (Hub writes firmware to inactive partition) |
| O-7 | Node OTA partition (inactive partition) | **Node** (writes downloaded firmware from Hub) | Hub sends `ota_update` WebSocket message to Node | **Observe** (Hub monitors Node state transitions) |
| O-8 | Dashboard event log entries | **Hub** (HubRegistry callback triggers dashboard log) | Registry state changes, OTA events, user actions | **Create** (Hub generates log entries) |
| O-9 | Dashboard UI widgets (buttons, progress bars) | **Hub** (LVGL widget creation in `HubDashboard.cpp`) | Dashboard initialization at boot | **Create** (Hub creates and manages UI) |
| O-10 | Node registry entries (`node_entry_t` in `std::map`) | **Hub** (on HELLO message) | Node connects via WebSocket and sends HELLO | **Create** (Hub registers nodes in RAM) |

### §7.2 External System Dependencies

| ID | External System | What It Creates | How App Knows | Failure Handling |
|----|-----------------|-----------------|---------------|------------------|
| E-1 | **OTA Server (Node.js)** | Firmware binaries, manifest JSON, SHA256 checksums | Hub polls `/api/v1/manifest/:device` every 5min + manual "Check for Updates" | Graceful: Log error to serial, continue operation, retry next poll cycle |
| E-2 | **PlatformIO build system** | Compiled `.bin` files (developer's responsibility to upload) | Developer uploads via OTA server web UI | Manual: Developer must rebuild and re-upload if `.bin` is missing or corrupted |
| E-3 | **ESP32 OTA Update.h library** | Inactive partition write, rollback on boot failure | Hub calls `Update.begin()`, `Update.write()`, `Update.end()`, checks `Update.isFinished()` | Automatic rollback: ESP32 reboots to previous partition after 3 failed boots |
| E-4 | **Node WebSocket client** | OTA status messages (`{"state": "downloading", "pct": 45}`) | Hub receives WebSocket message, calls `HubOtaDispatch::onNodeOtaStatus()` | Timeout: If no status updates for 180s, abort node update and mark node as LOST |
| E-5 | **Node firmware update process** | Node reboots, reconnects with new firmware version | Hub observes WebSocket HELLO message with updated `fw` field | Canary failure: If node doesn't reconnect within 30s, abort entire rollout |

### §7.3 Derived Ownership Rules
*These become Blueprint §2.3 Boundary Rules*

| Source | Rule | Rationale |
|--------|------|-----------|
| O-1 | Hub must NOT create firmware binaries | Developer/PlatformIO creates them; Hub only downloads |
| O-2 | Hub must NOT modify OTA server manifest | OTA server owns manifest; Hub only reads |
| O-3 | Hub must NOT calculate SHA256 for OTA server uploads | OTA server calculates on upload; Hub only verifies downloaded firmware |
| O-4 | OTA server must NOT access Hub's LittleFS | Hub owns local filesystem; OTA server only serves files over HTTP |
| O-5 | Hub must wait for Node to report OTA status | Node owns its update process; Hub observes WebSocket messages |
| O-6 | Hub must NOT assume Node update succeeded without READY state | Node may fail to boot; Hub must verify via keepalive and state transition |
| E-1 | Hub must handle OTA server unavailability gracefully | Server may be down; Hub continues LED operation and retries later |
| E-3 | Hub must trust ESP32 OTA library for rollback | ESP32 handles boot failure detection; Hub cannot manually rollback |
| E-5 | Hub must abort rollout if canary doesn't reconnect | Node boot failure is undetectable except via absence of HELLO message |

## §8. State Requirements
*→ Extracted into Blueprint §3 State Transition Specifications*

### §8.1 State Isolation

| ID | Requirement | Rationale | Enforcement |
|----|-------------|-----------|-------------|
| SI-1 | Each Node's OTA state is isolated (per-node `ota_state`, `ota_pct`, `ota_version`, `ota_error` in `node_entry_t`) | Nodes update independently; one node's failure shouldn't corrupt another's state | `node_entry_t` struct in `std::map<uint8_t, node_entry_t>` with per-node keys |
| SI-2 | Rollout state is global (single `HubOtaDispatch` state machine, one rollout at a time) | Only one deployment should run at a time to avoid race conditions and conflicting firmware versions | `HubOtaDispatch::state_` is single instance; `startRollout()` checks `state_ == OTA_DISPATCH_IDLE` before proceeding |
| SI-3 | Hub OTA state is separate from Node OTA state | Hub self-update should not interfere with Node rollout tracking | Hub OTA handled by `HubOtaClient` instance; Node OTA handled by `HubOtaDispatch` instance |
| SI-4 | Dashboard event log is append-only ring buffer (20 entries max) | Old events discarded to prevent memory growth; new events always visible | `log_entries_[MAX_LOG_ENTRIES]` with `log_head_` and `log_count_` tracking |
| SI-5 | OTA server state is per-device (Hub manifest separate from Node manifest) | Hub and Node firmware are independent; different versions, sizes, checksums | `ota-server/firmware/hub/manifest.json` vs `ota-server/firmware/node/manifest.json` |

### §8.2 State Lifecycle

| ID | State | Initial | Created When | Cleared When | Persists Across |
|----|-------|---------|--------------|--------------|------------------|
| SL-1 | `HubOtaDispatch::state_` | `OTA_DISPATCH_IDLE` | `startRollout()` called → `OTA_DISPATCH_IN_PROGRESS` | `processNextNode()` completes last node → `OTA_DISPATCH_COMPLETE`, OR `abort()` → `OTA_DISPATCH_ABORTED` | RAM only (lost on Hub reboot) |
| SL-2 | `HubOtaDispatch::nodeIds_` | Empty vector | `startRollout(nodeIds)` populates vector | `abort()` or `processNextNode()` completion → `nodeIds_.clear()` | RAM only (lost on Hub reboot) |
| SL-3 | `HubOtaDispatch::currentNodeId_` | 0 | `processNextNode()` assigns canary node ID | `abort()` → 0 | RAM only (lost on Hub reboot) |
| SL-4 | `HubOtaDispatch::completedCount_` | 0 | `startRollout()` initializes to 0 | `tick()` increments on node READY; `abort()` resets to 0 | RAM only (lost on Hub reboot) |
| SL-5 | `node_entry_t::ota_state` | "idle" | Hub sends `ota_update` WS message → Node reports "downloading" | Node reports "complete" OR "error" → reset to "idle" | RAM only (lost on Hub reboot) |
| SL-6 | `node_entry_t::ota_pct` | 0 | Node reports download progress | Node reports "complete" OR "error" → reset to 0 | RAM only (lost on Hub reboot) |
| SL-7 | `node_entry_t::ota_version` | "" | Node reports OTA start with version string | Node reconnects with new `fw` version → cleared | RAM only (lost on Hub reboot) |
| SL-8 | `node_entry_t::ota_error` | "" | Node reports "error" state with error message | Next OTA attempt → cleared | RAM only (lost on Hub reboot) |
| SL-9 | Dashboard log entries (`log_entries_[]`) | Empty (all zeroed) | Registry event callback or OTA event → `addLogEntry()` | Never explicitly cleared; oldest entries overwritten by ring buffer | RAM only (lost on Hub reboot) |
| SL-10 | Hub LittleFS cached Node firmware (`/ota/node_vX.Y.Z.bin`) | Absent (initially empty) | Hub downloads from OTA server after poll detects new version | Manual deletion via serial command OR filesystem format | Flash (persists across reboots) |
| SL-11 | Hub LittleFS manifest (`/ota/manifest.json`) | Initially manual upload | Hub updates after successful Node firmware cache | Manual deletion via serial command OR filesystem format | Flash (persists across reboots) |
| SL-12 | OTA server firmware binaries (`ota-server/firmware/{device}/vX.Y.Z.bin`) | Absent | Developer uploads via web UI | Never (keep all versions forever per Q20) | Disk (persists indefinitely) |
| SL-13 | OTA server manifest JSON (`ota-server/firmware/{device}/manifest.json`) | Empty array or initial entry | First firmware upload creates/updates | Never (keep all versions forever per Q20) | Disk (persists indefinitely) |

## §9. Technical Considerations
*→ Informs Blueprint §5, §6, §9, §11*

### §9.1 Architecture Decisions

**Decision 1: External OTA server instead of Hub self-hosting**
- **Rationale:** Hub cannot host its own firmware updates (can't update itself while serving files). External server solves "who updates the Hub" problem.
- **Trade-offs:** Requires additional server process, but enables Hub self-update and centralized version control.
- **Implementation:** Node.js Express server on `192.168.x.x:8080` with REST API and web UI.

**Decision 2: Dual-partition ESP32 OTA for Hub self-update**
- **Rationale:** ESP32 hardware supports dual-partition OTA with automatic rollback on boot failure.
- **Trade-offs:** Requires 2x firmware space in flash, but provides safety net for Hub updates.
- **Implementation:** Use Arduino `Update.h` library with `app0`/`app1` partitions; ESP32 bootloader handles rollback.

**Decision 3: Abort-on-reboot for rollout state**
- **Rationale:** Simplifies implementation; avoids complex LittleFS state persistence and recovery logic.
- **Trade-offs:** Operator must manually restart rollout after Hub reboot, but prevents corrupted partial deployments.
- **Implementation:** All rollout state (`nodeIds_`, `completedCount_`, etc.) stored in RAM; lost on reboot.

**Decision 4: Canary rollout with fixed 30s verification**
- **Rationale:** Mitigates risk of bad Node firmware; fixed timing is simpler than configurable.
- **Trade-offs:** 30s may be too short for some bugs or too long for safe updates, but is a reasonable default.
- **Implementation:** `HubOtaDispatch` updates canary node, waits 30s, checks `NODE_STATE_READY`, then proceeds or aborts.

**Decision 5: Automatic SHA256 calculation on OTA server upload**
- **Rationale:** Eliminates human error; ensures checksums always match uploaded firmware.
- **Trade-offs:** Slightly slower uploads (hash calculation), but prevents bricked devices from bad checksums.
- **Implementation:** Node.js `crypto` module calculates SHA256 during file upload, stores in manifest.

**Decision 6: JSON manifests instead of database**
- **Rationale:** Simpler for local development; human-editable; matches Hub's existing LittleFS manifest format.
- **Trade-offs:** Less powerful querying, but sufficient for firmware version tracking.
- **Implementation:** `ota-server/firmware/hub/manifest.json` and `node/manifest.json` with version/size/SHA256.

**Decision 7: Keep all firmware versions forever**
- **Rationale:** Disk space is cheap; full rollback capability is valuable.
- **Trade-offs:** Disk usage grows unbounded, but firmware files are small (~300-500KB).
- **Implementation:** No auto-deletion logic; developer can manually delete old versions if needed.

**Decision 8: 1KB download chunks**
- **Rationale:** Proven safe on ESP32-S3 Nodes with 384KB SRAM; Hub has more RAM (704KB) so 1KB is conservative.
- **Trade-offs:** Slower downloads than larger chunks, but safe memory usage.
- **Implementation:** `HTTPClient` reads 1KB at a time, writes to flash, updates progress.

**Decision 9: Fixed 5-minute polling interval**
- **Rationale:** Fast update detection without configuration complexity; negligible overhead on local network.
- **Trade-offs:** Uses ~288KB/day bandwidth (very small), but provides near-instant update awareness.
- **Implementation:** FreeRTOS timer on Core 0 triggers HTTP GET every 5 minutes.

**Decision 10: Prioritize small API requests over firmware downloads**
- **Rationale:** Keeps dashboard responsive during large downloads.
- **Trade-offs:** Firmware downloads may take slightly longer, but UX is better.
- **Implementation:** AsyncWebServer with lower priority for `/api/v1/firmware/*` routes.

### §9.2 Integration Points

**Integration 1: OTA Server ↔ Hub OTA Client**
- **Protocol:** HTTP REST API at `http://192.168.x.x:8080/api/v1/*`
- **Endpoints:** `GET /manifest/:device`, `GET /firmware/:device/:version`, `POST /firmware/upload`
- **Data format:** JSON manifests with `{version, url, sha256, size}` fields
- **Error handling:** Hub logs errors, retries on next poll cycle; graceful degradation

**Integration 2: Hub OTA Client ↔ Hub LittleFS**
- **Storage:** Downloaded Node firmware cached in `/ota/node_vX.Y.Z.bin`
- **Manifest:** `/ota/manifest.json` updated after successful cache
- **Verification:** SHA256 checksum verified before writing to LittleFS

**Integration 3: Hub OTA Dispatch ↔ Node WebSocket**
- **Protocol:** WebSocket messages: `{"type": "ota_update", "version": "...", "url": "...", "sha256": "..."}`
- **Status reporting:** Node sends `{"type": "ota_status", "state": "...", "pct": ...}` messages
- **State tracking:** Hub updates `node_entry_t::ota_state`, `ota_pct`, `ota_version`, `ota_error`

**Integration 4: Hub OTA Dispatch ↔ Hub Registry**
- **State monitoring:** Dispatch checks `registry_->getNode(nodeId)->state` for `NODE_STATE_READY`
- **Event callbacks:** Registry calls `setOtaState()` on Node status updates
- **Timeout handling:** Registry marks nodes as `LOST` after keepalive timeout

**Integration 5: Hub OTA ↔ Dashboard**
- **Event logging:** `HubMain::onRegistryEvent()` calls `dashboard_.logEvent()` for all OTA events
- **Button callbacks:** `btn_ota_cb` triggers `otaDispatch_->startRollout()`, `btn_refresh_cb` triggers `otaClient_->checkForUpdates()`
- **Progress display:** Dashboard polls `otaDispatch_->getProgress()` and `registry_->getNode()->ota_pct` for real-time updates

**Integration 6: Hub Self-Update ↔ ESP32 OTA**
- **Library:** Arduino `Update.h` with `esp_ota_*` functions
- **Partitions:** Dual app partitions (`app0`, `app1`) configured in partition table
- **Rollback:** ESP32 bootloader automatically rolls back after 3 failed boots

### §9.3 Constraints

**Hardware Constraints:**
- **Hub:** ESP32-P4, 704KB SRAM (limit download chunk sizes), 4MB PSRAM, LittleFS for storage
- **Nodes:** ESP32-S3, 384KB SRAM (proven 1KB chunks safe), WebSocket client only (no HTTP server)
- **Network:** Local WiFi, assume low latency (<100ms), no internet access required

**Software Constraints:**
- **FreeRTOS:** Hub runs dual-core; network on Core 0, rendering on Core 1 (don't block rendering)
- **LVGL:** Dashboard UI runs on Core 1; all UI updates must use LVGL API (not thread-safe across cores)
- **AsyncWebServer:** Single-threaded event loop; avoid blocking operations in request handlers
- **LittleFS:** Limited space (~1-2MB); limit cached firmware versions if space becomes issue

**Performance Constraints:**
- **LED rendering:** 120 FPS target; polling/downloads must not block Core 1
- **Dashboard responsiveness:** Small API requests (<100ms) prioritized over large downloads
- **OTA download speed:** ~300KB firmware takes 10-30s with 1KB chunks and SHA256 verification

**Safety Constraints:**
- **SHA256 mandatory:** All firmware verified before applying; reject mismatches
- **Canary mandatory:** Always test on 1 node before fleet-wide deployment
- **Rollback mandatory:** Hub must use dual-partition OTA; never overwrite only partition
- **Abort on canary failure:** Never proceed with rollout if canary doesn't reach READY state

## §10. Success Metrics
*→ Informs Blueprint §10 Testing Strategy*

| ID | Metric | Target | Measurement Method |
|----|--------|--------|--------------------|
| M-1 | **Hub self-update success rate** | >95% | Log Hub self-update attempts vs. successes over 100 deployments |
| M-2 | **Canary detection rate** | 100% | Canary failures must abort 100% of rollouts before fleet-wide deployment |
| M-3 | **Node update success rate** (after canary passes) | >98% | Log Node update attempts vs. successes over 1000 node updates |
| M-4 | **Rollback success rate** (Hub boot failure) | 100% | Intentionally deploy bad Hub firmware; verify ESP32 rolls back to previous partition |
| M-5 | **SHA256 mismatch detection** | 100% | Intentionally corrupt firmware file; verify Hub/Node reject with checksum error |
| M-6 | **Update detection latency** | <5min (automatic), <2s (manual) | Measure time from firmware upload to Hub detecting update |
| M-7 | **Dashboard responsiveness during downloads** | <200ms API response time | Measure `/api/v1/nodes` response time while firmware downloading in background |
| M-8 | **Zero Hub crashes during self-update** | 0 crashes/100 updates | Monitor Hub crash logs during self-update process |
| M-9 | **Zero partial deployments** (inconsistent firmware versions) | 0 occurrences | After rollout abort, verify all nodes either on old version or new version (no mix) |
| M-10 | **Integration test pass rate** | 100% | Run `tools/test_integration.sh` and `tools/test_ws_protocol.js` before every release |

---

## Appendix A: Answer Traceability

| User Answer (Summary) | Captured In |
|-----------------------|-------------|
| Q1: Local network accessible (192.168.x.x:8080) | §4.1 FR-1, §9.1 Decision 1 |
| Q2: Automatic SHA256 calculation | §4.1 FR-3, §9.1 Decision 5 |
| Q3: Abort and start fresh (after analysis) | §4.1 FR-16, §5.8, §9.1 Decision 3 |
| Q4: Manual firmware upload via web UI | §4.1 FR-5, §5.2, §7.1 O-1 |
| Q5: Keep /api/v1/ URL pattern consistent | §4.1 FR-2, §4.2 NFR-8 |
| Q6: Same error format as Hub | §4.1 FR-6, §4.2 NFR-8 |
| Q7: No authentication (open like Hub API) | §4.1 FR-7 |
| Q8: Store metadata in JSON files | §4.1 FR-4, §9.1 Decision 6 |
| Q9: Show detailed progress | §4.1 FR-18, §6.1 V-6, V-7, V-8, §6.2 T-3, T-4, T-5 |
| Q10: Show countdown during canary wait | §4.1 FR-19, §5.7, §6.1 V-9, §6.2 T-6 |
| Q11: Abort rollout if Hub reboots | §4.1 FR-16, §5.8, §8.2 SL-1 to SL-4 |
| Q12: Show loading indicator "Checking for updates..." | §4.1 FR-22, §6.1 V-1, §6.2 T-1 |
| Q13: Keep same button style/colors | §4.1 FR-21, §4.2 NFR-9 |
| Q14: Use existing color scheme (Green/Yellow/Red) | §4.1 FR-20, §4.2 NFR-9 |
| Q15: Single touch triggers action | §4.1 FR-17, §4.2 NFR-10 |
| Q16: Use same bars/labels style | §4.1 FR-21, §4.2 NFR-9 |
| Q17: Same folder structure and manifest format | §4.1 FR-4, §7.1 O-2, §9.1 Decision 6 |
| Q18: Use JSON files like Hub | §4.1 FR-4, §4.2 NFR-13, §9.1 Decision 6 |
| Q19: Auto-calculate SHA256 on upload | §4.1 FR-3, FR-19, §7.1 O-3, §9.1 Decision 5 |
| Q20: Keep all versions forever | §4.1 FR-8, §8.2 SL-12, SL-13, §9.1 Decision 7 |
| Q21: Every 5 minutes fixed polling | §4.1 FR-9, §4.2 NFR-1, §9.1 Decision 9 |
| Q22: Use same 1KB chunks as NodeOtaClient | §4.1 FR-11, §4.2 NFR-2, §9.1 Decision 8 |
| Q23: Fixed 30 seconds canary wait | §4.1 FR-14, §9.1 Decision 4 |
| Q24: Prioritize small API requests | §4.1 FR-23, §4.2 NFR-3, NFR-12, §9.1 Decision 10 |

**Validation:** ✅ All 24 user answers have been captured across multiple sections. No information lost.

---

*End of Product Requirements Document*
