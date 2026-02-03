//
//  ZoneConfig.swift
//  LightwaveOS
//
//  Zone configuration and blend modes for multi-zone composition.
//

import Foundation

/// Configuration for a single zone (0-3)
struct ZoneConfig: Codable, Sendable, Identifiable, Hashable {
    /// Zone ID (0-3)
    let id: Int

    /// Whether this zone is active
    var enabled: Bool

    /// Effect ID for this zone
    var effectId: Int?

    /// Effect display name (from metadata)
    var effectName: String?

    /// Brightness (0-255)
    var brightness: Int

    /// Animation speed (1-50)
    var speed: Int

    /// Palette ID (0-74)
    var paletteId: Int?

    /// Palette display name (from metadata)
    var paletteName: String?

    /// Blend mode for compositing (0-7)
    var blendMode: Int?

    /// Blend mode display name (from API)
    var blendModeName: String?

    // MARK: - Coding Keys

    enum CodingKeys: String, CodingKey {
        case id
        case enabled
        case effectId
        case effectName
        case brightness
        case speed
        case paletteId
        case paletteName
        case blendMode
        case blendModeName
    }

    // MARK: - Initialization

    init(
        id: Int,
        enabled: Bool = true,
        effectId: Int? = nil,
        effectName: String? = nil,
        brightness: Int = 255,
        speed: Int = 15,
        paletteId: Int? = nil,
        paletteName: String? = nil,
        blendMode: Int? = nil,
        blendModeName: String? = nil
    ) {
        self.id = id
        self.enabled = enabled
        self.effectId = effectId
        self.effectName = effectName
        self.brightness = brightness
        self.speed = speed
        self.paletteId = paletteId
        self.paletteName = paletteName
        self.blendMode = blendMode
        self.blendModeName = blendModeName
    }

    // MARK: - Computed Properties

    /// Blend mode as enum
    var blendModeEnum: BlendMode {
        guard let mode = blendMode else { return .overwrite }
        return BlendMode(rawValue: mode) ?? .overwrite
    }

    /// Display name for UI
    var displayName: String {
        effectName ?? "Effect \(effectId ?? 0)"
    }

    /// Display brightness percentage
    var brightnessPercent: Int {
        Int((Double(brightness) / 255.0) * 100)
    }
}

// MARK: - Blend Modes

/// Blend modes for zone composition (matches firmware BlendMode 0-7)
enum BlendMode: Int, Codable, Sendable, CaseIterable, Identifiable {
    case overwrite = 0
    case additive = 1
    case multiply = 2
    case screen = 3
    case overlay = 4
    case alpha = 5
    case lighten = 6
    case darken = 7

    var id: Int { rawValue }

    /// Human-readable name (matches firmware getBlendModeName)
    var displayName: String {
        switch self {
        case .overwrite: return "Overwrite"
        case .additive: return "Additive"
        case .multiply: return "Multiply"
        case .screen: return "Screen"
        case .overlay: return "Overlay"
        case .alpha: return "Alpha"
        case .lighten: return "Lighten"
        case .darken: return "Darken"
        }
    }

    /// Short description of blend mode behaviour
    var description: String {
        switch self {
        case .overwrite: return "Replace underlying zones"
        case .additive: return "Add colours together (brighten)"
        case .multiply: return "Multiply colours (darken)"
        case .screen: return "Inverse multiply (soft lighten)"
        case .overlay: return "Combination of multiply and screen"
        case .alpha: return "50/50 blend"
        case .lighten: return "Take brighter pixel"
        case .darken: return "Take darker pixel"
        }
    }

    /// Icon name for SF Symbols
    var iconName: String {
        switch self {
        case .overwrite: return "square.fill"
        case .additive: return "plus.square.fill"
        case .multiply: return "xmark.square.fill"
        case .screen: return "light.max"
        case .overlay: return "square.stack.fill"
        case .alpha: return "square.lefthalf.filled"
        case .lighten: return "sun.max.fill"
        case .darken: return "moon.fill"
        }
    }
}

// MARK: - Sample Data

#if DEBUG
extension ZoneConfig {
    static let preview = ZoneConfig(
        id: 0,
        enabled: true,
        effectId: 0,
        effectName: "LGP Interference",
        brightness: 255,
        speed: 15,
        paletteId: 0,
        paletteName: "Spectral Nova",
        blendMode: 0,
        blendModeName: "Overwrite"
    )

    static let previewDisabled = ZoneConfig(
        id: 1,
        enabled: false,
        effectId: 12,
        effectName: "Beat Pulse",
        brightness: 128,
        speed: 20,
        paletteId: 33,
        paletteName: "Batlow",
        blendMode: 1,
        blendModeName: "Additive"
    )

    static let previewZone2 = ZoneConfig(
        id: 2,
        enabled: true,
        effectId: 5,
        effectName: "Shockwave",
        brightness: 200,
        speed: 18,
        paletteId: 12,
        paletteName: "Sunset Ridge",
        blendMode: 3,
        blendModeName: "Screen"
    )
}
#endif
