//
//  ZoneHeaderCard.swift
//  LightwaveOS
//
//  Zone mode toggle, zone count selector, and preset picker.
//

import SwiftUI

struct ZoneHeaderCard: View {
    @Environment(AppViewModel.self) private var appVM

    var body: some View {
        VStack(alignment: .leading, spacing: Spacing.md) {
            // Row 1: "ZONE COMPOSER" + toggle
            HStack {
                Text("ZONE COMPOSER")
                    .font(.sectionHeader)
                    .foregroundStyle(Color.lwTextPrimary)
                    .textCase(.uppercase)
                    .tracking(0.8)

                Spacer()

                Toggle("", isOn: Binding(
                    get: { appVM.zones.zonesEnabled },
                    set: { _ in Task { await appVM.zones.toggleZones() } }
                ))
                .tint(.lwGold)
                .labelsHidden()
            }

            // Row 2: "Zones" label + compact picker (only when enabled)
            if appVM.zones.zonesEnabled {
                HStack {
                    Text("Zones")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextSecondary)

                    Spacer()

                    // Compact 2|3 picker (fixed width ~120pt)
                    HStack(spacing: 0) {
                        ZoneCountButton(
                            count: 2,
                            isSelected: appVM.zones.zoneCount == 2,
                            action: { selectZoneCount(2) }
                        )

                        ZoneCountButton(
                            count: 3,
                            isSelected: appVM.zones.zoneCount == 3,
                            action: { selectZoneCount(3) }
                        )
                    }
                    .frame(width: 120)
                    .padding(2)
                    .background(
                        RoundedRectangle(cornerRadius: CornerRadius.nested)
                            .fill(Color.lwElevated)
                    )
                }
            }
        }
        .padding(16)
        .background(
            RoundedRectangle(cornerRadius: CornerRadius.card)
                .fill(Color.lwCard)
        )
        .ambientShadow()
    }

    // MARK: - Helpers

    /// Load a zone count preset and reload zones after delay
    private func selectZoneCount(_ count: Int) {
        guard count == 2 || count == 3 else { return }
        let presetId = count - 1

        Task {
            await appVM.zones.loadPreset(presetId: presetId, using: appVM.ws)
            try? await Task.sleep(nanoseconds: 800_000_000) // 800ms for firmware to process
            await appVM.zones.loadZones()
        }
    }
}

// MARK: - Zone Count Button

/// Individual zone count selector button (2-3 zones only)
private struct ZoneCountButton: View {
    let count: Int
    let isSelected: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Text("\(count)")
                .font(.bodyValue)
                .fontWeight(isSelected ? .semibold : .medium)
                .foregroundStyle(isSelected ? Color.black : Color.lwTextSecondary)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 10)
                .background(
                    RoundedRectangle(cornerRadius: CornerRadius.nested)
                        .fill(isSelected ? Color.lwGold : Color.lwElevated)
                )
        }
        .buttonStyle(.plain)
    }
}

// MARK: - Preview

#Preview("Zone Header Card - Enabled") {
    let appVM = AppViewModel()
    appVM.zones.zonesEnabled = true
    appVM.zones.zoneCount = 2
    appVM.zones.presets = [
        ZonePreset(id: 0, name: "Unified"),
        ZonePreset(id: 1, name: "Dual Split"),
        ZonePreset(id: 2, name: "Triple Rings")
    ]

    return ZoneHeaderCard()
        .environment(appVM)
        .padding(Spacing.lg)
        .background(Color.lwBase)
}

#Preview("Zone Header Card - Disabled") {
    let appVM = AppViewModel()
    appVM.zones.zonesEnabled = false

    return ZoneHeaderCard()
        .environment(appVM)
        .padding(Spacing.lg)
        .background(Color.lwBase)
}
