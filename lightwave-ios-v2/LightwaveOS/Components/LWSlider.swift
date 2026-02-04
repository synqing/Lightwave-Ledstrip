//
//  LWSlider.swift
//  LightwaveOS
//
//  Gold-accent slider with gradient track, glow effect, and endpoint labels.
//  Used for brightness (0-255), speed (1-50), mood (0-255), etc.
//

import SwiftUI

struct LWSlider: View {
    let title: String
    @Binding var value: Double
    let range: ClosedRange<Double>
    let step: Double
    var trackGradient: LinearGradient?
    var leftLabel: String?
    var rightLabel: String?
    var onChanged: ((Double) -> Void)?
    var onEnded: (() -> Void)?

    @State private var isDragging = false
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    var body: some View {
        VStack(alignment: .leading, spacing: Spacing.sm) {
            // Title and value
            HStack {
                Text(title)
                    .font(.cardLabel)
                    .foregroundStyle(Color.lwTextSecondary)
                    .textCase(.uppercase)

                Spacer()

                Text("\(Int(value))")
                    .font(.sliderValue)
                    .monospacedDigit()
                    .foregroundStyle(Color.lwGold)
            }

            // Custom slider track with gradient
            GeometryReader { geometry in
                ZStack(alignment: .leading) {
                    // Background track
                    RoundedRectangle(cornerRadius: 5)
                        .fill(Color.lwCard)
                        .frame(height: 10)

                    // Filled portion with gradient
                    let fillWidth = CGFloat((value - range.lowerBound) / (range.upperBound - range.lowerBound)) * geometry.size.width
                    RoundedRectangle(cornerRadius: 5)
                        .fill(trackGradient ?? defaultGradient)
                        .frame(width: max(0, fillWidth), height: 10)

                    // Thumb
                    Circle()
                        .fill(Color.lwGold)
                        .frame(width: 24, height: 24)
                        .shadow(
                            color: isDragging && !reduceMotion ? Color.lwGold.opacity(0.6) : Color.clear,
                            radius: isDragging && !reduceMotion ? 8 : 0,
                            x: 0,
                            y: 0
                        )
                        .position(
                            x: fillWidth,
                            y: geometry.size.height / 2
                        )
                        .gesture(
                            DragGesture(minimumDistance: 0)
                                .onChanged { gesture in
                                    if !isDragging {
                                        isDragging = true
                                    }
                                    updateValue(from: gesture.location.x, in: geometry.size.width)
                                    onChanged?(value)
                                }
                                .onEnded { _ in
                                    withAnimation(.easeOut(duration: 0.3)) {
                                        isDragging = false
                                    }
                                    onEnded?()
                                }
                        )
                }
            }
            .frame(height: 24)

            // Endpoint labels
            if let leftLabel = leftLabel, let rightLabel = rightLabel {
                HStack {
                    Text(leftLabel)
                        .font(.caption)
                        .foregroundStyle(Color.lwTextTertiary)

                    Spacer()

                    Text(rightLabel)
                        .font(.caption)
                        .foregroundStyle(Color.lwTextTertiary)
                }
            }
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(Text(title))
        .accessibilityValue(Text("\(Int(value))"))
        .accessibilityHint(Text("Swipe up or down to adjust."))
        .accessibilityAdjustableAction { direction in
            let delta = step
            switch direction {
            case .increment:
                value = min(value + delta, range.upperBound)
            case .decrement:
                value = max(value - delta, range.lowerBound)
            @unknown default:
                break
            }
            onChanged?(value)
        }
    }

    // MARK: - Helpers

    private var defaultGradient: LinearGradient {
        LinearGradient(
            colors: [Color.lwGold, Color.lwGold],
            startPoint: .leading,
            endPoint: .trailing
        )
    }

    private func updateValue(from x: CGFloat, in width: CGFloat) {
        let percent = min(max(0, x / width), 1)
        let newValue = range.lowerBound + (range.upperBound - range.lowerBound) * Double(percent)
        let steppedValue = round(newValue / step) * step
        value = min(max(steppedValue, range.lowerBound), range.upperBound)
    }
}

// MARK: - Preview

#Preview("Slider Variants") {
    struct PreviewWrapper: View {
        @State private var brightness: Double = 178
        @State private var speed: Double = 15
        @State private var saturation: Double = 200
        @State private var mood: Double = 100

        var body: some View {
            ScrollView {
                VStack(spacing: Spacing.lg) {
                    LWCard(title: "Primary Controls") {
                        VStack(spacing: Spacing.md) {
                            LWSlider(
                                title: "Brightness",
                                value: $brightness,
                                range: 0...255,
                                step: 1,
                                onChanged: { value in
                                    print("Brightness changing: \(value)")
                                },
                                onEnded: {
                                    print("Brightness ended at: \(brightness)")
                                }
                            )

                            LWSlider(
                                title: "Speed",
                                value: $speed,
                                range: 1...50,
                                step: 1,
                                trackGradient: LinearGradient(
                                    colors: [Color.lwCyan, Color.lwGold],
                                    startPoint: .leading,
                                    endPoint: .trailing
                                )
                            )
                        }
                    }

                    LWCard(title: "Expression Parameters") {
                        VStack(spacing: Spacing.md) {
                            LWSlider(
                                title: "Saturation",
                                value: $saturation,
                                range: 0...255,
                                step: 1,
                                trackGradient: LinearGradient(
                                    colors: [Color.gray, Color.lwGold],
                                    startPoint: .leading,
                                    endPoint: .trailing
                                )
                            )

                            LWSlider(
                                title: "Mood",
                                value: $mood,
                                range: 0...255,
                                step: 1,
                                leftLabel: "Reactive",
                                rightLabel: "Smooth"
                            )
                        }
                    }
                }
                .padding(Spacing.lg)
            }
            .background(Color.lwBase)
        }
    }

    return PreviewWrapper()
}
