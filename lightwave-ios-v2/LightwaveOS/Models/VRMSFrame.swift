//
//  VRMSFrame.swift
//  LightwaveOS
//
//  Phase 2 — Task P2-2: VRMS (Visual RMS) frame decoder.
//
//  Wire format (sourced from firmware):
//    `firmware-v3/src/network/WebServer.cpp` lines 884-924
//    `firmware-v3/src/metrics/VRMSMetrics.h` lines 40-50 (struct VRMSVector).
//
//  Unlike the STM stream, VRMS is broadcast as a JSON TEXT message rather
//  than a binary frame. Cadence is 10 Hz (intervalMs = 100). Envelope:
//
//      { "type": "vrms.frame",
//        "metrics": { "dominantHue": Float, "colourVariance": Float,
//                     "spatialCentroid": Float, "symmetryScore": Float,
//                     "brightnessMean": Float, "brightnessVariance": Float,
//                     "temporalFreq": Float, "audioVisualCorr": Float },
//        "timestamp": UInt32 }
//
//  British English in comments throughout. The wire field uses `colour`
//  spelling (matches firmware), keeping iOS aligned with the on-device
//  metric names — the firmware `VRMSVector` struct member is `colourVariance`.
//

import Foundation

// MARK: - VRMSFrame

/// Decoded VRMS (Visual RMS) perceptual metrics produced by the firmware's
/// `VRMSMetricsEngine`. Captures dominant hue, colour variance, spatial
/// centroid, symmetry, brightness mean/variance, temporal frequency and
/// audio-visual correlation. Useful for live visualisation health checks.
struct VRMSFrame: Codable, Sendable, Equatable {

    /// Dominant hue in HSV colour space, normalised range `0..<256`.
    let dominantHue: Float

    /// Circular standard deviation of pixel hues, normalised `0..<1`.
    let colourVariance: Float

    /// Pixel-index centroid of brightness, range `0..<320` (160 = strip centre).
    let spatialCentroid: Float

    /// Pearson correlation between left and right halves of the strip,
    /// range `-1..<1` (1 = perfectly symmetric, 0 = uncorrelated).
    let symmetryScore: Float

    /// Mean brightness across all 320 LEDs, range `0..<256`.
    let brightnessMean: Float

    /// Brightness variance, range `0..<65025` (max = 255²).
    let brightnessVariance: Float

    /// Normalised frame-difference magnitude, range `0..<1`.
    let temporalFreq: Float

    /// Pearson correlation of audio RMS against luminance, range `-1..<1`.
    let audioVisualCorr: Float
}

// MARK: - Wire envelope

/// Envelope wrapper matching the firmware JSON shape `{ type, metrics, timestamp }`.
/// Decoded by `WebSocketService` when a `vrms.frame` text message arrives.
struct VRMSEnvelope: Codable, Sendable {
    let type: String
    let metrics: VRMSFrame
    let timestamp: UInt32
}

// MARK: - Mock + helpers

extension VRMSFrame {
    /// Mock frame for SwiftUI previews.
    static let mock: VRMSFrame = VRMSFrame(
        dominantHue: 192.0,        // Cyan-ish hue
        colourVariance: 0.18,
        spatialCentroid: 159.5,    // Centred
        symmetryScore: 0.92,       // Highly symmetric
        brightnessMean: 144.0,
        brightnessVariance: 1850.0,
        temporalFreq: 0.045,
        audioVisualCorr: 0.42
    )
}
