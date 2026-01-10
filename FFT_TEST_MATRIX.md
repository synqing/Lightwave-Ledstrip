# FFT WebSocket Streaming - Test Matrix

**Feature:** Feature C - FFT WebSocket Streaming
**Commit:** 101c4b3
**Test Date:** 2026-01-08

---

## Test Categories

### 1. Basic Functionality Tests

#### Test 1.1: Subscription Acceptance
- **Objective:** Verify FFT subscription command is accepted
- **Steps:**
  1. Connect WebSocket to `ws://lightwaveos.local/ws`
  2. Send: `{type: "audio.fft.subscribe", requestId: "test-1.1"}`
  3. Wait for response (max 100ms)
- **Expected:**
  - Response type: `audio.fft.subscribed`
  - Response includes clientId, fftBins: 64, updateRateHz: 31
  - `accepted: true`
- **Pass Criteria:** Response received within 100ms with all fields

#### Test 1.2: Frame Reception
- **Objective:** Verify FFT frames are streamed after subscription
- **Steps:**
  1. Subscribe (from Test 1.1)
  2. Listen for frames for 5 seconds
  3. Count frames received
  4. Calculate average interval
- **Expected:**
  - Frames arrive at ~31 Hz (32ms intervals)
  - Count: 150-160 frames in 5 seconds (31 Hz ±5%)
  - No gaps > 100ms
- **Pass Criteria:** 145+ frames received, average interval 30-35ms

#### Test 1.3: Frame Format
- **Objective:** Verify FFT frame structure
- **Steps:**
  1. Capture 10 consecutive frames
  2. Parse JSON
  3. Verify fields and data types
  4. Check bin values
- **Expected:**
  - All frames have: type="audio.fft.frame", hopSeq, bins[]
  - hopSeq is uint32 and increasing/sequential
  - bins array has exactly 64 elements
  - All bin values: 0.0 ≤ value ≤ 1.0 (float)
- **Pass Criteria:** All 10 frames valid, correct structure

---

### 2. Subscription Limit Tests

#### Test 2.1: Multiple Client Subscriptions (1-4)
- **Objective:** Verify first 4 clients can subscribe
- **Steps:**
  1. Open 4 separate WebSocket connections
  2. Send subscribe command from each
  3. Wait for all responses
  4. Verify all receive frames
- **Expected:**
  - All 4 subscriptions: `audio.fft.subscribed` with `accepted: true`
  - All 4 clients receive FFT frames
  - No interference between clients
- **Pass Criteria:** All 4 clients receive confirmed

#### Test 2.2: Fifth Client Rejection
- **Objective:** Verify 5th client is rejected with RESOURCE_EXHAUSTED
- **Steps:**
  1. Keep 4 clients from Test 2.1 connected
  2. Connect 5th WebSocket
  3. Send subscribe command from 5th client
  4. Wait for response
- **Expected:**
  - Response type: `audio.fft.rejected`
  - error.code: "RESOURCE_EXHAUSTED"
  - error.message: "FFT subscriber table full (max 4 concurrent clients)"
  - `success: false`
- **Pass Criteria:** 5th client receives rejection

#### Test 2.3: Resubscription After Disconnection
- **Objective:** Verify new client can subscribe after one disconnects
- **Steps:**
  1. Start with 4 connected clients (Test 2.1)
  2. Disconnect client 1 (kill network/close socket)
  3. Connect new 5th client
  4. Send subscribe from 5th client
  5. Wait for response
- **Expected:**
  - Disconnected client 1 is cleaned up within 1-2 seconds
  - 5th client receives: `audio.fft.subscribed` with `accepted: true`
  - 5th client starts receiving frames
  - Original clients 2-4 still receiving frames
- **Pass Criteria:** 5th client accepted and receives frames

---

### 3. Frame Rate & Timing Tests

#### Test 3.1: Frame Rate Consistency (31 Hz ±5%)
- **Objective:** Measure actual frame rate
- **Steps:**
  1. Subscribe to FFT stream
  2. Record timestamp of first frame
  3. Collect 100 consecutive frames, recording timestamp of each
  4. Calculate intervals and average
- **Expected:**
  - Average interval: 30-35ms (31 Hz ±5%)
  - Min interval: 25ms
  - Max interval: 40ms
  - 90%+ of intervals within ±10% of average
- **Pass Criteria:** Average 30-35ms, <5% frames outside tolerance

#### Test 3.2: Jitter Analysis
- **Objective:** Measure frame-to-frame jitter
- **Steps:**
  1. From Test 3.1 data, calculate standard deviation of intervals
  2. Calculate coefficient of variation (σ / mean)
- **Expected:**
  - Standard deviation: <5ms
  - Coefficient of variation: <15%
- **Pass Criteria:** Jitter metrics within range

