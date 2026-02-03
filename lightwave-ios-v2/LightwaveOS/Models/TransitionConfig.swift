//
//  TransitionConfig.swift
//  LightwaveOS
//
//  Transition configuration for effect changes.
//

import Foundation

/// Configuration for effect transitions
struct TransitionConfig: Codable, Sendable, Equatable {
    /// Transition type
    var type: TransitionType

    /// Duration in milliseconds (200-5000)
    var duration: Int

    // MARK: - Initialization

    init(type: TransitionType = .crossfade, duration: Int = 1200) {
        self.type = type
        self.duration = duration
    }

    // MARK: - Coding Keys

    enum CodingKeys: String, CodingKey {
        case type
        case duration
    }
}

/// Transition types for effect changes (matches firmware)
enum TransitionType: Int, Codable, Sendable, CaseIterable, Identifiable {
    case cut = 0
    case crossfade = 1
    case wipeRight = 2
    case wipeLeft = 3
    case iris = 4
    case dissolve = 5
    case slideUp = 6
    case slideDown = 7
    case spiral = 8
    case scatter = 9
    case bloom = 10
    case shatter = 11

    var id: Int { rawValue }

    /// Human-readable name
    var name: String {
        switch self {
        case .cut: return "Cut"
        case .crossfade: return "Crossfade"
        case .wipeRight: return "Wipe Right"
        case .wipeLeft: return "Wipe Left"
        case .iris: return "Iris"
        case .dissolve: return "Dissolve"
        case .slideUp: return "Slide Up"
        case .slideDown: return "Slide Down"
        case .spiral: return "Spiral"
        case .scatter: return "Scatter"
        case .bloom: return "Bloom"
        case .shatter: return "Shatter"
        }
    }

    /// Short description
    var description: String {
        switch self {
        case .cut: return "Instant change"
        case .crossfade: return "Smooth blend"
        case .wipeRight: return "Wipe from left to right"
        case .wipeLeft: return "Wipe from right to left"
        case .iris: return "Circular reveal from centre"
        case .dissolve: return "Random pixel dissolve"
        case .slideUp: return "Slide upward"
        case .slideDown: return "Slide downward"
        case .spiral: return "Spiral from centre"
        case .scatter: return "Scatter from centre"
        case .bloom: return "Bloom from centre"
        case .shatter: return "Shatter effect"
        }
    }

    /// Icon name for SF Symbols
    var iconName: String {
        switch self {
        case .cut: return "scissors"
        case .crossfade: return "waveform.circle"
        case .wipeRight: return "arrow.right"
        case .wipeLeft: return "arrow.left"
        case .iris: return "circle.dotted"
        case .dissolve: return "sparkles"
        case .slideUp: return "arrow.up"
        case .slideDown: return "arrow.down"
        case .spiral: return "rotate.right"
        case .scatter: return "burst"
        case .bloom: return "light.max"
        case .shatter: return "shield.slash"
        }
    }
}

// MARK: - Sample Data

#if DEBUG
extension TransitionConfig {
    static let preview = TransitionConfig(type: .iris, duration: 1200)
    static let previewFast = TransitionConfig(type: .cut, duration: 200)
    static let previewSlow = TransitionConfig(type: .crossfade, duration: 3000)
}
#endif
