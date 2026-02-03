//
//  StepperControl.swift
//  LightwaveOS
//
//  Horizontal stepper control with ◀/value/▶ buttons for precise parameter adjustment.
//  iOS 17+, Swift 6.
//

import SwiftUI

/// Reusable stepper control with increment/decrement buttons
struct StepperControl: View {
    let label: String
    @Binding var value: Double
    let step: Double
    let format: String
    var onChanged: (() -> Void)?

    var body: some View {
        HStack {
            Text(label)
                .font(.cardLabel)
                .foregroundStyle(Color.lwTextSecondary)

            Spacer()

            // Stepper control group
            HStack(spacing: 0) {
                // Decrement button (◀)
                Button {
                    value = max(0, value - step)
                    onChanged?()
                } label: {
                    Image(systemName: "chevron.left")
                        .font(.system(size: 12, weight: .semibold))
                        .foregroundStyle(Color.lwGold)
                        .frame(width: 32, height: 28)
                        .background(Color.lwElevated)
                        .overlay(
                            RoundedRectangle(cornerRadius: 6)
                                .strokeBorder(Color.lwGold.opacity(0.2), lineWidth: 1)
                        )
                        .clipShape(
                            UnevenRoundedRectangle(
                                topLeadingRadius: 6,
                                bottomLeadingRadius: 6,
                                bottomTrailingRadius: 0,
                                topTrailingRadius: 0
                            )
                        )
                }
                .buttonStyle(.plain)

                // Value display
                Text(String(format: format, value))
                    .font(.stepperValue)
                    .monospacedDigit()
                    .foregroundStyle(Color.lwTextPrimary)
                    .frame(width: 52)
                    .frame(height: 28)
                    .background(Color.lwBase)
                    .overlay(
                        Rectangle()
                            .strokeBorder(Color.lwGold.opacity(0.2), lineWidth: 1)
                            .padding(.vertical, 0)
                    )

                // Increment button (▶)
                Button {
                    value += step
                    onChanged?()
                } label: {
                    Image(systemName: "chevron.right")
                        .font(.system(size: 12, weight: .semibold))
                        .foregroundStyle(Color.lwGold)
                        .frame(width: 32, height: 28)
                        .background(Color.lwElevated)
                        .overlay(
                            RoundedRectangle(cornerRadius: 6)
                                .strokeBorder(Color.lwGold.opacity(0.2), lineWidth: 1)
                        )
                        .clipShape(
                            UnevenRoundedRectangle(
                                topLeadingRadius: 0,
                                bottomLeadingRadius: 0,
                                bottomTrailingRadius: 6,
                                topTrailingRadius: 6
                            )
                        )
                }
                .buttonStyle(.plain)
            }
        }
    }
}

// MARK: - Preview

#Preview("Stepper Control Variants") {
    struct PreviewContainer: View {
        @State private var alphaFast = 0.35
        @State private var alphaSlow = 0.12
        @State private var hysteresis = 5000.0
        @State private var fluxScale = 1.0

        var body: some View {
            VStack(spacing: Spacing.md) {
                StepperControl(
                    label: "Alpha Fast",
                    value: $alphaFast,
                    step: 0.01,
                    format: "%.2f"
                )

                StepperControl(
                    label: "Alpha Slow",
                    value: $alphaSlow,
                    step: 0.01,
                    format: "%.2f"
                )

                StepperControl(
                    label: "Hysteresis",
                    value: $hysteresis,
                    step: 500,
                    format: "%.0f ms"
                )

                StepperControl(
                    label: "Flux Scale",
                    value: $fluxScale,
                    step: 0.1,
                    format: "%.1f"
                )
            }
            .padding(16)
            .background(
                RoundedRectangle(cornerRadius: CornerRadius.card)
                    .fill(Color.lwCard)
            )
            .padding(Spacing.lg)
            .background(Color.lwBase)
        }
    }

    return PreviewContainer()
        .preferredColorScheme(.dark)
}
