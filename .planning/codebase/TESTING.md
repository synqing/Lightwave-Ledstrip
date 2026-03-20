---
abstract: "Testing infrastructure across firmware-v3 (native unit tests via PlatformIO), lightwave-ios-v2 (XCTest with mock URLProtocol), and Tab5 (no dedicated test suite). Includes test structure, mocking patterns, coverage status, and run commands."
---

# Testing Patterns

**Analysis Date:** 2026-03-21

## Test Framework

### Firmware-v3 (C++)

**Runner:**
- PlatformIO native build: `pio run -e native_test`
- Host-based unit tests (not embedded firmware tests)
- Compiles to x86/x64 binary for fast iteration
- Platform: Linux, macOS, Windows all supported

**Assertion Library:**
- `unity.h` — embedded-focused assertion framework included with PlatformIO
- Assertions: `TEST_ASSERT_TRUE()`, `TEST_ASSERT_EQUAL_STRING()`, `TEST_ASSERT_MESSAGE()`
- Execution: RUN automatically via PlatformIO; prints results to console

**Run Commands:**
```bash
cd firmware-v3

# Run all native tests
pio run -e native_test

# Build only (no run)
pio run -e native_test --target build

# Verbose output
pio run -e native_test -v

# Specific test file (configure in platformio.ini)
# Tests in firmware-v3/test/ are auto-discovered
```

**Test Environments:**
- `native_test` — primary; compiles codecs and contracts for host
- No device/embedded testing framework configured
- All tests are unit-level; no integration tests in firmware

### Lightwave-iOS-v2 (Swift)

**Runner:**
- Xcode/xcodebuild with XCTest framework
- Tests live in `LightwaveOSTests/` target
- Runs on iOS Simulator (iPhone 16) or physical device

**Assertion Library:**
- `XCTest` — Apple's native framework
- Assertions: `XCTAssertTrue()`, `XCTAssertEqual()`, `XCTAssertThrows()`
- No third-party assertion libraries

**Run Commands:**
```bash
cd lightwave-ios-v2

# Run all tests in Xcode
xcodebuild test -scheme LightwaveOS -destination 'platform=iOS Simulator,name=iPhone 16'

# Run specific test class
xcodebuild test -scheme LightwaveOS -destination 'platform=iOS Simulator,name=iPhone 16' \
  -only-testing 'LightwaveOSTests/RESTClientTests'

# Generate coverage report
xcodebuild test -scheme LightwaveOS \
  -enableCodeCoverage YES \
  -derivedDataPath .build/

# Run from Xcode UI
# Cmd+U or Product → Test
```

**Test Configuration:**
- Test target: `LightwaveOSTests` in Xcode project
- Simulator default: iPhone 16, iOS 18+
- No test scheme modifications needed; use default

### Tab5-Encoder (C++)

**Status:** No dedicated test framework configured
- Device-specific (ESP32-P4 hardware + LVGL UI)
- Hardware dependencies make native testing difficult
- Testing via manual soak/integration tests (serial monitor observation)
- See: `.claude/harness/HARNESS_RULES.md` for harness test patterns (if applicable)

## Test File Organization

### Firmware-v3 Location

**Directory Structure:**
```
firmware-v3/
├── test/
│   ├── test_ws_motion_codec/
│   │   └── test_ws_motion_codec.cpp
│   ├── test_ws_effects_codec/
│   │   └── test_ws_effects_codec.cpp
│   ├── test_http_palette_codec/
│   │   └── test_http_palette_codec.cpp
│   └── ...more codec tests...
├── platformio.ini
└── src/
```

**Naming:**
- Test directory: `test_{module_name}/`
- Test file: `test_{module_name}.cpp`
- One test suite per module (codec, contract, handler)

**Key Test Files:**
- `firmware-v3/test/test_ws_motion_codec/test_ws_motion_codec.cpp` — WebSocket motion command decoding
- `firmware-v3/test/test_ws_effects_codec/test_ws_effects_codec.cpp` — Effect selection codec
- `firmware-v3/test/test_http_palette_codec/test_http_palette_codec.cpp` — Palette HTTP responses
- `firmware-v3/test/test_audio_mapping_registry/test_audio_mapping_registry.cpp` — Audio event mapping

### Lightwave-iOS-v2 Location

