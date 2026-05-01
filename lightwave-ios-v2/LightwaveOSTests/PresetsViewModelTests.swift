//
//  PresetsViewModelTests.swift
//  LightwaveOSTests
//
//  Phase 2 (P2-3) — TDD coverage for the effect / zone preset CRUD surface.
//
//  These tests exercise the VM's REST list-load path (via `RESTClient`
//  fronted by `MockURLProtocol` from `RESTClientTests.swift`) and the WS
//  command-dispatch path (via a recording test double conforming to
//  `PresetWebSocketCommanding`).
//
//  British English in comments; wire payload field names follow the firmware
//  protocol verbatim.
//

import XCTest
import os
@testable import LightwaveOS

// MARK: - Recording Mock for WebSocket Commands

/// Records every `sendCommand(_:params:)` invocation made by the VM so tests
/// can assert on the exact command name and parameter map dispatched. The
/// production `WebSocketService` actor conforms to the same protocol, so the
/// VM call sites are identical between tests and production.
///
/// Storage is guarded by `OSAllocatedUnfairLock` which is async-safe under
/// Swift 6 strict concurrency (unlike `NSLock.lock()`).
@available(iOS 17.0, *)
final class RecordingPresetsCommandSink: PresetWebSocketCommanding, @unchecked Sendable {
    struct Invocation: Sendable {
        let command: String
        let params: [String: String]   // Stringified for trivial Sendable comparison.
    }

    private let storage = OSAllocatedUnfairLock<[Invocation]>(initialState: [])

    var invocations: [Invocation] {
        storage.withLock { $0 }
    }

    func sendCommand(_ command: String, params: [String: any Sendable]) async {
        let stringified = params.reduce(into: [String: String]()) { acc, kv in
            acc[kv.key] = "\(kv.value)"
        }
        storage.withLock { list in
            list.append(Invocation(command: command, params: stringified))
        }
    }
}

// MARK: - PresetsViewModelTests

@available(iOS 17.0, *)
@MainActor
final class PresetsViewModelTests: XCTestCase {

    // MARK: - Setup

    private var session: URLSession!

    override func setUp() {
        super.setUp()
        MockURLProtocol.requestHandler = nil
        let config = URLSessionConfiguration.ephemeral
        config.protocolClasses = [MockURLProtocol.self]
        session = URLSession(configuration: config)
    }

    override func tearDown() {
        MockURLProtocol.requestHandler = nil
        session = nil
        super.tearDown()
    }

    // MARK: - REST list load

    /// `loadEffectPresets(client:)` should hit `/api/v1/effect-presets`,
    /// transition `loading: true → false`, and store the decoded list.
    func test_loadsEffectPresetsViaREST() async throws {
        let payload = """
        {
          "success": true,
          "data": {
            "presets": [
              {"id": 0, "name": "Sunset", "effectId": 42},
              {"id": 1, "name": "Reef", "effectId": 17}
            ],
            "count": 2,
            "maxSlots": 16
          }
        }
        """.data(using: .utf8)!

        MockURLProtocol.requestHandler = { request in
            XCTAssertEqual(request.url?.path, "/api/v1/effect-presets",
                           "Effect-preset list must hit /api/v1/effect-presets")
            XCTAssertEqual(request.httpMethod, "GET")
            let resp = HTTPURLResponse(
                url: request.url!, statusCode: 200,
                httpVersion: "HTTP/1.1", headerFields: nil
            )!
            return (resp, payload)
        }

        let rest = RESTClient(host: "192.168.4.1", session: session)
        let vm = PresetsViewModel()

        XCTAssertEqual(vm.effectPresets.count, 0)
        XCTAssertFalse(vm.loading)

        await vm.loadEffectPresets(client: rest)

        XCTAssertFalse(vm.loading, "VM must clear loading flag after fetch completes")
        XCTAssertNil(vm.error, "Successful fetch must leave error nil")
        XCTAssertEqual(vm.effectPresets.count, 2)
        XCTAssertEqual(vm.effectPresets.first?.id, 0)
        XCTAssertEqual(vm.effectPresets.first?.name, "Sunset")
        XCTAssertEqual(vm.effectPresets.first?.effectId, 42)
    }

