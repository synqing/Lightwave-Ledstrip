//
//  ColourCorrectionView.swift
//  LightwaveOS
//
//  Gamma, auto-exposure, brown guardrail, and mode controls.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import SwiftUI

struct ColourCorrectionView: View {
    @Environment(AppViewModel.self) private var appVM

    var body: some View {
        @Bindable var cc = appVM.colourCorrection

        List {
            Section("Gamma") {
                Toggle("Enable Gamma Correction", isOn: $cc.gammaEnabled)
                    .tint(.lwGold)

                HStack {
                    Text("Gamma Value:")
                        .foregroundStyle(Color.lwTextSecondary)
                    Spacer()
                    Text(String(format: "%.1f", cc.gammaValue))
                        .foregroundStyle(Color.lwGold)
                        .monospacedDigit()
                }

                if cc.gammaEnabled {
                    Slider(value: $cc.gammaValue, in: 1.0...3.0, step: 0.1)
                        .tint(.lwGold)
                }
            }
            .listRowBackground(Color.lwCard)

            Section("Auto-Exposure") {
                Toggle("Enable Auto-Exposure", isOn: $cc.autoExposureEnabled)
                    .tint(.lwGold)

                HStack {
                    Text("Target Brightness:")
                        .foregroundStyle(Color.lwTextSecondary)
                    Spacer()
                    Text("\(cc.autoExposureTarget)")
                        .foregroundStyle(Color.lwGold)
                        .monospacedDigit()
                }

                if cc.autoExposureEnabled {
                    Slider(
                        value: Binding(
                            get: { Double(cc.autoExposureTarget) },
                            set: { cc.autoExposureTarget = Int($0) }
                        ),
                        in: 0...255,
                        step: 1
                    )
                    .tint(.lwGold)
                }
            }
            .listRowBackground(Color.lwCard)

            Section("Brown Guardrail") {
                Toggle("Enable Brown Guardrail", isOn: $cc.brownGuardrailEnabled)
                    .tint(.lwGold)
            }
            .listRowBackground(Color.lwCard)

            Section("Mode") {
                Picker("Correction Mode", selection: $cc.mode) {
                    ForEach(CCMode.allCases) { mode in
                        Text(mode.name).tag(mode)
                    }
                }
                .pickerStyle(.segmented)
                .tint(.lwGold)
            }
            .listRowBackground(Color.lwCard)
        }
        .listStyle(.insetGrouped)
        .scrollContentBackground(.hidden)
        .background(Color.lwBase)
        .navigationTitle("Colour Correction")
        .navigationBarTitleDisplayMode(.inline)
    }
}

// MARK: - Preview

#Preview("Colour Correction View - Default") {
    NavigationStack {
        ColourCorrectionView()
            .environment(AppViewModel())
    }
}

#Preview("Colour Correction View - Full") {
    let appVM = {
        let vm = AppViewModel()
        vm.colourCorrection.config = ColourCorrectionConfig(
            gammaEnabled: true,
            gammaValue: 2.4,
            autoExposureEnabled: true,
            autoExposureTarget: 180,
            brownGuardrailEnabled: true,
            mode: .both
        )
        return vm
    }()

    NavigationStack {
        ColourCorrectionView()
            .environment(appVM)
    }
}
