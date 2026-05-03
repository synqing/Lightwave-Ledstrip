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

                // Zone cards (dynamically generated based on zone count).
                //
                // SwiftUI overload-resolution footgun: `ForEach(0..<runtimeUpper, id:\.self)`
                // is silently treated as a constant-range form on iOS 17/18, so the
                // rendered cardinality does not re-diff when `zoneCount` mutates.
                // Iterating the Identifiable collection directly (keyed by
                // `ZoneConfig.id`, which is the 1-indexed wire id) gives SwiftUI a
                // stable identity it can diff cleanly across zone-count changes.
                // `prefix(zoneCount)` keeps the visible cardinality bounded by the
                // live zoneCount even if the firmware broadcast briefly contains
                // more entries than zones currently rendered.
                if app.zones.zonesEnabled {
                    ForEach(Array(app.zones.zones.prefix(app.zones.zoneCount).enumerated()),
                            id: \.element.id) { (offset, _) in
                        ZoneCard(zoneIndex: offset)
                            .padding(.horizontal, 16)
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
                    id: 1,
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
                    id: 2,
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
                ZoneSegment(zoneId: 1, s1LeftStart: 40, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 119),
                ZoneSegment(zoneId: 2, s1LeftStart: 0, s1LeftEnd: 39, s1RightStart: 120, s1RightEnd: 159)
            ]
            vm.zones.presets = [
                ZonePreset(id: 0, name: "Unified"),
                ZonePreset(id: 1, name: "Dual Split"),
                ZonePreset(id: 2, name: "Triple Rings"),
                ZonePreset(id: 3, name: "Heartbeat Focus")
            ]
            return vm
        }())
}
