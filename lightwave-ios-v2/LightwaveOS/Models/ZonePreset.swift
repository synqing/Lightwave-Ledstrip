//
//  ZonePreset.swift
//  LightwaveOS
//
//  Built-in zone layout presets.
//

import Foundation

/// Built-in zone preset (0-4)
struct ZonePreset: Codable, Sendable, Identifiable, Hashable {
    /// Preset ID (0-4)
    let id: Int

    /// Display name
    let name: String

    // MARK: - Initialization

    init(id: Int, name: String) {
        self.id = id
        self.name = name
    }

    // MARK: - Built-in Presets

    /// All built-in zone presets (matches firmware)
    static let builtInPresets: [ZonePreset] = [
        ZonePreset(id: 0, name: "Unified"),
        ZonePreset(id: 1, name: "Dual Split"),
        ZonePreset(id: 2, name: "Triple Rings"),
        ZonePreset(id: 3, name: "Quad Active"),
        ZonePreset(id: 4, name: "Heartbeat Focus")
    ]

    /// Get preset by ID with fallback
    static func preset(id: Int) -> ZonePreset {
        builtInPresets.first(where: { $0.id == id }) ?? builtInPresets[0]
    }
}

// MARK: - Sample Data

#if DEBUG
extension ZonePreset {
    static let preview = ZonePreset(id: 0, name: "Unified")
    static let previewDualSplit = ZonePreset(id: 1, name: "Dual Split")
    static let previewTripleRings = ZonePreset(id: 2, name: "Triple Rings")
}
#endif
