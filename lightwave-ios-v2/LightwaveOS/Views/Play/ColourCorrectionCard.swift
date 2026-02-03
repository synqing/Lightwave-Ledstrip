//
//  ColourCorrectionCard.swift
//  LightwaveOS
//
//  Card-based colour correction controls for Play tab.
//  Gamma, auto-exposure, brown guardrail, and mode.
//

import SwiftUI

struct ColourCorrectionCard: View {
    @Environment(AppViewModel.self) private var appVM

    var body: some View {
        @Bindable var cc = appVM.colourCorrection

        LWCard {
            VStack(alignment: .leading, spacing: Spacing.md) {
                // Section header
                Text("COLOUR CORRECTION")
                    .font(.sectionHeader)
                    .foregroundStyle(Color.lwTextTertiary)
                    .textCase(.uppercase)
                    .tracking(0.8)

                // Gamma toggle + value
                Toggle("Gamma Correction", isOn: $cc.gammaEnabled)
                    .tint(.lwGold)
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextPrimary)

                if cc.gammaEnabled {
                    LWSlider(
                        title: "Gamma",
                        value: Binding(
                            get: { cc.gammaValue },
                            set: { cc.gammaValue = $0 }
                        ),
                        range: 1.0...3.0,
                        step: 0.1
                    )
                }

                // Divider
                Rectangle()
                    .fill(Color.white.opacity(0.04))
                    .frame(height: 1)

                // Auto-Exposure toggle + value
                Toggle("Auto-Exposure", isOn: $cc.autoExposureEnabled)
                    .tint(.lwGold)
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextPrimary)

                if cc.autoExposureEnabled {
                    LWSlider(
                        title: "Target Brightness",
                        value: Binding(
                            get: { Double(cc.autoExposureTarget) },
                            set: { cc.autoExposureTarget = Int($0) }
                        ),
                        range: 0...255,
                        step: 1
                    )
                }

                // Divider
                Rectangle()
                    .fill(Color.white.opacity(0.04))
                    .frame(height: 1)

                // Brown Guardrail toggle
                Toggle("Brown Guardrail", isOn: $cc.brownGuardrailEnabled)
                    .tint(.lwGold)
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextPrimary)

                // Divider
                Rectangle()
                    .fill(Color.white.opacity(0.04))
                    .frame(height: 1)

                // Mode picker
                HStack {
                    Text("Mode")
                        .font(.cardLabel)
                        .foregroundStyle(Color.lwTextSecondary)

                    Spacer()

                    Picker("", selection: $cc.mode) {
                        ForEach(CCMode.allCases) { mode in
                            Text(mode.name).tag(mode)
                        }
                    }
                    .pickerStyle(.segmented)
                    .frame(maxWidth: 240)
                    .tint(.lwGold)
                }
            }
        }
    }
}

// MARK: - Preview

#Preview("Colour Correction Card") {
    VStack(spacing: Spacing.lg) {
        ColourCorrectionCard()
    }
    .padding(Spacing.lg)
    .background(Color.lwBase)
    .environment(AppViewModel())
}
