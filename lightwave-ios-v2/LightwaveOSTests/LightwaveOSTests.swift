//
//  LightwaveOSTests.swift
//  LightwaveOSTests
//
//  Placeholder for unit tests.
//

import XCTest
@testable import LightwaveOS

final class LightwaveOSTests: XCTestCase {
    func testDesignTokensExist() {
        // Verify design tokens are accessible
        XCTAssertEqual(Spacing.xs, 4)
        XCTAssertEqual(Spacing.sm, 8)
        XCTAssertEqual(Spacing.md, 12)
        XCTAssertEqual(Spacing.lg, 24)

        XCTAssertEqual(CornerRadius.hero, 24)
        XCTAssertEqual(CornerRadius.card, 16)
        XCTAssertEqual(CornerRadius.nested, 12)
    }

    func testGlassTokensExist() {
        // Verify glass tokens are accessible
        XCTAssertEqual(GlassTokens.beatBurstMax, 1.35)
        XCTAssertEqual(GlassTokens.beatBurstRegular, 1.15)
        XCTAssertGreaterThan(GlassTokens.surfaceBlur, 0)
    }

    func testAnimationTokensExist() {
        // Verify animation tokens are accessible
        XCTAssertGreaterThan(AnimationTokens.energyPulse, 0)
        XCTAssertGreaterThan(AnimationTokens.shimmerDuration, 0)
    }
}