    /// `loadZonePresets(client:)` should hit `/api/v1/zone-presets` and decode
    /// the firmware's mixed built-in / user-saved list correctly.
    func test_loadsZonePresetsViaREST() async throws {
        let payload = """
        {
          "success": true,
          "data": {
            "presets": [
              {"id": 0, "name": "Unified", "zoneCount": 1, "builtin": true,  "timestamp": 0},
              {"id": 5, "name": "My Mix",  "zoneCount": 3, "builtin": false, "timestamp": 12345}
            ],
            "count": 2
          }
        }
        """.data(using: .utf8)!

        MockURLProtocol.requestHandler = { request in
            XCTAssertEqual(request.url?.path, "/api/v1/zone-presets",
                           "Zone-preset list must hit /api/v1/zone-presets")
            XCTAssertEqual(request.httpMethod, "GET")
            let resp = HTTPURLResponse(
                url: request.url!, statusCode: 200,
                httpVersion: "HTTP/1.1", headerFields: nil
            )!
            return (resp, payload)
        }

        let rest = RESTClient(host: "192.168.4.1", session: session)
        let vm = PresetsViewModel()

        await vm.loadZonePresets(client: rest)

        XCTAssertFalse(vm.loading)
        XCTAssertNil(vm.error)
        XCTAssertEqual(vm.zonePresets.count, 2)
        XCTAssertEqual(vm.zonePresets[0].builtin, true,
                       "Built-in flag must round-trip via decoder")
        XCTAssertEqual(vm.zonePresets[1].builtin, false)
        XCTAssertEqual(vm.zonePresets[1].name, "My Mix")
    }

    /// HTTP failure on list-load must populate `error` and clear `loading`.
    func test_loadEffectPresetsRecordsErrorOnHTTPFailure() async throws {
        MockURLProtocol.requestHandler = { request in
            let resp = HTTPURLResponse(
                url: request.url!, statusCode: 500,
                httpVersion: "HTTP/1.1", headerFields: nil
            )!
            return (resp, "internal".data(using: .utf8))
        }

        let rest = RESTClient(host: "192.168.4.1", session: session)
        let vm = PresetsViewModel()

        await vm.loadEffectPresets(client: rest)

        XCTAssertFalse(vm.loading)
        XCTAssertNotNil(vm.error, "HTTP 500 must surface as a non-nil error string")
    }

    // MARK: - Effect preset WS commands

    /// `saveCurrentEffectPreset(slot:name:ws:)` must dispatch
    /// `effectPresets.saveCurrent` with the slot and name fields the firmware
    /// expects.
    func test_savesCurrentEffectPresetViaWSCommand() async throws {
        let sink = RecordingPresetsCommandSink()
        let vm = PresetsViewModel()

        await vm.saveCurrentEffectPreset(slot: 3, name: "My Effect", ws: sink)

        let invocations = sink.invocations
        XCTAssertEqual(invocations.count, 1)
        XCTAssertEqual(invocations.first?.command, "effectPresets.saveCurrent")
        XCTAssertEqual(invocations.first?.params["slot"], "3")
        XCTAssertEqual(invocations.first?.params["name"], "My Effect")
    }

    /// `loadEffectPreset(id:ws:)` must dispatch `effectPresets.load` with `id`.
    func test_loadsEffectPresetViaWSCommand() async throws {
        let sink = RecordingPresetsCommandSink()
        let vm = PresetsViewModel()

        await vm.loadEffectPreset(id: 7, ws: sink)

        let invocations = sink.invocations
        XCTAssertEqual(invocations.count, 1)
        XCTAssertEqual(invocations.first?.command, "effectPresets.load")
        XCTAssertEqual(invocations.first?.params["id"], "7")
    }

    /// `deleteEffectPreset(id:ws:)` must dispatch `effectPresets.delete` with `id`.
    func test_deletesEffectPresetViaWSCommand() async throws {
        let sink = RecordingPresetsCommandSink()
        let vm = PresetsViewModel()

        await vm.deleteEffectPreset(id: 2, ws: sink)

        let invocations = sink.invocations
        XCTAssertEqual(invocations.count, 1)
        XCTAssertEqual(invocations.first?.command, "effectPresets.delete")
        XCTAssertEqual(invocations.first?.params["id"], "2")
    }

    // MARK: - Zone preset WS commands

    /// `saveCurrentZonePreset(slot:name:ws:)` must dispatch
    /// `zonePresets.saveCurrent` with the slot/name fields the firmware expects.
    func test_savesCurrentZonePresetViaWSCommand() async throws {
        let sink = RecordingPresetsCommandSink()
        let vm = PresetsViewModel()

        await vm.saveCurrentZonePreset(slot: 8, name: "Triple Split", ws: sink)

        let invocations = sink.invocations
        XCTAssertEqual(invocations.count, 1)
        XCTAssertEqual(invocations.first?.command, "zonePresets.saveCurrent")
        XCTAssertEqual(invocations.first?.params["slot"], "8")
        XCTAssertEqual(invocations.first?.params["name"], "Triple Split")
    }

