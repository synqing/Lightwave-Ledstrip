//
//  BeatEvent.swift
//  LightwaveOS
//
//  Beat tracking event from WebSocket audio sync messages.
//

import Foundation

/// Real-time beat tracking event from audio sync
struct BeatEvent: Sendable, Identifiable {
    /// Unique identifier (using timestamp)
    var id: Date { timestamp }

    /// Detected tempo in beats per minute
    let bpm: Double

    /// Beat detection confidence (0.0-1.0)
    let confidence: Double

    /// Whether this is a beat tick (true on beat)
    let tick: Bool

    /// Whether this is a downbeat (first beat of bar)
    let downbeat: Bool

    /// Phase within current beat (0.0-1.0)
    let beatPhase: Double

    /// Beat index since tracking started
    let beatIndex: Int

    /// Bar index since tracking started
    let barIndex: Int

    /// Beat position within bar (0-3 for 4/4 time)
    let beatInBar: Int

    /// Event timestamp
    let timestamp: Date

    // MARK: - Initialization

    /// Initialize from WebSocket JSON dictionary
    /// - Parameter dict: JSON dictionary from WS message
    init(from dict: [String: Any]) {
        self.bpm = dict["bpm"] as? Double ?? 0.0
        self.confidence = dict["confidence"] as? Double ?? 0.0
        self.tick = dict["tick"] as? Bool ?? false
        self.downbeat = dict["downbeat"] as? Bool ?? false
        self.beatPhase = dict["beatPhase"] as? Double ?? 0.0
        self.beatIndex = dict["beatIndex"] as? Int ?? 0
        self.barIndex = dict["barIndex"] as? Int ?? 0
        self.beatInBar = dict["beatInBar"] as? Int ?? 0
        self.timestamp = Date()
    }

    /// Direct initialization (for testing)
    init(
        bpm: Double,
        confidence: Double,
        tick: Bool,
        downbeat: Bool,
        beatPhase: Double,
        beatIndex: Int,
        barIndex: Int,
        beatInBar: Int,
        timestamp: Date = Date()
    ) {
        self.bpm = bpm
        self.confidence = confidence
        self.tick = tick
        self.downbeat = downbeat
        self.beatPhase = beatPhase
        self.beatIndex = beatIndex
        self.barIndex = barIndex
        self.beatInBar = beatInBar
        self.timestamp = timestamp
    }

    // MARK: - Computed Properties

    /// Tempo category
    var tempoCategory: String {
        switch bpm {
        case 0..<60: return "Very Slow"
        case 60..<90: return "Slow"
        case 90..<120: return "Moderate"
        case 120..<140: return "Fast"
        case 140..<180: return "Very Fast"
        default: return "Extreme"
        }
    }

    /// Confidence category
    var confidenceCategory: String {
        switch confidence {
        case 0..<0.3: return "Low"
        case 0.3..<0.6: return "Medium"
        case 0.6..<0.8: return "High"
        default: return "Very High"
        }
    }

    /// Formatted BPM string
    var formattedBPM: String {
        String(format: "%.1f BPM", bpm)
    }

    /// Formatted confidence percentage
    var formattedConfidence: String {
        String(format: "%.0f%%", confidence * 100)
    }

    /// Beat position display (e.g., "3 of 4")
    var beatPositionDisplay: String {
        "\(beatInBar + 1) of 4"
    }
}

// MARK: - Sample Data

#if DEBUG
extension BeatEvent {
    static let preview = BeatEvent(
        bpm: 120.5,
        confidence: 0.85,
        tick: true,
        downbeat: false,
        beatPhase: 0.25,
        beatIndex: 42,
        barIndex: 10,
        beatInBar: 1
    )

    static let previewDownbeat = BeatEvent(
        bpm: 128.0,
        confidence: 0.92,
        tick: true,
        downbeat: true,
        beatPhase: 0.0,
        beatIndex: 40,
        barIndex: 10,
        beatInBar: 0
    )

    static let previewLowConfidence = BeatEvent(
        bpm: 95.3,
        confidence: 0.42,
        tick: false,
        downbeat: false,
        beatPhase: 0.67,
        beatIndex: 15,
        barIndex: 3,
        beatInBar: 3
    )
}
#endif
