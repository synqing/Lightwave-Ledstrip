//
//  SavedZonePreset.swift
//  LightwaveOS
//
//  User-saved zone preset with full zone configuration.
//  Distinct from ZonePreset which represents built-in layout presets.
//

import Foundation

/// User-saved zone preset containing complete zone configuration
struct SavedZonePreset: Identifiable, Codable, Sendable, Hashable {
    /// Preset ID (server-assigned)
    let id: Int

    /// User-defined preset name
    let name: String

    /// Number of active zones in this preset
    let zoneCount: Int

    /// Optional array of zone configurations
    let zones: [ZonePresetEntry]?

    /// Creation/modification timestamp (Unix epoch seconds)
    let timestamp: TimeInterval?

    // MARK: - Nested Types

    /// Configuration for a single zone within a preset
    struct ZonePresetEntry: Codable, Sendable, Hashable {
        /// Effect ID for this zone
        let effectId: Int

        /// Palette ID for this zone
        let paletteId: Int

        /// Brightness level (0-255)
        let brightness: Int

        /// Speed level (0-255)
        let speed: Int

        /// Blend mode (0-4: Replace, Add, Screen, Multiply, Overlay)
        let blendMode: Int

        /// LED segment assignments for this zone
        let segments: ZoneSegments

        // MARK: - Nested Types

        /// LED segment ranges for a zone (centre-origin layout)
        struct ZoneSegments: Codable, Sendable, Hashable {
            /// Strip 1 left segment start index
            let s1LeftStart: Int

            /// Strip 1 left segment end index
            let s1LeftEnd: Int

            /// Strip 1 right segment start index
            let s1RightStart: Int

            /// Strip 1 right segment end index
            let s1RightEnd: Int

            // MARK: - Initialization

            init(
                s1LeftStart: Int,
                s1LeftEnd: Int,
                s1RightStart: Int,
                s1RightEnd: Int
            ) {
                self.s1LeftStart = s1LeftStart
                self.s1LeftEnd = s1LeftEnd
                self.s1RightStart = s1RightStart
                self.s1RightEnd = s1RightEnd
            }
        }

        // MARK: - Initialization

        init(
            effectId: Int,
            paletteId: Int,
            brightness: Int,
            speed: Int,
            blendMode: Int,
            segments: ZoneSegments
        ) {
            self.effectId = effectId
            self.paletteId = paletteId
            self.brightness = brightness
            self.speed = speed
            self.blendMode = blendMode
            self.segments = segments
        }
    }

    // MARK: - Initialization

    init(
        id: Int,
        name: String,
        zoneCount: Int,
        zones: [ZonePresetEntry]? = nil,
        timestamp: TimeInterval? = nil
    ) {
        self.id = id
        self.name = name
        self.zoneCount = zoneCount
        self.zones = zones
        self.timestamp = timestamp
    }

    // MARK: - Computed Properties

    /// Display name for UI
    var displayName: String { name }

    /// Formatted date string for display
    var formattedDate: String? {
        guard let timestamp = timestamp else { return nil }
        let date = Date(timeIntervalSince1970: timestamp)
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        formatter.timeStyle = .short
        return formatter.string(from: date)
    }
}

// MARK: - Sample Data

#if DEBUG
extension SavedZonePreset {
    static let preview = SavedZonePreset(
        id: 1,
        name: "Party Mode",
        zoneCount: 2,
        zones: [
            ZonePresetEntry(
                effectId: 12,
                paletteId: 8,
                brightness: 255,
                speed: 200,
                blendMode: 0,
                segments: ZonePresetEntry.ZoneSegments(
                    s1LeftStart: 0,
                    s1LeftEnd: 79,
                    s1RightStart: 80,
                    s1RightEnd: 159
                )
            ),
            ZonePresetEntry(
                effectId: 5,
                paletteId: 12,
                brightness: 200,
                speed: 128,
                blendMode: 1,
                segments: ZonePresetEntry.ZoneSegments(
                    s1LeftStart: 160,
                    s1LeftEnd: 239,
                    s1RightStart: 240,
                    s1RightEnd: 319
                )
            )
        ],
        timestamp: Date().timeIntervalSince1970
    )

    static let previewMinimal = SavedZonePreset(
        id: 2,
        name: "Simple Dual",
        zoneCount: 2,
        zones: nil,
        timestamp: nil
    )

    static let previewQuadZone = SavedZonePreset(
        id: 3,
        name: "Quad Rainbow",
        zoneCount: 4,
        zones: [
            ZonePresetEntry(
                effectId: 0,
                paletteId: 0,
                brightness: 255,
                speed: 128,
                blendMode: 0,
                segments: ZonePresetEntry.ZoneSegments(
                    s1LeftStart: 0,
                    s1LeftEnd: 39,
                    s1RightStart: 40,
                    s1RightEnd: 79
                )
            ),
            ZonePresetEntry(
                effectId: 1,
                paletteId: 1,
                brightness: 255,
                speed: 128,
                blendMode: 0,
                segments: ZonePresetEntry.ZoneSegments(
                    s1LeftStart: 80,
                    s1LeftEnd: 119,
                    s1RightStart: 120,
                    s1RightEnd: 159
                )
            ),
            ZonePresetEntry(
                effectId: 2,
                paletteId: 2,
                brightness: 255,
                speed: 128,
                blendMode: 0,
                segments: ZonePresetEntry.ZoneSegments(
                    s1LeftStart: 160,
                    s1LeftEnd: 199,
                    s1RightStart: 200,
                    s1RightEnd: 239
                )
            ),
            ZonePresetEntry(
                effectId: 3,
                paletteId: 3,
                brightness: 255,
                speed: 128,
                blendMode: 0,
                segments: ZonePresetEntry.ZoneSegments(
                    s1LeftStart: 240,
                    s1LeftEnd: 279,
                    s1RightStart: 280,
                    s1RightEnd: 319
                )
            )
        ],
        timestamp: Date().timeIntervalSince1970 - 172800
    )

    static let previewList: [SavedZonePreset] = [
        preview,
        previewMinimal,
        previewQuadZone
    ]
}

extension SavedZonePreset.ZonePresetEntry {
    static let preview = SavedZonePreset.ZonePresetEntry(
        effectId: 0,
        paletteId: 0,
        brightness: 255,
        speed: 128,
        blendMode: 0,
        segments: ZoneSegments(
            s1LeftStart: 0,
            s1LeftEnd: 79,
            s1RightStart: 80,
            s1RightEnd: 159
        )
    )
}

extension SavedZonePreset.ZonePresetEntry.ZoneSegments {
    static let preview = SavedZonePreset.ZonePresetEntry.ZoneSegments(
        s1LeftStart: 0,
        s1LeftEnd: 79,
        s1RightStart: 80,
        s1RightEnd: 159
    )
}
#endif
