//
//  DSPHealthCard.swift
//  LightwaveOS
//
//  DSP health metrics card with noise floor and clip detection.
//  iOS 17+, Swift 6.
//

import SwiftUI

struct DSPHealthCard: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        VStack(alignment: .leading, spacing: Spacing.sm) {
            Text("DSP HEALTH")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwTextPrimary)
                .textCase(.uppercase)
                .tracking(1.5)

            VStack(spacing: 12) {
                MetricBar(
                    label: "NOISE FLOOR",
                    value: String(format: "%.4f", app.audio.noiseFloor),
                    fill: Double(app.audio.noiseFloor) * 20, // Scale for visibility
                    fillColor: .lwError
                )

                BooleanIndicator(
                    label: "Clip Count",
                    value: "\(app.audio.clipCount)",
                    isOn: app.audio.clipCount > 0
                )
            }
        }
        .padding(16)
        .background(
            RoundedRectangle(cornerRadius: CornerRadius.card)
                .fill(Color.lwCard)
        )
        .shadow(color: .black.opacity(0.4), radius: 12, x: 0, y: 2)
    }
}

// MARK: - Preview

#Preview("DSP Health Card") {
    DSPHealthCard()
        .environment({
            let vm = AppViewModel()
            vm.audio.noiseFloor = 0.0004
            vm.audio.clipCount = 0
            return vm
        }())
        .padding(Spacing.lg)
        .background(Color.lwBase)
        .preferredColorScheme(.dark)
}
