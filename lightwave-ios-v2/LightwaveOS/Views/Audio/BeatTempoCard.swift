//
//  BeatTempoCard.swift
//  LightwaveOS
//
//  Beat and tempo metrics card with confidence and tempo lock status.
//  iOS 17+, Swift 6.
//

import SwiftUI

struct BeatTempoCard: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        VStack(alignment: .leading, spacing: Spacing.sm) {
            Text("BEAT / TEMPO")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwTextPrimary)
                .textCase(.uppercase)
                .tracking(1.5)

            VStack(spacing: 12) {
                MetricBar(
                    label: "CONFIDENCE",
                    value: String(format: "%.0f%%", app.audio.bpmConfidence * 100),
                    fill: app.audio.bpmConfidence,
                    fillColor: .lwGold
                )

                BooleanIndicator(
                    label: "Tempo Locked",
                    value: app.audio.tempoLocked ? "YES" : "NO",
                    isOn: app.audio.tempoLocked
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

#Preview("Beat Tempo Card") {
    BeatTempoCard()
        .environment({
            let vm = AppViewModel()
            vm.audio.bpmConfidence = 0.87
            vm.audio.tempoLocked = true
            return vm
        }())
        .padding(Spacing.lg)
        .background(Color.lwBase)
        .preferredColorScheme(.dark)
}
