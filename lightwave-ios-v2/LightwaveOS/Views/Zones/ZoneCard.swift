//
//  ZoneCard.swift
//  LightwaveOS
//
//  Per-zone controls: effect, palette, speed, brightness, split slider.
//  Uses pill title above card and ◀/▶ navigation buttons for effect/palette.
//

import SwiftUI

struct ZoneCard: View {
    let zoneIndex: Int
    @Environment(AppViewModel.self) private var appVM

    // Local state for sliders (sync with zone config)
    @State private var localSpeed: Double = 15
    @State private var localBrightness: Double = 255
    @State private var localSplit: Double = 40 // Split position for inner zones
    @State private var showEffectSelector = false
    @State private var showPaletteSelector = false

    @ViewBuilder
    var body: some View {
        if let zone = zoneForIndex {
            let segment = appVM.zones.segments.first(where: { $0.zoneId == zone.id })
            let effectIndex = appVM.effects.allEffects.firstIndex(where: { $0.id == zone.effectId }) ?? -1
            let paletteIndex = appVM.palettes.allPalettes.firstIndex(where: { $0.id == zone.paletteId }) ?? -1
            let paletteColors = currentPaletteColors(for: zone)
            let isInnerZone = determineIfInnerZone(segment: segment)

            ZStack(alignment: .topLeading) {
                // Main card content
                VStack(alignment: .leading, spacing: Spacing.md) {
                    // LED count metadata
                    if let segment = segment {
                        Text("LEDs \(segment.compactRange) \u{2022} \(segment.totalLeds) LEDs")
                            .font(.monospace)
                            .foregroundStyle(isInnerZone ? Color.lwSuccess : Color.lwTextTertiary)
                    }

                    // Effect row with ◀/▶ navigation
                    EffectNavigationRow(
                        zone: zone,
                        currentIndex: effectIndex,
                        totalCount: appVM.effects.allEffects.count,
                        onPrevious: { previousEffect(for: zone) },
                        onNext: { nextEffect(for: zone) },
                        onSelect: { showEffectSelector = true }
                    )

                    // Palette row with ◀/▶ navigation
                    PaletteNavigationRow(
                        zone: zone,
                        currentIndex: paletteIndex,
                        totalCount: appVM.palettes.allPalettes.count,
                        paletteColors: paletteColors,
                        onPrevious: { previousPalette(for: zone) },
                        onNext: { nextPalette(for: zone) },
                        onSelect: { showPaletteSelector = true }
                    )

                    // Speed slider
                    LWSlider(
                        title: "Speed",
                        value: $localSpeed,
                        range: 1...100,
                        step: 1,
                        trackGradient: LinearGradient(
                            colors: [Color.lwCyan, Color.lwGold],
                            startPoint: .leading,
                            endPoint: .trailing
                        ),
                        onChanged: { newValue in
                            appVM.zones.setZoneSpeed(zoneId: zone.id, speed: Int(newValue))
                        }
                    )

                    // Brightness slider
                    LWSlider(
                        title: "Brightness",
                        value: $localBrightness,
                        range: 0...255,
                        step: 1,
                        onChanged: { newValue in
                            appVM.zones.setZoneBrightness(zoneId: zone.id, brightness: Int(newValue))
                        }
                    )

                    // Zone split slider (controls this zone's boundary)
                    if let boundaryIdx = boundaryIndexForZone(segment: segment) {
                        ZoneSplitSlider(
                            value: $localSplit,
                            zoneColor: Color.zoneColor(zoneIndex),
                            onChanged: { newValue in
                                appVM.zones.setZoneSplit(boundaryIndex: boundaryIdx, value: Int(newValue))
                            }
                        )
                    }
                }
                .padding(.horizontal, 16)
                .padding(.top, 24) // Extra top padding for pill overlap
                .padding(.bottom, 12)
                .background(
                    RoundedRectangle(cornerRadius: CornerRadius.card)
                        .fill(Color.lwCardGradient)
                )
                .ambientShadow()

                // Pill title overlapping top border (left-aligned)
                ZonePillTitle(zoneIndex: zoneIndex, isInner: isInnerZone)
                    .offset(y: -12) // Half of pill height to overlap border
                    .padding(.leading, 16)
            }
            .onAppear {
                // Sync local state with zone config on appear
                localSpeed = Double(zone.speed)
                localBrightness = Double(zone.brightness)
                // Sync split from boundary model
                if let boundaryIdx = boundaryIndexForZone(segment: segment) {
                    localSplit = Double(boundaryIdx == 0 ? appVM.zones.boundary0 : appVM.zones.boundary1)
                }
            }
            .onChange(of: zone.speed) { _, newValue in
                localSpeed = Double(newValue)
            }
            .onChange(of: zone.brightness) { _, newValue in
                localBrightness = Double(newValue)
            }
            .onChange(of: appVM.zones.boundary0) { _, newValue in
                if boundaryIndexForZone(segment: segment) == 0 {
                    localSplit = Double(newValue)
                }
            }
            .onChange(of: appVM.zones.boundary1) { _, newValue in
                if boundaryIndexForZone(segment: segment) == 1 {
                    localSplit = Double(newValue)
                }
            }
            .sheet(isPresented: $showEffectSelector) {
                ZoneEffectSelectorView(zoneId: zone.id)
            }
            .sheet(isPresented: $showPaletteSelector) {
                ZonePaletteSelectorView(zoneId: zone.id)
            }
        }
    }

