//
//  PlayTab.swift
//  LightwaveOS
//
//  Main performance tab with LED preview, pattern/palette selectors,
//  primary controls, expression parameters, and transitions.
//

import SwiftUI

struct PlayTab: View {
    @Environment(AppViewModel.self) private var appVM

    var body: some View {
        ScrollView {
            VStack(spacing: Spacing.lg) {
                // Hero LED preview (120pt)
                HeroLEDPreview()

                // Pattern and Palette pills
                PatternPill()
                PalettePill()

                // Primary controls (brightness + speed)
                PrimaryControlsCard()

                // Expression parameters (collapsible, 7 params)
                ExpressionParametersCard()

                // Colour correction (replaces transition card)
                ColourCorrectionCard()
            }
            .padding(.horizontal, 16)
            .padding(.vertical, Spacing.lg)
        }
        .background(Color.lwBase)
    }
}

// MARK: - Preview

#Preview("Play Tab") {
    PlayTab()
        .environment(AppViewModel())
}

#Preview("Play Tab - Alternative State") {
    PlayTab()
        .environment({
            let vm = AppViewModel()
            vm.effects.currentEffectName = "Neural Mesh"
            vm.effects.currentEffectId = 15
            vm.palettes.currentPaletteId = 22
            vm.parameters.brightness = 120
            vm.parameters.speed = 35
            vm.parameters.hue = 200
            vm.parameters.saturation = 180
            vm.parameters.mood = 50
            vm.parameters.fadeAmount = 100
            vm.parameters.intensity = 200
            vm.parameters.complexity = 180
            vm.parameters.variation = 80
            vm.audio.currentBpm = 0
            return vm
        }())
}
