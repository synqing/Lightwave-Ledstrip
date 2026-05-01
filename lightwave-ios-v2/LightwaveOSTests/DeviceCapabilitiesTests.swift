//
//  DeviceCapabilitiesTests.swift
//  LightwaveOSTests
//
//  TDD coverage for Phase 1 capability discovery.
//  Verifies the DeviceCapabilities decoder accepts both /api/v1/firmware/version
//  and /api/v1/openapi.json shapes, and that RESTClient.getCapabilities()
//  degrades gracefully (returns nil) when the transport fails.
//

import XCTest
@testable import LightwaveOS

// MARK: - Mock URL Protocol (capability-discovery scope)

/// Independent mock from RESTClientTests — keeps test isolation crisp.
/// Returning nil from the handler signals "transport error" (akin to a 404 / connection drop).
final class CapabilitiesMockURLProtocol: URLProtocol {
    nonisolated(unsafe) static var requestHandler: ((URLRequest) throws -> (HTTPURLResponse, Data?))?

    override class func canInit(with request: URLRequest) -> Bool { true }
    override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }

    override func startLoading() {
        guard let handler = CapabilitiesMockURLProtocol.requestHandler else {
            XCTFail("CapabilitiesMockURLProtocol.requestHandler not set")
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

    override func stopLoading() { /* no-op */ }
}

// MARK: - Tests

final class DeviceCapabilitiesTests: XCTestCase {

    override func setUp() {
        super.setUp()
        CapabilitiesMockURLProtocol.requestHandler = nil
    }

    override func tearDown() {
        CapabilitiesMockURLProtocol.requestHandler = nil
        super.tearDown()
    }

    // MARK: - Decoder shape: /api/v1/firmware/version

    /// Given a JSON like the firmware/version endpoint emits, the decoder produces a DeviceCapabilities.
    func test_decodes_firmware_version_response() throws {
        let json = """
        {
            "success": true,
            "data": {
                "version": "v3.7.0",
                "build": "6404cd77",
                "buildDate": "2026-04-30"
            }
        }
        """.data(using: .utf8)!

        let result = DeviceCapabilities.decodeFirmwareVersion(from: json)
        let capabilities = try XCTUnwrap(result, "Decoder should succeed for a valid firmware/version payload")

        XCTAssertEqual(capabilities.firmwareVersion, "v3.7.0")
        XCTAssertEqual(capabilities.buildHash, "6404cd77")
        XCTAssertEqual(capabilities.buildDate, "2026-04-30")
        // firmware/version endpoint does not enumerate paths
        XCTAssertNil(capabilities.availablePaths)
    }

    /// firmware/version with only a version field still decodes (other fields optional).
    func test_decodes_firmware_version_minimal() throws {
        let json = """
        {
            "success": true,
            "data": {
                "version": "v3.6.0"
            }
        }
        """.data(using: .utf8)!

        let result = DeviceCapabilities.decodeFirmwareVersion(from: json)
        let capabilities = try XCTUnwrap(result)

        XCTAssertEqual(capabilities.firmwareVersion, "v3.6.0")
        XCTAssertNil(capabilities.buildHash)
        XCTAssertNil(capabilities.buildDate)
    }

    // MARK: - Decoder shape: /api/v1/openapi.json

    /// Given a minimal OpenAPI document, the decoder extracts version + a non-empty paths set.
    func test_decodes_openapi_skeleton() throws {
        let json = """
        {
            "openapi": "3.0.0",
            "info": {
                "title": "LightwaveOS API",
                "version": "v3.7.0"
            },
            "paths": {
                "/api/v1/device/info": { "get": {} },
                "/api/v1/effects": { "get": {} },
                "/api/v1/parameters": { "get": {}, "post": {} }
            }
        }
        """.data(using: .utf8)!

        let result = DeviceCapabilities.decodeOpenAPI(from: json)
        let capabilities = try XCTUnwrap(result, "Decoder should succeed for a valid OpenAPI skeleton")

        XCTAssertEqual(capabilities.firmwareVersion, "v3.7.0")
        let paths = try XCTUnwrap(capabilities.availablePaths, "OpenAPI decode should populate availablePaths")
        XCTAssertFalse(paths.isEmpty, "OpenAPI paths must be non-empty")
        XCTAssertTrue(paths.contains("/api/v1/effects"))
        XCTAssertTrue(paths.contains("/api/v1/device/info"))
        XCTAssertTrue(paths.contains("/api/v1/parameters"))
    }

    /// OpenAPI document missing the info.version field still decodes — version becomes nil.
    func test_decodes_openapi_without_version() throws {
        let json = """
        {
            "openapi": "3.0.0",
            "info": { "title": "LightwaveOS API" },
            "paths": { "/api/v1/health": { "get": {} } }
        }
        """.data(using: .utf8)!

        let result = DeviceCapabilities.decodeOpenAPI(from: json)
        let capabilities = try XCTUnwrap(result)

        XCTAssertNil(capabilities.firmwareVersion)
        XCTAssertEqual(capabilities.availablePaths, ["/api/v1/health"])
    }

    // MARK: - Graceful degradation

    /// When BOTH probe endpoints return non-2xx (e.g. 404 / 500), getCapabilities() returns nil.
    func test_handles_404_gracefully() async throws {
        // Build a URLSession that routes through our mock protocol.
        let config = URLSessionConfiguration.ephemeral
        config.protocolClasses = [CapabilitiesMockURLProtocol.self]
        let session = URLSession(configuration: config)

        // Both probes fail with 404 — capability fetch must NOT throw, must return nil.
        CapabilitiesMockURLProtocol.requestHandler = { request in
            let response = HTTPURLResponse(
                url: request.url!,
                statusCode: 404,
                httpVersion: "HTTP/1.1",
                headerFields: nil
            )!
            return (response, nil)
        }

        let client = RESTClient(host: "127.0.0.1", port: 80, session: session)
        let result = await client.getCapabilities()

        XCTAssertNil(result, "getCapabilities() must return nil when both probe endpoints fail")
    }

    /// When the OpenAPI probe succeeds, getCapabilities() returns the OpenAPI-derived value.
    func test_returns_openapi_when_available() async throws {
        let config = URLSessionConfiguration.ephemeral
        config.protocolClasses = [CapabilitiesMockURLProtocol.self]
        let session = URLSession(configuration: config)

        CapabilitiesMockURLProtocol.requestHandler = { request in
            let path = request.url?.path ?? ""
            if path.contains("openapi.json") {
                let body = """
                {
                    "openapi": "3.0.0",
                    "info": { "version": "v3.7.0" },
                    "paths": { "/api/v1/effects": { "get": {} } }
                }
                """.data(using: .utf8)!
                let response = HTTPURLResponse(
                    url: request.url!,
                    statusCode: 200,
                    httpVersion: "HTTP/1.1",
                    headerFields: ["Content-Type": "application/json"]
                )!
                return (response, body)
            }
            // Fallback path should not be hit when openapi succeeded.
            let response = HTTPURLResponse(
                url: request.url!,
                statusCode: 404,
                httpVersion: "HTTP/1.1",
                headerFields: nil
            )!
            return (response, nil)
        }

        let client = RESTClient(host: "127.0.0.1", port: 80, session: session)
        let result = await client.getCapabilities()

        let capabilities = try XCTUnwrap(result)
        XCTAssertEqual(capabilities.firmwareVersion, "v3.7.0")
        XCTAssertEqual(capabilities.availablePaths, ["/api/v1/effects"])
    }

    /// When the OpenAPI probe fails but firmware/version succeeds, getCapabilities() falls back.
    func test_falls_back_to_firmware_version() async throws {
        let config = URLSessionConfiguration.ephemeral
        config.protocolClasses = [CapabilitiesMockURLProtocol.self]
        let session = URLSession(configuration: config)

        CapabilitiesMockURLProtocol.requestHandler = { request in
            let path = request.url?.path ?? ""
            if path.contains("openapi.json") {
                // OpenAPI not exposed on this firmware build.
                let response = HTTPURLResponse(
                    url: request.url!,
                    statusCode: 404,
                    httpVersion: "HTTP/1.1",
                    headerFields: nil
                )!
                return (response, nil)
            }
            if path.contains("firmware/version") {
                let body = """
                {
                    "success": true,
                    "data": {
                        "version": "v3.6.5",
                        "build": "abc12345"
                    }
                }
                """.data(using: .utf8)!
                let response = HTTPURLResponse(
                    url: request.url!,
                    statusCode: 200,
                    httpVersion: "HTTP/1.1",
                    headerFields: ["Content-Type": "application/json"]
                )!
                return (response, body)
            }
            let response = HTTPURLResponse(
                url: request.url!,
                statusCode: 404,
                httpVersion: "HTTP/1.1",
                headerFields: nil
            )!
            return (response, nil)
        }

        let client = RESTClient(host: "127.0.0.1", port: 80, session: session)
        let result = await client.getCapabilities()

        let capabilities = try XCTUnwrap(result, "Fallback to firmware/version should succeed")
        XCTAssertEqual(capabilities.firmwareVersion, "v3.6.5")
        XCTAssertEqual(capabilities.buildHash, "abc12345")
        XCTAssertNil(capabilities.availablePaths,
                     "firmware/version response carries no path enumeration")
    }
}