**Directory Structure:**
```
lightwave-ios-v2/
├── LightwaveOSTests/
│   ├── RESTClientTests.swift
│   └── LightwaveOSUITests.swift
└── LightwaveOS/
    ├── Views/
    ├── ViewModels/
    └── Network/
```

**Naming:**
- Test files: `{TargetName}Tests.swift` (e.g., `RESTClientTests.swift`)
- Test classes: `{Module}Tests : XCTestCase`
- Test methods: `test{DescriptiveScenario}()` — camelCase, no underscores

**Key Test Files:**
- `LightwaveOSTests/RESTClientTests.swift` — Network client mocking and response decoding
- `LightwaveOSUITests/LightwaveOSUITests.swift` — UI integration tests (basic)

## Test Structure

### C++ Test Pattern

**Suite Structure:**
```cpp
#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include "../../src/codec/WsMotionCodec.h"

using namespace lightwaveos::codec;

// ============================================================================
// Helper Functions
// ============================================================================

static bool loadJsonString(const char* jsonStr, JsonDocument& doc) {
    DeserializationError error = deserializeJson(doc, jsonStr);
    return error == DeserializationError::Ok;
}

// ============================================================================
// Test: Valid Simple Request
// ============================================================================

void test_motion_simple_valid() {
    const char* json = R"({"requestId": "test123"})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    MotionSimpleDecodeResult result = WsMotionCodec::decodeSimple(root);

    TEST_ASSERT_TRUE_MESSAGE(result.success, "Decode should succeed");
    TEST_ASSERT_EQUAL_STRING("test123", result.request.requestId);
}

void test_motion_invalid_extra_keys() {
    const char* json = R"({"requestId": "test", "unknown": 123})";
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(loadJsonString(json, doc), "JSON should parse");

    JsonObjectConst root = doc.as<JsonObjectConst>();
    MotionSimpleDecodeResult result = WsMotionCodec::decodeSimple(root);

    TEST_ASSERT_FALSE_MESSAGE(result.success, "Should reject unknown keys");
}

// Unity test runner registration
void setUp(void) {
    // Per-test setup (rarely needed)
}

void tearDown(void) {
    // Per-test cleanup (rarely needed)
}

#endif
```

**Patterns:**
- Conditional compile: `#ifdef NATIVE_BUILD` guards test code (never included in embedded firmware)
- Helper functions: static utility functions before test functions
- Test function naming: `test_{feature}_{scenario}()` — PlatformIO convention
- Assertions with messages: `TEST_ASSERT_*_MESSAGE()` for clarity
- setUp/tearDown: optional; only if test state needs initialization

### Swift Test Pattern

**Suite Structure:**
```swift
import XCTest
@testable import LightwaveOS

final class RESTClientTests: XCTestCase {

    // MARK: - Mock Infrastructure

    final class MockURLProtocol: URLProtocol {
        nonisolated(unsafe) static var requestHandler: ((URLRequest) throws -> (HTTPURLResponse, Data?))?

        override class func canInit(with request: URLRequest) -> Bool {
            true  // Intercept all requests
        }

        override func startLoading() {
            guard let handler = MockURLProtocol.requestHandler else {
                XCTFail("Handler not set")
                return
            }
            do {
                let (response, data) = try handler(request)
                client?.urlProtocol(self, didReceive: response, cacheStoragePolicy: .notAllowed)
                if let data {
                    client?.urlProtocol(self, didLoad: data)
                }
                client?.urlProtocolDidFinishLoading(self)
            } catch {
                client?.urlProtocol(self, didFailWithError: error)
            }
        }
    }

    // MARK: - Setup/Teardown

    override func setUp() {
        super.setUp()
        MockURLProtocol.requestHandler = nil
    }

    override func tearDown() {
        MockURLProtocol.requestHandler = nil
        super.tearDown()
    }

    // MARK: - Tests

    func testDeviceInfoResponseDecoding() throws {
        let json = """
        {
            "success": true,
            "data": {
                "firmware": "2.4.1",
                "board": "ESP32-S3"
            }
        }
        """.data(using: .utf8)!

        let decoder = JSONDecoder()
        let response = try decoder.decode(DeviceInfoResponse.self, from: json)

        XCTAssertTrue(response.success)
        XCTAssertEqual(response.data.firmware, "2.4.1")
    }

    func testHTTPErrorHandling() throws {
        MockURLProtocol.requestHandler = { _ in
            let response = HTTPURLResponse(
                url: URL(string: "http://test")!,
                statusCode: 500,
                httpVersion: nil,
                headerFields: nil
            )!
            return (response, nil)
        }

        // Test error handling in client code
        let client = RESTClient(baseURL: "http://test")
        // ... call client and assert error handling
    }
}
```

