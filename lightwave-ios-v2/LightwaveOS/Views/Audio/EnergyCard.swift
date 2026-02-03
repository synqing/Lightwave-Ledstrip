//
//  EnergyCard.swift
//  LightwaveOS
//
//  Energy metrics card displaying RMS, Flux, RMS Raw, and RMS Mapped values.
//  iOS 17+, Swift 6.
//

import SwiftUI

struct EnergyCard: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        VStack(alignment: .leading, spacing: Spacing.sm) {
            Text("ENERGY")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwTextPrimary)
                .textCase(.uppercase)
                .tracking(1.5)

            VStack(spacing: 12) {
                MetricBar(
                    label: "RMS",
                    value: String(format: "%.3f", app.audio.rms),
                    fill: Double(app.audio.rms),
                    fillColor: .lwCyan
                )

                MetricBar(
                    label: "FLUX",
                    value: String(format: "%.3f", app.audio.flux),
                    fill: Double(app.audio.flux),
                    fillColor: .lwCyan
                )

                MetricBar(
                    label: "RMS RAW",
                    value: String(format: "%.3f", app.audio.rmsRaw),
                    fill: Double(app.audio.rmsRaw),
                    fillColor: .lwCyan
                )

                MetricBar(
                    label: "RMS MAP",
                    value: String(format: "%.3f", app.audio.rmsMapped),
                    fill: Double(app.audio.rmsMapped),
                    fillColor: .lwCyan
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

#Preview("Energy Card") {
    EnergyCard()
        .environment({
            let vm = AppViewModel()
            vm.audio.rms = 0.342
            vm.audio.flux = 0.128
            vm.audio.rmsRaw = 0.287
            vm.audio.rmsMapped = 0.451
            return vm
        }())
        .padding(Spacing.lg)
        .background(Color.lwBase)
        .preferredColorScheme(.dark)
}
