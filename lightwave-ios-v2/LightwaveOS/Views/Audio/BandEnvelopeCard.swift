//
//  BandEnvelopeCard.swift
//  LightwaveOS
//
//  Band envelope parameters controlling attack and release for frequency bands.
//  iOS 17+, Swift 6.
//

import SwiftUI

struct BandEnvelopeCard: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        @Bindable var audio = app.audio

        VStack(alignment: .leading, spacing: Spacing.sm) {
            Text("BAND ENVELOPE")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwTextPrimary)
                .textCase(.uppercase)
                .tracking(1.5)

            VStack(spacing: 12) {
                StepperControl(
                    label: "Band Attack",
                    value: $audio.bandAttack,
                    step: 0.01,
                    format: "%.2f"
                ) {
                    // TODO: Send update to firmware
                }

                StepperControl(
                    label: "Band Release",
                    value: $audio.bandRelease,
                    step: 0.01,
                    format: "%.2f"
                ) {
                    // TODO: Send update to firmware
                }

                StepperControl(
                    label: "Heavy Attack",
                    value: $audio.heavyAttack,
                    step: 0.01,
                    format: "%.2f"
                ) {
                    // TODO: Send update to firmware
                }

                StepperControl(
                    label: "Heavy Release",
                    value: $audio.heavyRelease,
                    step: 0.01,
                    format: "%.2f"
                ) {
                    // TODO: Send update to firmware
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

#Preview("Band Envelope Card") {
    BandEnvelopeCard()
        .environment({
            let vm = AppViewModel()
            vm.audio.bandAttack = 0.15
            vm.audio.bandRelease = 0.03
            vm.audio.heavyAttack = 0.08
            vm.audio.heavyRelease = 0.02
            return vm
        }())
        .padding(Spacing.lg)
        .background(Color.lwBase)
        .preferredColorScheme(.dark)
}
