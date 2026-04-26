//
//  EffectMetadata.swift
//  LightwaveOS
//
//  Effect metadata from /api/v1/effects endpoint.
//

import Foundation

/// Metadata for a single visual effect
struct EffectMetadata: Codable, Sendable, Identifiable, Hashable {
    /// Effect ID (0-100)
    let id: Int

    /// Effect display name
    let name: String

    /// Category name (e.g., "Waves", "Particles")
    let category: String?

    /// Category ID for grouping
    let categoryId: Int?

    /// Whether effect responds to audio input
    let isAudioReactive: Bool

    /// Whether effect is tagged experimental and should be excluded from the
    /// default production rotation. Absent in old K1 firmware responses — defaults
    /// to false for backward compatibility.
    let isExperimental: Bool

    /// Human-readable category name for display grouping
    let categoryName: String?

    /// Effect description (optional)
    let description: String?

    // MARK: - Coding Keys

    enum CodingKeys: String, CodingKey {
        case id
        case name
        case category
        case categoryId
        case isAudioReactive
        case isExperimental
        case categoryName
        case description
    }

    // MARK: - Initialization

    init(
        id: Int,
        name: String,
        category: String? = nil,
        categoryId: Int? = nil,
        isAudioReactive: Bool = false,
        isExperimental: Bool = false,
        categoryName: String? = nil,
        description: String? = nil
    ) {
        self.id = id
        self.name = name
        self.category = category
        self.categoryId = categoryId
        self.isAudioReactive = isAudioReactive
        self.isExperimental = isExperimental
        self.categoryName = categoryName
        self.description = description
    }

    // MARK: - Decodable

    /// Custom decoder to handle old K1 firmware responses that omit isExperimental.
    /// Absent field decodes as false (backward-compatible default).
    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        id = try container.decode(Int.self, forKey: .id)
        name = try container.decode(String.self, forKey: .name)
        category = try container.decodeIfPresent(String.self, forKey: .category)
        categoryId = try container.decodeIfPresent(Int.self, forKey: .categoryId)
        isAudioReactive = try container.decodeIfPresent(Bool.self, forKey: .isAudioReactive) ?? false
        isExperimental = try container.decodeIfPresent(Bool.self, forKey: .isExperimental) ?? false
        categoryName = try container.decodeIfPresent(String.self, forKey: .categoryName)
        description = try container.decodeIfPresent(String.self, forKey: .description)
    }

    // MARK: - Computed Properties

    /// Display category with fallback
    var displayCategory: String {
        categoryName ?? category ?? "Other"
    }

    /// Audio-reactive icon name
    var audioReactiveIcon: String {
        isAudioReactive ? "waveform" : ""
    }
}

// MARK: - Sample Data

#if DEBUG
extension EffectMetadata {
    static let preview = EffectMetadata(
        id: 0,
        name: "LGP Interference",
        category: "Light Guide Plate",
        categoryId: 1,
        isAudioReactive: false,
        categoryName: "Light Guide Plate",
        description: "Interference patterns optimised for light guide plates"
    )

    static let previewAudioReactive = EffectMetadata(
        id: 12,
        name: "Beat Pulse",
        category: "Audio Reactive",
        categoryId: 3,
        isAudioReactive: true,
        categoryName: "Audio Reactive",
        description: "Pulse-based beat-reactive effect"
    )

    static let previewParticles = EffectMetadata(
        id: 25,
        name: "Starfield",
        category: "Particles",
        categoryId: 5,
        isAudioReactive: false,
        categoryName: "Particles",
        description: "Particle-based starfield effect"
    )
}
#endif
