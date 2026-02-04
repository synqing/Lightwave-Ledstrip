//
//  NoveltyOnsetCard.swift
//  LightwaveOS
//
//  Novelty and onset detection with spectral flux toggle and scale control.
//  iOS 17+, Swift 6.
//

import SwiftUI

struct NoveltyOnsetCard: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        @Bindable var audio = app.audio

        VStack(alignment: .leading, spacing: Spacing.sm) {
            Text("NOVELTY / ONSET")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwTextPrimary)
                .textCase(.uppercase)
                .tracking(1.5)

            VStack(spacing: 12) {
                // Spectral flux toggle
                HStack {
                    Text("Spectral Flux")
                        .font(.cardLabel)
                        .foregroundStyle(Color.lwTextSecondary)

                    Spacer()

                    Toggle("", isOn: $audio.spectralFluxEnabled)
                        .labelsHidden()
                        .tint(.lwGold)
                        .onChange(of: audio.spectralFluxEnabled) { _, _ in
                            audio.queueAudioTuningUpdate()
                        }
                }

                StepperControl(
                    label: "Flux Scale",
                    value: $audio.fluxScale,
                    step: 0.1,
                    format: "%.1f"
                ) {
                    audio.queueAudioTuningUpdate()
                }
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

#Preview("Novelty Onset Card") {
    NoveltyOnsetCard()
        .environment({
            let vm = AppViewModel()
            vm.audio.spectralFluxEnabled = true
            vm.audio.fluxScale = 1.0
            return vm
        }())
        .padding(Spacing.lg)
        .background(Color.lwBase)
        .preferredColorScheme(.dark)
}