#### Test 3.3: Latency (Audio to WebSocket)
- **Objective:** Measure end-to-end latency
- **Steps:**
  1. Known constraint: ~32ms from audio capture to frame transmission
  2. Measure time from sending command to first frame receipt
  3. Note: includes network round-trip (~10-50ms depending on WiFi)
- **Expected:**
  - Time to first frame: 50-150ms (including WiFi latency)
  - Sustained latency: ~32ms per frame
- **Pass Criteria:** First frame within 150ms, sustained 30-35ms

---

### 4. Error Handling Tests

#### Test 4.1: Unsubscribe Command
- **Objective:** Verify unsubscribe stops stream delivery
- **Steps:**
  1. Subscribe (Test 1.1)
  2. Verify receiving frames (5+ frames minimum)
  3. Send: `{type: "audio.fft.unsubscribe", requestId: "test-4.1"}`
  4. Wait for response
  5. Continue listening for 3 more seconds
- **Expected:**
  - Response: `audio.fft.unsubscribed` with matching requestId
  - No frames received after 50ms following unsubscribe response
- **Pass Criteria:** Unsubscribe confirmed, frames stop

#### Test 4.2: Re-subscription
- **Objective:** Verify client can resubscribe after unsubscribe
- **Steps:**
  1. Complete Test 4.1 (unsubscribe)
  2. Send subscribe command again with new requestId
  3. Wait for response and frames
- **Expected:**
  - Second subscription: `audio.fft.subscribed` with new requestId
  - Frames resume within 100ms
- **Pass Criteria:** Re-subscription accepted and frames resume

