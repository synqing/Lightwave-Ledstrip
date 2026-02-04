//
//  PrimaryControlsCard.swift
//  LightwaveOS
//
//  Brightness and Speed sliders in a card.
//

import SwiftUI

struct PrimaryControlsCard: View {
    @Environment(AppViewModel.self) private var appVM

    var body: some View {
        @Bindable var params = appVM.parameters

        LWCard {
            VStack(spacing: Spacing.md) {
                // Brightness slider with gold gradient
                LWSlider(
                    title: "Brightness",
                    value: $params.brightness,
                    range: 0...255,
                    step: 1,
                    trackGradient: LinearGradient(
                        colors: [Color.lwGold.opacity(0.5), Color.lwGold],
                        startPoint: .leading,
                        endPoint: .trailing
                    ),
                    onChanged: { value in
                        params.sliderChanged("brightness", value: value)
                    },
                    onEnded: {
                        params.sliderReleased("brightness")
                    }
                )

                // Speed slider with cyan→gold gradient
                LWSlider(
                    title: "Speed",
                    value: $params.speed,
                    range: 1...100,
                    step: 1,
                    trackGradient: LinearGradient(
                        colors: [Color.lwCyan, Color.lwGold],
                        startPoint: .leading,
                        endPoint: .trailing
                    ),
                    onChanged: { value in
                        params.sliderChanged("speed", value: value)
                    },
                    onEnded: {
                        params.sliderReleased("speed")
                    }
                )
            }
        }
    }
}

// MARK: - Preview

#Preview("Primary Controls Card") {
    VStack(spacing: Spacing.lg) {
        PrimaryControlsCard()

        PrimaryControlsCard()
            .environment({
                let vm = AppViewModel()
                vm.parameters.brightness = 255
                vm.parameters.speed = 50
                return vm
            }())

        PrimaryControlsCard()
            .environment({
                let vm = AppViewModel()
                vm.parameters.brightness = 64
                vm.parameters.speed = 5
                return vm
            }())
    }
    .padding(Spacing.lg)
    .background(Color.lwBase)
    .environment(AppViewModel())
}
