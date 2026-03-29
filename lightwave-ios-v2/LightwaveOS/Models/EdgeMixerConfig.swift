//
//  EdgeMixerConfig.swift
//  LightwaveOS
//
//  Model for EdgeMixer post-processor state.
//  EdgeMixer transforms Strip 2 colours to create perceived depth
//  through the Light Guide Plate via 3 composable stages:
//  Temporal (T), Colour (C), and Spatial (S).
//

import Foundation

// MARK: - Colour Transform Mode

enum EdgeMixerMode: Int, Codable, Sendable, CaseIterable, Identifiable {
    case mirror = 0
    case analogous = 1
    case complementary = 2
    case splitComplementary = 3
    case saturationVeil = 4
    case triadic = 5
    case tetradic = 6

    var id: Int { rawValue }

    var name: String {
        switch self {
        case .mirror: "Mirror"
        case .analogous: "Analogous"
        case .complementary: "Complementary"
        case .splitComplementary: "Split Comp."
        case .saturationVeil: "Sat. Veil"
        case .triadic: "Triadic"
        case .tetradic: "Tetradic"
        }
    }

    /// Whether this mode uses the spread parameter.
    /// Mirror, Complementary, and Split Complementary have fixed angles.
    var usesSpread: Bool {
        switch self {
        case .mirror, .complementary, .splitComplementary: false
        case .analogous, .saturationVeil, .triadic, .tetradic: true
        }
    }
}

// MARK: - Spatial Mask Mode

enum EdgeMixerSpatial: Int, Codable, Sendable, CaseIterable, Identifiable {
    case uniform = 0
    case centreGradient = 1

    var id: Int { rawValue }

    var name: String {
        switch self {
        case .uniform: "Uniform"
        case .centreGradient: "Centre Grad."
        }
    }
}

// MARK: - Temporal Modulation Mode

enum EdgeMixerTemporal: Int, Codable, Sendable, CaseIterable, Identifiable {
    /// No modulation — strength fixed at 255.
    case staticMode = 0
    /// Strength tracks smoothed audio RMS with ~55ms time constant.
    case rmsGate = 1

    var id: Int { rawValue }

    var name: String {
        switch self {
        case .staticMode: "Static"
        case .rmsGate: "RMS Gate"
        }
    }
}

// MARK: - Configuration

struct EdgeMixerConfig: Codable, Sendable, Equatable {
    var mode: EdgeMixerMode = .mirror
    var spread: Int = 30        // 0-60 degrees
    var strength: Int = 255     // 0-255 mix blend
    var spatial: EdgeMixerSpatial = .uniform
    var temporal: EdgeMixerTemporal = .staticMode
}
