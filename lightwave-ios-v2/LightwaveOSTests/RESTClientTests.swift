//
//  RESTClientTests.swift
//  LightwaveOSTests
//
//  Unit tests for RESTClient response decoding and error handling.
//  Uses a custom URLProtocol subclass to intercept network requests.
//

import XCTest
@testable import LightwaveOS

// MARK: - Mock URL Protocol

/// Intercepts URLSession requests and returns pre-configured responses.
/// Register handlers via the static `requestHandler` closure before each test.
final class MockURLProtocol: URLProtocol {
    /// Handler called for every intercepted request.
    /// Must return (HTTPURLResponse, Data?) or throw an error.
    nonisolated(unsafe) static var requestHandler: ((URLRequest) throws -> (HTTPURLResponse, Data?))?

    override class func canInit(with request: URLRequest) -> Bool {
        true
    }

    override class func canonicalRequest(for request: URLRequest) -> URLRequest {
        request
    }

    override func startLoading() {
        guard let handler = MockURLProtocol.requestHandler else {
            XCTFail("MockURLProtocol.requestHandler not set")
            return
        }

        do {
            let (response, data) = try handler(request)
            client?.urlProtocol(self, didReceive: response,
                                cacheStoragePolicy: .notAllowed)
            if let data {
                client?.urlProtocol(self, didLoad: data)
            }
            client?.urlProtocolDidFinishLoading(self)
        } catch {
            client?.urlProtocol(self, didFailWithError: error)
        }
    }

    override func stopLoading() {
        // No-op; request completes synchronously in startLoading.
    }
}

// MARK: - Tests

final class RESTClientTests: XCTestCase {

    // MARK: - Setup

    override func setUp() {
        super.setUp()
        MockURLProtocol.requestHandler = nil
    }

    override func tearDown() {
        MockURLProtocol.requestHandler = nil
        super.tearDown()
    }

    // MARK: - DeviceInfoResponse Decoding

    /// A well-formed JSON payload should decode into DeviceInfoResponse.
    func testDeviceInfoResponseDecoding() throws {
        let json = """
        {
            "success": true,
            "data": {
                "firmware": "2.4.1",
                "firmwareVersionNumber": 241,
                "board": "ESP32-S3",
                "sdk": "5.1.2",
                "flashSize": 16777216,
                "sketchSize": 1843200,
                "freeSketch": 14286848,
                "architecture": "xtensa"
            },
            "timestamp": 123456,
            "version": "1.0"
        }
        """.data(using: .utf8)!

        let decoder = JSONDecoder()
        let response = try decoder.decode(DeviceInfoResponse.self, from: json)

        XCTAssertTrue(response.success)
        XCTAssertEqual(response.data.firmware, "2.4.1")
        XCTAssertEqual(response.data.firmwareVersionNumber, 241)
        XCTAssertEqual(response.data.board, "ESP32-S3")
        XCTAssertEqual(response.data.sdk, "5.1.2")
        XCTAssertEqual(response.data.flashSize, 16777216)
        XCTAssertEqual(response.data.sketchSize, 1843200)
        XCTAssertEqual(response.data.freeSketch, 14286848)
        XCTAssertEqual(response.data.architecture, "xtensa")
        XCTAssertEqual(response.timestamp, 123456)
        XCTAssertEqual(response.version, "1.0")
    }

    /// Computed backward-compatibility properties should return correct values.
    func testDeviceInfoComputedProperties() throws {
        let json = """
        {
            "success": true,
            "data": {
                "firmware": "2.4.1",
                "board": "ESP32-S3",
                "sdk": "5.1.2"
            }
        }
        """.data(using: .utf8)!

        let decoder = JSONDecoder()
        let response = try decoder.decode(DeviceInfoResponse.self, from: json)

        XCTAssertEqual(response.data.firmwareVersion, "2.4.1",
                       "firmwareVersion should be an alias for firmware")
        XCTAssertEqual(response.data.sdkVersion, "5.1.2",
                       "sdkVersion should be an alias for sdk")
    }

    // MARK: - DeviceStatusResponse Decoding

    /// Device status JSON should decode with all fields.
    func testDeviceStatusResponseDecoding() throws {
        let json = """
        {
            "success": true,
            "data": {
                "uptime": 3600,
                "freeHeap": 245760,
                "heapSize": 327680,
                "cpuFreq": 240,
                "fps": 60.5,
                "cpuPercent": 45.2,
                "framesRendered": 216000,
                "wsClients": 2,
                "network": {
                    "connected": true,
                    "apMode": false,
                    "ip": "192.168.1.101",
                    "rssi": -52
                }
            },
            "timestamp": 789012
        }
        """.data(using: .utf8)!

        let decoder = JSONDecoder()
        let response = try decoder.decode(DeviceStatusResponse.self, from: json)

        XCTAssertTrue(response.success)
        XCTAssertEqual(response.data.uptime, 3600)
        XCTAssertEqual(response.data.freeHeap, 245760)
        XCTAssertEqual(response.data.heapSize, 327680)
        XCTAssertEqual(response.data.cpuFreq, 240)
        XCTAssertEqual(response.data.fps ?? 0, Float(60.5), accuracy: Float(0.1))
        XCTAssertEqual(response.data.wsClients, 2)
        XCTAssertEqual(response.data.network?.ip, "192.168.1.101")
        XCTAssertEqual(response.data.network?.rssi, -52)
        XCTAssertEqual(response.data.network?.connected, true)
        XCTAssertEqual(response.data.network?.apMode, false)
    }

