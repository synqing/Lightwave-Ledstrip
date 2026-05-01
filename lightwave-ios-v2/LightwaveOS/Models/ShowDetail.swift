//
//  ShowDetail.swift
//  LightwaveOS
//
//  Phase 2 P2-4 — Show playback transport.
//  The current playback frame returned by `GET /api/v1/shows/current`
//  and broadcast on the WS `show.status` channel. Conveys whether a show
//  is playing, which one, the elapsed/duration timestamps, and whether
//  it is currently paused.
//
//  Wire shape per `firmware-v3/src/network/webserver/handlers/ShowHandlers.cpp`
//  and `firmware-v3/src/network/webserver/ws/WsShowCommands.cpp` (see
//  `handleShowStatus` and `ShowHandlers::handleCurrent`).
//
//  British English in all comments.
//

import Foundation

/// Active playback state for the current show. Modelled as Codable for the
/// REST `current` endpoint but the same field set is reused by the WS
/// `show.status` push (decoded ad-hoc in `ShowViewModel.handleShowStatus`).
struct ShowDetail: Codable, Sendable, Equatable {
    /// `true` if a show is currently playing (regardless of pause state).
    let playing: Bool
    /// Identifier of the playing show, or nil when idle.
    let showId: String?
    /// Display name of the playing show, or nil when idle.
    let showName: String?
    /// Milliseconds elapsed since playback started.
    let elapsedMs: Int
    /// Total duration of the playing show in milliseconds.
    let durationMs: Int
    /// `true` when the playing show is paused (transport-level pause, not stop).
    let paused: Bool
    /// Number of dynamic shows currently uploaded — diagnostic field.
    let dynamicShowCount: Int?
    /// Total PSRAM used by dynamic shows, in bytes — diagnostic field.
    let dynamicShowRamBytes: Int?
}

/// REST response wrapper for `GET /api/v1/shows/current`.
struct CurrentShowResponse: Codable, Sendable {
    let success: Bool
    let data: ShowDetail
    let timestamp: Int?
}