**Patterns:**
- Test class inherits from `XCTestCase`
- Mock infrastructure: nested `URLProtocol` subclass for network interception
- setUp/tearDown: reset mock state before/after each test
- Test methods: `func test{Feature}() throws`
- Assertions: `XCTAssertEqual()`, `XCTAssertTrue()`, `XCTAssertThrows()`
- Async/await tests: `func testAsync() async throws { ... }` (if needed)

## Mocking

### C++ Mocking

**Framework:** No mocking framework; manual fakes via conditional compilation

**Pattern:**
```cpp
// In header or test file
class FakeAudioAdapter : public IAudioAdapter {
    void process() override {
        // Stub implementation for testing
    }
};

// In test
void test_effect_with_audio() {
    FakeAudioAdapter fakeAudio;
    MyEffect effect;

    // Inject fake into effect context
    EffectContext ctx = { ... fakeAudio ... };
    effect.render(ctx);

    TEST_ASSERT_EQUAL(expectedValue, ctx.result);
}
```

**What to Mock:**
- Audio data sources (fakes provide canned test data)
- LED buffer (test writes, then inspect `ctx.leds[]` array)
- External dependencies (file I/O, network — unlikely in effect code)

**What NOT to Mock:**
- FastLED functions (too tightly coupled; test via buffer inspection)
- Math functions (test expected output values)
- Internal state (test via render output, not private members)

### Swift Mocking

**Framework:** URLProtocol subclass for network mocking

**Pattern:**
```swift
// Set up mock handler before test
MockURLProtocol.requestHandler = { request in
    guard request.url?.path.contains("/api/v1/device") == true else {
        throw APIClientError.invalidResponse
    }

    let json = """
    {
        "success": true,
        "data": { "firmware": "2.4.1" }
    }
    """.data(using: .utf8)!

    let response = HTTPURLResponse(
        url: request.url!,
        statusCode: 200,
        httpVersion: nil,
        headerFields: nil
    )!

    return (response, json)
}

// Test runs and URLSession uses mock
let client = RESTClient(baseURL: "http://device:80")
let info = try await client.getDeviceInfo()
XCTAssertEqual(info.firmware, "2.4.1")
```

**What to Mock:**
- HTTP responses (mock handler returns canned JSON)
- Error conditions (404, 500, timeout)
- Network unavailability (handler throws URLError)

**What NOT to Mock:**
- URLSession (test via real URLSession + mock protocol)
- JSONDecoder (test real decoding from mocked data)
- Networking layer (test REST client end-to-end)

## Fixtures and Factories

### C++ Test Data

**Pattern:**
```cpp
// In test file or shared helper
static JsonDocument createValidMotionRequest(const char* requestId = "default") {
    JsonDocument doc;
    doc["requestId"] = requestId;
    return doc;
}

// Usage in multiple tests
void test_motion_with_id() {
    JsonDocument doc = createValidMotionRequest("custom123");
    MotionSimpleDecodeResult result = WsMotionCodec::decodeSimple(doc.as<JsonObject>());
    TEST_ASSERT_EQUAL_STRING("custom123", result.request.requestId);
}
```

**Location:** Helper functions in test file itself (one per test module)

**Patterns:**
- Small factory functions for common test objects
- JSON documents created inline with ArduinoJson API
- No separate fixture files; keep test-specific data in test code

### Swift Test Data

**Pattern:**
```swift
// In test file or extension
extension RESTClientTests {
    func makeDeviceInfoJSON() -> Data {
        """
        {
            "success": true,
            "data": {
                "firmware": "2.4.1",
                "board": "ESP32-S3"
            }
        }
        """.data(using: .utf8)!
    }
}

// Usage
func testDecoding() throws {
    let data = makeDeviceInfoJSON()
    let response = try JSONDecoder().decode(DeviceInfoResponse.self, from: data)
    XCTAssertEqual(response.data.firmware, "2.4.1")
}
```

**Location:** Helper methods in test class extension

**Patterns:**
- Factory methods for common response objects
- String interpolation for JSON templates
- Reusable across multiple test methods

## Coverage

### Firmware-v3

