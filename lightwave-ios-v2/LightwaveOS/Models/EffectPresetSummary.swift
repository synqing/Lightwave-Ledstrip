//
//  EffectPresetSummary.swift
//  LightwaveOS
//
//  Phase 2 (P2-3) — DTO for `GET /api/v1/effect-presets` list rows.
//  iOS 17+, Swift 6 with strict concurrency. British English comments.
//

import Foundation

// MARK: - EffectPresetSummary

/// One row in the user-saved effect-preset list returned by
/// `GET /api/v1/effect-presets`.
///
/// Per firmware (`EffectPresetHandlers::handleList` —
/// `firmware-v3/src/network/webserver/handlers/EffectPresetHandlers.cpp`),
/// each row carries the slot `id`, `name`, and the `effectId` that the preset
/// was captured against. The detailed payload (parameters, palette, etc.) is
/// fetched on demand via `GET /api/v1/effect-presets/get?id=<id>` — this
/// summary type intentionally omits those fields.
struct EffectPresetSummary: Codable, Sendable, Identifiable, Hashable {
    /// Slot index (0-15 per firmware's `MAX_PRESETS = 16`).
    let id: Int
    /// User-supplied display name (1-31 characters).
    let name: String
    /// Effect ID captured when the preset was saved.
    let effectId: Int
}

// MARK: - EffectPresetsListResponse

/// Envelope returned by `GET /api/v1/effect-presets`.
///
/// Wire shape:
/// ```json
/// {
///   "success": true,
///   "data": {
///     "presets": [{"id": 0, "name": "...", "effectId": 42}, ...],
///     "count": 3,
///     "maxSlots": 16
///   }
/// }
/// ```
struct EffectPresetsListResponse: Codable, Sendable {
    let success: Bool
    let data: ListData
    let timestamp: Int?

    struct ListData: Codable, Sendable {
        let presets: [EffectPresetSummary]
        let count: Int?
        let maxSlots: Int?
    }
}
