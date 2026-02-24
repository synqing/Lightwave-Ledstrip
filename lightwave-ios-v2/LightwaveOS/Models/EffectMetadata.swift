//
//  EffectMetadata.swift
//  LightwaveOS
//
//  Effect metadata from /api/v1/effects endpoint.
//

import Foundation

/// Metadata for a single visual effect
struct EffectMetadata: Codable, Sendable, Identifiable, Hashable {
    /// Effect ID (hex-based, e.g. 0x0100 = 256)
    let id: Int

    /// Effect display name
    let name: String

    /// Category name (e.g., "Waves", "Particles")
    let category: String?

    /// Category ID for grouping
    let categoryId: Int?

    /// Whether effect responds to audio input
    let isAudioReactive: Bool

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
        categoryName: String? = nil,
        description: String? = nil
    ) {
        self.id = id
        self.name = name
        self.category = category
        self.categoryId = categoryId
        self.isAudioReactive = isAudioReactive
        self.categoryName = categoryName
        self.description = description
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