**Requirements:** None enforced (no coverage tool configured)

**Practical Coverage:**
- Codec tests: aim for all happy-path cases + error cases (invalid JSON, missing fields)
- Current: ~15 test files covering REST/WebSocket codecs, audio mapping, transitions
- Gaps: No effect render() tests (hardware-dependent); no integration tests

**View Coverage:**
```bash
# PlatformIO native_test does not generate coverage reports
# Manual inspection: run tests and verify all test functions pass
pio run -e native_test
```

### Lightwave-iOS-v2

**Requirements:** None enforced; no CI coverage gate

**Current Coverage:**
- Network client: ~70% (REST response decoding, error handling)
- ViewModels: ~20% (basic state tests, mostly untested)
- Views: ~0% (UI tests are minimal, no snapshot testing)

**Generate Coverage:**
```bash
xcodebuild test -scheme LightwaveOS \
  -enableCodeCoverage YES \
  -derivedDataPath .build/
# Coverage data: .build/Intermediates.noindex/.../.profdata
# Xcode UI shows coverage in source editor
```

**Gaps:**
- No ViewModel unit tests for state transitions
- No network error recovery tests
- No UI interaction tests (swipe, tap, scroll)
- No animation frame tests

### Tab5-Encoder

**Status:** No test framework; testing is manual/observational

**Verification Methods:**
- Serial monitor: observe WiFi connection, WebSocket messages
- Visual: confirm encoders control parameters on K1 device
- Soak tests: 30-minute runs with encoder cycling (manual harness)

## Error Testing

### C++ Pattern

```cpp
void test_codec_invalid_json() {
    const char* invalidJson = R"({invalid})";
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, invalidJson);

    TEST_ASSERT_NOT_EQUAL(DeserializationError::Ok, error);
    // Codec should handle gracefully
}

void test_missing_required_field() {
    const char* json = R"({"type": "motion"})";  // Missing "data"
    JsonDocument doc;
    TEST_ASSERT_TRUE(loadJsonString(json, doc));

    JsonObjectConst root = doc.as<JsonObjectConst>();
    MotionSimpleDecodeResult result = WsMotionCodec::decodeSimple(root);

    TEST_ASSERT_FALSE(result.success);  // Should fail on missing field
}
```

### Swift Pattern

```swift
func testHTTPErrorResponse() throws {
    MockURLProtocol.requestHandler = { _ in
        let response = HTTPURLResponse(
            url: URL(string: "http://test/api/v1/device")!,
            statusCode: 500,
            httpVersion: nil,
            headerFields: nil
        )!
        return (response, nil)
    }

    let client = RESTClient(baseURL: "http://test")
    do {
        _ = try await client.getDeviceInfo()
        XCTFail("Should throw APIClientError.httpError")
    } catch APIClientError.httpError(let code, _) {
        XCTAssertEqual(code, 500)
    } catch {
        XCTFail("Wrong error type: \(error)")
    }
}

func testNetworkTimeout() throws {
    MockURLProtocol.requestHandler = { _ in
        throw URLError(.timedOut)
    }

    let client = RESTClient(baseURL: "http://test")
    do {
        _ = try await client.getDeviceInfo()
        XCTFail("Should throw APIClientError.connectionFailed")
    } catch APIClientError.connectionFailed {
        // Expected
    }
}
```

## Common Patterns

### Async Testing (C++)

No async support in C++ unit test framework; all tests are synchronous.

### Async Testing (Swift)

```swift
func testAsyncNetworkCall() async throws {
    MockURLProtocol.requestHandler = { _ in
        let data = """
        { "success": true, "data": { "firmware": "2.4.1" } }
        """.data(using: .utf8)!

        let response = HTTPURLResponse(
            url: URL(string: "http://test")!,
            statusCode: 200,
            httpVersion: nil,
            headerFields: nil
        )!

        return (response, data)
    }

    let client = RESTClient(baseURL: "http://test")
    let info = try await client.getDeviceInfo()  // Await network call
    XCTAssertEqual(info.firmware, "2.4.1")
}

// XCTest runs async tests automatically; use 'async throws' return type
```

---

*Testing analysis: 2026-03-21*

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-21 | agent:gsd-mapper | Created with C++ (Unity + native_test), Swift (XCTest + URLProtocol mocking), and Tab5 (no framework) test patterns; included codec test examples, mock infrastructure, coverage gaps, error handling patterns |
