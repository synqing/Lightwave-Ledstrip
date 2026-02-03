//
//  PatternPill.swift
//  LightwaveOS
//
//  Full-width tappable card with prev/next buttons and effect gallery sheet.
//

import SwiftUI

struct PatternPill: View {
    @Environment(AppViewModel.self) private var appVM
    @State private var showEffectSelector = false

    var body: some View {
        HStack(spacing: 0) {
            // Previous button
            Button {
                Task { await appVM.effects.previousEffect() }
            } label: {
                Image(systemName: "chevron.left")
                    .font(.system(size: 16, weight: .semibold))
                    .foregroundStyle(Color.lwGold)
                    .frame(width: 44, height: 44)
            }
            .buttonStyle(.plain)

            // Centre area — tappable to open sheet
            Button {
                showEffectSelector = true
            } label: {
                HStack(alignment: .top, spacing: Spacing.md) {
                    VStack(alignment: .leading, spacing: Spacing.xs) {
                        Text("PATTERN")
                            .font(.sectionHeader)
                            .foregroundStyle(Color.lwTextTertiary)
                            .textCase(.uppercase)
                            .tracking(0.8)

                        Text(appVM.effects.currentEffectName)
                            .font(.bodyValue)
                            .foregroundStyle(Color.lwTextPrimary)

                        if appVM.effects.allEffects.first(where: { $0.id == appVM.effects.currentEffectId })?.isAudioReactive == true {
                            HStack(spacing: Spacing.xs) {
                                Image(systemName: "waveform")
                                    .font(.system(size: 11))
                                Text("Audio-Reactive")
                                    .font(.caption)
                            }
                            .foregroundStyle(Color.lwGold)
                        }
                    }

                    Spacer()

                    HStack(spacing: Spacing.xs) {
                        Text("\(appVM.effects.currentEffectId)/\(appVM.effects.allEffects.count)")
                            .font(.caption)
                            .foregroundStyle(Color.lwTextTertiary)

                        Image(systemName: "chevron.right")
                            .font(.system(size: 13, weight: .semibold))
                            .foregroundStyle(Color.lwGold)
                    }
                }
            }
            .buttonStyle(.plain)

            // Next button
            Button {
                Task { await appVM.effects.nextEffect() }
            } label: {
                Image(systemName: "chevron.right")
                    .font(.system(size: 16, weight: .semibold))
                    .foregroundStyle(Color.lwGold)
                    .frame(width: 44, height: 44)
            }
            .buttonStyle(.plain)
        }
        .padding(.horizontal, 4)
        .padding(.vertical, 12)
        .background(
            RoundedRectangle(cornerRadius: CornerRadius.card)
                .fill(Color.lwCardGradient)
        )
        .ambientShadow()
        .sheet(isPresented: $showEffectSelector) {
            EffectSelectorView()
        }
    }
}

// MARK: - Preview

#Preview("Pattern Pill") {
    VStack(spacing: Spacing.lg) {
        PatternPill()

        PatternPill()
            .environment({
                let vm = AppViewModel()
                vm.effects.currentEffectName = "Starfield"
                vm.effects.currentEffectId = 7
                return vm
            }())
    }
    .padding(Spacing.lg)
    .background(Color.lwBase)
    .environment(AppViewModel())
}
