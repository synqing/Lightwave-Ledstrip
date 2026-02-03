//
//  BeatHapticsCard.swift
//  LightwaveOS
//
//  Beat haptics toggle with subtitle for vibration on beat detection.
//  iOS 17+, Swift 6.
//

import SwiftUI

struct BeatHapticsCard: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        @Bindable var audio = app.audio

        VStack(alignment: .leading, spacing: Spacing.sm) {
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text("Beat Haptics")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)

                    Text("Vibrate on beat detection")
                        .font(.caption)
                        .foregroundStyle(Color.lwTextTertiary)
                }

                Spacer()

                Toggle("", isOn: $audio.hapticsEnabled)
                    .labelsHidden()
                    .tint(.lwGold)
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

#Preview("Beat Haptics Card") {
    BeatHapticsCard()
        .environment({
            let vm = AppViewModel()
            vm.audio.hapticsEnabled = true
            return vm
        }())
        .padding(Spacing.lg)
        .background(Color.lwBase)
        .preferredColorScheme(.dark)
}
