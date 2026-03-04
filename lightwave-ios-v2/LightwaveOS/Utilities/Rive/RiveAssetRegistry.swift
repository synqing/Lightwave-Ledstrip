//
//  RiveAssetRegistry.swift
//  LightwaveOS
//
//  Central asset IDs used by RiveViewContainer call sites.
//

import Foundation

struct RiveAsset: Sendable, Hashable {
    let fileName: String
    let artboard: String?
    let stateMachine: String?

    init(fileName: String, artboard: String? = nil, stateMachine: String? = nil) {
        self.fileName = fileName
        self.artboard = artboard
        self.stateMachine = stateMachine
    }
}

enum RiveAssetRegistry {
    static let heroOverlay = RiveAsset(fileName: "hero_overlay")
    static let beatPulse = RiveAsset(fileName: "beat_pulse")
    static let connectionIndicator = RiveAsset(fileName: "connection_indicator")
    static let discoveryEmptyState = RiveAsset(fileName: "discovery_empty_state")
    static let patternPillAccent = RiveAsset(fileName: "pattern_pill_accent")
    static let palettePillAccent = RiveAsset(fileName: "palette_pill_accent")
    static let audioMicroInteraction = RiveAsset(fileName: "audio_micro_interaction")
}

