//
//  ShowViewModel.swift
//  LightwaveOS
//
//  Phase 2 P2-4 — Show playback transport.
//
//  State of record for show playback on the iOS client: list of available
//  shows (read via REST), the currently playing show, and the playback
//  state machine (idle / loading / playing / paused / stopped).
//
//  - List load goes through `RESTClient.getShows()`.
//  - Transport commands (play / pause / resume / stop / seek) are dispatched
//    over WebSocket for low latency.
//  - Inbound `show.status` broadcasts call `handleShowStatus(_:)` to keep
//    the local state machine aligned with the firmware.
//
//  British English in all comments / logs / UI strings.
//

import Foundation
import Observation

@MainActor
@Observable
class ShowViewModel {

    // MARK: - State Machine

    /// Playback state machine. Mirrors the firmware-side phases reported via
    /// `show.status` plus a local `.loading` state used while the REST list
    /// request is in flight.
    enum PlaybackState: Equatable, Sendable {
        case idle
        case loading
        case playing
        case paused
        case stopped
    }

    // MARK: - Observable State

    /// Available shows (built-in + dynamic). Populated by `loadShows`.
    var shows: [ShowSummary] = []
    /// The show currently playing (or paused). `nil` when idle.
    var currentShow: ShowSummary?
    /// Current playback state.
    var playbackState: PlaybackState = .idle
    /// Milliseconds elapsed within the current show. Drives the seek slider.
    var elapsedMs: Int = 0
    /// Duration of the current show in milliseconds.
    var durationMs: Int = 0
    /// Last user-facing error message, or nil.
    var error: String?

    // MARK: - Network Dependencies

    /// Injected by `AppViewModel` after a successful connect.
    var restClient: RESTClient?
    /// Injected by `AppViewModel` after a successful connect.
    var ws: WebSocketService?

    // MARK: - REST: List Load

    /// Load the list of shows from the device. Transitions the state machine
    /// through `.loading`; on success preserves the prior playback state and
    /// merely refreshes the `shows` array.
    func loadShows() async {
        guard let client = restClient else {
            error = "No REST client configured"
            return
        }

        let priorState = playbackState
        playbackState = .loading

        do {
            let response = try await client.getShows()
            applyShowsList(response.data.shows)
            // Restore the previous playback state — list reload is independent
            // of transport state.
            playbackState = priorState == .loading ? .idle : priorState
            error = nil
        } catch {
            self.error = "Failed to load shows: \(error.localizedDescription)"
            playbackState = priorState == .loading ? .idle : priorState
        }
    }

    /// Internal hook used both by `loadShows` and by tests. Sets `shows`
    /// directly without touching the playback state machine.
    func applyShowsList(_ list: [ShowSummary]) {
        self.shows = list
    }

    // MARK: - WS: Transport Commands

    /// Begin playback of the show with the given id. Optimistically updates
    /// the state machine to `.playing` so the UI responds without waiting
    /// for the firmware status push.
    func play(showId: String) async {
        guard let ws = ws else {
            error = "No WebSocket service configured"
            return
        }
        await ws.sendShowPlay(showId: showId)
        markPlaying(showId: showId)
    }

    /// Pause the currently playing show.
    func pause() async {
        guard let ws = ws else { return }
        await ws.sendShowPause()
        markPaused()
    }

    /// Resume a paused show.
    func resume() async {
        guard let ws = ws else { return }
        await ws.sendShowResume()
        // Optimistic — firmware will confirm via show.status.
        if playbackState == .paused {
            playbackState = .playing
        }
    }

    /// Stop the currently playing show. Returns the state machine to
    /// `.stopped` and clears the elapsed-ms display.
    func stop() async {
        guard let ws = ws else { return }
        await ws.sendShowStop()
        markStopped()
    }

    /// Seek the currently playing show to the given millisecond position.
    /// The firmware accepts a `timeMs` field; clamp to `[0, durationMs]`.
    func seek(toMillis ms: Int) async {
        guard let ws = ws else { return }
        let clamped = max(0, min(ms, max(0, durationMs)))
        elapsedMs = clamped
        await ws.sendShowSeek(timeMs: clamped)
    }

    /// Request a fresh `show.status` push from the firmware over WS.
    func requestStatus() async {
        guard let ws = ws else { return }
        await ws.sendShowStatus()
    }

    // MARK: - State Machine Helpers

    /// Optimistic transition: idle/stopped/paused → playing.
    func markPlaying(showId: String) {
        if let match = shows.first(where: { $0.id == showId }) {
            currentShow = match
            durationMs = match.durationMs
        }
        playbackState = .playing
    }

    /// playing → paused (no-op from any other state).
    func markPaused() {
        if playbackState == .playing {
            playbackState = .paused
        }
    }

    /// any → stopped, clears elapsed.
    func markStopped() {
        playbackState = .stopped
        elapsedMs = 0
    }

    // MARK: - WS: Inbound show.status

    /// Apply a `show.status` push from the firmware. The firmware emits a
    /// flat object with `playing`, `paused`, `showId`, `showName`,
    /// `elapsedMs`, `durationMs` keys. We update the state machine and the
    /// associated metadata atomically.
    func handleShowStatus(_ payload: WebSocketService.WebSocketPayload) {
        let data = payload.data

        let playing = data["playing"] as? Bool ?? false
        let paused = data["paused"] as? Bool ?? false
        let showId = data["showId"] as? String
        let showName = data["showName"] as? String
        let elapsed = data["elapsedMs"] as? Int ?? 0
        let duration = data["durationMs"] as? Int ?? 0

        elapsedMs = elapsed
        durationMs = duration

        // Resolve currentShow. If we already have it in the catalogue, use the
        // catalogue entry (richer metadata); otherwise build a minimal stub
        // from the status fields so the UI can show the name immediately.
        if let id = showId, !id.isEmpty {
            if let match = shows.first(where: { $0.id == id }) {
                currentShow = match
            } else {
                currentShow = ShowSummary(
                    id: id,
                    name: showName ?? id,
                    durationMs: duration,
                    builtin: false,
                    looping: false,
                    cueCount: nil,
                    chapterCount: nil,
                    ramBytes: nil,
                    slot: nil
                )
            }
        } else {
            currentShow = nil
        }

        // Resolve state. `paused` overrides `playing`. No active show → idle.
        if paused && (showId != nil) {
            playbackState = .paused
        } else if playing && (showId != nil) {
            playbackState = .playing
        } else {
            playbackState = .idle
            elapsedMs = 0
        }
    }
}
