//
//  ControlBusSmoothingCard.swift
//  LightwaveOS
//
//  Control bus smoothing parameters with alpha fast and slow steppers.
//  iOS 17+, Swift 6.
//

import SwiftUI

struct ControlBusSmoothingCard: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        @Bindable var audio = app.audio

        VStack(alignment: .leading, spacing: Spacing.sm) {
            Text("CONTROL BUS SMOOTHING")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwTextPrimary)
                .textCase(.uppercase)
                .tracking(1.5)

            VStack(spacing: 12) {
                StepperControl(
                    label: "Alpha Fast",
                    value: $audio.alphaFast,
                    step: 0.01,
                    format: "%.2f"
                ) {
                    audio.queueAudioTuningUpdate()
                }

                StepperControl(
                    label: "Alpha Slow",
                    value: $audio.alphaSlow,
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

#Preview("Control Bus Smoothing Card") {
    ControlBusSmoothingCard()
        .environment({
            let vm = AppViewModel()
            vm.audio.alphaFast = 0.35
            vm.audio.alphaSlow = 0.12
            return vm
        }())
        .padding(Spacing.lg)
        .background(Color.lwBase)
        .preferredColorScheme(.dark)
}
