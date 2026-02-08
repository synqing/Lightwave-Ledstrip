//
//  LWSliderSnapshotTests.swift
//  LightwaveOSTests
//
//  Snapshot tests for LWSlider component with various value ranges and gradients.
//

import XCTest
import SwiftUI
@testable import LightwaveOS

final class LWSliderSnapshotTests: SnapshotTestCase {

    // MARK: - Basic Slider States

    func testSliderBrightnessLow() {
        let view = StatefulWrapper(value: 40.0) { value in
            LWSlider(
                title: "Brightness",
                value: value,
                range: 0...255,
                step: 1
            )
            .padding(Spacing.lg)
        }

        assertSnapshot(
            of: view,
            width: 375,
            height: 120,
            named: "slider-brightness-low"
        )
    }

    func testSliderBrightnessMedium() {
        let view = StatefulWrapper(value: 128.0) { value in
            LWSlider(
                title: "Brightness",
                value: value,
                range: 0...255,
                step: 1
            )
            .padding(Spacing.lg)
        }

        assertSnapshot(
            of: view,
            width: 375,
            height: 120,
            named: "slider-brightness-medium"
        )
    }

    func testSliderBrightnessHigh() {
        let view = StatefulWrapper(value: 220.0) { value in
            LWSlider(
                title: "Brightness",
                value: value,
                range: 0...255,
                step: 1
            )
            .padding(Spacing.lg)
        }

        assertSnapshot(
            of: view,
            width: 375,
            height: 120,
            named: "slider-brightness-high"
        )
    }

    // MARK: - Custom Gradient Sliders

    func testSliderSpeedWithGradient() {
        let view = StatefulWrapper(value: 25.0) { value in
            LWSlider(
                title: "Speed",
                value: value,
                range: 1...50,
                step: 1,
                trackGradient: LinearGradient(
                    colors: [Color.lwCyan, Color.lwGold],
                    startPoint: .leading,
                    endPoint: .trailing
                )
            )
            .padding(Spacing.lg)
        }

        assertSnapshot(
            of: view,
            width: 375,
            height: 120,
            named: "slider-speed-gradient"
        )
    }

    func testSliderSaturationGradient() {
        let view = StatefulWrapper(value: 180.0) { value in
            LWSlider(
                title: "Saturation",
                value: value,
                range: 0...255,
                step: 1,
                trackGradient: LinearGradient(
                    colors: [Color.gray, Color.lwGold],
                    startPoint: .leading,
                    endPoint: .trailing
                )
            )
            .padding(Spacing.lg)
        }

        assertSnapshot(
            of: view,
            width: 375,
            height: 120,
            named: "slider-saturation-gradient"
        )
    }

    // MARK: - Sliders with Endpoint Labels

    func testSliderWithEndpointLabels() {
        let view = StatefulWrapper(value: 150.0) { value in
            LWSlider(
                title: "Mood",
                value: value,
                range: 0...255,
                step: 1,
                leftLabel: "Reactive",
                rightLabel: "Smooth"
            )
            .padding(Spacing.lg)
        }

        assertSnapshot(
            of: view,
            width: 375,
            height: 140,
            named: "slider-with-labels"
        )
    }

    func testSliderComplexityWithLabels() {
        let view = StatefulWrapper(value: 200.0) { value in
            LWSlider(
                title: "Complexity",
                value: value,
                range: 0...255,
                step: 1,
                trackGradient: LinearGradient(
                    colors: [Color.lwCyan, Color.lwSuccess, Color.lwGold],
                    startPoint: .leading,
                    endPoint: .trailing
                ),
                leftLabel: "Simple",
                rightLabel: "Complex"
            )
            .padding(Spacing.lg)
        }

        assertSnapshot(
            of: view,
            width: 375,
            height: 140,
            named: "slider-complexity-labels"
        )
    }

    // MARK: - Multiple Sliders in Card

    func testMultipleSlidersInCard() {
        let view = LWCard(title: "Expression Parameters") {
            VStack(spacing: Spacing.md) {
                StatefulWrapper(value: 178.0) { value in
                    LWSlider(
                        title: "Intensity",
                        value: value,
                        range: 0...255,
                        step: 1
                    )
                }

                StatefulWrapper(value: 120.0) { value in
                    LWSlider(
                        title: "Trails",
                        value: value,
                        range: 0...255,
                        step: 1,
                        trackGradient: LinearGradient(
                            colors: [Color.lwCard, Color.lwGold],
                            startPoint: .leading,
                            endPoint: .trailing
                        ),
                        leftLabel: "Off",
                        rightLabel: "Long"
                    )
                }

                StatefulWrapper(value: 200.0) { value in
                    LWSlider(
                        title: "Saturation",
                        value: value,
                        range: 0...255,
                        step: 1,
                        trackGradient: LinearGradient(
                            colors: [Color.gray, Color.lwGold],
                            startPoint: .leading,
                            endPoint: .trailing
                        )
                    )
                }
            }
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 375,
            height: 400,
            named: "multiple-sliders-in-card"
        )
    }

    // MARK: - Edge Cases

    func testSliderAtMinimum() {
        let view = StatefulWrapper(value: 0.0) { value in
            LWSlider(
                title: "Value at Min",
                value: value,
                range: 0...255,
                step: 1,
                leftLabel: "Min",
                rightLabel: "Max"
            )
            .padding(Spacing.lg)
        }

        assertSnapshot(
            of: view,
            width: 375,
            height: 140,
            named: "slider-at-minimum"
        )
    }

    func testSliderAtMaximum() {
        let view = StatefulWrapper(value: 255.0) { value in
            LWSlider(
                title: "Value at Max",
                value: value,
                range: 0...255,
                step: 1,
                leftLabel: "Min",
                rightLabel: "Max"
            )
            .padding(Spacing.lg)
        }

        assertSnapshot(
            of: view,
            width: 375,
            height: 140,
            named: "slider-at-maximum"
        )
    }
}

// MARK: - Helper Views

/// Wrapper for testing views with @Binding requirements
private struct StatefulWrapper<Content: View>: View {
    @State private var value: Double

    let content: (Binding<Double>) -> Content

    init(value: Double, @ViewBuilder content: @escaping (Binding<Double>) -> Content) {
        self._value = State(initialValue: value)
        self.content = content
    }

    var body: some View {
        content($value)
    }
}
