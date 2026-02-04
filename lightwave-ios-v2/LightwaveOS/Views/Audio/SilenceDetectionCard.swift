//
//  SilenceDetectionCard.swift
//  LightwaveOS
//
//  Silence detection parameters with hysteresis and threshold controls.
//  iOS 17+, Swift 6.
//

import SwiftUI

struct SilenceDetectionCard: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        @Bindable var audio = app.audio

        VStack(alignment: .leading, spacing: Spacing.sm) {
            Text("SILENCE DETECTION")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwTextPrimary)
                .textCase(.uppercase)
                .tracking(1.5)

            VStack(spacing: 12) {
                StepperControl(
                    label: "Hysteresis",
                    value: $audio.silenceHysteresis,
                    step: 500,
                    format: "%.0f ms"
                ) {
                    audio.queueAudioTuningUpdate()
                }

                StepperControl(
                    label: "Threshold",
                    value: $audio.silenceThreshold,
                    step: 0.01,
                    format: "%.2f"
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

#Preview("Silence Detection Card") {
    SilenceDetectionCard()
        .environment({
            let vm = AppViewModel()
            vm.audio.silenceHysteresis = 5000
            vm.audio.silenceThreshold = 0.01
            return vm
        }())
        .padding(Spacing.lg)
        .background(Color.lwBase)
        .preferredColorScheme(.dark)
}