    // MARK: - Helpers

    private var zoneForIndex: ZoneConfig? {
        guard zoneIndex < appVM.zones.zones.count else { return nil }
        return appVM.zones.zones[zoneIndex]
    }

    /// Determine if this zone is an inner zone (not the outermost zone)
    private func determineIfInnerZone(segment: ZoneSegment?) -> Bool {
        guard let segment = segment else { return false }
        // Inner zones don't start at LED 0 on the left side
        return segment.s1LeftStart > 0
    }

    /// Returns the boundary index this zone's split slider controls, or nil if no slider
    private func boundaryIndexForZone(segment: ZoneSegment?) -> Int? {
        guard let segment = segment else { return nil }

        if appVM.zones.zoneCount == 2 {
            // Only inner zone (zone 0) has a split slider
            return segment.zoneId == 0 ? 0 : nil
        } else if appVM.zones.zoneCount == 3 {
            // Zone 0 controls b0, Zone 1 controls b1, Zone 2 has no slider
            switch segment.zoneId {
            case 0: return 0
            case 1: return 1
            default: return nil
            }
        }
        return nil
    }

    /// Navigate to previous effect
    private func previousEffect(for zone: ZoneConfig) {
        guard !appVM.effects.allEffects.isEmpty else { return }

        let currentIndex = appVM.effects.allEffects.firstIndex(where: { $0.id == zone.effectId }) ?? 0
        let previousIndex = currentIndex > 0 ? currentIndex - 1 : appVM.effects.allEffects.count - 1
        let previousEffect = appVM.effects.allEffects[previousIndex]

        Task {
            await appVM.zones.setZoneEffect(
                zoneId: zone.id,
                effectId: previousEffect.id,
                effectName: previousEffect.name
            )
        }
    }

    /// Navigate to next effect
    private func nextEffect(for zone: ZoneConfig) {
        guard !appVM.effects.allEffects.isEmpty else { return }

        let currentIndex = appVM.effects.allEffects.firstIndex(where: { $0.id == zone.effectId }) ?? 0
        let nextIndex = (currentIndex + 1) % appVM.effects.allEffects.count
        let nextEffect = appVM.effects.allEffects[nextIndex]

        Task {
            await appVM.zones.setZoneEffect(
                zoneId: zone.id,
                effectId: nextEffect.id,
                effectName: nextEffect.name
            )
        }
    }

