//
//  AudioTab.swift
//  LightwaveOS
//
//  Audio tab with spectrum visualisation cards and DSP diagnostic/control cards.
//  iOS 17+, Swift 6.
//

import SwiftUI

struct AudioTab: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        ScrollView {
            VStack(spacing: Spacing.lg) {
                // Spectrum visualisation cards (top 3)
                SpectrumVisualisationCard(title: "WATERFALL") { palette in
                    WaterfallCanvas(palette: palette)
                }

                SpectrumVisualisationCard(title: "DOT RIDGE") { palette in
                    DotRidgeCanvas(palette: palette)
                }

                SpectrumVisualisationCard(title: "DOT MATRIX") { palette in
                    DotMatrixCanvas(palette: palette)
                }

                // DSP diagnostic cards
                EnergyCard()

                BeatTempoCard()

                StateCard()

                WaveformSpectrumCard()

                DSPHealthCard()

                BeatHapticsCard()

                // SPH0645 tuning (formerly Audio Parameters)
                AudioParametersCard()

                // DSP control cards
                ControlBusSmoothingCard()

                BandEnvelopeCard()

                SilenceDetectionCard()

                NoveltyOnsetCard()
            }
            .padding(.horizontal, 16)
            .padding(.vertical, Spacing.lg)
        }
        .background(Color.lwBase)
    }
}

// MARK: - Preview

#Preview("Audio Tab") {
    AudioTab()
        .environment({
            let vm = AppViewModel()
            // Mock audio metrics
            vm.audio.rms = 0.342
            vm.audio.flux = 0.128
            vm.audio.rmsRaw = 0.287
            vm.audio.rmsMapped = 0.451
            vm.audio.bpmConfidence = 0.87
            vm.audio.tempoLocked = true
            vm.audio.isSilent = false
            vm.audio.silentScale = 0.85
            vm.audio.noiseFloor = 0.0004
            vm.audio.clipCount = 0
            vm.audio.waveform = (0..<128).map { i in
                Float(sin(Double(i) * 0.1) * 0.5)
            }
            vm.audio.bands = [0.8, 0.6, 0.5, 0.4, 0.3, 0.2, 0.15, 0.1]
            return vm
        }())
        .preferredColorScheme(.dark)
}
