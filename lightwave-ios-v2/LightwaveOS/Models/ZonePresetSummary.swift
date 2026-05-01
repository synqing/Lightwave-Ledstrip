//
//  ZonePresetSummary.swift
//  LightwaveOS
//
//  Phase 2 (P2-3) — DTO for `GET /api/v1/zone-presets` list rows.
//  iOS 17+, Swift 6 with strict concurrency. British English comments.
//

import Foundation

// MARK: - ZonePresetSummary

/// One row in the zone-preset list returned by `GET /api/v1/zone-presets`.
///
/// Per firmware (`ZonePresetHandlers::handleList` —
/// `firmware-v3/src/network/webserver/handlers/ZonePresetHandlers.cpp`),
/// the response contains both built-in factory presets (`builtin: true`,
/// timestamp 0) and user-saved presets (`builtin: false`, timestamp = save
/// time). Both kinds are represented uniformly here; the UI can branch on
/// `builtin` to decide whether deletion is permitted.
///
/// This type is deliberately separate from `Models/ZonePreset.swift` (which
/// hard-codes the four legacy built-in zone layouts shown in the UI before the
/// firmware-driven preset list landed). New code targeting the live
/// `/api/v1/zone-presets` endpoint should use `ZonePresetSummary`.
struct ZonePresetSummary: Codable, Sendable, Identifiable, Hashable {
    /// Slot identifier (built-in IDs and user IDs share one numeric space).
    let id: Int
    /// User-supplied or factory-defined display name.
    let name: String
    /// Number of active zones in the saved configuration.
    let zoneCount: Int?
    /// `true` when this is a factory preset — UI must hide delete affordance.
    let builtin: Bool?
    /// Seconds-since-boot when saved (built-in presets always carry 0).
    let timestamp: Int?
}

// MARK: - ZonePresetsListResponse

/// Envelope returned by `GET /api/v1/zone-presets`.
///
/// Wire shape:
/// ```json
/// {
///   "success": true,
///   "data": {
///     "presets": [{"id": 0, "name": "Unified", "zoneCount": 1,
///                  "builtin": true, "timestamp": 0}, ...],
///     "count": 7
///   }
/// }
/// ```
struct ZonePresetsListResponse: Codable, Sendable {
    let success: Bool
    let data: ListData
    let timestamp: Int?

    struct ListData: Codable, Sendable {
        let presets: [ZonePresetSummary]
        let count: Int?
    }
}
