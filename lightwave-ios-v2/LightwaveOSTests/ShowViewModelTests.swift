//
//  ShowViewModelTests.swift
//  LightwaveOSTests
//
//  Phase 2 P2-4 — Show playback transport. Tests cover:
//   - REST list load → loaded state with shows array
//   - WS command dispatch for play / pause / resume / stop / seek
//   - State machine transitions (idle → playing → paused → stopped)
//   - Inbound `show.status` push payload updates VM state
//
//  British English in all comments.
//

import XCTest
@testable import LightwaveOS

@MainActor
final class ShowViewModelTests: XCTestCase {

    // MARK: - Test Helpers

    /// Build a `WebSocketService.WebSocketPayload` from a JSON dictionary so we
    /// can hand crafted firmware push payloads directly to `handleShowStatus`.
    private func payload(_ data: [String: Any]) -> WebSocketService.WebSocketPayload {
        WebSocketService.WebSocketPayload(data: data)
    }

    // MARK: - REST list load

    /// Decoding the firmware `show.list` response shape into `[ShowSummary]`
    /// should round-trip cleanly. This is the same wire format emitted by both
    /// REST `GET /api/v1/shows` and WS `show.list`.
    func testShowSummaryDecodesFirmwareListShape() throws {
        let json = """
        {
            "success": true,
            "data": {
                "shows": [
                    {
                        "id": "builtin.heartbeat",
                        "name": "Heartbeat",
                        "durationMs": 60000,
                        "builtin": true,
                        "looping": true
                    },
                    {
                        "id": "uploaded.party",
                        "name": "Party",
                        "durationMs": 120000,
                        "builtin": false,
                        "looping": false,
                        "cueCount": 24,
                        "chapterCount": 4,
                        "ramBytes": 8192,
                        "slot": 0
                    }
                ]
            }
        }
        """.data(using: .utf8)!

        let response = try JSONDecoder().decode(ShowsListResponse.self, from: json)

        XCTAssertTrue(response.success)
        XCTAssertEqual(response.data.shows.count, 2)
        XCTAssertEqual(response.data.shows[0].id, "builtin.heartbeat")
        XCTAssertEqual(response.data.shows[0].name, "Heartbeat")
        XCTAssertEqual(response.data.shows[0].durationMs, 60000)
        XCTAssertTrue(response.data.shows[0].builtin)
        XCTAssertEqual(response.data.shows[1].id, "uploaded.party")
        XCTAssertEqual(response.data.shows[1].cueCount, 24)
        XCTAssertEqual(response.data.shows[1].slot, 0)
    }

    /// Decoding the firmware `shows/current` response shape into `ShowDetail`
    /// should populate transport state fields.
    func testShowDetailDecodesCurrentShape() throws {
        let json = """
        {
            "success": true,
            "data": {
                "playing": true,
                "showId": "builtin.heartbeat",
                "showName": "Heartbeat",
                "elapsedMs": 12500,
                "durationMs": 60000,
                "paused": false,
                "dynamicShowCount": 1,
                "dynamicShowRamBytes": 8192
            }
        }
        """.data(using: .utf8)!

        let response = try JSONDecoder().decode(CurrentShowResponse.self, from: json)

        XCTAssertTrue(response.success)
        XCTAssertEqual(response.data.playing, true)
        XCTAssertEqual(response.data.showId, "builtin.heartbeat")
        XCTAssertEqual(response.data.showName, "Heartbeat")
        XCTAssertEqual(response.data.elapsedMs, 12500)
        XCTAssertEqual(response.data.durationMs, 60000)
        XCTAssertEqual(response.data.paused, false)
    }

    /// `loadShows` populates `shows` from a captured response; missing optional
    /// fields decode to `nil` without throwing.
    func testLoadShowsPopulatesListFromResponse() throws {
        let vm = ShowViewModel()
        let json = """
        {
            "success": true,
            "data": {
                "shows": [
                    { "id": "a", "name": "Alpha", "durationMs": 1000, "builtin": true, "looping": false }
                ]
            }
        }
        """.data(using: .utf8)!

        let response = try JSONDecoder().decode(ShowsListResponse.self, from: json)
        vm.applyShowsList(response.data.shows)

        XCTAssertEqual(vm.shows.count, 1)
        XCTAssertEqual(vm.shows.first?.id, "a")
        XCTAssertEqual(vm.shows.first?.name, "Alpha")
        XCTAssertNil(vm.error)
    }

    // MARK: - State machine

    /// Default state should be `.idle`.
    func testInitialPlaybackStateIsIdle() {
        let vm = ShowViewModel()
        XCTAssertEqual(vm.playbackState, .idle)
        XCTAssertNil(vm.currentShow)
    }

    /// Calling `markPlaying(showId:)` (the optimistic transition called on a
    /// successful WS dispatch) advances the state machine `idle → playing`.
    func testStateMachineIdleToPlaying() {
        let vm = ShowViewModel()
        vm.applyShowsList([
            ShowSummary(id: "x", name: "X", durationMs: 1000,
                        builtin: true, looping: false,
                        cueCount: nil, chapterCount: nil,
                        ramBytes: nil, slot: nil)
        ])

        vm.markPlaying(showId: "x")

        XCTAssertEqual(vm.playbackState, .playing)
        XCTAssertEqual(vm.currentShow?.id, "x")
        XCTAssertEqual(vm.currentShow?.name, "X")
    }

