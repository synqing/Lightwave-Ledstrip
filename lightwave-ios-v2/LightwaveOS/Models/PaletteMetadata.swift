//
//  PaletteMetadata.swift
//  LightwaveOS
//
//  Palette metadata with gradient colour data for swatch rendering.
//

import Foundation
import SwiftUI

/// Palette flag metadata from firmware/API
struct PaletteFlags: Codable, Sendable, Hashable {
    let warm: Bool?
    let cool: Bool?
    let calm: Bool?
    let vivid: Bool?
    let cvdFriendly: Bool?
    let whiteHeavy: Bool?
}

/// Palette flag keys for UI filters
enum PaletteFlagKey: String, CaseIterable, Identifiable {
    case warm
    case cool
    case calm
    case vivid
    case cvdFriendly
    case whiteHeavy

    var id: String { rawValue }

    var label: String {
        switch self {
        case .warm:
            return "Warm"
        case .cool:
            return "Cool"
        case .calm:
            return "Calm"
        case .vivid:
            return "Vivid"
        case .cvdFriendly:
            return "CVD-Friendly"
        case .whiteHeavy:
            return "White-Heavy"
        }
    }
}

/// Single colour stop in a palette gradient
struct PaletteColor: Codable, Sendable, Hashable {
    /// Position in gradient (0.0-1.0, or 0-255 from firmware)
    let position: Double

    /// Red component (0-255)
    let r: UInt8

    /// Green component (0-255)
    let g: UInt8

    /// Blue component (0-255)
    let b: UInt8

    // MARK: - Computed Properties

    /// SwiftUI Color representation
    var color: Color {
        Color(red: Double(r) / 255.0, green: Double(g) / 255.0, blue: Double(b) / 255.0)
    }
}

/// Metadata for a colour palette
struct PaletteMetadata: Codable, Identifiable, Sendable, Hashable {
    /// Palette ID (0-74)
    let id: Int

    /// Display name
    let name: String

    /// Category grouping (e.g., "CPT-City Artistic", "Crameri Scientific")
    let category: String?

    /// Array of colour stops for gradient swatch rendering
    let colors: [PaletteColor]?

    /// Flag metadata (warm, cool, etc.)
    let flags: PaletteFlags?

    /// Average brightness estimate (0-255)
    let avgBrightness: Int?

    /// Maximum brightness estimate (0-255)
    let maxBrightness: Int?

    // MARK: - Coding Keys

    enum CodingKeys: String, CodingKey {
        case id
        case name
        case category
        case colors
        case flags
        case avgBrightness
        case maxBrightness
    }

    // MARK: - Initialization

    init(
        id: Int,
        name: String,
        category: String? = nil,
        colors: [PaletteColor]? = nil,
        flags: PaletteFlags? = nil,
        avgBrightness: Int? = nil,
        maxBrightness: Int? = nil
    ) {
        self.id = id
        self.name = name
        self.category = category
        self.colors = colors
        self.flags = flags
        self.avgBrightness = avgBrightness
        self.maxBrightness = maxBrightness
    }

    // MARK: - Computed Properties

    /// Short display name for compact UI
    var shortName: String {
        name.replacingOccurrences(of: "Sequential ", with: "")
            .replacingOccurrences(of: "Diverging ", with: "")
            .replacingOccurrences(of: "Qualitative ", with: "")
    }

    /// SwiftUI Color array for gradient rendering
    var gradientColors: [Color] {
        guard let colors = colors, !colors.isEmpty else {
            return [.gray, .white] // Fallback gradient
        }
        return colors.map { $0.color }
    }

    /// Display category with fallback
    var displayCategory: String {
        category ?? "Custom"
    }

    /// Returns true when the palette has the requested flag
    func hasFlag(_ flag: PaletteFlagKey) -> Bool {
        switch flag {
        case .warm:
            return flags?.warm ?? false
        case .cool:
            return flags?.cool ?? false
        case .calm:
            return flags?.calm ?? false
        case .vivid:
            return flags?.vivid ?? false
        case .cvdFriendly:
            return flags?.cvdFriendly ?? false
        case .whiteHeavy:
            return flags?.whiteHeavy ?? false
        }
    }
}

// MARK: - Palette Store (Fallback)

enum PaletteStore {
    /// All palettes (exposed for PaletteViewModel init)
    static var all: [PaletteMetadata] { loadFromBundle() ?? defaults }

    struct MasterPalettePayload: Codable {
        let version: Int
        let source: String
        let palettes: [PaletteMetadata]
    }

