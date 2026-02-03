//
//  AudioTuningParameters.swift
//  LightwaveOS
//
//  All audio DSP tuning parameters matching the firmware's audio pipeline.
//  Used by AudioParametersCard for the full parameter UI.
//

import Foundation

/// All audio DSP tuning parameters matching the firmware's audio pipeline.
/// Used by AudioParametersCard for the full parameter UI.
@Observable @MainActor
final class AudioTuningParameters {
    // MARK: - Input
    var micType: MicType = .inmp441
    var gain: Double = 1.0          // 0.0 ... 2.0
    var threshold: Double = 0.05    // 0.0 ... 1.0

    // MARK: - AGC (Automatic Gain Control)
    var agcEnabled: Bool = true
    var agcTarget: Double = 0.5     // 0.0 ... 1.0
    var agcAttack: Double = 0.01    // 0.001 ... 0.1
    var agcRelease: Double = 0.001  // 0.0001 ... 0.01

    // MARK: - Silence Detection
    var silenceThreshold: Double = 0.02  // 0.0 ... 0.2
    var silenceTimeout: Double = 3.0     // 0.5 ... 30.0 seconds

    // MARK: - Beat Detection
    var beatSensitivity: Double = 0.5    // 0.0 ... 1.0
    var bpmMin: Double = 60              // 40 ... 200
    var bpmMax: Double = 180             // 60 ... 300

    // MARK: - Spectral
    var fluxSmoothing: Double = 0.3      // 0.0 ... 1.0
    var bandSmoothing: Double = 0.5      // 0.0 ... 1.0

    enum MicType: String, CaseIterable, Sendable {
        case inmp441 = "INMP441"
        case max4466 = "MAX4466"
        case max9814 = "MAX9814"
        case lineIn = "LINE_IN"
    }
}
