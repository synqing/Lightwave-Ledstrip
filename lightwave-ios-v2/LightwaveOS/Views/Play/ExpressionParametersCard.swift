//
//  ExpressionParametersCard.swift
//  LightwaveOS
//
//  Collapsible card with 7 expression parameters in 2 semantic groups:
//  COLOUR (hue, saturation, mood) and MOTION (trails, intensity, complexity, variation).
//

import SwiftUI

struct ExpressionParametersCard: View {
    @Environment(AppViewModel.self) private var appVM
    @AppStorage("expressionParametersExpanded") private var isExpanded = true

    var body: some View {
        @Bindable var params = appVM.parameters

        LWCard {
            VStack(alignment: .leading, spacing: Spacing.sm) {
                // Collapsible header
                Button {
                    withAnimation(.spring(duration: 0.3, bounce: 0.15)) {
                        isExpanded.toggle()
                    }
                } label: {
                    HStack {
                        Text("EXPRESSION PARAMETERS")
                            .font(.sectionHeader)
                            .foregroundStyle(Color.lwTextTertiary)
                            .textCase(.uppercase)
                            .tracking(0.8)

                        Spacer()

                        Image(systemName: "chevron.right")
                            .font(.system(size: 13, weight: .semibold))
                            .foregroundStyle(Color.lwGold)
                            .rotationEffect(.degrees(isExpanded ? 90 : 0))
                    }
                }
                .buttonStyle(.plain)

                // Expanded content
                if isExpanded {
                    VStack(spacing: Spacing.lg) {
                        // COLOUR group
                        VStack(alignment: .leading, spacing: Spacing.sm) {
                            Text("COLOUR")
                                .font(.cardLabel)
                                .foregroundStyle(Color.lwTextTertiary)
                                .textCase(.uppercase)
                                .tracking(0.6)

                            VStack(spacing: Spacing.xs) {
                                LWSlider(
                                    title: "Saturation",
                                    value: $params.saturation,
                                    range: 0...255,
                                    step: 1,
                                    trackGradient: LinearGradient(
                                        colors: [Color.gray, Color.lwGold],
                                        startPoint: .leading,
                                        endPoint: .trailing
                                    ),
                                    onChanged: { value in
                                        params.sliderChanged("saturation", value: value)
                                    },
                                    onEnded: {
                                        params.sliderReleased("saturation")
                                    }
                                )

                                LWSlider(
                                    title: "Mood",
                                    value: $params.mood,
                                    range: 0...255,
                                    step: 1,
                                    leftLabel: "Reactive",
                                    rightLabel: "Smooth",
                                    onChanged: { value in
                                        params.sliderChanged("mood", value: value)
                                    },
                                    onEnded: {
                                        params.sliderReleased("mood")
                                    }
                                )
                            }
                        }

                        // MOTION group
                        VStack(alignment: .leading, spacing: Spacing.sm) {
                            Text("MOTION")
                                .font(.cardLabel)
                                .foregroundStyle(Color.lwTextTertiary)
                                .textCase(.uppercase)
                                .tracking(0.6)

                            VStack(spacing: Spacing.xs) {
                                LWSlider(
                                    title: "Trails",
                                    value: $params.fadeAmount,
                                    range: 0...255,
                                    step: 1,
                                    leftLabel: "Short",
                                    rightLabel: "Long",
                                    onChanged: { value in
                                        params.sliderChanged("fadeAmount", value: value)
                                    },
                                    onEnded: {
                                        params.sliderReleased("fadeAmount")
                                    }
                                )

                                LWSlider(
                                    title: "Intensity",
                                    value: $params.intensity,
                                    range: 0...255,
                                    step: 1,
                                    leftLabel: "Subtle",
                                    rightLabel: "Bold",
                                    onChanged: { value in
                                        params.sliderChanged("intensity", value: value)
                                    },
                                    onEnded: {
                                        params.sliderReleased("intensity")
                                    }
                                )

                                LWSlider(
                                    title: "Complexity",
                                    value: $params.complexity,
                                    range: 0...255,
                                    step: 1,
                                    leftLabel: "Minimal",
                                    rightLabel: "Dense",
                                    onChanged: { value in
                                        params.sliderChanged("complexity", value: value)
                                    },
                                    onEnded: {
                                        params.sliderReleased("complexity")
                                    }
                                )

                                LWSlider(
                                    title: "Variation",
                                    value: $params.variation,
                                    range: 0...255,
                                    step: 1,
                                    leftLabel: "Uniform",
                                    rightLabel: "Chaotic",
                                    onChanged: { value in
                                        params.sliderChanged("variation", value: value)
                                    },
                                    onEnded: {
                                        params.sliderReleased("variation")
                                    }
                                )
                            }
                        }
                    }
                    .transition(.opacity.combined(with: .scale(scale: 0.98, anchor: .top)))
                }
            }
        }
    }
}

// MARK: - Preview

#Preview("Expression Parameters Card") {
    VStack(spacing: Spacing.lg) {
        ExpressionParametersCard()

        ExpressionParametersCard()
            .environment({
                let vm = AppViewModel()
                vm.parameters.hue = 128
                vm.parameters.saturation = 255
                vm.parameters.mood = 0
                vm.parameters.fadeAmount = 255
                vm.parameters.intensity = 200
                vm.parameters.complexity = 150
                vm.parameters.variation = 100
                return vm
            }())
    }
    .padding(Spacing.lg)
    .background(Color.lwBase)
    .environment(AppViewModel())
}