    static func loadFromBundle(bundle: Bundle = .main) -> [PaletteMetadata]? {
        guard let url = bundle.url(forResource: "Palettes_Master", withExtension: "json") else {
            return nil
        }

        do {
            let data = try Data(contentsOf: url)
            let payload = try JSONDecoder().decode(MasterPalettePayload.self, from: data)
            return payload.palettes
        } catch {
            print("PaletteStore: failed to load bundle palettes: \(error)")
            return nil
        }
    }

    /// Hardcoded fallback palettes (real firmware names from Palettes_MasterData.cpp)
    static let defaults: [PaletteMetadata] = [
        // cpt-city Artistic (0-32)
        PaletteMetadata(id: 0, name: "Sunset Real", category: "Artistic"),
        PaletteMetadata(id: 1, name: "Rivendell", category: "Artistic"),
        PaletteMetadata(id: 2, name: "Ocean Breeze 036", category: "Artistic"),
        PaletteMetadata(id: 3, name: "RGI 15", category: "Artistic"),
        PaletteMetadata(id: 4, name: "Retro 2", category: "Artistic"),
        PaletteMetadata(id: 5, name: "Analogous 1", category: "Artistic"),
        PaletteMetadata(id: 6, name: "Pink Splash 08", category: "Artistic"),
        PaletteMetadata(id: 7, name: "Coral Reef", category: "Artistic"),
        PaletteMetadata(id: 8, name: "Ocean Breeze 068", category: "Artistic"),
        PaletteMetadata(id: 9, name: "Pink Splash 07", category: "Artistic"),
        PaletteMetadata(id: 10, name: "Vintage 01", category: "Artistic"),
        PaletteMetadata(id: 11, name: "Departure", category: "Artistic"),
        PaletteMetadata(id: 12, name: "Landscape 64", category: "Artistic"),
        PaletteMetadata(id: 13, name: "Landscape 33", category: "Artistic"),
        PaletteMetadata(id: 14, name: "Rainbow Sherbet", category: "Artistic"),
        PaletteMetadata(id: 15, name: "GR65 Hult", category: "Artistic"),
        PaletteMetadata(id: 16, name: "GR64 Hult", category: "Artistic"),
        PaletteMetadata(id: 17, name: "GMT Dry Wet", category: "Artistic"),
        PaletteMetadata(id: 18, name: "IB Jul01", category: "Artistic"),
        PaletteMetadata(id: 19, name: "Vintage 57", category: "Artistic"),
        PaletteMetadata(id: 20, name: "IB15", category: "Artistic"),
        PaletteMetadata(id: 21, name: "Fuschia 7", category: "Artistic"),
        PaletteMetadata(id: 22, name: "Emerald Dragon", category: "Artistic"),
        PaletteMetadata(id: 23, name: "Lava", category: "Artistic"),
        PaletteMetadata(id: 24, name: "Fire", category: "Artistic"),
        PaletteMetadata(id: 25, name: "Colorful", category: "Artistic"),
        PaletteMetadata(id: 26, name: "Magenta Evening", category: "Artistic"),
        PaletteMetadata(id: 27, name: "Pink Purple", category: "Artistic"),
        PaletteMetadata(id: 28, name: "Autumn 19", category: "Artistic"),
        PaletteMetadata(id: 29, name: "Blue Magenta White", category: "Artistic"),
        PaletteMetadata(id: 30, name: "Black Magenta Red", category: "Artistic"),
        PaletteMetadata(id: 31, name: "Red Magenta Yellow", category: "Artistic"),
        PaletteMetadata(id: 32, name: "Blue Cyan Yellow", category: "Artistic"),

        // Crameri Scientific (33-56)
        PaletteMetadata(id: 33, name: "Vik", category: "Scientific"),
        PaletteMetadata(id: 34, name: "Tokyo", category: "Scientific"),
        PaletteMetadata(id: 35, name: "Roma", category: "Scientific"),
        PaletteMetadata(id: 36, name: "Oleron", category: "Scientific"),
        PaletteMetadata(id: 37, name: "Lisbon", category: "Scientific"),
        PaletteMetadata(id: 38, name: "La Jolla", category: "Scientific"),
        PaletteMetadata(id: 39, name: "Hawaii", category: "Scientific"),
        PaletteMetadata(id: 40, name: "Devon", category: "Scientific"),
        PaletteMetadata(id: 41, name: "Cork", category: "Scientific"),
        PaletteMetadata(id: 42, name: "Broc", category: "Scientific"),
        PaletteMetadata(id: 43, name: "Berlin", category: "Scientific"),
        PaletteMetadata(id: 44, name: "Bamako", category: "Scientific"),
        PaletteMetadata(id: 45, name: "Acton", category: "Scientific"),
        PaletteMetadata(id: 46, name: "Batlow", category: "Scientific"),
        PaletteMetadata(id: 47, name: "Bilbao", category: "Scientific"),
        PaletteMetadata(id: 48, name: "Buda", category: "Scientific"),
        PaletteMetadata(id: 49, name: "Davos", category: "Scientific"),
        PaletteMetadata(id: 50, name: "GrayC", category: "Scientific"),
        PaletteMetadata(id: 51, name: "Imola", category: "Scientific"),
        PaletteMetadata(id: 52, name: "La Paz", category: "Scientific"),
        PaletteMetadata(id: 53, name: "Nuuk", category: "Scientific"),
        PaletteMetadata(id: 54, name: "Oslo", category: "Scientific"),
        PaletteMetadata(id: 55, name: "Tofino", category: "Scientific"),
        PaletteMetadata(id: 56, name: "Turku", category: "Scientific"),

        // R Colorspace / LGP-Optimised (57-74)
        PaletteMetadata(id: 57, name: "Viridis", category: "LGP-Optimised"),
        PaletteMetadata(id: 58, name: "Plasma", category: "LGP-Optimised"),
        PaletteMetadata(id: 59, name: "Inferno", category: "LGP-Optimised"),
        PaletteMetadata(id: 60, name: "Magma", category: "LGP-Optimised"),
        PaletteMetadata(id: 61, name: "Cubhelix", category: "LGP-Optimised"),
        PaletteMetadata(id: 62, name: "Abyss", category: "LGP-Optimised"),
        PaletteMetadata(id: 63, name: "Bathy", category: "LGP-Optimised"),
        PaletteMetadata(id: 64, name: "Ocean", category: "LGP-Optimised"),
        PaletteMetadata(id: 65, name: "Nighttime", category: "LGP-Optimised"),
        PaletteMetadata(id: 66, name: "Seafloor", category: "LGP-Optimised"),
        PaletteMetadata(id: 67, name: "IBCSO", category: "LGP-Optimised"),
        PaletteMetadata(id: 68, name: "Copper", category: "LGP-Optimised"),
        PaletteMetadata(id: 69, name: "Hot", category: "LGP-Optimised"),
        PaletteMetadata(id: 70, name: "Cool", category: "LGP-Optimised"),
        PaletteMetadata(id: 71, name: "Earth", category: "LGP-Optimised"),
        PaletteMetadata(id: 72, name: "Sealand", category: "LGP-Optimised"),
        PaletteMetadata(id: 73, name: "Split", category: "LGP-Optimised"),
        PaletteMetadata(id: 74, name: "Red2Green", category: "LGP-Optimised")
    ]

