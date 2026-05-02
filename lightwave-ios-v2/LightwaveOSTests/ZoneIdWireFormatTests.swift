//
//  ZoneIdWireFormatTests.swift
//  LightwaveOSTests
//
//  Coverage for the 2026-05-02 wire-format zoneId migration (firmware
//  commit `d53092ad`). Verifies that:
//
//    1. RESTClient per-zone setters reject the reserved wire value 0
//       before any network round-trip and surface
//       `APIClientError.invalidZoneId`.
//    2. RESTClient per-zone setters interpolate the 1-indexed `zoneId`
//       directly into the URL path (no off-by-one translation).
//    3. `setZoneLayout` rejects a batch that contains a segment with
//       the reserved zoneId 0.
//    4. `ZoneViewModel.segmentsFromBoundaries()` emits 1-indexed
//       segments matching the wire format.
//    5. `ZoneViewModel.handleZoneUpdate` ignores rows / segments / zone
//       updates that carry the reserved wire value 0.
//

import XCTest
@testable import LightwaveOS

final class ZoneIdWireFormatTests: XCTestCase {

    // MARK: - Helpers

    /// Build a RESTClient backed by `MockURLProtocol`. Reuses the
    /// existing test seam from `RESTClientTests.swift`.
    private func makeClient() -> RESTClient {
        let config = URLSessionConfiguration.ephemeral
        config.protocolClasses = [MockURLProtocol.self]
        let session = URLSession(configuration: config)
        return RESTClient(host: "192.168.4.1", session: session)
    }

    override func setUp() {
        super.setUp()
        MockURLProtocol.requestHandler = nil
    }

    override func tearDown() {
        MockURLProtocol.requestHandler = nil
        super.tearDown()
    }

    // MARK: - REST guards

    /// `setZoneEffect(zoneId: 0)` must throw `APIClientError.invalidZoneId`
    /// without dispatching the request to the firmware.
    func testSetZoneEffectRejectsReservedZoneIdZero() async {
        let client = makeClient()
        var requestSent = false
        MockURLProtocol.requestHandler = { _ in
            requestSent = true
            let response = HTTPURLResponse(
                url: URL(string: "http://192.168.4.1/")!,
                statusCode: 200, httpVersion: nil, headerFields: nil)!
            return (response, nil)
        }

        do {
            try await client.setZoneEffect(zoneId: 0, effectId: 5)
            XCTFail("setZoneEffect(zoneId: 0) must throw")
        } catch APIClientError.invalidZoneId(let zoneId) {
            XCTAssertEqual(zoneId, 0,
                           "Thrown error must carry the offending zoneId")
        } catch {
            XCTFail("Unexpected error type: \(error)")
        }

        XCTAssertFalse(requestSent,
                       "Reserved zoneId must short-circuit before the URLSession call")
    }

    /// `setZonePalette(zoneId: 4)` must throw — only 1..3 are valid wire
    /// values post-B2.
    func testSetZonePaletteRejectsZoneIdAboveRange() async {
        let client = makeClient()
        do {
            try await client.setZonePalette(zoneId: 4, paletteId: 12)
            XCTFail("setZonePalette(zoneId: 4) must throw")
        } catch APIClientError.invalidZoneId(let zoneId) {
            XCTAssertEqual(zoneId, 4)
        } catch {
            XCTFail("Unexpected error type: \(error)")
        }
    }

    /// `setZoneEffect(zoneId: 1)` must build a request whose URL path
    /// contains `/zones/1/effect` — the wire `zoneId` is interpolated
    /// verbatim, no internal-to-wire translation.
    func testSetZoneEffectInterpolatesWireZoneIdIntoPath() async throws {
        let client = makeClient()
        let pathExpectation = expectation(description: "request path captured")
        var capturedPath: String?

        MockURLProtocol.requestHandler = { request in
            capturedPath = request.url?.path
            pathExpectation.fulfill()
            let response = HTTPURLResponse(
                url: request.url!,
                statusCode: 200, httpVersion: nil, headerFields: nil)!
            let body = "{\"success\":true}".data(using: .utf8)
            return (response, body)
        }

        try await client.setZoneEffect(zoneId: 1, effectId: 7)
        await fulfillment(of: [pathExpectation], timeout: 2.0)

        XCTAssertEqual(capturedPath, "/api/v1/zones/1/effect",
                       "Wire zoneId 1 must appear directly in the URL path")
    }

    /// `setZoneEffect(zoneId: 3)` (the maximum wire value) must hit
    /// `/zones/3/effect` — confirms the upper bound is permitted.
    func testSetZoneEffectAcceptsWireZoneIdThree() async throws {
        let client = makeClient()
        let pathExpectation = expectation(description: "request path captured")
        var capturedPath: String?

        MockURLProtocol.requestHandler = { request in
            capturedPath = request.url?.path
            pathExpectation.fulfill()
            let response = HTTPURLResponse(
                url: request.url!,
                statusCode: 200, httpVersion: nil, headerFields: nil)!
            let body = "{\"success\":true}".data(using: .utf8)
            return (response, body)
        }

        try await client.setZoneEffect(zoneId: 3, effectId: 9)
        await fulfillment(of: [pathExpectation], timeout: 2.0)

        XCTAssertEqual(capturedPath, "/api/v1/zones/3/effect")
    }