#### Test 4.3: Graceful Disconnection Cleanup
- **Objective:** Verify disconnected clients are cleaned up
- **Steps:**
  1. Subscribe (Test 1.1)
  2. Receive 5+ frames
  3. Kill WebSocket (close socket, don't send unsubscribe)
  4. Attempt to subscribe new client
  5. Time how long until new client can subscribe
- **Expected:**
  - New client subscription accepted within 2-5 seconds
  - Cleanup happens automatically during broadcast cycle
- **Pass Criteria:** New subscription within 5 seconds

---

### 5. Data Integrity Tests

#### Test 5.1: FFT Bin Value Range
- **Objective:** Verify all bin values are normalized to [0, 1]
- **Steps:**
  1. Collect 100 frames
  2. For each frame, check all 64 bin values
  3. Find min, max, and count any violations
- **Expected:**
  - All bin values: 0.0 ≤ value ≤ 1.0
  - No NaN, Infinity, or negative values
  - Min value ≥ 0.0
  - Max value ≤ 1.0
- **Pass Criteria:** 100% of values within [0, 1]

#### Test 5.2: Bin Count Verification
- **Objective:** Verify exactly 64 bins per frame
- **Steps:**
  1. Collect 50 frames
  2. For each frame, count bins array length
  3. Check all are 64
- **Expected:**
  - All 50 frames have bins.length === 64
  - No truncated or extended frames
- **Pass Criteria:** 100% of frames have 64 bins

#### Test 5.3: hopSeq Sequence
- **Objective:** Verify hopSeq counter increments
- **Steps:**
  1. Collect 100 frames
  2. Extract hopSeq from each
  3. Verify monotonic increase (no decreases/resets)
  4. Check for gaps in sequence
- **Expected:**
  - hopSeq increases by 1 for each frame (or larger jumps if audio skipped)
  - No resets to lower values
  - Gaps only during audio silence
- **Pass Criteria:** Strictly monotonic increase or silence-induced gaps only

---

### 6. Concurrent Client Tests

#### Test 6.1: Multiple Clients Receive Identical Frames
- **Objective:** Verify all clients get same frame data
- **Steps:**
  1. Subscribe 3 clients
  2. Collect 10 frames from each simultaneously
  3. Compare frame data between clients
- **Expected:**
  - hopSeq values match between all clients (same frame)
  - bins[] arrays are identical between clients
  - Timestamps within 50ms of each other
- **Pass Criteria:** All clients receive identical frame data

#### Test 6.2: Client Isolation
- **Objective:** Verify one client's actions don't affect others
- **Steps:**
  1. Subscribe 4 clients
  2. Verify all receive frames
  3. Unsubscribe client 1
  4. Verify clients 2-4 still receive frames
  5. Subscribe new client 5 (fails with RESOURCE_EXHAUSTED)
  6. Verify clients 2-4 still receiving frames
- **Expected:**
  - Unsubscribe only affects client 1
  - Failed subscription (client 5) doesn't affect clients 2-4
  - Clients 2-4 maintain continuous frame stream
- **Pass Criteria:** Other clients unaffected by individual client actions

---

### 7. Stress & Stability Tests

#### Test 7.1: Extended Runtime (10 minutes)
- **Objective:** Verify stability over extended period
- **Steps:**
  1. Subscribe 4 clients
  2. Run for 10 minutes (600 seconds)
  3. Monitor for:
     - Frame reception rate (should stay ~31 Hz)
     - No disconnections
     - No data corruption
     - No memory leaks
- **Expected:**
  - All 4 clients maintain frame reception
  - Frame rate stays 29-32 Hz throughout
  - No errors or warnings in device logs
  - Memory usage stable (no growth)
- **Pass Criteria:** 10-minute test completes without issues

#### Test 7.2: Rapid Subscribe/Unsubscribe
- **Objective:** Verify rapid subscription changes work correctly
- **Steps:**
  1. Rapidly send 20 subscribe/unsubscribe pairs (one every 100ms)
  2. Count successful subscriptions
  3. Verify no crashes or state corruption
- **Expected:**
  - All 20 subscriptions accepted
  - All 20 unsubscriptions confirmed
  - No RESOURCE_EXHAUSTED errors
  - System remains stable
- **Pass Criteria:** All operations complete successfully

#### Test 7.3: High CPU Load (Effects Rendering)
- **Objective:** Verify FFT streaming maintains frame rate under load
- **Steps:**
  1. Subscribe to FFT stream
  2. Start CPU-intensive effects (e.g., physics simulation)
  3. Measure FFT frame rate
  4. Check device CPU usage
- **Expected:**
  - FFT frame rate drops at most 2-3 Hz (from 31 to 28-29)
  - Device CPU usage stays <85%
  - No frame drops or errors
- **Pass Criteria:** Frame rate maintained within 90% of nominal

---

### 8. Network Tests

#### Test 8.1: WiFi Signal Strength Variation
- **Objective:** Test behavior with varying WiFi signal
- **Steps:**
  1. Subscribe to FFT stream in normal WiFi location
  2. Move to location with weaker signal (-70 dBm)
  3. Continue monitoring frames
  4. Return to normal location
- **Expected:**
  - No frame drops at any signal strength > -70 dBm
  - Frame rate may vary ±1-2 Hz but stable
  - Auto-reconnect if disconnected
- **Pass Criteria:** Continuous operation across signal variations

#### Test 8.2: Network Latency Handling
- **Objective:** Test behavior with high latency
- **Steps:**
  1. Subscribe to FFT stream
  2. Add artificial network delay (tc command on router if possible)
  3. Monitor frame reception
  4. Measure impact on frame rate
- **Expected:**
  - Still receive ~31 Hz despite latency
  - Frame rate degrades gracefully if latency >100ms
  - No corruption or data loss
- **Pass Criteria:** Graceful behavior under latency

---

## Test Execution Summary

### Critical Path (Must Pass)
1. Test 1.1 - Subscription Acceptance
2. Test 1.2 - Frame Reception
3. Test 1.3 - Frame Format
4. Test 2.2 - Fifth Client Rejection
5. Test 3.1 - Frame Rate Consistency
6. Test 5.1 - Bin Value Range
7. Test 5.2 - Bin Count Verification

### Extended Path (Nice to Have)
- All remaining tests for comprehensive verification

### Regression Path (Before Deployment)
- Test 6.1 - Multiple Clients
- Test 7.1 - Extended Runtime
- Test 7.3 - High CPU Load

---

## Pass/Fail Criteria

**PASS:** ≥90% of critical path tests pass
**CONDITIONAL PASS:** 80-90% of critical path tests pass (minor issues found)
**FAIL:** <80% of critical path tests pass (blocking issues)

---

## Test Equipment Required

- Laptop/PC with WebSocket client capability
- LightwaveOS v2 device running esp32dev_audio build
- WiFi network with stable connectivity
- Browser with WebSocket support (all modern browsers)
- Serial monitor for device logs (optional)
- Network analyzer tool (optional, for latency testing)

---

## Test Results Template

```
Test ID: ___________
Tester: ___________
Date: ___________

Steps Executed: [ ] Yes [ ] No
Expected Result Achieved: [ ] Yes [ ] No
Pass/Fail: [ ] PASS [ ] FAIL

Notes:
_________________________________
_________________________________

Data Logged: [ ] Yes [ ] No
Appendix: ______________________
```

---

## Success Criteria Summary

| Category | Target | Threshold |
|----------|--------|-----------|
| Frame Rate | 31 Hz | 29-32 Hz |
| Frame Jitter | <5ms std dev | <10ms acceptable |
| Client Limit | 4 max | Exactly 4 enforced |
| Max Latency | 100ms first frame | 150ms acceptable |
| Uptime | 10+ minutes | Continuous operation |
| Data Integrity | 100% valid | 0 corruptions |
| CPU Overhead | <0.5% | <1% acceptable |

All success criteria must be met for PASS status.
