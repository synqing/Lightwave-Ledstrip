# Protected Files Registry

**Purpose:** This registry documents files that have caused recurring bugs when modified carelessly. Agents MUST read this before modifying any listed file.

**Last Updated:** 2025-12-31

---

## Criticality Levels

| Level | Meaning | Required Before Modification |
|-------|---------|------------------------------|
| CRITICAL | Core infrastructure, has caused production bugs 3+ times | Full file read, unit tests, integration test, explicit commit message |
| HIGH | Important subsystem, has caused bugs 1-2 times | Full file read, relevant tests, verification on hardware |
| MEDIUM | Complex code, high regression risk | Understand related functions, spot-check tests |

---

## Protected Files

### 1. WiFiManager

| Property | Value |
|----------|-------|
| **File(s)** | `v2/src/network/WiFiManager.cpp`, `v2/src/network/WiFiManager.h` |
| **Criticality** | CRITICAL |
| **Domain** | Network, FreeRTOS synchronization |
| **Bug Count** | 3+ occurrences of same bug |
| **Last Known-Good Commit** | (verify with `git log -1 --format=%H -- v2/src/network/WiFiManager.cpp`) |

#### Known Landmines

1. **Stale EventGroup Bits (The "IP: 0.0.0.0" Bug)**
   - **Symptom:** Log shows "Connected! IP: 0.0.0.0, RSSI: 0 dBm" before actual connection
   - **Cause:** `xEventGroupClearBits()` not called when entering CONNECTING state
   - **Location:** `setState()` function, lines ~499-505
   - **Fix:** MUST clear EVENT_CONNECTED, EVENT_GOT_IP, EVENT_CONNECTION_FAILED on CONNECTING entry

2. **Mutex Deadlock Risk**
   - **Risk:** Calling blocking operations while holding `m_stateMutex`
   - **Rule:** Never call WiFi API functions while holding the mutex

3. **Event Handler Thread Safety**
   - **Risk:** WiFi event handlers run on system task, not WiFiManager task
   - **Rule:** Use event group bits to signal, not direct state changes

#### Required Tests

```bash
# Build with WiFi
pio run -e esp32dev_audio

# Upload and verify connection sequence
pio run -e esp32dev_audio -t upload
pio device monitor -b 115200

# Expected log sequence:
# 1. [WiFi] Connecting to <SSID>...
# 2. [WiFi] Connected! (no IP yet)
# 3. [WiFi] Got IP: 192.168.x.x
#
# NOT acceptable:
# - "Connected! IP: 0.0.0.0" appearing at any point
```

#### State Machine Reference

```
INIT → SCANNING → CONNECTING → CONNECTED → (stable)
                      ↓              ↓
                 CONNECT_FAILED → DISCONNECTED → CONNECTING (retry)
```

Each state transition MUST:
1. Acquire `m_stateMutex`
2. Clear relevant EventGroup bits
3. Update `m_currentState`
4. Release mutex

---

### 2. WebServer

| Property | Value |
|----------|-------|
| **File(s)** | `v2/src/network/WebServer.cpp`, `v2/src/network/WebServer.h` |
| **Criticality** | HIGH |
| **Domain** | HTTP/WebSocket, async callbacks |
| **Bug Count** | 1-2 |

#### Known Landmines

1. **AsyncWebServer Callback Threading**
   - HTTP and WebSocket handlers run on different tasks
   - Shared state (rate limiter, connection count) needs mutex protection

2. **Rate Limiter State**
   - Per-IP rate limiting uses shared data structure
   - Must be protected with mutex for concurrent access

3. **JSON Response Buffer**
   - Large JSON responses can fragment heap
   - Use stack buffers or pre-allocated pools where possible

#### Required Tests

```bash
# Smoke test
curl http://lightwaveos.local/api/v1/device/status

# Rate limit test (should get 429 after ~20 rapid requests)
for i in {1..25}; do curl -s -o /dev/null -w "%{http_code}\n" http://lightwaveos.local/api/v1/device/status; done
```

---

### 3. AudioActor

| Property | Value |
|----------|-------|
| **File(s)** | `v2/src/audio/AudioActor.cpp`, `v2/src/audio/AudioActor.h` |
| **Criticality** | HIGH |
| **Domain** | Audio capture, DSP pipeline, inter-task communication |
| **Bug Count** | 1 |

#### Known Landmines

1. **I2S Buffer Timing**
   - I2S DMA buffers have strict timing requirements
   - Buffer underrun causes audio glitches and corrupted hop data

2. **ControlBus Thread Safety**
   - Audio task writes, Renderer task reads
   - Use atomic operations or proper synchronization

3. **Feature Flag Dependencies**
   - Many components conditionally compiled with FEATURE_AUDIO_SYNC
   - Breaking the include chain causes confusing build errors

---

## Adding New Protected Files

When a file causes recurring bugs (2+ occurrences), add it here:

1. Document the file path and criticality
2. Describe all known landmines
3. Specify required tests before modification
4. Update CLAUDE.md "Protected Files" section

---

## Verification Checklist

Before modifying ANY protected file:

- [ ] Read entire file to understand architecture
- [ ] Read this document's entry for the file
- [ ] Identify which landmines might be affected by your change
- [ ] Run required tests BEFORE making changes (baseline)
- [ ] Make changes incrementally
- [ ] Run required tests AFTER changes
- [ ] Write explicit commit message explaining what was changed and why
- [ ] If touching FreeRTOS primitives, verify with `git diff` that synchronization is correct
