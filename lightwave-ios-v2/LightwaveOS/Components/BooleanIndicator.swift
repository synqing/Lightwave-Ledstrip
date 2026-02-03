//
//  BooleanIndicator.swift
//  LightwaveOS
//
//  Boolean state indicator row with status dot and value label.
//  iOS 17+, Swift 6.
//

import SwiftUI

/// Reusable boolean indicator with status dot and value display
struct BooleanIndicator: View {
    let label: String
    let value: String
    let isOn: Bool

    var body: some View {
        HStack(spacing: 8) {
            // Status dot (8px)
            Circle()
                .fill(isOn ? Color.lwSuccess : Color.lwTextTertiary)
                .frame(width: 8, height: 8)

            Text(label)
                .font(.caption)
                .foregroundStyle(Color.lwTextPrimary)

            Spacer()

            Text(value)
                .font(.metricValue)
                .monospacedDigit()
                .foregroundStyle(isOn ? Color.lwSuccess : Color.lwTextSecondary)
        }
    }
}

// MARK: - Preview

#Preview("Boolean Indicator Variants") {
    VStack(spacing: Spacing.md) {
        BooleanIndicator(
            label: "Tempo Locked",
            value: "YES",
            isOn: true
        )

        BooleanIndicator(
            label: "Silent",
            value: "NO",
            isOn: false
        )

        BooleanIndicator(
            label: "Clip Count",
            value: "0",
            isOn: false
        )

        BooleanIndicator(
            label: "Spectral Flux",
            value: "ENABLED",
            isOn: true
        )
    }
    .padding(16)
    .background(
        RoundedRectangle(cornerRadius: CornerRadius.card)
            .fill(Color.lwCard)
    )
    .padding(Spacing.lg)
    .background(Color.lwBase)
    .preferredColorScheme(.dark)
}