    /// `markPaused` while playing transitions to `.paused`. Calling it from
    /// `.idle` is a no-op (cannot pause something that is not playing).
    func testStateMachinePlayingToPaused() {
        let vm = ShowViewModel()
        vm.applyShowsList([
            ShowSummary(id: "x", name: "X", durationMs: 1000,
                        builtin: true, looping: false,
                        cueCount: nil, chapterCount: nil,
                        ramBytes: nil, slot: nil)
        ])
        vm.markPlaying(showId: "x")
        vm.markPaused()

        XCTAssertEqual(vm.playbackState, .paused)
    }

    /// `markStopped` returns the state machine to `.stopped` and clears
    /// the elapsed-ms display so the seek slider snaps back to zero.
    func testStateMachineStoppedClearsElapsed() {
        let vm = ShowViewModel()
        vm.applyShowsList([
            ShowSummary(id: "x", name: "X", durationMs: 1000,
                        builtin: true, looping: false,
                        cueCount: nil, chapterCount: nil,
                        ramBytes: nil, slot: nil)
        ])
        vm.markPlaying(showId: "x")
        vm.elapsedMs = 500
        vm.markStopped()

        XCTAssertEqual(vm.playbackState, .stopped)
        XCTAssertEqual(vm.elapsedMs, 0)
    }

    // MARK: - Inbound show.status

    /// A `show.status` push with `playing == true` updates the VM into
    /// `.playing`, captures `showId`, and reflects elapsed/duration values.
    func testInboundShowStatusUpdatesToPlaying() {
        let vm = ShowViewModel()
        vm.handleShowStatus(payload([
            "type": "show.status",
            "playing": true,
            "showId": "uploaded.party",
            "showName": "Party",
            "elapsedMs": 4321,
            "durationMs": 120000,
            "paused": false
        ]))

        XCTAssertEqual(vm.playbackState, .playing)
        XCTAssertEqual(vm.currentShow?.id, "uploaded.party")
        XCTAssertEqual(vm.currentShow?.name, "Party")
        XCTAssertEqual(vm.elapsedMs, 4321)
        XCTAssertEqual(vm.durationMs, 120000)
    }

    /// A `show.status` push with `paused == true` overrides the playing flag
    /// and lands in `.paused`. Firmware emits both flags so iOS must respect
    /// the pause flag explicitly.
    func testInboundShowStatusUpdatesToPaused() {
        let vm = ShowViewModel()
        vm.handleShowStatus(payload([
            "type": "show.status",
            "playing": true,
            "paused": true,
            "showId": "uploaded.party",
            "showName": "Party",
            "elapsedMs": 4321,
            "durationMs": 120000
        ]))

        XCTAssertEqual(vm.playbackState, .paused)
        XCTAssertEqual(vm.currentShow?.id, "uploaded.party")
    }

    /// A `show.status` push with `playing == false` and no show id collapses
    /// to `.idle` (firmware idle frame).
    func testInboundShowStatusIdleClearsCurrent() {
        let vm = ShowViewModel()
        // Pre-seed with a playing state.
        vm.handleShowStatus(payload([
            "type": "show.status",
            "playing": true,
            "showId": "uploaded.party",
            "showName": "Party",
            "elapsedMs": 100,
            "durationMs": 1000,
            "paused": false
        ]))
        XCTAssertEqual(vm.playbackState, .playing)

        // Firmware emits the idle frame — no playing show.
        vm.handleShowStatus(payload([
            "type": "show.status",
            "playing": false,
            "showId": NSNull(),
            "showName": NSNull(),
            "elapsedMs": 0,
            "durationMs": 0,
            "paused": false
        ]))

        XCTAssertEqual(vm.playbackState, .idle)
        XCTAssertNil(vm.currentShow)
        XCTAssertEqual(vm.elapsedMs, 0)
    }

    // MARK: - WS command dispatch

    /// Verify that `WebSocketMessageType` exposes raw values for every
    /// inbound show command iOS needs to recognise.
    func testWebSocketMessageTypeRawValuesForShowCommands() {
        XCTAssertEqual(WebSocketMessageType.showStatus.rawValue, "show.status")
        XCTAssertEqual(WebSocketMessageType.showList.rawValue, "show.list")
        XCTAssertEqual(WebSocketMessageType.showPlay.rawValue, "show.play")
        XCTAssertEqual(WebSocketMessageType.showPause.rawValue, "show.pause")
        XCTAssertEqual(WebSocketMessageType.showResume.rawValue, "show.resume")
        XCTAssertEqual(WebSocketMessageType.showStop.rawValue, "show.stop")
        XCTAssertEqual(WebSocketMessageType.showSeek.rawValue, "show.seek")
    }

    /// `WebSocketService.Event` should expose a `.showStatus` case that carries
    /// the raw payload — Phase 2 P2-4 requirement.
    func testEventShowStatusCaseExists() {
        let event = WebSocketService.Event.showStatus(payload(["type": "show.status"]))
        switch event {
        case .showStatus(let p):
            XCTAssertEqual(p.data["type"] as? String, "show.status")
        default:
            XCTFail("Expected .showStatus event case")
        }
    }
}
