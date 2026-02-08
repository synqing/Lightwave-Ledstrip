//
//  LWCardSnapshotTests.swift
//  LightwaveOSTests
//
//  Snapshot tests for LWCard component with various configurations.
//

import XCTest
import SwiftUI
@testable import LightwaveOS

final class LWCardSnapshotTests: SnapshotTestCase {

    // MARK: - Basic Card States

    func testCardWithTitle() {
        let card = LWCard(title: "System Status") {
            VStack(alignment: .leading, spacing: Spacing.sm) {
                HStack {
                    Text("Uptime")
                        .foregroundStyle(Color.lwTextSecondary)
                    Spacer()
                    Text("2h 34m")
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    Text("FPS")
                        .foregroundStyle(Color.lwTextSecondary)
                    Spacer()
                    Text("120")
                        .foregroundStyle(Color.lwSuccess)
                }

                HStack {
                    Text("Free Heap")
                        .foregroundStyle(Color.lwTextSecondary)
                    Spacer()
                    Text("142 KB")
                        .foregroundStyle(Color.lwTextPrimary)
                }
            }
            .font(.bodyValue)
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: card,
            width: 375,
            height: 200,
            named: "card-with-title"
        )
    }

    func testCardWithoutTitle() {
        let card = LWCard {
            VStack(spacing: Spacing.xs) {
                Text("Card without title")
                    .foregroundStyle(Color.lwTextPrimary)
                    .font(.bodyValue)

                Text("This demonstrates a card with no header")
                    .font(.caption)
                    .foregroundStyle(Color.lwTextSecondary)
            }
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: card,
            width: 375,
            height: 150,
            named: "card-without-title"
        )
    }

    // MARK: - Elevated Card

    func testElevatedCard() {
        let card = LWCard(title: "Elevated Card", elevated: true) {
            Text("This card uses the elevated shadow style")
                .foregroundStyle(Color.lwTextPrimary)
                .font(.bodyValue)
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: card,
            width: 375,
            height: 150,
            named: "card-elevated"
        )
    }

    // MARK: - Zone Accent Borders

    func testZone0AccentBorder() {
        let card = LWCard(title: "Zone 0", accentBorder: .lwZone0) {
            VStack(alignment: .leading, spacing: Spacing.sm) {
                Text("80 LEDs · L[40-79] R[80-119]")
                    .font(.caption)
                    .foregroundStyle(Color.lwTextSecondary)

                HStack {
                    Text("Effect")
                        .foregroundStyle(Color.lwTextSecondary)
                    Spacer()
                    Text("Ripple Enhanced")
                        .foregroundStyle(Color.lwTextPrimary)
                }
            }
            .font(.bodyValue)
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: card,
            width: 375,
            height: 150,
            named: "card-zone0-border"
        )
    }

    func testZone2AccentBorderElevated() {
        let card = LWCard(title: "Zone 2", elevated: true, accentBorder: .lwZone2) {
            VStack(alignment: .leading, spacing: Spacing.sm) {
                Text("Accent border with elevated shadow")
                    .foregroundStyle(Color.lwTextPrimary)
                    .font(.bodyValue)

                HStack {
                    Text("Brightness")
                        .foregroundStyle(Color.lwTextSecondary)
                    Spacer()
                    Text("178")
                        .foregroundStyle(Color.lwGold)
                }
            }
            .font(.bodyValue)
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: card,
            width: 375,
            height: 150,
            named: "card-zone2-elevated-border"
        )
    }

    // MARK: - Multiple Cards Layout

    func testMultipleCardsVerticalStack() {
        let view = VStack(spacing: Spacing.md) {
            LWCard(title: "Card One") {
                Text("First card content")
                    .foregroundStyle(Color.lwTextPrimary)
                    .font(.bodyValue)
            }

            LWCard(title: "Card Two", elevated: true) {
                Text("Second card with elevation")
                    .foregroundStyle(Color.lwTextPrimary)
                    .font(.bodyValue)
            }

            LWCard(title: "Card Three", accentBorder: .lwGold) {
                Text("Third card with accent")
                    .foregroundStyle(Color.lwTextPrimary)
                    .font(.bodyValue)
            }
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 375,
            height: 500,
            named: "multiple-cards-stack"
        )
    }
}
