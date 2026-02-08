//
//  EffectPreset.swift
//  LightwaveOS
//
//  Saved effect preset from /api/v1/presets endpoint.
//

import Foundation

/// Saved effect preset containing effect, palette, and parameter configuration
struct EffectPreset: Identifiable, Codable, Sendable, Hashable {
    /// Preset ID (server-assigned)
    let id: Int

    /// User-defined preset name
    let name: String

    /// Effect ID reference
    let effectId: Int

    /// Palette ID reference
    let paletteId: Int

    /// Optional brightness override (0-255)
    let brightness: Int?

    /// Optional speed override (0-255)
    let speed: Int?

    /// Optional effect parameters
    let parameters: EffectParameters?

    /// Creation/modification timestamp (Unix epoch seconds)
    let timestamp: TimeInterval?

    // MARK: - Nested Types

    /// Effect parameter values
    struct EffectParameters: Codable, Sendable, Hashable {
        /// Mood parameter (0-255)
        let mood: Int?

        /// Trails parameter (0-255)
        let trails: Int?

        /// Hue parameter (0-255)
        let hue: Int?

        /// Saturation parameter (0-255)
        let saturation: Int?

        /// Intensity parameter (0-255)
        let intensity: Int?

        /// Complexity parameter (0-255)
        let complexity: Int?

        /// Variation parameter (0-255)
        let variation: Int?

        // MARK: - Initialization

        init(
            mood: Int? = nil,
            trails: Int? = nil,
            hue: Int? = nil,
            saturation: Int? = nil,
            intensity: Int? = nil,
            complexity: Int? = nil,
            variation: Int? = nil
        ) {
            self.mood = mood
            self.trails = trails
            self.hue = hue
            self.saturation = saturation
            self.intensity = intensity
            self.complexity = complexity
            self.variation = variation
        }
    }

    // MARK: - Initialization

    init(
        id: Int,
        name: String,
        effectId: Int,
        paletteId: Int,
        brightness: Int? = nil,
        speed: Int? = nil,
        parameters: EffectParameters? = nil,
        timestamp: TimeInterval? = nil
    ) {
        self.id = id
        self.name = name
        self.effectId = effectId
        self.paletteId = paletteId
        self.brightness = brightness
        self.speed = speed
        self.parameters = parameters
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
extension EffectPreset {
    static let preview = EffectPreset(
        id: 1,
        name: "Chill Vibes",
        effectId: 5,
        paletteId: 12,
        brightness: 180,
        speed: 128,
        parameters: EffectParameters(
            mood: 64,
            trails: 200,
            intensity: 128
        ),
        timestamp: Date().timeIntervalSince1970
    )

    static let previewMinimal = EffectPreset(
        id: 2,
        name: "Quick Glow",
        effectId: 0,
        paletteId: 0,
        brightness: nil,
        speed: nil,
        parameters: nil,
        timestamp: nil
    )

    static let previewAudioReactive = EffectPreset(
        id: 3,
        name: "Beat Drop",
        effectId: 12,
        paletteId: 8,
        brightness: 255,
        speed: 200,
        parameters: EffectParameters(
            mood: 200,
            trails: 100,
            intensity: 255,
            complexity: 180
        ),
        timestamp: Date().timeIntervalSince1970 - 86400
    )

    static let previewList: [EffectPreset] = [
        preview,
        previewMinimal,
        previewAudioReactive
    ]
}

extension EffectPreset.EffectParameters {
    static let preview = EffectPreset.EffectParameters(
        mood: 128,
        trails: 192,
        hue: 64,
        saturation: 255,
        intensity: 200,
        complexity: 128,
        variation: 64
    )
}
#endif