    // MARK: - APIClientError

    /// APIClientError cases should produce non-empty error descriptions.
    func testAPIClientErrorDescriptions() {
        let cases: [APIClientError] = [
            .connectionFailed(NSError(domain: "test", code: -1)),
            .invalidResponse,
            .httpError(500, "Internal Server Error"),
            .decodingError(NSError(domain: "decode", code: -1)),
            .rateLimited,
            .encodingError,
        ]

        for error in cases {
            XCTAssertNotNil(error.errorDescription,
                            "\(error) should have a non-nil error description")
            XCTAssertFalse(error.errorDescription?.isEmpty ?? true,
                           "\(error) should have a non-empty error description")
        }
    }

    /// HTTP error should include the status code in its description.
    func testHTTPErrorIncludesStatusCode() {
        let error = APIClientError.httpError(503, "Service Unavailable")
        let description = error.errorDescription ?? ""

        XCTAssertTrue(description.contains("503"),
                      "HTTP error description should contain the status code")
    }

    /// Rate limited error should have a meaningful description.
    func testRateLimitedErrorDescription() {
        let error = APIClientError.rateLimited
        let description = error.errorDescription ?? ""

        XCTAssertTrue(description.lowercased().contains("rate"),
                      "Rate limited description should mention rate limiting")
    }

    // MARK: - Response Model Minimal Decoding

    /// DeviceInfoResponse should decode with only required fields present.
    func testDeviceInfoMinimalJSON() throws {
        let json = """
        {
            "success": false,
            "data": {
                "firmware": "1.0.0",
                "board": "unknown",
                "sdk": "4.0"
            }
        }
        """.data(using: .utf8)!

        let decoder = JSONDecoder()
        let response = try decoder.decode(DeviceInfoResponse.self, from: json)

        XCTAssertFalse(response.success)
        XCTAssertEqual(response.data.firmware, "1.0.0")
        XCTAssertNil(response.data.firmwareVersionNumber)
        XCTAssertNil(response.data.flashSize)
        XCTAssertNil(response.data.sketchSize)
        XCTAssertNil(response.data.freeSketch)
        XCTAssertNil(response.data.architecture)
        XCTAssertNil(response.timestamp)
        XCTAssertNil(response.version)
    }

    /// DeviceStatusResponse should decode with only required fields.
    func testDeviceStatusMinimalJSON() throws {
        let json = """
        {
            "success": true,
            "data": {
                "uptime": 0,
                "freeHeap": 0
            }
        }
        """.data(using: .utf8)!

        let decoder = JSONDecoder()
        let response = try decoder.decode(DeviceStatusResponse.self, from: json)

        XCTAssertTrue(response.success)
        XCTAssertEqual(response.data.uptime, 0)
        XCTAssertEqual(response.data.freeHeap, 0)
        XCTAssertNil(response.data.heapSize)
        XCTAssertNil(response.data.cpuFreq)
        XCTAssertNil(response.data.fps)
        XCTAssertNil(response.data.network)
    }

    // MARK: - Malformed JSON

    /// Completely invalid JSON should throw a decoding error.
    func testMalformedJSONThrows() {
        let garbage = "not json at all".data(using: .utf8)!

        XCTAssertThrowsError(
            try JSONDecoder().decode(DeviceInfoResponse.self, from: garbage)
        ) { error in
            XCTAssertTrue(error is DecodingError,
                          "Should throw a DecodingError for malformed JSON")
        }
    }

    /// JSON with wrong field types should throw a decoding error.
    func testWrongFieldTypesThrows() {
        let json = """
        {
            "success": "yes",
            "data": {
                "firmware": 123,
                "board": true,
                "sdk": null
            }
        }
        """.data(using: .utf8)!

        XCTAssertThrowsError(
            try JSONDecoder().decode(DeviceInfoResponse.self, from: json)
        ) { error in
            XCTAssertTrue(error is DecodingError,
                          "Should throw DecodingError for type mismatches")
        }
    }

    // MARK: - GenericResponse Decoding

    /// GenericResponse should decode success and message fields.
    func testGenericResponseDecoding() throws {
        let json = """
        {
            "success": true,
            "message": "Effect set successfully",
            "timestamp": 999
        }
        """.data(using: .utf8)!

        let decoder = JSONDecoder()
        let response = try decoder.decode(GenericResponse.self, from: json)

        XCTAssertTrue(response.success)
        XCTAssertEqual(response.message, "Effect set successfully")
        XCTAssertEqual(response.timestamp, 999)
    }
}
