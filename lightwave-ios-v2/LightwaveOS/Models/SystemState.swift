//
//  SystemState.swift
//  LightwaveOS
//
//  Central system state model for visual parameters and device status.
//

import Foundation

/// Central system state synchronized via WebSocket
struct SystemState: Sendable, Equatable {
    // MARK: - Effect State

    /// Current effect ID (0-100)
    var effectId: Int = 0

    /// Current effect name
    var effectName: String = ""

    // MARK: - Palette State

    /// Current palette ID (0-74)
    var paletteId: Int = 0

    /// Current palette name
    var paletteName: String = ""

    // MARK: - Visual Parameters

    /// Global brightness (0-255)
    var brightness: Int = 128

    /// Animation speed (1-50)
    var speed: Int = 15

    /// Mood parameter (0-255)
    var mood: Int = 128

    /// Fade amount for transitions (0-255)
    var fadeAmount: Int = 20

    /// Hue parameter (0-255)
    var hue: Int = 0

    /// Saturation parameter (0-255)
    var saturation: Int = 255

    /// Intensity parameter (0-255)
    var intensity: Int = 128

    /// Complexity parameter (0-255)
    var complexity: Int = 128

    /// Variation parameter (0-255)
    var variation: Int = 0

    // MARK: - Device Status

    /// Current frames per second
    var fps: Int = 0

    /// Free heap memory in bytes
    var freeHeap: Int = 0

    /// Uptime in seconds
    var uptime: Int = 0

    // MARK: - Update from WebSocket

    /// Update state from WebSocket status broadcast
    /// - Parameter status: JSON dictionary from WS message
    mutating func update(from status: [String: Any]) {
        // Effect state
        if let id = status["effectId"] as? Int {
            effectId = id
        }
        if let name = status["effectName"] as? String {
            effectName = name
        }

        // Palette state
        if let id = status["paletteId"] as? Int {
            paletteId = id
        }
        if let name = status["paletteName"] as? String {
            paletteName = name
        }

        // Visual parameters
        if let value = status["brightness"] as? Int {
            brightness = value
        }
        if let value = status["speed"] as? Int {
            speed = value
        }
        if let value = status["mood"] as? Int {
            mood = value
        }
        if let value = status["fadeAmount"] as? Int {
            fadeAmount = value
        }
        if let value = status["hue"] as? Int {
            hue = value
        }
        if let value = status["saturation"] as? Int {
            saturation = value
        }
        if let value = status["intensity"] as? Int {
            intensity = value
        }
        if let value = status["complexity"] as? Int {
            complexity = value
        }
        if let value = status["variation"] as? Int {
            variation = value
        }

        // Device status
        if let value = status["fps"] as? Int {
            fps = value
        }
        if let value = status["freeHeap"] as? Int {
            freeHeap = value
        }
        if let value = status["uptime"] as? Int {
            uptime = value
        }
    }

    // MARK: - Computed Properties

    /// Brightness as 0.0-1.0 ratio
    var brightnessRatio: Double {
        Double(brightness) / 255.0
    }

    /// Speed as 0.0-1.0 ratio
    var speedRatio: Double {
        Double(speed) / 50.0
    }

    /// Formatted uptime string
    var formattedUptime: String {
        let hours = uptime / 3600
        let minutes = (uptime % 3600) / 60
        let seconds = uptime % 60

        if hours > 0 {
            return String(format: "%dh %02dm %02ds", hours, minutes, seconds)
        } else if minutes > 0 {
            return String(format: "%dm %02ds", minutes, seconds)
        } else {
            return String(format: "%ds", seconds)
        }
    }

    /// Formatted heap memory
    var formattedHeap: String {
        let kb = Double(freeHeap) / 1024.0
        if kb > 1024 {
            return String(format: "%.1f MB", kb / 1024.0)
        } else {
            return String(format: "%.1f KB", kb)
        }
    }

    /// Formatted FPS
    var formattedFPS: String {
        "\(fps) FPS"
    }
}

// MARK: - Parameter Metadata

extension SystemState {
    /// Metadata for visual parameters (for UI generation)
    struct ParameterInfo {
        let key: String
        let displayName: String
        let min: Int
        let max: Int
        let step: Int
        let defaultValue: Int

        /// Common parameters
        static let brightness = ParameterInfo(key: "brightness", displayName: "Brightness", min: 0, max: 255, step: 1, defaultValue: 128)
        static let speed = ParameterInfo(key: "speed", displayName: "Speed", min: 1, max: 50, step: 1, defaultValue: 15)
        static let mood = ParameterInfo(key: "mood", displayName: "Mood", min: 0, max: 255, step: 1, defaultValue: 128)
        static let fadeAmount = ParameterInfo(key: "fadeAmount", displayName: "Trails", min: 0, max: 255, step: 1, defaultValue: 20)
        static let hue = ParameterInfo(key: "hue", displayName: "Hue", min: 0, max: 255, step: 1, defaultValue: 0)
        static let saturation = ParameterInfo(key: "saturation", displayName: "Saturation", min: 0, max: 255, step: 1, defaultValue: 255)
        static let intensity = ParameterInfo(key: "intensity", displayName: "Intensity", min: 0, max: 255, step: 1, defaultValue: 128)
        static let complexity = ParameterInfo(key: "complexity", displayName: "Complexity", min: 0, max: 255, step: 1, defaultValue: 128)
        static let variation = ParameterInfo(key: "variation", displayName: "Variation", min: 0, max: 255, step: 1, defaultValue: 0)

        static let allParameters: [ParameterInfo] = [
            .brightness, .speed, .mood, .fadeAmount,
            .hue, .saturation, .intensity, .complexity, .variation
        ]
    }
}

// MARK: - Sample Data

#if DEBUG
extension SystemState {
    static let preview = SystemState(
        effectId: 0,
        effectName: "LGP Interference",
        paletteId: 0,
        paletteName: "Spectral Nova",
        brightness: 200,
        speed: 20,
        mood: 150,
        fadeAmount: 30,
        hue: 120,
        saturation: 255,
        intensity: 180,
        complexity: 150,
        variation: 50,
        fps: 120,
        freeHeap: 180000,
        uptime: 3665
    )

    static let previewLowPerformance = SystemState(
        effectId: 12,
        effectName: "Beat Pulse",
        paletteId: 33,
        paletteName: "Batlow",
        brightness: 255,
        speed: 30,
        mood: 200,
        fadeAmount: 10,
        hue: 0,
        saturation: 255,
        intensity: 255,
        complexity: 200,
        variation: 100,
        fps: 85,
        freeHeap: 120000,
        uptime: 86400
    )
}
#endif
