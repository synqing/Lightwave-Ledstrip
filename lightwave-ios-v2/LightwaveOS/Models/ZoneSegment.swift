//
//  ZoneSegment.swift
//  LightwaveOS
//
//  Zone LED segment mapping for dual-strip symmetric layout.
//

import Foundation

/// LED segment mapping for a zone (dual-strip symmetric layout).
///
/// Wire-format note (2026-05-02 migration): `zoneId` is the firmware
/// `zoneId` as transmitted on REST and WebSocket. Valid wire values are
/// 1, 2, 3. Wire value 0 is RESERVED and rejected by firmware. iOS
/// internal storage matches the wire format exactly — segments emitted
/// by `ZoneViewModel.segmentsFromBoundaries()` use 1-indexed identifiers.
struct ZoneSegment: Codable, Sendable, Identifiable, Hashable {
    /// Wire-format zoneId (1-indexed, 1..3). Sent verbatim to firmware.
    let zoneId: Int

    var id: Int { zoneId }

    // MARK: Strip 1 Left Side (0-79)

    /// Start index on Strip 1 left side
    let s1LeftStart: Int

    /// End index on Strip 1 left side (inclusive)
    let s1LeftEnd: Int

    // MARK: Strip 1 Right Side (80-159)

    /// Start index on Strip 1 right side
    let s1RightStart: Int

    /// End index on Strip 1 right side (inclusive)
    let s1RightEnd: Int

    /// Total LEDs in this zone
    let totalLeds: Int

    // MARK: - Coding Keys (firmware sends camelCase)

    enum CodingKeys: String, CodingKey {
        case zoneId
        case s1LeftStart
        case s1LeftEnd
        case s1RightStart
        case s1RightEnd
        case totalLeds
    }

    // MARK: - Initialization

    init(
        zoneId: Int,
        s1LeftStart: Int,
        s1LeftEnd: Int,
        s1RightStart: Int,
        s1RightEnd: Int,
        totalLeds: Int? = nil
    ) {
        self.zoneId = zoneId
        self.s1LeftStart = s1LeftStart
        self.s1LeftEnd = s1LeftEnd
        self.s1RightStart = s1RightStart
        self.s1RightEnd = s1RightEnd

        // Compute total LEDs if not provided
        if let total = totalLeds {
            self.totalLeds = total
        } else {
            let leftCount = s1LeftEnd - s1LeftStart + 1
            let rightCount = s1RightEnd - s1RightStart + 1
            self.totalLeds = leftCount + rightCount
        }
    }

    // MARK: - Computed Properties

    /// Left side LED count
    var leftLedCount: Int {
        s1LeftEnd - s1LeftStart + 1
    }

    /// Right side LED count
    var rightLedCount: Int {
        s1RightEnd - s1RightStart + 1
    }

    /// Display range string for UI
    var displayRange: String {
        "L: \(s1LeftStart)-\(s1LeftEnd), R: \(s1RightStart)-\(s1RightEnd)"
    }

    /// Compact range display (for zone cards)
    var compactRange: String {
        "\(s1LeftStart)-\(s1LeftEnd) / \(s1RightStart)-\(s1RightEnd)"
    }
}

// MARK: - Sample Data

#if DEBUG
extension ZoneSegment {
    static let preview = ZoneSegment(
        zoneId: 1,
        s1LeftStart: 0,
        s1LeftEnd: 39,
        s1RightStart: 80,
        s1RightEnd: 119,
        totalLeds: 80
    )

    static let previewZone1 = ZoneSegment(
        zoneId: 2,
        s1LeftStart: 40,
        s1LeftEnd: 79,
        s1RightStart: 120,
        s1RightEnd: 159,
        totalLeds: 80
    )

    static let previewCentreZone = ZoneSegment(
        zoneId: 1,
        s1LeftStart: 65,
        s1LeftEnd: 79,
        s1RightStart: 80,
        s1RightEnd: 94,
        totalLeds: 30
    )
}
#endif