    /// Navigate to previous palette
    private func previousPalette(for zone: ZoneConfig) {
        guard !appVM.palettes.allPalettes.isEmpty else { return }

        let currentIndex = appVM.palettes.allPalettes.firstIndex(where: { $0.id == zone.paletteId }) ?? 0
        let previousIndex = currentIndex > 0 ? currentIndex - 1 : appVM.palettes.allPalettes.count - 1
        let previousPalette = appVM.palettes.allPalettes[previousIndex]

        Task {
            await appVM.zones.setZonePalette(
                zoneId: zone.id,
                paletteId: previousPalette.id,
                paletteName: previousPalette.name
            )
        }
    }

    /// Navigate to next palette
    private func nextPalette(for zone: ZoneConfig) {
        guard !appVM.palettes.allPalettes.isEmpty else { return }

        let currentIndex = appVM.palettes.allPalettes.firstIndex(where: { $0.id == zone.paletteId }) ?? 0
        let nextIndex = (currentIndex + 1) % appVM.palettes.allPalettes.count
        let nextPalette = appVM.palettes.allPalettes[nextIndex]

        Task {
            await appVM.zones.setZonePalette(
                zoneId: zone.id,
                paletteId: nextPalette.id,
                paletteName: nextPalette.name
            )
        }
    }

    private func currentPaletteColors(for zone: ZoneConfig) -> [Color] {
        guard
            let paletteId = zone.paletteId,
            let palette = appVM.palettes.allPalettes.first(where: { $0.id == paletteId })
        else {
            return [Color.gray, Color.white]
        }
        return palette.gradientColors
    }
}

// MARK: - Supporting Views

/// Pill title for zone card (overlaps top border)
private struct ZonePillTitle: View {
    let zoneIndex: Int
    let isInner: Bool

    var body: some View {
        Text(zoneName)
            .font(.zonePillLabel)
            .tracking(1.5)
            .foregroundStyle(Color.zoneColor(zoneIndex))
            .padding(.horizontal, 16)
            .padding(.vertical, 6)
            .background(
                Capsule()
                    .fill(Color.lwCard)
                    .overlay(
                        Capsule()
                            .strokeBorder(Color.white.opacity(0.25), lineWidth: 2)
                    )
            )
    }

    private var zoneName: String {
        if zoneIndex == 0 {
            return "ZONE 1 — INNER"
        } else if isInner {
            return "ZONE \(zoneIndex + 1) — MIDDLE"
        } else {
            return "ZONE \(zoneIndex + 1) — OUTER"
        }
    }
}

/// Effect navigation row with ◀/▶ buttons
private struct EffectNavigationRow: View {
    let zone: ZoneConfig
    let currentIndex: Int
    let totalCount: Int
    let onPrevious: () -> Void
    let onNext: () -> Void
    let onSelect: () -> Void

    var body: some View {
        HStack(spacing: 0) {
            NavigationButton(direction: .previous, action: onPrevious)

            Button(action: onSelect) {
                HStack(spacing: Spacing.md) {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("EFFECT")
                            .font(.sectionHeader)
                            .foregroundStyle(Color.lwTextTertiary)
                            .textCase(.uppercase)
                            .tracking(0.8)

                        Text(zone.effectName ?? "Select Effect")
                            .font(.bodyValue)
                            .foregroundStyle(Color.lwTextPrimary)
                    }

                    Spacer()

                    HStack(spacing: Spacing.xs) {
                        if totalCount > 0 {
                            Text("\(max(currentIndex, 0) + 1)/\(totalCount)")
                                .font(.caption)
                                .foregroundStyle(Color.lwTextTertiary)
                        }
                        Image(systemName: "chevron.right")
                            .font(.iconSmall)
                            .foregroundStyle(Color.lwGold)
                    }
                }
                .padding(.vertical, 8)
            }
            .buttonStyle(.plain)
            .accessibilityLabel(Text("Select effect for zone \(zone.id + 1)"))
            .accessibilityValue(Text(zone.effectName ?? "Select Effect"))
            .accessibilityHint(Text("Opens the effect selector."))

            NavigationButton(direction: .next, action: onNext)
        }
        .padding(.horizontal, 6)
        .background(
            RoundedRectangle(cornerRadius: CornerRadius.card)
                .fill(Color.lwElevated)
                .overlay(
                    RoundedRectangle(cornerRadius: CornerRadius.card)
                        .strokeBorder(Color.white.opacity(0.06), lineWidth: 1)
                )
        )
    }
}