    /// Grouped by category
    static var grouped: [String: [PaletteMetadata]] {
        Dictionary(grouping: defaults, by: { $0.displayCategory })
    }

    /// Get palette by ID with fallback
    static func palette(id: Int) -> PaletteMetadata {
        defaults.first(where: { $0.id == id }) ?? defaults[0]
    }
}

// MARK: - Sample Data

#if DEBUG
extension PaletteMetadata {
    static let preview = PaletteMetadata(
        id: 0,
        name: "Sunset Real",
        category: "Artistic",
        colors: [
            PaletteColor(position: 0.0, r: 255, g: 0, b: 0),
            PaletteColor(position: 0.5, r: 255, g: 128, b: 0),
            PaletteColor(position: 1.0, r: 255, g: 255, b: 0)
        ]
    )

    static let previewScientific = PaletteMetadata(
        id: 33,
        name: "Vik",
        category: "Scientific",
        colors: [
            PaletteColor(position: 0.0, r: 0, g: 32, b: 64),
            PaletteColor(position: 0.5, r: 128, g: 64, b: 32),
            PaletteColor(position: 1.0, r: 255, g: 192, b: 128)
        ]
    )

    static let previewColorspace = PaletteMetadata(
        id: 57,
        name: "Viridis",
        category: "LGP-Optimised",
        colors: [
            PaletteColor(position: 0.0, r: 68, g: 1, b: 84),
            PaletteColor(position: 0.5, r: 33, g: 145, b: 140),
            PaletteColor(position: 1.0, r: 253, g: 231, b: 37)
        ]
    )
}
#endif
