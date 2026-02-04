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
                    .font(.iconSmall)
                    .foregroundStyle(Color.lwGold)
                    .frame(width: 44, height: 44)
            }
            .buttonStyle(.plain)
            .accessibilityLabel(Text("Previous effect"))

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
                                    .font(.iconTiny)
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
                            .foregroundStyle(Color.lwTextSecondary)

                        Image(systemName: "chevron.right")
                            .font(.iconSmall)
                            .foregroundStyle(Color.lwGold)
                    }
                }
            }
            .buttonStyle(.plain)
            .accessibilityLabel(Text("Select effect"))
            .accessibilityValue(Text(appVM.effects.currentEffectName))
            .accessibilityHint(Text("Opens the effect selector."))

            // Next button
            Button {
                Task { await appVM.effects.nextEffect() }
            } label: {
                Image(systemName: "chevron.right")
                    .font(.iconSmall)
                    .foregroundStyle(Color.lwGold)
                    .frame(width: 44, height: 44)
            }
            .buttonStyle(.plain)
            .accessibilityLabel(Text("Next effect"))
        }
        .padding(.horizontal, 4)
        .padding(.vertical, 12)
        .background(
            RoundedRectangle(cornerRadius: CornerRadius.card)
                .fill(Color.lwCardGradient)
        )
        .overlay(alignment: .topTrailing) {
            patternAccent
                .padding(.top, 8)
                .padding(.trailing, 12)
        }
        .ambientShadow()
        .sheet(isPresented: $showEffectSelector) {
            EffectSelectorView()
        }
    }

    private var patternAccent: some View {
        return AnyView(
            RiveViewContainer(
                asset: RiveAssetRegistry.patternPillAccent,
                inputs: [
                    .number("index", Double(appVM.effects.currentEffectId)),
                    .number("count", Double(appVM.effects.allEffects.count))
                ],
                fallback: AnyView(EmptyView())
            )
            .frame(width: 36, height: 36)
            .opacity(0.9)
            .allowsHitTesting(false)
        )
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