/// Palette navigation row with ◀/▶ buttons
private struct PaletteNavigationRow: View {
    let zone: ZoneConfig
    let currentIndex: Int
    let totalCount: Int
    let paletteColors: [Color]
    let onPrevious: () -> Void
    let onNext: () -> Void
    let onSelect: () -> Void

    var body: some View {
        HStack(spacing: 0) {
            NavigationButton(direction: .previous, action: onPrevious)

            Button(action: onSelect) {
                HStack(spacing: Spacing.md) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("PALETTE")
                            .font(.sectionHeader)
                            .foregroundStyle(Color.lwTextTertiary)
                            .textCase(.uppercase)
                            .tracking(0.8)

                        LinearGradient(
                            colors: paletteColors,
                            startPoint: .leading,
                            endPoint: .trailing
                        )
                        .frame(height: 10)
                        .clipShape(Capsule())

                        Text(zone.paletteName ?? "Select Palette")
                            .font(.bodyValue)
                            .foregroundStyle(Color.lwTextPrimary)
                    }

                    Spacer()

                    HStack(spacing: Spacing.xs) {
                        if totalCount > 0 {
                            Text("\(max(currentIndex, 0) + 1)/\(totalCount)")
                                .font(.caption)
                                .foregroundStyle(Color.lwTextTertiary)
                        }
                        Image(systemName: "chevron.right")
                            .font(.iconSmall)
                            .foregroundStyle(Color.lwGold)
                    }
                }
                .padding(.vertical, 8)
            }
            .buttonStyle(.plain)
            .accessibilityLabel(Text("Select palette for zone \(zone.id + 1)"))
            .accessibilityValue(Text(zone.paletteName ?? "Select Palette"))
            .accessibilityHint(Text("Opens the palette selector."))

            NavigationButton(direction: .next, action: onNext)
        }
        .padding(.horizontal, 6)
        .background(
            RoundedRectangle(cornerRadius: CornerRadius.card)
                .fill(Color.lwElevated)
                .overlay(
                    RoundedRectangle(cornerRadius: CornerRadius.card)
                        .strokeBorder(Color.white.opacity(0.06), lineWidth: 1)
                )
        )
    }
}

/// Navigation button (◀ or ▶)
private struct NavigationButton: View {
    enum Direction {
        case previous, next

        var symbol: String {
            self == .previous ? "◀" : "▶"
        }
    }

    let direction: Direction
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(direction.symbol)
                .font(.iconNav)
                .foregroundStyle(Color.lwGold)
                .frame(width: 26, height: 26)
                .background(
                    RoundedRectangle(cornerRadius: 6)
                        .fill(Color.lwElevated)
                        .overlay(
                            RoundedRectangle(cornerRadius: 6)
                                .strokeBorder(Color.lwGold.opacity(0.1), lineWidth: 1)
                        )
                )
        }
        .buttonStyle(.plain)
        .accessibilityLabel(Text(direction == .previous ? "Previous" : "Next"))
    }
}

/// Zone split slider (for inner zones only)
private struct ZoneSplitSlider: View {
    @Binding var value: Double
    let zoneColor: Color
    var onChanged: ((Double) -> Void)?

    @State private var isDragging = false