    /// `setZoneLayout` must reject a batch that contains a segment with
    /// zoneId == 0 before dispatching.
    func testSetZoneLayoutRejectsBatchWithReservedZoneId() async {
        let client = makeClient()
        var requestSent = false
        MockURLProtocol.requestHandler = { _ in
            requestSent = true
            let response = HTTPURLResponse(
                url: URL(string: "http://192.168.4.1/")!,
                statusCode: 200, httpVersion: nil, headerFields: nil)!
            return (response, nil)
        }

        let badZones: [[String: Int]] = [
            ["zoneId": 1, "s1LeftStart": 40, "s1LeftEnd": 79,
             "s1RightStart": 80, "s1RightEnd": 119],
            ["zoneId": 0, "s1LeftStart": 0, "s1LeftEnd": 39,
             "s1RightStart": 120, "s1RightEnd": 159]
        ]

        do {
            try await client.setZoneLayout(zones: badZones)
            XCTFail("setZoneLayout must throw on a segment with zoneId 0")
        } catch APIClientError.invalidZoneId(let zoneId) {
            XCTAssertEqual(zoneId, 0)
        } catch {
            XCTFail("Unexpected error type: \(error)")
        }

        XCTAssertFalse(requestSent,
                       "Reserved zoneId in batch must short-circuit before URLSession")
    }

    // MARK: - ZoneViewModel boundary emission

    /// Segments emitted by `segmentsFromBoundaries()` must use 1-indexed
    /// `zoneId` values (1, 2, 3) so that downstream REST/WS payloads
    /// match the post-B2 wire format.
    @MainActor
    func testSegmentsFromBoundariesEmitOneIndexedZoneIds() {
        let vm = ZoneViewModel()

        vm.zoneCount = 1
        XCTAssertEqual(vm.segmentsFromBoundaries().map(\.zoneId), [1],
                       "Single-zone layout must emit zoneId 1 (innermost)")

        vm.zoneCount = 2
        XCTAssertEqual(vm.segmentsFromBoundaries().map(\.zoneId), [1, 2],
                       "Two-zone layout must emit 1, 2 (inner, outer)")

        vm.zoneCount = 3
        XCTAssertEqual(vm.segmentsFromBoundaries().map(\.zoneId), [1, 2, 3],
                       "Three-zone layout must emit 1, 2, 3 (inner, middle, outer)")
    }

    // MARK: - WS inbound guards

    /// `handleZoneUpdate` must drop a `zones[]` row whose `id` is the
    /// reserved wire value 0 — guards against firmware regression.
    @MainActor
    func testHandleZoneUpdateDropsZoneRowWithReservedId() {
        let vm = ZoneViewModel()
        vm.handleZoneUpdate([
            "zones": [
                ["id": 0, "effectId": 1, "speed": 10, "paletteId": 0, "blendMode": 0],
                ["id": 1, "effectId": 2, "speed": 20, "paletteId": 5, "blendMode": 1]
            ]
        ])

        XCTAssertEqual(vm.zones.count, 1,
                       "Zone row with reserved id 0 must be dropped")
        XCTAssertEqual(vm.zones.first?.id, 1)
    }

    /// `handleZoneUpdate` must drop a `segments[]` entry whose `zoneId`
    /// is the reserved wire value 0.
    @MainActor
    func testHandleZoneUpdateDropsSegmentWithReservedId() {
        let vm = ZoneViewModel()
        vm.handleZoneUpdate([
            "segments": [
                ["zoneId": 0, "s1LeftStart": 0, "s1LeftEnd": 39,
                 "s1RightStart": 120, "s1RightEnd": 159],
                ["zoneId": 1, "s1LeftStart": 40, "s1LeftEnd": 79,
                 "s1RightStart": 80, "s1RightEnd": 119]
            ]
        ])

        XCTAssertEqual(vm.segments.count, 1,
                       "Segment with reserved zoneId 0 must be dropped")
        XCTAssertEqual(vm.segments.first?.zoneId, 1)
    }

    /// A targeted `zoneId == 0` update must be ignored entirely, not
    /// matched against any internal id.
    @MainActor
    func testHandleZoneUpdateIgnoresTargetedReservedZoneId() {
        let vm = ZoneViewModel()
        vm.zones = [
            ZoneConfig(id: 1, enabled: true, effectId: 5, speed: 20)
        ]
        let originalSpeed = vm.zones[0].speed

        vm.handleZoneUpdate(["zoneId": 0, "speed": 99])

        XCTAssertEqual(vm.zones[0].speed, originalSpeed,
                       "Reserved targeted zoneId must not mutate any zone state")
    }
}
