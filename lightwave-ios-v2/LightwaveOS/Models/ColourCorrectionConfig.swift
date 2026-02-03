//
//  ColourCorrectionConfig.swift
//  LightwaveOS
//
//  Colour correction configuration for LED output.
//

import Foundation

/// Configuration for colour correction pipeline
struct ColourCorrectionConfig: Codable, Sendable, Equatable {
    /// Gamma correction enabled
    var gammaEnabled: Bool

    /// Gamma value (1.0-3.0)
    var gammaValue: Double

    /// Auto-exposure enabled
    var autoExposureEnabled: Bool

    /// Auto-exposure target brightness (0-255)
    var autoExposureTarget: Int

    /// Brown guardrail enabled
    var brownGuardrailEnabled: Bool

    /// Colour correction mode
    var mode: CCMode

    // MARK: - Initialization

    init(
        gammaEnabled: Bool = false,
        gammaValue: Double = 2.2,
        autoExposureEnabled: Bool = false,
        autoExposureTarget: Int = 128,
        brownGuardrailEnabled: Bool = false,
        mode: CCMode = .off
    ) {
        self.gammaEnabled = gammaEnabled
        self.gammaValue = gammaValue
        self.autoExposureEnabled = autoExposureEnabled
        self.autoExposureTarget = autoExposureTarget
        self.brownGuardrailEnabled = brownGuardrailEnabled
        self.mode = mode
    }

    // MARK: - Coding Keys

    enum CodingKeys: String, CodingKey {
        case gammaEnabled
        case gammaValue
        case autoExposureEnabled
        case autoExposureTarget
        case brownGuardrailEnabled
        case mode
    }
}

/// Colour correction mode (matches firmware)
enum CCMode: Int, Codable, Sendable, CaseIterable, Identifiable {
    case off = 0
    case hsv = 1
    case rgb = 2
    case both = 3

    var id: Int { rawValue }

    /// Human-readable name
    var name: String {
        switch self {
        case .off: return "Off"
        case .hsv: return "HSV Only"
        case .rgb: return "RGB Only"
        case .both: return "HSV + RGB"
        }
    }

    /// Short description
    var description: String {
        switch self {
        case .off: return "No colour correction"
        case .hsv: return "HSV colour space correction"
        case .rgb: return "RGB colour space correction"
        case .both: return "Full HSV and RGB correction"
        }
    }
}

// MARK: - Sample Data

#if DEBUG
extension ColourCorrectionConfig {
    static let preview = ColourCorrectionConfig(
        gammaEnabled: true,
        gammaValue: 2.2,
        autoExposureEnabled: false,
        autoExposureTarget: 128,
        brownGuardrailEnabled: true,
        mode: .hsv
    )

    static let previewDisabled = ColourCorrectionConfig(
        gammaEnabled: false,
        gammaValue: 1.0,
        autoExposureEnabled: false,
        autoExposureTarget: 128,
        brownGuardrailEnabled: false,
        mode: .off
    )

    static let previewFull = ColourCorrectionConfig(
        gammaEnabled: true,
        gammaValue: 2.4,
        autoExposureEnabled: true,
        autoExposureTarget: 180,
        brownGuardrailEnabled: true,
        mode: .both
    )
}
#endif
