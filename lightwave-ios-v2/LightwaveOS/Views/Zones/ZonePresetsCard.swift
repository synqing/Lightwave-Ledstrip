//
//  ZonePresetsCard.swift
//  LightwaveOS
//
//  Zone preset management card with horizontal scrolling chips,
//  save current configuration, and quick-load functionality.
//  Positioned directly below zone selector buttons in ZonesTab.
//

import SwiftUI

struct ZonePresetsCard: View {
    @Environment(AppViewModel.self) private var appVM

    @State private var showingSaveSheet = false
    @State private var presetToDelete: ZonePresetSummary?
    @State private var showingDeleteConfirmation = false

    var body: some View {
        VStack(alignment: .leading, spacing: Spacing.md) {
            // Header row: title + save button
            HStack {
                Text("ZONE PRESETS")
                    .font(.sectionHeader)
                    .foregroundStyle(Color.lwTextPrimary)
                    .textCase(.uppercase)
                    .tracking(0.8)

                Spacer()

                // Save current configuration button
                Button {
                    showingSaveSheet = true
                } label: {
                    Image(systemName: "plus.circle.fill")
                        .font(.iconSmall)
                        .foregroundStyle(appVM.connectionState.isReady ? Color.lwGold : Color.lwTextTertiary)
                }
                .disabled(!appVM.connectionState.isReady)
                .accessibilityLabel("Save current zone configuration")
            }

            // Content: loading, empty, or preset chips
            contentView
        }
        .padding(16)
        .background(
            RoundedRectangle(cornerRadius: CornerRadius.card)
                .fill(Color.lwCard)
        )
        .ambientShadow()
        .sheet(isPresented: $showingSaveSheet) {
            ZonePresetSaveSheet { name, slot in
                await appVM.presets.saveCurrentAsZonePreset(name: name, slot: slot)
            }
        }
        .confirmationDialog(
            "Delete Preset",
            isPresented: $showingDeleteConfirmation,
            titleVisibility: .visible,
            presenting: presetToDelete
        ) { preset in
            Button("Delete \"\(preset.name)\"", role: .destructive) {
                Task {
                    await appVM.presets.deleteZonePreset(id: preset.id)
                }
            }
            Button("Cancel", role: .cancel) {
                presetToDelete = nil
            }
        } message: { _ in
            Text("This action cannot be undone.")
        }
        .task {
            // Load zone presets when card appears
            if appVM.presets.zonePresets.isEmpty && appVM.connectionState.isReady {
                await appVM.presets.loadZonePresets()
            }
        }
    }

    // MARK: - Content View

    @ViewBuilder
    private var contentView: some View {
        if let error = appVM.presets.errorMessage {
            errorView(message: error)
        } else if appVM.presets.isLoading {
            loadingView
        } else if appVM.presets.zonePresets.isEmpty {
            emptyView
        } else {
            presetChipsView
        }
    }

    // MARK: - Error View

    private func errorView(message: String) -> some View {
        VStack(alignment: .leading, spacing: Spacing.xs) {
            HStack(spacing: Spacing.xs) {
                Image(systemName: "exclamationmark.triangle.fill")
                    .foregroundStyle(Color.orange)
                Text("Error loading presets")
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextSecondary)
            }

            Text(message)
                .font(.caption)
                .foregroundStyle(Color.lwTextTertiary)

            Button("Retry") {
                Task {
                    appVM.presets.clearError()
                    await appVM.presets.loadZonePresets()
                }
            }
            .font(.caption)
            .foregroundStyle(Color.lwGold)
        }
        .padding(.vertical, Spacing.xs)
    }

    // MARK: - Loading View

    private var loadingView: some View {
        HStack(spacing: Spacing.sm) {
            ProgressView()
                .tint(Color.lwGold)
            Text("Loading presets...")
                .font(.caption)
                .foregroundStyle(Color.lwTextSecondary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(.vertical, Spacing.sm)
    }

    // MARK: - Empty View

    private var emptyView: some View {
        VStack(alignment: .leading, spacing: Spacing.xs) {
            Text("No saved presets")
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextSecondary)

            Text("Tap + to save your current zone layout")
                .font(.caption)
                .foregroundStyle(Color.lwTextTertiary)
        }
        .padding(.vertical, Spacing.xs)
    }

    // MARK: - Preset Chips View

    private var presetChipsView: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: Spacing.sm) {
                ForEach(appVM.presets.zonePresets) { preset in
                    ZonePresetChip(
                        preset: preset,
                        isSelected: appVM.presets.currentZonePresetId == preset.id,
                        onTap: {
                            Task {
                                await appVM.presets.loadZonePreset(id: preset.id)
                                // Reload zones to reflect the new layout
                                try? await Task.sleep(nanoseconds: 500_000_000)
                                await appVM.zones.loadZones()
                            }
                        },
                        onDelete: {
                            presetToDelete = preset
                            showingDeleteConfirmation = true
                        }
                    )
                }
            }
            .padding(.vertical, 2) // Extra space for shadow
        }
    }
}

