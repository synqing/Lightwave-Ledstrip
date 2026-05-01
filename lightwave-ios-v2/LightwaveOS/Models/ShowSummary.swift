//
//  ShowSummary.swift
//  LightwaveOS
//
//  Phase 2 P2-4 — Show playback transport.
//  A summary entry returned by `GET /api/v1/shows` and the WS `show.list`
//  command. The firmware emits both built-in (PROGMEM) shows and dynamic
//  (PSRAM-uploaded) shows in the same array — the `builtin` flag
//  distinguishes them. Dynamic shows additionally carry `cueCount`,
//  `chapterCount`, `ramBytes`, and `slot` fields.
//
//  Wire shape per `firmware-v3/src/network/webserver/handlers/ShowHandlers.cpp`
//  and `firmware-v3/src/network/webserver/ws/WsShowCommands.cpp`.
//
//  British English in all comments.
//

import Foundation

/// A single show row as returned by firmware in the shows list.
struct ShowSummary: Codable, Sendable, Identifiable, Equatable {
    /// Unique show identifier (string, e.g. `"builtin.heartbeat"`).
    let id: String
    /// Human-readable show name.
    let name: String
    /// Total show duration in milliseconds. Drives the seek slider range.
    let durationMs: Int
    /// `true` if this is a built-in PROGMEM show, `false` if dynamic (uploaded).
    let builtin: Bool
    /// Whether the show loops on completion.
    let looping: Bool

    // The following fields are emitted ONLY for dynamic uploaded shows.
    // Built-in shows omit them entirely so the optional decode produces nil.

    /// Number of cues in the show timeline (dynamic only).
    let cueCount: Int?
    /// Number of chapters in the narrative structure (dynamic only).
    let chapterCount: Int?
    /// PSRAM occupied by this show, in bytes (dynamic only).
    let ramBytes: Int?
    /// Slot index `0..3` in `DynamicShowStore` (dynamic only).
    let slot: Int?
}

/// REST response wrapper for `GET /api/v1/shows`.
struct ShowsListResponse: Codable, Sendable {
    let success: Bool
    let data: ShowsListData
    let timestamp: Int?

    struct ShowsListData: Codable, Sendable {
        let shows: [ShowSummary]
    }
}
