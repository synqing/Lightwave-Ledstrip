//
//  AudioMicroInteractionCard.swift
//  LightwaveOS
//
//  Small Rive-driven indicator for audio activity.
//

import SwiftUI

struct AudioMicroInteractionCard: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        LWCard(title: "AUDIO FLOW") {
            HStack(spacing: Spacing.md) {
                fallbackIndicator
                    .frame(width: 120, height: 48)

                VStack(alignment: .leading, spacing: 4) {
                    Text("Live input")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)

                    Text(app.audio.isSilent ? "Silent" : "Active")
                        .font(.caption)
                        .foregroundStyle(app.audio.isSilent ? Color.lwTextTertiary : Color.lwSuccess)
                }

                Spacer()
            }
        }
    }

    private var fallbackIndicator: some View {
        HStack(spacing: 4) {
            ForEach(0..<8, id: \.self) { index in
                RoundedRectangle(cornerRadius: 2)
                    .fill(Color.lwGold.opacity(0.6))
                    .frame(width: 6, height: CGFloat(8 + (index * 2)))
            }
        }
        .frame(height: 48)
    }
}

#Preview("Audio Micro Interaction") {
    AudioMicroInteractionCard()
        .environment(AppViewModel())
        .padding(Spacing.lg)
        .background(Color.lwBase)
}