// MARK: - Zone Preset Chip

private struct ZonePresetChip: View {
    let preset: ZonePresetSummary
    let isSelected: Bool
    let onTap: () -> Void
    let onDelete: () -> Void

    var body: some View {
        Button(action: onTap) {
            HStack(spacing: Spacing.xs) {
                // Zone count badge
                Text("\(preset.zoneCount)")
                    .font(.caption)
                    .fontWeight(.bold)
                    .foregroundStyle(isSelected ? Color.black : Color.lwTextSecondary)
                    .frame(width: 20, height: 20)
                    .background(
                        Circle()
                            .fill(isSelected ? Color.lwGold.opacity(0.3) : Color.lwElevated)
                    )

                // Preset name
                Text(preset.name)
                    .font(.bodyValue)
                    .fontWeight(isSelected ? .semibold : .medium)
                    .foregroundStyle(isSelected ? Color.black : Color.lwTextPrimary)
                    .lineLimit(1)
            }
            .padding(.horizontal, Spacing.sm)
            .padding(.vertical, Spacing.xs)
            .background(
                Capsule()
                    .fill(isSelected ? Color.lwGold : Color.lwElevated)
            )
            .overlay(
                Capsule()
                    .strokeBorder(
                        isSelected ? Color.lwGold.opacity(0.5) : Color.white.opacity(0.06),
                        lineWidth: 1
                    )
            )
        }
        .buttonStyle(.plain)
        .contextMenu {
            Button(role: .destructive, action: onDelete) {
                Label("Delete Preset", systemImage: "trash")
            }
        }
        .accessibilityLabel("\(preset.name), \(preset.zoneCount) zones")
        .accessibilityHint(isSelected ? "Currently loaded" : "Double tap to load this preset")
        .accessibilityAddTraits(isSelected ? [.isButton, .isSelected] : .isButton)
    }
}

// MARK: - Zone Preset Save Sheet

