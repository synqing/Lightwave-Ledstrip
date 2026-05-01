//
//  PresetsView.swift
//  LightwaveOS
//
//  Phase 2 (P2-3) — Effect / zone preset CRUD surface.
//
//  One view, two modes (`.effect` / `.zone`) selected by `DeviceTab` so the
//  Presets section can compose two NavigationLinks without duplicating
//  scaffolding. Lists fetched from REST on appear; mutations dispatched as
//  WS commands; broadcast confirmations refresh the list automatically.
//

import SwiftUI

struct PresetsView: View {

    // MARK: - Mode

    enum PresetType {
        case effect
        case zone
    }

    let type: PresetType

    // MARK: - Dependencies

    @Environment(AppViewModel.self) private var appVM

    // MARK: - Local state

    @State private var vm = PresetsViewModel()
    @State private var newPresetName: String = ""
    @State private var newPresetSlot: Int = 0

    // MARK: - Body

    var body: some View {
        List {
            saveCurrentSection
            presetListSection
            if let error = vm.error {
                Section {
                    Text(error)
                        .font(.caption)
                        .foregroundStyle(Color.lwError)
                }
                .listRowBackground(Color.lwCard)
            }
        }
        .listStyle(.insetGrouped)
        .scrollContentBackground(.hidden)
        .background(Color.lwBase)
        .navigationTitle(type == .effect ? "Effect Presets" : "Zone Presets")
        .navigationBarTitleDisplayMode(.inline)
        .task {
            await refresh()
        }
        .refreshable {
            await refresh()
        }
    }

    // MARK: - Sections

    @ViewBuilder
    private var saveCurrentSection: some View {
        Section("Save Current") {
            HStack {
                Text("Slot")
                    .foregroundStyle(Color.lwTextSecondary)
                Spacer()
                Stepper("\(newPresetSlot)",
                        value: $newPresetSlot,
                        in: 0...15)
                    .labelsHidden()
                    .foregroundStyle(Color.lwTextPrimary)
                Text("\(newPresetSlot)")
                    .foregroundStyle(Color.lwTextPrimary)
                    .monospacedDigit()
                    .frame(width: 32, alignment: .trailing)
            }

            TextField("Preset name", text: $newPresetName)
                .textFieldStyle(.roundedBorder)
                .foregroundStyle(Color.lwTextPrimary)

            Button {
                Task { await saveCurrent() }
            } label: {
                Label("Save Current", systemImage: "square.and.arrow.down.fill")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            .tint(Color.lwGold)
            .disabled(newPresetName.trimmingCharacters(in: .whitespaces).isEmpty)
        }
        .listRowBackground(Color.lwCard)
    }

    @ViewBuilder
    private var presetListSection: some View {
        Section(type == .effect ? "Effect Presets" : "Zone Presets") {
            if vm.loading && currentList.isEmpty {
                HStack {
                    ProgressView()
                    Text("Loading…")
                        .foregroundStyle(Color.lwTextTertiary)
                }
            } else if currentList.isEmpty {
                Text("No presets saved")
                    .font(.caption)
                    .foregroundStyle(Color.lwTextTertiary)
            } else {
                ForEach(rowIndices, id: \.self) { index in
                    presetRow(at: index)
                }
            }
        }
        .listRowBackground(Color.lwCard)
    }

    // MARK: - Row builder

    @ViewBuilder
    private func presetRow(at index: Int) -> some View {
        switch type {
        case .effect:
            let preset = vm.effectPresets[index]
            HStack {
                VStack(alignment: .leading) {
                    Text(preset.name)
                        .foregroundStyle(Color.lwTextPrimary)
                    Text("Effect \(preset.effectId) · slot \(preset.id)")
                        .font(.caption2)
                        .foregroundStyle(Color.lwTextTertiary)
                }
                Spacer()
                Button {
                    Task { await loadEffectPreset(id: preset.id) }
                } label: {
                    Image(systemName: "play.fill")
                }
                .buttonStyle(.borderless)
                .tint(Color.lwGold)

                Button {
                    Task { await deleteEffectPreset(id: preset.id) }
                } label: {
                    Image(systemName: "trash")
                }
                .buttonStyle(.borderless)
                .tint(Color.lwError)
            }

        case .zone:
            let preset = vm.zonePresets[index]
            let isBuiltIn = preset.builtin == true
            HStack {
                VStack(alignment: .leading) {
                    Text(preset.name)
                        .foregroundStyle(Color.lwTextPrimary)
                    HStack(spacing: 6) {
                        if isBuiltIn {
                            Text("Built-in")
                                .font(.caption2)
                                .foregroundStyle(Color.lwGold)
                        }
                        Text("\(preset.zoneCount ?? 0) zones · slot \(preset.id)")
                            .font(.caption2)
                            .foregroundStyle(Color.lwTextTertiary)
                    }
                }
                Spacer()
                Button {
                    Task { await loadZonePreset(id: preset.id) }
                } label: {
                    Image(systemName: "play.fill")
                }
                .buttonStyle(.borderless)
                .tint(Color.lwGold)

                if !isBuiltIn {
                    Button {
                        Task { await deleteZonePreset(id: preset.id) }
                    } label: {
                        Image(systemName: "trash")
                    }
                    .buttonStyle(.borderless)
                    .tint(Color.lwError)
                }
            }
        }
    }

    // MARK: - Helpers

    private var currentList: [Any] {
        switch type {
        case .effect: return vm.effectPresets
        case .zone:   return vm.zonePresets
        }
    }

    private var rowIndices: Range<Int> {
        switch type {
        case .effect: return 0..<vm.effectPresets.count
        case .zone:   return 0..<vm.zonePresets.count
        }
    }

    // MARK: - Actions

    private func refresh() async {
        guard let rest = appVM.rest else {
            vm.error = "Not connected to device"
            return
        }
        switch type {
        case .effect: await vm.loadEffectPresets(client: rest)
        case .zone:   await vm.loadZonePresets(client: rest)
        }
    }

    private func saveCurrent() async {
        let trimmed = newPresetName.trimmingCharacters(in: .whitespaces)
        guard !trimmed.isEmpty else { return }
        let ws = appVM.ws
        switch type {
        case .effect:
            await vm.saveCurrentEffectPreset(slot: newPresetSlot, name: trimmed, ws: ws)
        case .zone:
            await vm.saveCurrentZonePreset(slot: newPresetSlot, name: trimmed, ws: ws)
        }
        newPresetName = ""
        // Optimistically refetch — the firmware also broadcasts
        // `effectPresets.saved` / `zonePresets.saved`, but Phase 3 wires that
        // path; refetch here keeps the UI live until then.
        await refresh()
    }

    private func loadEffectPreset(id: Int) async {
        await vm.loadEffectPreset(id: id, ws: appVM.ws)
    }

    private func deleteEffectPreset(id: Int) async {
        await vm.deleteEffectPreset(id: id, ws: appVM.ws)
        await refresh()
    }

    private func loadZonePreset(id: Int) async {
        await vm.loadZonePreset(id: id, ws: appVM.ws)
    }

    private func deleteZonePreset(id: Int) async {
        await vm.deleteZonePreset(id: id, ws: appVM.ws)
        await refresh()
    }
}

// MARK: - Preview

#Preview("Presets - Effect") {
    NavigationStack {
        PresetsView(type: .effect)
            .environment(AppViewModel())
    }
}

#Preview("Presets - Zone") {
    NavigationStack {
        PresetsView(type: .zone)
            .environment(AppViewModel())
    }
}
