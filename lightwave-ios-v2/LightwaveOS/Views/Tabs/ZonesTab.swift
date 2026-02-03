//
//  ZonesTab.swift
//  LightwaveOS
//
//  Multi-zone composition tab with zone header, LED strip visualization, and per-zone cards.
//

import SwiftUI

struct ZonesTab: View {
    @Environment(AppViewModel.self) private var app

    var body: some View {
        ScrollView {
            VStack(spacing: Spacing.lg) {
                // LED strip visualisation (top of tab)
                if app.zones.zonesEnabled {
                    LEDStripView()
                        .padding(.horizontal, 16)
                }

                // Zone Composer header card
                ZoneHeaderCard()
                    .padding(.horizontal, 16)

                // Zone cards (dynamically generated based on zone count)
                if app.zones.zonesEnabled {
                    ForEach(0..<app.zones.zoneCount, id: \.self) { index in
                        if index < app.zones.zones.count {
                            ZoneCard(zoneIndex: index)
                                .padding(.horizontal, 16)
                        }
                    }
                }
            }
            .padding(.vertical, Spacing.md)
        }
        .background(Color.lwBase)
    }
}

// MARK: - Preview

#Preview("Zones Tab - Disabled") {
    ZonesTab()
        .environment({
            let vm = AppViewModel()
            vm.zones.zonesEnabled = false
            return vm
        }())
}

#Preview("Zones Tab - 2 Zones") {
    ZonesTab()
        .environment({
            let vm = AppViewModel()
            vm.zones.zonesEnabled = true
            vm.zones.zoneCount = 2
            vm.zones.zones = [
                ZoneConfig(
                    id: 0,
                    enabled: true,
                    effectId: 5,
                    effectName: "Ripple Enhanced",
                    brightness: 200,
                    speed: 25,
                    paletteId: 0,
                    paletteName: "Copper",
                    blendMode: 1,
                    blendModeName: "Additive"
                ),
                ZoneConfig(
                    id: 1,
                    enabled: true,
                    effectId: 12,
                    effectName: "LGP Holographic",
                    brightness: 200,
                    speed: 18,
                    paletteId: 5,
                    paletteName: "Sunset Real",
                    blendMode: 3,
                    blendModeName: "Screen"
                )
            ]
            vm.zones.segments = [
                ZoneSegment(zoneId: 0, s1LeftStart: 40, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 119),
                ZoneSegment(zoneId: 1, s1LeftStart: 0, s1LeftEnd: 39, s1RightStart: 120, s1RightEnd: 159)
            ]
            vm.zones.presets = [
                ZonePreset(id: 0, name: "Unified"),
                ZonePreset(id: 1, name: "Dual Split"),
                ZonePreset(id: 2, name: "Triple Rings"),
                ZonePreset(id: 3, name: "Quad Active")
            ]
            return vm
        }())
}
