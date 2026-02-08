//
//  LWSlider.swift
//  LightwaveOS
//
//  Gold-accent slider with "Liquid Glass" treatment, gradient track, haptic feedback, and endpoint labels.
//  Used for brightness (0-255), speed (1-50), mood (0-255), etc.
//
//  Glass treatment:
//  - Track: glassSurface with thin style (5pt corner radius)
//  - Thumb: 3-layer premium design (base, specular highlight, drag glow)
//  - Haptic feedback: Light impact on value changes
//  - Drag feedback: 1.1x scale and gold glow
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
    @State private var lastHapticValue: Int = 0
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    // Haptic feedback generator
    private let hapticGenerator = UIImpactFeedbackGenerator(style: .light)

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

            // Custom slider track with gradient and glass treatment
            GeometryReader { geometry in
                let thumbRadius: CGFloat = 8 // Half of 16pt thumb width
                let clampedFillWidth = clampedThumbPosition(
                    percent: (value - range.lowerBound) / (range.upperBound - range.lowerBound),
                    width: geometry.size.width,
                    thumbRadius: thumbRadius
                )

                ZStack(alignment: .leading) {
                    // Background track with glass surface
                    RoundedRectangle(cornerRadius: 5)
                        .fill(Color.lwCard)
                        .frame(height: 10)
                        .glassSurface(style: .base, cornerRadius: 5)
                        .contentShape(Rectangle())
                        .gesture(
                            DragGesture(minimumDistance: 0)
                                .onChanged { gesture in
                                    handleDragChanged(at: gesture.location.x, in: geometry.size.width)
                                }
                                .onEnded { _ in
                                    handleDragEnded()
                                }
                        )

                    // Filled portion with gradient
                    RoundedRectangle(cornerRadius: 5)
                        .fill(trackGradient ?? defaultGradient)
                        .frame(width: max(0, clampedFillWidth), height: 10)

                    // Premium 3-layer thumb with 44pt hit target
                    ZStack {
                        premiumThumb()
                    }
                    .frame(width: 44, height: 44) // 44pt hit target
                    .position(
                        x: clampedFillWidth,
                        y: geometry.size.height / 2
                    )
                    .gesture(
                        DragGesture(minimumDistance: 0)
                            .onChanged { gesture in
                                handleDragChanged(at: gesture.location.x, in: geometry.size.width)
                            }
                            .onEnded { _ in
                                handleDragEnded()
                            }
                    )
                }
            }
            .frame(height: 44) // Increased from 24 to accommodate 44pt hit target

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
            hapticGenerator.impactOccurred()
            onChanged?(value)
        }
        .onAppear {
            lastHapticValue = Int(value)
        }
    }

    // MARK: - Helpers

    /// Premium 3-layer thumb with glass treatment
    @ViewBuilder
    private func premiumThumb() -> some View {
        ZStack {
            // Layer 1: Base circle (lwElevated fill)
            Circle()
                .fill(Color.lwElevated)
                .frame(width: 16, height: 16)

            // Layer 2: Specular highlight (top-left ellipse)
            Ellipse()
                .fill(Color.white.opacity(0.3))
                .frame(width: 6, height: 3)
                .offset(x: -2, y: -3)

            // Layer 3: Drag glow (gold, 140% scale, appears on drag)
            if isDragging && !reduceMotion {
                Circle()
                    .fill(Color.lwGold)
                    .frame(width: 16, height: 16)
                    .scaleEffect(1.4)
                    .opacity(0.4)
                    .blur(radius: 8)
            }
        }
        .scaleEffect(isDragging && !reduceMotion ? 1.1 : 1.0)
        .animation(.spring(response: 0.3, dampingFraction: 0.7), value: isDragging)
    }

    private var defaultGradient: LinearGradient {
        LinearGradient(
            colors: [Color.lwGold, Color.lwGold],
            startPoint: .leading,
            endPoint: .trailing
        )
    }

    /// Clamp thumb position so it stays fully within track bounds
    private func clampedThumbPosition(percent: Double, width: CGFloat, thumbRadius: CGFloat) -> CGFloat {
        let rawPosition = CGFloat(percent) * width
        return min(max(rawPosition, thumbRadius), width - thumbRadius)
    }

    /// Unified drag handling with haptic feedback
    private func handleDragChanged(at x: CGFloat, in width: CGFloat) {
        if !isDragging {
            isDragging = true
            hapticGenerator.prepare()
        }

        let oldValue = value
        updateValue(from: x, in: width)

        // Haptic feedback on value changes
        let currentHapticValue = Int(value)
        if currentHapticValue != lastHapticValue {
            hapticGenerator.impactOccurred()
            lastHapticValue = currentHapticValue
        }

        // onChanged is for local UI updates only, no external callback during drag
        // External callbacks happen only on drag end to avoid network spam
    }

    /// Handle drag end with external callback
    private func handleDragEnded() {
        withAnimation(.spring(response: 0.3, dampingFraction: 0.7)) {
            isDragging = false
        }

        // Commit final value to external callback
        onChanged?(value)
        onEnded?()
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
