//
//  TransitionCard.swift
//  LightwaveOS
//
//  Transition controls: type picker, duration slider, and trigger button.
//

import SwiftUI

struct TransitionCard: View {
    @State private var selectedTransitionType: TransitionType = .crossfade
    @State private var transitionDuration: Double = 1200 // milliseconds

    var body: some View {
        LWCard {
            VStack(alignment: .leading, spacing: Spacing.md) {
                // Section header
                Text("TRANSITION")
                    .font(.sectionHeader)
                    .foregroundStyle(Color.lwTextTertiary)
                    .textCase(.uppercase)
                    .tracking(0.8)

                // Type picker
                HStack {
                    Text("Type")
                        .font(.cardLabel)
                        .foregroundStyle(Color.lwTextSecondary)

                    Spacer()

                    Menu {
                        ForEach(TransitionType.allCases) { type in
                            Button {
                                selectedTransitionType = type
                            } label: {
                                HStack {
                                    Text(type.name)
                                    if type == selectedTransitionType {
                                        Image(systemName: "checkmark")
                                    }
                                }
                            }
                        }
                    } label: {
                        HStack(spacing: Spacing.xs) {
                            Text(selectedTransitionType.name)
                                .font(.bodyValue)
                                .foregroundStyle(Color.lwTextPrimary)

                            Image(systemName: "chevron.up.chevron.down")
                                .font(.system(size: 11))
                                .foregroundStyle(Color.lwGold)
                        }
                    }
                }

                // Duration slider
                LWSlider(
                    title: "Duration",
                    value: $transitionDuration,
                    range: 200...5000,
                    step: 100,
                    onChanged: { value in
                        // Live update duration
                    },
                    onEnded: {
                        // Commit duration
                    }
                )

                // Duration value display
                HStack {
                    Spacer()
                    Text("\(Int(transitionDuration)) ms")
                        .font(.caption)
                        .foregroundStyle(Color.lwTextTertiary)
                }

                // Trigger button
                Button {
                    triggerTransition()
                } label: {
                    Text("Trigger Transition")
                        .font(.bodyValue)
                        .fontWeight(.semibold)
                        .foregroundStyle(Color.lwBase)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 12)
                        .background(
                            RoundedRectangle(cornerRadius: CornerRadius.nested)
                                .fill(Color.lwGold)
                        )
                }
                .buttonStyle(.plain)
            }
        }
    }

    // MARK: - Actions

    private func triggerTransition() {
        // This will send the transition command to the device via WebSocket
        print("Triggering transition: \(selectedTransitionType.name) @ \(Int(transitionDuration))ms")
    }
}

// MARK: - Preview

#Preview("Transition Card") {
    VStack(spacing: Spacing.lg) {
        TransitionCard()
    }
    .padding(Spacing.lg)
    .background(Color.lwBase)
}
