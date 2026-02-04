//
//  TransitionCard.swift
//  LightwaveOS
//
//  Transition controls: type picker, duration slider, and trigger button.
//

import SwiftUI

struct TransitionCard: View {
    @Environment(AppViewModel.self) private var appVM
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
                                appVM.transition.setType(type)
                            } label: {
                                HStack {
                                    Text(type.name)
                                    if type == appVM.transition.currentType {
                                        Image(systemName: "checkmark")
                                    }
                                }
                            }
                        }
                    } label: {
                        HStack(spacing: Spacing.xs) {
                            Text(appVM.transition.currentType.name)
                                .font(.bodyValue)
                                .foregroundStyle(Color.lwTextPrimary)

                            Image(systemName: "chevron.up.chevron.down")
                                .font(.iconTiny)
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
                        appVM.transition.setDuration(Int(value))
                    },
                    onEnded: {
                        appVM.transition.setDuration(Int(transitionDuration))
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
        .onAppear {
            transitionDuration = Double(appVM.transition.duration)
        }
        .onChange(of: appVM.transition.duration) { _, newValue in
            transitionDuration = Double(newValue)
        }
    }

    // MARK: - Actions

    private func triggerTransition() {
        Task {
            await appVM.transition.triggerTransition()
        }
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