    /// `loadZonePreset(id:ws:)` must dispatch `zonePresets.load` with `id`.
    func test_loadsZonePresetViaWSCommand() async throws {
        let sink = RecordingPresetsCommandSink()
        let vm = PresetsViewModel()

        await vm.loadZonePreset(id: 5, ws: sink)

        let invocations = sink.invocations
        XCTAssertEqual(invocations.count, 1)
        XCTAssertEqual(invocations.first?.command, "zonePresets.load")
        XCTAssertEqual(invocations.first?.params["id"], "5")
    }

    /// `deleteZonePreset(id:ws:)` must dispatch `zonePresets.delete` with `id`.
    func test_deletesZonePresetViaWSCommand() async throws {
        let sink = RecordingPresetsCommandSink()
        let vm = PresetsViewModel()

        await vm.deleteZonePreset(id: 4, ws: sink)

        let invocations = sink.invocations
        XCTAssertEqual(invocations.count, 1)
        XCTAssertEqual(invocations.first?.command, "zonePresets.delete")
        XCTAssertEqual(invocations.first?.params["id"], "4")
    }

    // MARK: - Broadcast-driven refresh

    /// When the `effectPresets.saved` broadcast arrives, the VM must refetch
    /// the effect-preset list so the UI reflects the newly-persisted slot.
    /// Implemented as a public `handleEffectPresetsSavedBroadcast(client:)`
    /// hook the AppViewModel call site (Phase 3) will invoke; tested here in
    /// isolation by calling the hook directly.
    func test_effectPresetSavedBroadcastRefreshesList() async throws {
        let firstPayload = """
        {"success":true,"data":{"presets":[{"id":0,"name":"A","effectId":1}],"count":1,"maxSlots":16}}
        """.data(using: .utf8)!
        let secondPayload = """
        {"success":true,"data":{"presets":[{"id":0,"name":"A","effectId":1},{"id":1,"name":"B","effectId":2}],"count":2,"maxSlots":16}}
        """.data(using: .utf8)!

        let counter = HitCounter()
        MockURLProtocol.requestHandler = { request in
            let payload = counter.next() == 1 ? firstPayload : secondPayload
            let resp = HTTPURLResponse(
                url: request.url!, statusCode: 200,
                httpVersion: "HTTP/1.1", headerFields: nil
            )!
            return (resp, payload)
        }

        let rest = RESTClient(host: "192.168.4.1", session: session)
        let vm = PresetsViewModel()

        await vm.loadEffectPresets(client: rest)
        XCTAssertEqual(vm.effectPresets.count, 1)

        // Simulate the inbound broadcast.
        await vm.handleEffectPresetsSavedBroadcast(client: rest)

        XCTAssertEqual(vm.effectPresets.count, 2,
                       "Broadcast handler must trigger a refetch")
    }

    /// Same contract for the `effectPresets.deleted` broadcast.
    func test_effectPresetDeletedBroadcastRefreshesList() async throws {
        let firstPayload = """
        {"success":true,"data":{"presets":[{"id":0,"name":"A","effectId":1},{"id":1,"name":"B","effectId":2}],"count":2,"maxSlots":16}}
        """.data(using: .utf8)!
        let secondPayload = """
        {"success":true,"data":{"presets":[{"id":0,"name":"A","effectId":1}],"count":1,"maxSlots":16}}
        """.data(using: .utf8)!

        let counter = HitCounter()
        MockURLProtocol.requestHandler = { request in
            let payload = counter.next() == 1 ? firstPayload : secondPayload
            let resp = HTTPURLResponse(
                url: request.url!, statusCode: 200,
                httpVersion: "HTTP/1.1", headerFields: nil
            )!
            return (resp, payload)
        }

        let rest = RESTClient(host: "192.168.4.1", session: session)
        let vm = PresetsViewModel()

        await vm.loadEffectPresets(client: rest)
        XCTAssertEqual(vm.effectPresets.count, 2)

        await vm.handleEffectPresetsDeletedBroadcast(client: rest)

        XCTAssertEqual(vm.effectPresets.count, 1,
                       "Delete-broadcast handler must trigger a refetch")
    }
}

// MARK: - Test helpers

/// Tiny thread-safe counter shared between concurrent MockURLProtocol invocations.
private final class HitCounter: @unchecked Sendable {
    private let storage = OSAllocatedUnfairLock<Int>(initialState: 0)
    func next() -> Int {
        storage.withLock { count in
            count += 1
            return count
        }
    }
}