    var body: some View {
        VStack(spacing: Spacing.sm) {
            // Separator line
            Rectangle()
                .fill(Color.white.opacity(0.04))
                .frame(height: 1)
                .padding(.vertical, Spacing.xs)

            // Split label and value
            HStack {
                Text("Split")
                    .font(.cardLabel)
                    .foregroundStyle(Color.lwTextSecondary)
                    .textCase(.uppercase)

                Spacer()

                Text("LED \(Int(value))")
                    .font(.sliderValue)
                    .monospacedDigit()
                    .foregroundStyle(Color.lwGold)
            }

            // Custom slider track
            GeometryReader { geometry in
                ZStack(alignment: .leading) {
                    // Background track
                    RoundedRectangle(cornerRadius: 4)
                        .fill(Color.lwElevated)
                        .frame(height: 8)

                    // Filled portion with zone colour
                    let fillWidth = CGFloat((value - 1) / 77) * geometry.size.width
                    RoundedRectangle(cornerRadius: 4)
                        .fill(zoneColor.opacity(0.3))
                        .frame(width: max(0, fillWidth), height: 8)

                    // Thumb with gold glow
                    Circle()
                        .fill(Color.lwGold)
                        .frame(width: 18, height: 18)
                        .overlay(
                            Circle()
                                .strokeBorder(Color.lwBase, lineWidth: 2)
                        )
                        .shadow(
                            color: isDragging ? Color.lwGold.opacity(0.6) : Color.clear,
                            radius: isDragging ? 8 : 0,
                            x: 0,
                            y: 0
                        )
                        .position(
                            x: fillWidth,
                            y: geometry.size.height / 2
                        )
                        .gesture(
                            DragGesture(minimumDistance: 0)
                                .onChanged { gesture in
                                    if !isDragging {
                                        isDragging = true
                                    }
                                    updateValue(from: gesture.location.x, in: geometry.size.width)
                                    onChanged?(value)
                                }
                                .onEnded { _ in
                                    withAnimation(.easeOut(duration: 0.3)) {
                                        isDragging = false
                                    }
                                }
                        )
                }
            }
            .frame(height: 24)
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(Text("Split"))
        .accessibilityValue(Text("LED \(Int(value))"))
        .accessibilityHint(Text("Swipe up or down to adjust."))
        .accessibilityAdjustableAction { direction in
            switch direction {
            case .increment:
                value = min(value + 1, 78)
            case .decrement:
                value = max(value - 1, 1)
            @unknown default:
                break
            }
            onChanged?(value)
        }
    }

    private func updateValue(from x: CGFloat, in width: CGFloat) {
        let percent = min(max(0, x / width), 1)
        let newValue = 1 + (77 * Double(percent)) // LED range 1-78
        value = min(max(round(newValue), 1), 78)
    }
}

// MARK: - Preview

#Preview("Zone Card - Inner Zone") {
    struct PreviewWrapper: View {
        @State private var appVM = AppViewModel()

        var body: some View {
            ScrollView {
                VStack(spacing: Spacing.lg) {
                    ZoneCard(zoneIndex: 0)
                    ZoneCard(zoneIndex: 1)
                }
                .padding(Spacing.lg)
            }
            .background(Color.lwBase)
            .environment(appVM)
            .onAppear {
                // Mock zone data for 2-zone mode
                appVM.zones.zonesEnabled = true
                appVM.zones.zoneCount = 2
                appVM.zones.zones = [
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
                appVM.zones.segments = [
                    ZoneSegment(zoneId: 0, s1LeftStart: 40, s1LeftEnd: 79, s1RightStart: 80, s1RightEnd: 119),
                    ZoneSegment(zoneId: 1, s1LeftStart: 0, s1LeftEnd: 39, s1RightStart: 120, s1RightEnd: 159)
                ]
                appVM.effects.allEffects = [
                    EffectMetadata(id: 5, name: "Ripple Enhanced", category: "Centre-Origin", isAudioReactive: false),
                    EffectMetadata(id: 12, name: "LGP Holographic", category: "Ambient", isAudioReactive: false)
                ]
                appVM.palettes.allPalettes = [
                    PaletteMetadata(id: 0, name: "Copper", category: "Metallic"),
                    PaletteMetadata(id: 5, name: "Sunset Real", category: "Natural")
                ]
            }
        }
    }

    return PreviewWrapper()
}
