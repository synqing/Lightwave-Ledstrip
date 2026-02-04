//
//  RiveAssetRegistry.swift
//  LightwaveOS
//
//  Central registry for Rive assets and their expected contracts.
//

import Foundation

struct RiveAssetDescriptor: Identifiable, Equatable {
    let id: String
    let fileName: String
    let artboardName: String?
    let stateMachineName: String?
    let inputs: [RiveInputDescriptor]
    let events: [String]
    let notes: String
}

struct RiveInputDescriptor: Equatable {
    let name: String
    let kind: RiveInputKindDescriptor
    let notes: String

    static func bool(_ name: String, notes: String = "") -> RiveInputDescriptor {
        RiveInputDescriptor(name: name, kind: .bool, notes: notes)
    }

    static func number(_ name: String, notes: String = "") -> RiveInputDescriptor {
        RiveInputDescriptor(name: name, kind: .number, notes: notes)
    }

    static func text(_ name: String, notes: String = "") -> RiveInputDescriptor {
        RiveInputDescriptor(name: name, kind: .text, notes: notes)
    }

    static func trigger(_ name: String, notes: String = "") -> RiveInputDescriptor {
        RiveInputDescriptor(name: name, kind: .trigger, notes: notes)
    }
}

enum RiveInputKindDescriptor: String, Equatable {
    case bool
    case number
    case text
    case trigger
}

// MARK: - Registry

enum RiveAssetRegistry {
    static let connectionIndicator = RiveAssetDescriptor(
        id: "connection-indicator",
        fileName: "lw_connection_indicator",
        artboardName: "ConnectionIndicator",
        stateMachineName: "Connection",
        inputs: [
            .bool("isConnected", notes: "True when main connection is active"),
            .bool("isConnecting", notes: "True during connect handshake"),
            .bool("isDiscovering", notes: "True during device discovery"),
            .bool("hasError", notes: "True when error state is active")
        ],
        events: ["tap"],
        notes: "Status bar connection indicator. Keep animation subtle."
    )

    static let discoveryEmptyState = RiveAssetDescriptor(
        id: "discovery-empty-state",
        fileName: "lw_discovery_empty_state",
        artboardName: "Discovery",
        stateMachineName: "Discovery",
        inputs: [
            .bool("isSearching", notes: "True while discovery is running")
        ],
        events: [],
        notes: "Connection sheet empty state animation."
    )

    static let beatPulse = RiveAssetDescriptor(
        id: "beat-pulse",
        fileName: "lw_beat_pulse",
        artboardName: "BeatPulse",
        stateMachineName: "Beat",
        inputs: [
            .bool("isBeating", notes: "Set true on tick"),
            .bool("isDownbeat", notes: "Set true on downbeat"),
            .number("confidence", notes: "0-1 confidence value")
        ],
        events: [],
        notes: "Replacement for SwiftUI BeatPulse component."
    )

    static let heroOverlay = RiveAssetDescriptor(
        id: "hero-overlay",
        fileName: "lw_hero_overlay",
        artboardName: "HeroOverlay",
        stateMachineName: "Hero",
        inputs: [
            .number("bpm", notes: "Current BPM"),
            .number("confidence", notes: "0-1 confidence"),
            .bool("isBeating", notes: "Beat pulse flag")
        ],
        events: [],
        notes: "Optional overlay for Hero LED preview."
    )

    static let patternPillAccent = RiveAssetDescriptor(
        id: "pattern-pill-accent",
        fileName: "lw_pattern_pill_accent",
        artboardName: "PatternPill",
        stateMachineName: "Accent",
        inputs: [
            .number("index", notes: "Current effect index"),
            .number("count", notes: "Total effect count")
        ],
        events: [],
        notes: "Subtle accent animation in Pattern pill."
    )

    static let palettePillAccent = RiveAssetDescriptor(
        id: "palette-pill-accent",
        fileName: "lw_palette_pill_accent",
        artboardName: "PalettePill",
        stateMachineName: "Accent",
        inputs: [
            .number("index", notes: "Current palette index"),
            .number("count", notes: "Total palette count")
        ],
        events: [],
        notes: "Subtle accent animation in Palette pill."
    )

    static let audioMicroInteraction = RiveAssetDescriptor(
        id: "audio-micro-interaction",
        fileName: "lw_audio_micro",
        artboardName: "AudioMicro",
        stateMachineName: "Audio",
        inputs: [
            .number("rms", notes: "0-1 RMS"),
            .number("flux", notes: "Spectral flux"),
            .number("confidence", notes: "BPM confidence 0-1"),
            .bool("isSilent", notes: "Silence detection"),
            .bool("isBeating", notes: "Beat pulse flag")
        ],
        events: [],
        notes: "Small animated cue for the audio tab."
    )

    static let all: [RiveAssetDescriptor] = [
        connectionIndicator,
        discoveryEmptyState,
        beatPulse,
        heroOverlay,
        patternPillAccent,
        palettePillAccent,
        audioMicroInteraction
    ]
}
