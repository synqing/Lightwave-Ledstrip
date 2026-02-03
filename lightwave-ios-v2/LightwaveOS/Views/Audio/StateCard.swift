//
//  StateCard.swift
//  LightwaveOS
//
//  Audio state card displaying silence detection status and scale.
//  iOS 17+, Swift 6.
//

import SwiftUI

struct StateCard: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        VStack(alignment: .leading, spacing: Spacing.sm) {
            Text("STATE")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwTextPrimary)
                .textCase(.uppercase)
                .tracking(1.5)

            VStack(spacing: 12) {
                BooleanIndicator(
                    label: "Silent",
                    value: app.audio.isSilent ? "YES" : "NO",
                    isOn: app.audio.isSilent
                )

                MetricBar(
                    label: "SILENT SCALE",
                    value: String(format: "%.2f", app.audio.silentScale),
                    fill: Double(app.audio.silentScale),
                    fillColor: .lwSuccess
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

#Preview("State Card") {
    StateCard()
        .environment({
            let vm = AppViewModel()
            vm.audio.isSilent = false
            vm.audio.silentScale = 0.85
            return vm
        }())
        .padding(Spacing.lg)
        .background(Color.lwBase)
        .preferredColorScheme(.dark)
}
