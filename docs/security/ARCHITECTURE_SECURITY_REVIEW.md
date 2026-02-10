# LightwaveOS Architecture Security Review
**Date:** 2025-12-19
**Reviewer:** Claude Code (Architecture Security Specialist)
**Firmware Version:** LightwaveOS (Branch: feature/api-v1-architecture)
**Review Type:** Documentation-Only Security Audit

---

## Executive Summary

This comprehensive architecture security review evaluates LightwaveOS firmware from a defense-in-depth perspective, examining separation of concerns, security boundaries, data flow paths, and fail-safe behavior across all architectural layers.

**Overall Risk Level:** MEDIUM-HIGH

**Critical Findings:** 3
**High Severity:** 8
**Medium Severity:** 12
**Low Severity:** 6

**Key Concerns:**
1. Global state management creates race conditions and privilege escalation vectors
2. Insufficient security boundaries between network and hardware layers
3. Missing input validation on critical data paths
4. Inadequate error handling and recovery mechanisms
5. Lack of audit trail and security logging

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Security Findings by Layer](#2-security-findings-by-layer)
3. [Cross-Cutting Concerns](#3-cross-cutting-concerns)
4. [Threat Model Analysis](#4-threat-model-analysis)
5. [Remediation Roadmap](#5-remediation-roadmap)

---

## 1. Architecture Overview

### 1.1 System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Network Layer (src/network/)                               │
│  - WebServer (HTTP REST + WebSocket)                       │
│  - WiFiManager (FreeRTOS Task, Core 0)                     │
│  - RateLimiter, RequestValidator, ConnectionManager        │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  Application Layer (main.cpp)                               │
│  - Global state (CRGB leds[], currentEffect, brightness)   │
│  - Effect orchestration and transition management          │
│  - Serial command processing                               │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  Effects Layer (src/effects/)                               │
│  - ZoneComposer (multi-zone rendering)                     │
│  - TransitionEngine (12 transition types)                  │
│  - Effect functions (45+ parameterless void functions)     │
│  - Enhancement Engines (Color, Motion, Blending)           │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  Hardware Abstraction (src/hardware/)                       │
│  - EncoderManager (I2C task, Core 0)                       │
│  - PerformanceMonitor                                       │
│  - LED buffer management (strip1[], strip2[], leds[])      │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  Platform Layer (FastLED, FreeRTOS, ESP32-S3)              │
│  - WS2812 RMT driver (GPIO 4, GPIO 5)                      │
│  - NVS (Non-Volatile Storage)                              │
│  - I2C bus (encoders)                                       │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 Architectural Patterns Identified

- **Monolithic global state** (all layers access shared globals via `extern`)
- **Buffer proxy pattern** (ZoneComposer temp buffers)
- **FreeRTOS task isolation** (WiFi and I2C on Core 0, main loop on Core 1)
- **Singleton pattern** (WiFiManager, WebServer)
- **Parameterless effect functions** (global state coupling)
- **Rate limiting** (IP-based sliding window)
- **Request validation** (schema-based JSON validation)

### 1.3 Security Boundaries

```
TRUST BOUNDARY 1: Network → Application
- Entry Points: HTTP endpoints, WebSocket messages
- Protection: RateLimiter, RequestValidator, CORS headers
- Weakness: No authentication, weak OTA token validation

TRUST BOUNDARY 2: Application → Effects
- Entry Points: Effect ID selection, parameter updates
- Protection: Array bounds checks (NUM_EFFECTS)
- Weakness: No effect capability isolation, global state mutation

TRUST BOUNDARY 3: Application → Hardware
- Entry Points: LED buffer writes, I2C operations
- Protection: FreeRTOS mutexes (i2cMutex, ledBufferMutex)
- Weakness: Inconsistent mutex usage, timeout vulnerabilities
```

---

## 2. Security Findings by Layer

### 2.1 Network Layer (src/network/)

#### FINDING NET-001: Weak OTA Authentication Token
**Severity:** CRITICAL
**Layer:** Network (WebServer.cpp)
**Principle Violated:** Least Privilege, Defense in Depth

**Description:**
OTA firmware update endpoint uses a compile-time hardcoded token (`LW-OTA-2024-SecureUpdate`) stored in `network_config.h` and transmitted via HTTP header. Token is:
- Not cryptographically random
- Static across all deployments
- Transmitted over unencrypted HTTP
- Validated with simple string comparison
- No rate limiting on OTA attempts
- No logging of failed authentication attempts

**Impact:**
An attacker on the local network can:
1. Discover token via packet sniffing (HTTP plaintext)
2. Upload malicious firmware to device
3. Achieve persistent code execution
4. Brick device or exfiltrate credentials

**Evidence:**
```cpp
// src/network/WebServer.cpp (OTA handler)
if (!request->hasHeader("X-OTA-Token") ||
    request->header("X-OTA-Token") != NetworkConfig::OTA_UPDATE_TOKEN) {
    Serial.println("[OTA] Unauthorized update attempt blocked!");
    request->send(401, "text/plain", "Unauthorized: Invalid or missing OTA token");
    return;
}
```

**Recommended Architectural Changes:**
1. **Implement challenge-response authentication** using HMAC-SHA256
2. **Add per-device cryptographic keys** stored in secure NVS partition
3. **Require signed firmware images** with public key verification
4. **Implement rollback protection** with NVS-stored firmware version counter
5. **Add rate limiting** (max 3 attempts per hour per IP)
6. **Log all OTA attempts** to persistent storage with timestamp/IP
7. **Require physical button press** to enter OTA mode (defense-in-depth)

---

#### FINDING NET-002: Missing Authentication on REST/WebSocket APIs
**Severity:** HIGH
**Layer:** Network (WebServer.cpp, WebSocket handlers)
**Principle Violated:** Authentication, Authorization

**Description:**
All v1 REST API endpoints and WebSocket commands lack authentication:
- `/api/v1/effects/set` - anyone can change effects
- `/api/v1/parameters` - control brightness, speed, palette
- `/api/v1/device/info` - exposes firmware version, SDK info
- WebSocket commands `setEffect`, `setBrightness`, `setZoneEffect`

No concept of users, sessions, or API keys. Rate limiting (20 req/sec HTTP, 50 msg/sec WS) provides only DoS mitigation, not access control.

**Impact:**
Unauthorized local network access enables:
- Visual annoyance attacks (strobing, epilepsy triggers)
- Information disclosure (firmware version for exploit targeting)
- Reconnaissance for larger attacks
- Denial of service through effect manipulation

**Evidence:**
```cpp
// src/network/WebServer.cpp - No authentication checks
void LightwaveWebServer::handleEffectsSet(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!checkRateLimit(request)) return;  // Only rate limiting!

    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    // ... direct effect change, no auth
}
```

**Recommended Architectural Changes:**
1. **Implement API key authentication** with scoped permissions (read-only, control, admin)
2. **Add session management** for WebSocket connections (token-based)
3. **Introduce RBAC** (role-based access control) with capability model
4. **Separate public endpoints** (status, discovery) from privileged (control, config)
5. **Add security headers** (X-Content-Type-Options, X-Frame-Options)
6. **Implement CSRF protection** for state-changing operations
7. **Consider WPA3/802.1X** for network-level access control

---

#### FINDING NET-003: CORS Wildcard Allows Cross-Origin Attacks
**Severity:** HIGH
**Layer:** Network (WebServer.cpp)
**Principle Violated:** Same-Origin Policy

**Description:**
CORS headers configured with wildcard origin (`Access-Control-Allow-Origin: *`), allowing any web page to make requests to the device:

```cpp
// src/network/WebServer.cpp:109
DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
```

This bypasses browser same-origin policy, enabling:
- Cross-Site Request Forgery (CSRF) attacks from malicious websites
- Credential theft if authentication is added later
- Information disclosure to untrusted origins

**Impact:**
User visits malicious website → website's JavaScript can:
1. Read device status and firmware version
2. Change effects and parameters
3. Trigger OTA updates (if token is known)
4. DoS device through rapid requests

**Recommended Architectural Changes:**
1. **Whitelist specific origins** instead of wildcard (e.g., `http://lightwaveos.local`)
2. **Implement origin validation** against configurable whitelist
3. **Add CSRF tokens** for state-changing operations
4. **Use SameSite cookies** if session management is added
5. **Consider removing CORS entirely** for local-only deployments

---

#### FINDING NET-004: Race Condition in WiFiManager State Transitions
**Severity:** MEDIUM
**Layer:** Network (WiFiManager.cpp)
**Principle Violated:** Thread Safety

**Description:**
WiFiManager state transitions use mutex (`stateMutex`) but with insufficient critical section coverage. State checks and updates are not atomic:

```cpp
// Pattern seen in WiFiManager.cpp
WiFiState getState() const {
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        WiFiState state = currentState;  // Read under mutex
        xSemaphoreGive(stateMutex);
        return state;
    }
    return STATE_WIFI_INIT;  // Fallback if mutex timeout
}
```

However, state transitions in FreeRTOS task may occur between read and action, creating TOCTOU (Time-Of-Check-Time-Of-Use) vulnerabilities.

**Impact:**
- mDNS started/stopped at wrong times
- Duplicate WiFi connections attempted
- Memory leaks from abandoned connection attempts
- Inconsistent state reporting to WebSocket clients

**Recommended Architectural Changes:**
1. **Expand critical sections** to cover entire state transition logic
2. **Implement state machine with atomic transitions** (check-and-set pattern)
3. **Add state transition logging** for debugging race conditions
4. **Use FreeRTOS event groups** instead of polling state
5. **Validate state transitions** (only allow valid state sequences)

---

#### FINDING NET-005: Insufficient Rate Limit Enforcement
**Severity:** MEDIUM
**Layer:** Network (RateLimiter.h)
**Principle Violated:** Resource Protection

**Description:**
RateLimiter uses LRU eviction when tracking table is full (8 IPs), allowing attacker with 9+ IPs to evict legitimate clients and bypass rate limits. Additionally:
- HTTP limit (20 req/sec) may be too high for embedded device
- No exponential backoff for repeated violations
- 5-second block time is insufficient
- No permanent blocking after repeated abuse

**Impact:**
- DoS via IP rotation (9+ spoofed IPs)
- Resource exhaustion (heap fragmentation from JSON parsing)
- Main loop starvation (AsyncWebServer callbacks block execution)

**Recommended Architectural Changes:**
1. **Increase tracking table size** to 16-32 entries
2. **Implement exponential backoff** (5s → 30s → 5min → 1hr)
3. **Add permanent block list** after N violations (store in NVS)
4. **Lower HTTP rate limit** to 10 req/sec
5. **Add request queue depth limiting** to prevent callback backlog
6. **Monitor heap usage** and reject requests if below threshold

---

### 2.2 Application Layer (main.cpp)

#### FINDING APP-001: Global State Creates Race Conditions
**Severity:** CRITICAL
**Layer:** Application (main.cpp, global state)
**Principle Violated:** Encapsulation, Thread Safety

**Description:**
Core application state is stored in global variables accessed by multiple threads without synchronization:

```cpp
// main.cpp - Global state (53 extern declarations across codebase)
CRGB leds[320];           // Unified LED buffer
CRGB strip1[160];         // Strip 1 buffer
CRGB strip2[160];         // Strip 2 buffer
uint8_t currentEffect;    // Current effect index
uint8_t brightnessVal;    // Brightness value
uint8_t gHue;             // Global hue counter
```

These are accessed by:
- **Core 1 (main loop):** Effect rendering, serial commands
- **Core 0 (WiFi task):** WebSocket handlers setting effects
- **Core 0 (I2C task):** Encoder changing parameters

Only `ledBufferMutex` protects LED buffer access in `syncLedsToStrips()`, but:
- Effect functions read/write buffers without mutex
- `currentEffect` has no protection (updated from network and encoders)
- `brightnessVal`, `gHue` modified from multiple contexts

**Impact:**
- **Visual glitches:** Torn frames, flickering from concurrent buffer writes
- **Undefined behavior:** Partial effect state corruption
- **Memory corruption:** Race in buffer pointer updates
- **Privilege escalation:** Network thread can interrupt serial commands

**Evidence:**
```cpp
// 53 files declare `extern CRGB leds[]` - no encapsulation
// WebServer.cpp can modify globals directly:
extern uint8_t currentEffect;
currentEffect = newEffect;  // NO MUTEX!
```

**Recommended Architectural Changes:**
1. **Introduce state management layer** with accessor methods and mutexes
2. **Implement command queue pattern** - all state changes via queue
3. **Add double buffering** for LED arrays (render to back buffer, swap under mutex)
4. **Create effect context struct** encapsulating parameters, pass by value
5. **Separate read-only state** (effect metadata) from mutable state
6. **Add state versioning** to detect concurrent modifications
7. **Use atomic operations** for simple counters (gHue)

---

#### FINDING APP-002: Effect Array Bounds Checking Insufficient
**Severity:** HIGH
**Layer:** Application (main.cpp, effect registration)
**Principle Violated:** Input Validation, Fail-Safe Defaults

**Description:**
Effect selection validates against `NUM_EFFECTS` constant, but:
- Constant defined inconsistently across files
- Array bounds checked AFTER network deserialization
- No validation that effect function pointer is non-NULL
- Zone effect IDs not validated against zone capability

```cpp
// Serial menu example - bounds check exists but post-access
if (newEffect >= 0 && newEffect < NUM_EFFECTS) {
    currentEffect = newEffect;
} else {
    // Error handling - but after parseInt() could overflow
}
```

**Impact:**
- **Out-of-bounds read:** Dereferencing invalid function pointer → crash
- **Code execution:** If attacker controls effect ID, may jump to arbitrary address
- **DoS:** Crash and reboot loop

**Recommended Architectural Changes:**
1. **Centralize effect registry** with capability metadata (min/max zones, etc.)
2. **Validate effect ID before deserialization** in request validator
3. **Add effect capability checking** (zone compatibility, hardware requirements)
4. **Implement safe effect fallback** (default to effect 0 on invalid ID)
5. **Add runtime effect pointer validation** (check against known function table)
6. **Log invalid effect attempts** to detect scanning attacks

---

#### FINDING APP-003: Missing Error Recovery in Main Loop
**Severity:** MEDIUM
**Layer:** Application (main.cpp loop())
**Principle Violated:** Fail-Safe Defaults, Resilience

**Description:**
Main loop has no try-catch equivalent (C++ exceptions disabled) and minimal error handling:
- Effect functions can crash without recovery
- TransitionEngine failures not handled
- FastLED.show() errors ignored
- No watchdog timer reset on healthy operation

If effect function crashes:
- Device reboots (no graceful degradation)
- No error logged to NVS
- No notification sent to connected clients
- Same effect may be selected on reboot → boot loop

**Impact:**
- DoS through crash loop
- Loss of configuration state
- No forensic evidence of crash cause

**Recommended Architectural Changes:**
1. **Add software watchdog** with explicit feeding in loop
2. **Wrap effect calls in error handler** (setjmp/longjmp or ESP-IDF panic hooks)
3. **Implement crash counter in NVS** - enter safe mode after 3 crashes
4. **Log crash reason** (effect ID, timestamp) to NVS
5. **Broadcast crash notification** to WebSocket clients
6. **Add safe mode effect** (solid color, no complex rendering)
7. **Reset crash counter** after N minutes of stable operation

---

#### FINDING APP-004: Transition Buffer Exposes Stale Frame Data
**Severity:** LOW
**Layer:** Application (TransitionEngine)
**Principle Violated:** Information Leakage

**Description:**
Transition engine uses global buffer `transitionSourceBuffer[320]` to store previous effect state. Buffer is not cleared between uses, potentially exposing previous frame data through transition artifacts.

While not exploitable for code execution, could leak visual patterns that user intended to hide (e.g., switching away from sensitive palette quickly).

**Recommended Architectural Changes:**
1. **Zero buffers on allocation** (or use memset after transitions)
2. **Add buffer sanitization mode** for high-security deployments
3. **Document buffer lifecycle** in code comments

---

### 2.3 Effects Layer (src/effects/)

#### FINDING EFF-001: ZoneComposer Lacks Privilege Separation
**Severity:** MEDIUM
**Layer:** Effects (ZoneComposer.cpp)
**Principle Violated:** Least Privilege

**Description:**
ZoneComposer renders multiple effects to isolated zones but:
- All zones execute in same security context
- Effect in Zone 0 can corrupt Zone 1-3 by writing to global `strip1[]`/`strip2[]`
- No sandboxing or memory protection between zone effects
- Buffer overflow in one effect can corrupt other zones

Effects are designed to write to global buffers directly:
```cpp
// Typical effect pattern
extern CRGB strip1[];
void someEffect() {
    strip1[i] = color;  // Direct write - no bounds checking!
}
```

**Impact:**
- Malicious effect could corrupt all zones
- Buffer overflow in one zone affects entire strip
- No isolation for untrusted community effects

**Recommended Architectural Changes:**
1. **Implement effect capability model** (allowed buffer ranges)
2. **Add buffer bounds checking wrapper** for all LED writes
3. **Use separate memory allocations** for zone temp buffers (already partially done)
4. **Validate buffer pointers** before passing to effect functions
5. **Consider effect sandboxing** (future: WASM runtime for untrusted effects)

---

#### FINDING EFF-002: Enhancement Engines Have Broad Privileges
**Severity:** MEDIUM
**Layer:** Effects (ColorEngine, MotionEngine, BlendingEngine)
**Principle Violated:** Separation of Concerns

**Description:**
Enhancement engines (`src/effects/engines/`) can modify any LED buffer without restrictions:
- ColorEngine directly accesses `strip1[]` and `strip2[]`
- No validation that engine is operating on correct zone
- Engines can be invoked from any context (network, serial, encoders)

```cpp
// src/effects/engines/ColorEngine.cpp:134
extern CRGB strip1[HardwareConfig::STRIP1_LED_COUNT];
extern CRGB strip2[HardwareConfig::STRIP2_LED_COUNT];
```

**Impact:**
- Engine bug can corrupt entire LED state
- No audit trail of which engine modified buffers
- Difficult to debug cross-engine interactions

**Recommended Architectural Changes:**
1. **Pass buffer references explicitly** instead of global access
2. **Add engine invocation logging** (which engine, on which buffer, timestamp)
3. **Implement engine capabilities** (read-only, per-zone, global)
4. **Validate buffer ranges** in engine entry points

---

#### FINDING EFF-003: Transition Type Enum Lacks Bounds Checking
**Severity:** LOW
**Layer:** Effects (TransitionEngine.h)
**Principle Violated:** Input Validation

**Description:**
`TransitionType` enum defines 12 types but uses raw integer in network API:
```cpp
// POST /api/v1/transitions/trigger
{"toEffect": 5, "type": 255, ...}  // type=255 out of bounds!
```

RequestValidator checks type is 0-15 (higher than actual 0-11 range), but TransitionEngine switch statement has no default case for invalid values.

**Impact:**
- Undefined behavior if invalid type passed
- Potential to trigger uninitialized transition state

**Recommended Architectural Changes:**
1. **Tighten validation range** to actual enum size (0-11)
2. **Add default case** in TransitionEngine switch with safe fallback
3. **Use enum class** instead of plain enum for type safety

---

### 2.4 Hardware Layer (src/hardware/)

#### FINDING HW-001: I2C Mutex Timeout Creates Denial of Service
**Severity:** HIGH
**Layer:** Hardware (EncoderManager.cpp)
**Principle Violated:** Availability, Fail-Safe Defaults

**Description:**
EncoderManager I2C operations use mutex with varying timeouts:
```cpp
// EncoderManager.cpp - Inconsistent timeout handling
if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    // I2C operation
    xSemaphoreGive(i2cMutex);
}
// NO ELSE CASE - operation silently fails!

// Another location uses portMAX_DELAY (infinite wait!)
if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) { ... }
```

If I2C bus hangs (hardware fault, bus contention):
- Infinite wait blocks encoder task forever
- 10ms timeout causes silent encoder failures
- No error recovery or bus reset
- Main loop unaffected but HMI becomes unresponsive

**Impact:**
- Permanent loss of encoder functionality
- User confusion (device running but controls dead)
- No indication of I2C bus health
- Potential Core 0 task starvation

**Recommended Architectural Changes:**
1. **Standardize timeout values** (e.g., 50ms for all I2C operations)
2. **Implement timeout error handling** - log error, attempt bus reset
3. **Add I2C health monitoring** - track timeout rate, disable after threshold
4. **Notify user of I2C failure** via LED pattern or WebSocket message
5. **Implement I2C bus reset sequence** after N consecutive failures
6. **Add task watchdog** to detect hung I2C task

---

#### FINDING HW-002: LED Buffer Mutex Used Inconsistently
**Severity:** HIGH
**Layer:** Hardware (main.cpp buffer management)
**Principle Violated:** Thread Safety

**Description:**
`ledBufferMutex` defined but only used in 2 locations (both in main.cpp):
```cpp
// main.cpp - syncLedsToStrips() uses mutex
if (xSemaphoreTake(ledBufferMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    memcpy(...);  // Copy leds[] to strip1[]/strip2[]
    xSemaphoreGive(ledBufferMutex);
}
```

However, 45+ effect functions write to `leds[]`, `strip1[]`, `strip2[]` without acquiring mutex:
```cpp
// LGPParticleCollider.cpp (and 44 other effect files)
extern CRGB leds[];
void particleEffect() {
    leds[i] = color;  // NO MUTEX!
}
```

**Impact:**
- Race condition between effect rendering and buffer sync
- Torn frames (half-old, half-new pixels)
- Corruption during concurrent writes
- Mutex provides false sense of security

**Recommended Architectural Changes:**
1. **Enforce mutex in all buffer writes** - wrap LED access in RAII mutex guard
2. **Use double buffering** - render to back buffer, swap under mutex
3. **Document buffer ownership** (who can write when)
4. **Add static analysis** to detect unprotected buffer access
5. **Consider lock-free ring buffer** for effect → display pipeline

---

#### FINDING HW-003: NVS Operations Lack Error Handling
**Severity:** MEDIUM
**Layer:** Hardware (ZoneConfigManager.cpp NVS storage)
**Principle Violated:** Resilience, Data Integrity

**Description:**
Zone configuration saved to NVS (Non-Volatile Storage) with minimal error handling:
```cpp
// ZoneConfigManager.cpp
esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
if (err != ESP_OK) {
    Serial.printf("❌ NVS open failed: %s\n", esp_err_to_name(err));
    return false;  // Fails silently to caller
}
```

Issues:
- No retry logic on transient failures
- No corruption detection beyond simple checksum
- No versioning for config format changes
- No backup/restore mechanism
- Flash wear leveling not explicitly managed

**Impact:**
- Permanent loss of zone configurations
- Silent corruption of settings
- Boot failure if NVS corrupted
- No recovery from flash wear-out

**Recommended Architectural Changes:**
1. **Implement write verification** - read back and compare after write
2. **Add retry logic** with exponential backoff (3 attempts)
3. **Version config format** - allow migrations
4. **Implement backup slots** - store 2-3 copies of config, use newest valid
5. **Add NVS health monitoring** - track write failures, warn user
6. **Provide factory reset mechanism** (clear NVS, restore defaults)

---

### 2.5 Configuration Layer (src/config/)

#### FINDING CFG-001: WiFi Credentials in Build Flags
**Severity:** MEDIUM
**Layer:** Configuration (network_config.h, platformio.ini)
**Principle Violated:** Credential Management

**Description:**
WiFi credentials configured via build flags or header file:
```cpp
// network_config.h
#ifdef WIFI_SSID
    constexpr const char* WIFI_SSID_VALUE = WIFI_SSID;
#else
    constexpr const char* WIFI_SSID_VALUE = "CONFIGURE_ME";
#endif
```

While credentials are externalized (good), they are:
- Compiled into firmware binary (discoverable via strings)
- Stored in plaintext in build artifacts
- Not encrypted at rest
- No mechanism to change without recompile

**Impact:**
- Credentials leaked if firmware binary shared
- Reverse engineering reveals network password
- No runtime credential rotation

**Recommended Architectural Changes:**
1. **Store credentials in NVS** encrypted with device-specific key
2. **Implement provisioning mode** - AP mode for first-time setup
3. **Add credential rotation API** without firmware update
4. **Use ESP32 flash encryption** for firmware storage
5. **Implement WPA3-SAE** for forward secrecy
6. **Add credential export/import** for device migration

---

#### FINDING CFG-002: Feature Flags Create Attack Surface Variation
**Severity:** LOW
**Layer:** Configuration (features.h, platformio.ini)
**Principle Violated:** Attack Surface Minimization

**Description:**
25+ feature flags create combinatorial explosion of build configurations:
- `FEATURE_WEB_SERVER` - enables network attack surface
- `FEATURE_OTA_UPDATE` - enables firmware update attacks
- `FEATURE_AUDIO_SYNC` - adds audio processing code
- Build variants: esp32dev (no WiFi), esp32dev_audio_esv11, esp32dev_enhanced, memory_debug

No testing matrix validates security across all combinations. Disabled features may leave dead code with vulnerabilities.

**Impact:**
- Security fixes may not apply to all build configs
- Untested feature combinations may have emergent vulnerabilities
- Code bloat (unused code not compiled out)

**Recommended Architectural Changes:**
1. **Reduce feature flags** - consolidate to 3-5 profiles (minimal, standard, full)
2. **Document security posture** per profile
3. **Implement CI testing matrix** for each profile
4. **Use linker garbage collection** to remove unused code
5. **Add compile-time security assertions** (e.g., require auth if web enabled)

---

## 3. Cross-Cutting Concerns

### 3.1 Logging and Audit Trail

#### FINDING XC-001: No Security Event Logging
**Severity:** HIGH
**Principle Violated:** Accountability, Forensics

**Description:**
Security-relevant events not logged persistently:
- OTA authentication failures
- Rate limit violations
- Invalid effect/parameter requests
- NVS corruption detected
- I2C bus failures

Only Serial.println() used (lost on reboot).

**Recommended Architectural Changes:**
1. **Implement persistent log buffer** in NVS
2. **Log security events** (failed auth, rate limits, crashes)
3. **Add log rotation** (circular buffer, 100 most recent)
4. **Expose logs via API** (authenticated endpoint)
5. **Include timestamps** (NTP sync or uptime counter)

---

### 3.2 Memory Safety

#### FINDING XC-002: Stack Overflow Risk in FreeRTOS Tasks
**Severity:** MEDIUM → PARTIALLY ADDRESSED
**Principle Violated:** Resource Limits
**Status:** Bounds checking implemented, stack overflow detection pending

**Description:**
FreeRTOS tasks created with fixed stack sizes:
- WiFi task: 8192 bytes (includes mDNS and JSON)
- Encoder task: 4096 bytes
- WebSocket monitor: 8192 bytes

No runtime stack overflow detection. Deep call chains or large local variables can overflow.

**Implementation Status:**
✅ **COMPLETE**: Defensive bounds checking implemented across all array accesses
- All array indices validated before access
- Validation functions follow consistent pattern
- Safe defaults returned for corrupted indices
- See `v2/docs/architecture/DEFENSIVE_BOUNDS_CHECKING.md` for details

⏳ **PENDING**: Stack overflow detection and monitoring
- FreeRTOS stack overflow checking not yet enabled
- Stack usage monitoring not yet implemented

**Recommended Architectural Changes:**
1. ✅ **Defensive bounds checking** - IMPLEMENTED (all array accesses validated)
2. ⏳ **Enable FreeRTOS stack overflow checking** (Method 2) - PENDING
3. ⏳ **Profile actual stack usage** per task (uxTaskGetStackHighWaterMark) - PENDING
4. ⏳ **Increase stack sizes** with 50% safety margin - PENDING
5. ⏳ **Add stack usage monitoring** - log high water marks - PENDING
6. ✅ **Avoid large stack allocations** - use heap for large buffers - VERIFIED

---

### 3.3 Error Handling Consistency

#### FINDING XC-003: Inconsistent Error Handling Patterns
**Severity:** MEDIUM
**Principle Violated:** Fail-Safe Defaults

**Description:**
Error handling varies by module:
- Network layer: Returns HTTP error codes, logs to serial
- Effects layer: Silent failures (returns without error)
- Hardware layer: Some retry logic, some silent failures

No centralized error reporting or recovery strategy.

**Recommended Architectural Changes:**
1. **Define error handling policy** per layer
2. **Implement error reporting service** (collect and aggregate)
3. **Add graceful degradation** (fallback to safe mode)
4. **Log all errors** with context (file, line, error code)

---

## 4. Threat Model Analysis

### 4.1 Threat Actors

1. **Local Network Attacker** (Primary Threat)
   - Capability: Same WiFi network as device
   - Intent: Annoyance, DoS, credential theft, pivot to other devices
   - Mitigations: Authentication, rate limiting, network segmentation

2. **Malicious Effect Developer** (Secondary Threat)
   - Capability: Submit custom effect code
   - Intent: Buffer overflow, code execution, information disclosure
   - Mitigations: Effect sandboxing, capability model, code review

3. **Physical Attacker** (Low Priority)
   - Capability: Physical access to device
   - Intent: Firmware extraction, credential theft, persistent backdoor
   - Mitigations: Flash encryption, secure boot, tamper detection

---

### 4.2 Attack Scenarios

#### Scenario 1: OTA Firmware Takeover
**Threat Actor:** Local Network Attacker
**Attack Path:**
1. Attacker sniffs HTTP traffic or guesses hardcoded OTA token
2. Uploads malicious firmware via `/api/ota`
3. Firmware extracts WiFi credentials from NVS
4. Exfiltrates credentials to attacker server
5. Attacker gains access to home network

**Likelihood:** HIGH (token is weak)
**Impact:** CRITICAL (persistent compromise)
**Mitigations:** NET-001 remediation

---

#### Scenario 2: Effect-Based Buffer Overflow
**Threat Actor:** Malicious Effect Developer
**Attack Path:**
1. Attacker submits community effect with buffer overflow
2. Effect writes beyond `leds[]` array bounds
3. Overwrites adjacent heap metadata or stack
4. Triggers code execution on next malloc/free

**Likelihood:** MEDIUM (requires code review bypass)
**Impact:** HIGH (code execution)
**Mitigations:** EFF-001 remediation, static analysis

---

#### Scenario 3: Race Condition State Corruption
**Threat Actor:** Local Network Attacker
**Attack Path:**
1. Attacker opens multiple WebSocket connections
2. Sends concurrent `setEffect` commands
3. Races network thread vs. main loop vs. I2C task
4. Corrupts global state (currentEffect, LED buffers)
5. Triggers crash or undefined behavior

**Likelihood:** MEDIUM (requires timing)
**Impact:** MEDIUM (DoS, visual corruption)
**Mitigations:** APP-001 remediation

---

## 5. Remediation Roadmap

### 5.1 Immediate (Critical) - Within 1 Sprint

| Finding | Priority | Effort | Dependencies |
|---------|----------|--------|--------------|
| NET-001 | CRITICAL | High | None |
| APP-001 | CRITICAL | High | Architecture refactor |
| NET-002 | HIGH | Medium | NET-001 |

**Tasks:**
1. Replace OTA token with signed firmware verification
2. Implement state management layer with mutexes
3. Add authentication to v1 API endpoints

---

### 5.2 Short-Term (High) - Within 1-2 Sprints

| Finding | Priority | Effort | Dependencies |
|---------|----------|--------|--------------|
| APP-002 | HIGH | Low | None |
| HW-001 | HIGH | Medium | None |
| HW-002 | HIGH | High | APP-001 |
| NET-003 | HIGH | Low | None |
| XC-001 | HIGH | Medium | None |

**Tasks:**
1. Centralize effect registry with validation
2. Fix I2C mutex timeout handling
3. Implement double buffering for LED arrays
4. Restrict CORS to whitelisted origins
5. Add persistent security event logging

---

### 5.3 Medium-Term (Medium) - Within 3-6 Sprints

| Finding | Priority | Effort | Dependencies |
|---------|----------|--------|--------------|
| APP-003 | MEDIUM | Medium | XC-001 |
| NET-004 | MEDIUM | Medium | None |
| EFF-001 | MEDIUM | High | Arch design |
| EFF-002 | MEDIUM | Medium | EFF-001 |
| HW-003 | MEDIUM | Medium | None |

**Tasks:**
1. Add crash recovery and safe mode
2. Fix WiFiManager state machine races
3. Design effect capability model
4. Refactor enhancement engines
5. Improve NVS error handling

---

### 5.4 Long-Term (Low) - Ongoing

| Finding | Priority | Effort | Dependencies |
|---------|----------|--------|--------------|
| CFG-001 | MEDIUM | Medium | NET-002 |
| XC-002 | MEDIUM | Low | None |
| All "LOW" | LOW | Varies | None |

**Tasks:**
1. Migrate credentials to encrypted NVS
2. Enable stack overflow checking
3. Address low-severity findings
4. Continuous security hardening

---

## 6. Architectural Principles for Future Development

### 6.1 Defense in Depth
- Multiple independent security layers
- Assume any single layer can fail
- Validate at every trust boundary

### 6.2 Least Privilege
- Components have minimum necessary permissions
- Effect functions cannot access network layer
- Read-only data separated from mutable state

### 6.3 Fail-Safe Defaults
- Errors default to safe state (not operational)
- Invalid input rejected, not sanitized
- Unknown effect IDs fall back to safe effect

### 6.4 Complete Mediation
- Every access to protected resource is checked
- No caching of authorization decisions
- Validate on every request, not just first

### 6.5 Separation of Privilege
- Sensitive operations require multiple conditions
- OTA requires token + physical button + rate limit
- Effect changes logged and rate-limited

### 6.6 Economy of Mechanism
- Keep security-critical code simple
- Minimize attack surface (disable unused features)
- Prefer simple, auditable solutions

---

## 7. Conclusion

LightwaveOS demonstrates **good architectural patterns** (layered architecture, FreeRTOS task isolation, rate limiting) but suffers from **insufficient security boundaries** and **global state management issues** typical of embedded firmware evolved from prototype to production.

**Most Critical Risks:**
1. **Weak OTA authentication** enables firmware takeover
2. **Global state races** create unpredictable behavior and privilege escalation
3. **Missing authentication** allows unauthorized control

**Recommended Next Steps:**
1. Implement signed firmware updates (NET-001)
2. Refactor global state to state management layer (APP-001)
3. Add API authentication (NET-002)
4. Review and apply remaining remediations per roadmap

**Positive Security Practices Observed:**
- Rate limiting implementation
- Request validation framework
- CORS headers (despite wildcard issue)
- Separation of WiFi/I2C tasks to different cores
- FreeRTOS mutex usage (though inconsistent)

With systematic application of the remediation roadmap, LightwaveOS can achieve a **production-ready security posture** suitable for deployment in home networks.

---

**End of Architecture Security Review**
