//
//  MetricBar.swift
//  LightwaveOS
//
//  Horizontal metric bar row for displaying real-time DSP metrics.
//  iOS 17+, Swift 6.
//

import SwiftUI

/// Reusable metric bar component with label, fill bar, and numeric value
struct MetricBar: View {
    let label: String
    let value: String
    let fill: Double // 0...1
    let fillColor: Color

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text(label)
                    .font(.caption)
                    .foregroundStyle(Color.lwTextTertiary)

                Spacer()

                Text(value)
                    .font(.metricValue)
                    .monospacedDigit()
                    .foregroundStyle(Color.lwTextPrimary)
            }

            // Metric bar track
            GeometryReader { geometry in
                ZStack(alignment: .leading) {
                    // Background track
                    RoundedRectangle(cornerRadius: 4)
                        .fill(Color.lwElevated)
                        .frame(height: 8)

                    // Fill portion
                    let fillWidth = max(0, min(1, fill)) * geometry.size.width
                    RoundedRectangle(cornerRadius: 4)
                        .fill(fillColor)
                        .frame(width: fillWidth, height: 8)
                }
            }
            .frame(height: 8)
        }
    }
}

// MARK: - Preview

#Preview("Metric Bar Variants") {
    VStack(spacing: Spacing.md) {
        MetricBar(
            label: "RMS",
            value: "0.342",
            fill: 0.45,
            fillColor: .lwCyan
        )

        MetricBar(
            label: "FLUX",
            value: "0.128",
            fill: 0.25,
            fillColor: .lwCyan
        )

        MetricBar(
            label: "CONFIDENCE",
            value: "87%",
            fill: 0.87,
            fillColor: .lwGold
        )

        MetricBar(
            label: "NOISE FLOOR",
            value: "0.0004",
            fill: 0.08,
            fillColor: .lwError
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