struct ZonePresetSaveSheet: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(AppViewModel.self) private var appVM

    let onSave: (String, Int) async -> Void

    @State private var presetName = ""
    @State private var selectedSlot: Int = 0
    @State private var isSaving = false
    @FocusState private var isNameFieldFocused: Bool

    private var availableSlots: [Int] {
        // Show all slots 0-15, indicating which are occupied
        Array(0..<16)
    }

    var body: some View {
        NavigationStack {
            Form {
                // Name section
                Section {
                    TextField("Preset name", text: $presetName)
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                        .focused($isNameFieldFocused)
                        .autocorrectionDisabled()
                        .textInputAutocapitalization(.words)
                } header: {
                    Text("Name")
                        .font(.caption)
                        .foregroundStyle(Color.lwTextTertiary)
                } footer: {
                    Text("Save current zone layout as a preset")
                        .font(.caption)
                        .foregroundStyle(Color.lwTextTertiary)
                }
                .listRowBackground(Color.lwCard)

                // Slot section
                Section {
                    Picker("Slot", selection: $selectedSlot) {
                        ForEach(availableSlots, id: \.self) { slot in
                            HStack {
                                Text("Slot \(slot)")
                                if appVM.presets.isZoneSlotOccupied(slot) {
                                    Text("(occupied)")
                                        .foregroundStyle(Color.lwTextTertiary)
                                }
                            }
                            .tag(slot)
                        }
                    }
                    .pickerStyle(.menu)
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextPrimary)
                    .tint(Color.lwGold)
                } header: {
                    Text("Storage Slot")
                        .font(.caption)
                        .foregroundStyle(Color.lwTextTertiary)
                } footer: {
                    Text("Saving to an occupied slot will overwrite the existing preset")
                        .font(.caption)
                        .foregroundStyle(Color.lwTextTertiary)
                }
                .listRowBackground(Color.lwCard)

                // Current configuration preview
                Section {
                    HStack {
                        Text("Zone Count")
                            .font(.bodyValue)
                            .foregroundStyle(Color.lwTextSecondary)
                        Spacer()
                        Text("\(appVM.zones.zoneCount)")
                            .font(.bodyValue)
                            .fontWeight(.semibold)
                            .foregroundStyle(Color.lwTextPrimary)
                    }

                    ForEach(0..<appVM.zones.zoneCount, id: \.self) { index in
                        if index < appVM.zones.zones.count {
                            let zone = appVM.zones.zones[index]
                            HStack {
                                Circle()
                                    .fill(Color.zoneColor(index))
                                    .frame(width: 10, height: 10)
                                Text("Zone \(index + 1)")
                                    .font(.caption)
                                    .foregroundStyle(Color.lwTextSecondary)
                                Spacer()
                                Text(zone.effectName ?? "Effect \(zone.effectId ?? 0)")
                                    .font(.caption)
                                    .foregroundStyle(Color.lwTextTertiary)
                                    .lineLimit(1)
                            }
                        }
                    }
                } header: {
                    Text("Current Configuration")
                        .font(.caption)
                        .foregroundStyle(Color.lwTextTertiary)
                }
                .listRowBackground(Color.lwCard)
            }
            .scrollContentBackground(.hidden)
            .background(Color.lwBase)
            .navigationTitle("Save Zone Preset")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") {
                        dismiss()
                    }
                    .foregroundStyle(Color.lwTextSecondary)
                }

                ToolbarItem(placement: .confirmationAction) {
                    Button {
                        Task {
                            await save()
                        }
                    } label: {
                        if isSaving {
                            ProgressView()
                                .tint(Color.lwGold)
                        } else {
                            Text("Save")
                                .foregroundStyle(canSave ? Color.lwGold : Color.lwTextTertiary)
                        }
                    }
                    .disabled(!canSave || isSaving)
                }
            }
            .onAppear {
                isNameFieldFocused = true
                // Default to next available slot
                if let nextSlot = appVM.presets.nextAvailableZoneSlot {
                    selectedSlot = nextSlot
                }
            }
        }
        .presentationDetents([.medium, .large])
        .presentationDragIndicator(.visible)
    }

    private var canSave: Bool {
        !presetName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }

    private func save() async {
        guard canSave else { return }

        isSaving = true
        let trimmedName = presetName.trimmingCharacters(in: .whitespacesAndNewlines)
        await onSave(trimmedName, selectedSlot)
        isSaving = false
        dismiss()
    }
}

// MARK: - Previews

#Preview("Zone Presets Card - With Presets") {
    let appVM = AppViewModel()
    appVM.presets.zonePresets = [
        ZonePresetSummary(id: 0, name: "Dual Split", zoneCount: 2, timestamp: Int(Date().timeIntervalSince1970)),
        ZonePresetSummary(id: 1, name: "Triple Rings", zoneCount: 3, timestamp: Int(Date().timeIntervalSince1970 - 3600)),
        ZonePresetSummary(id: 2, name: "Party Mode", zoneCount: 3, timestamp: nil)
    ]
    appVM.presets.currentZonePresetId = 0

    return ZonePresetsCard()
        .environment(appVM)
        .padding(Spacing.lg)
        .background(Color.lwBase)
}

#Preview("Zone Presets Card - Empty") {
    let appVM = AppViewModel()
    appVM.presets.zonePresets = []

    return ZonePresetsCard()
        .environment(appVM)
        .padding(Spacing.lg)
        .background(Color.lwBase)
}

#Preview("Zone Presets Card - Loading") {
    let appVM = AppViewModel()
    appVM.presets.isLoading = true

    return ZonePresetsCard()
        .environment(appVM)
        .padding(Spacing.lg)
        .background(Color.lwBase)
}

#Preview("Zone Preset Save Sheet") {
    let appVM = AppViewModel()
    appVM.zones.zonesEnabled = true
    appVM.zones.zoneCount = 2
    appVM.zones.zones = [
        ZoneConfig(id: 0, enabled: true, effectId: 5, effectName: "Ripple Enhanced", brightness: 200, speed: 25, paletteId: 0, paletteName: "Copper", blendMode: 1, blendModeName: "Additive"),
        ZoneConfig(id: 1, enabled: true, effectId: 12, effectName: "LGP Holographic", brightness: 200, speed: 18, paletteId: 5, paletteName: "Sunset Real", blendMode: 3, blendModeName: "Screen")
    ]

    return ZonePresetSaveSheet { name, slot in
        print("Save preset: \(name) to slot \(slot)")
    }
    .environment(appVM)
}
