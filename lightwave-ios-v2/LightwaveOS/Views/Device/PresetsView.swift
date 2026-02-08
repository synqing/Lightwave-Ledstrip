//
//  PresetsView.swift
//  LightwaveOS
//
//  Effect and zone preset management.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import SwiftUI

// MARK: - PresetType

enum PresetType: Sendable {
    case effect
    case zone

    var title: String {
        switch self {
        case .effect: return "Effect Presets"
        case .zone: return "Zone Presets"
        }
    }

    var icon: String {
        switch self {
        case .effect: return "sparkles"
        case .zone: return "square.on.square"
        }
    }

    var emptyMessage: String {
        switch self {
        case .effect: return "No effect presets saved"
        case .zone: return "No zone presets saved"
        }
    }

    var savePrompt: String {
        switch self {
        case .effect: return "Save current effect configuration as a preset"
        case .zone: return "Save current zone layout as a preset"
        }
    }
}

// MARK: - PresetsView

struct PresetsView: View {
    @Environment(AppViewModel.self) private var appVM

    let type: PresetType

    @State private var isLoading = true
    @State private var presets: [PresetItem] = []
    @State private var currentPresetId: Int?
    @State private var showingSaveSheet = false
    @State private var presetToDelete: PresetItem?
    @State private var showingDeleteConfirmation = false
    @State private var errorMessage: String?

    var body: some View {
        List {
            if isLoading {
                loadingSection
            } else if presets.isEmpty {
                emptySection
            } else {
                presetsSection
            }
        }
        .listStyle(.insetGrouped)
        .scrollContentBackground(.hidden)
        .background(Color.lwBase)
        .navigationTitle(type.title)
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button {
                    showingSaveSheet = true
                } label: {
                    Image(systemName: "plus")
                        .foregroundStyle(Color.lwGold)
                }
                .disabled(!appVM.connectionState.isReady)
            }
        }
        .sheet(isPresented: $showingSaveSheet) {
            SavePresetSheet(type: type) { name, slot in
                await savePreset(name: name, slot: slot)
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
                    await deletePreset(preset)
                }
            }
            Button("Cancel", role: .cancel) {
                presetToDelete = nil
            }
        } message: { preset in
            Text("This action cannot be undone.")
        }
        .task {
            await loadPresets()
        }
        .refreshable {
            await loadPresets()
        }
        .alert("Error", isPresented: .init(
            get: { errorMessage != nil },
            set: { if !$0 { errorMessage = nil } }
        )) {
            Button("OK", role: .cancel) {
                errorMessage = nil
            }
        } message: {
            if let errorMessage {
                Text(errorMessage)
            }
        }
    }

    // MARK: - Sections

    @ViewBuilder
    private var loadingSection: some View {
        Section {
            HStack {
                Spacer()
                ProgressView()
                    .tint(Color.lwGold)
                Text("Loading presets...")
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextSecondary)
                Spacer()
            }
            .padding(.vertical, Spacing.sm)
        }
        .listRowBackground(Color.lwCard)
    }

    @ViewBuilder
    private var emptySection: some View {
        Section {
            VStack(spacing: Spacing.md) {
                Image(systemName: type.icon)
                    .font(.iconLarge)
                    .foregroundStyle(Color.lwTextTertiary)

                Text(type.emptyMessage)
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextSecondary)

                Text("Tap + to save your first preset")
                    .font(.caption)
                    .foregroundStyle(Color.lwTextTertiary)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, Spacing.lg)
        }
        .listRowBackground(Color.lwCard)
    }

    @ViewBuilder
    private var presetsSection: some View {
        Section {
            ForEach(presets) { preset in
                PresetRow(
                    preset: preset,
                    isCurrentPreset: preset.id == currentPresetId,
                    type: type
                )
                .contentShape(Rectangle())
                .onTapGesture {
                    Task {
                        await loadPreset(preset)
                    }
                }
                .swipeActions(edge: .trailing, allowsFullSwipe: false) {
                    Button(role: .destructive) {
                        presetToDelete = preset
                        showingDeleteConfirmation = true
                    } label: {
                        Label("Delete", systemImage: "trash")
                    }
                }
            }
        } header: {
            Text("\(presets.count) preset\(presets.count == 1 ? "" : "s")")
                .font(.caption)
                .foregroundStyle(Color.lwTextTertiary)
        }
        .listRowBackground(Color.lwCard)
    }

    // MARK: - Actions

    private func loadPresets() async {
        guard let rest = appVM.rest else {
            isLoading = false
            return
        }

        isLoading = true

        do {
            let response = try await rest.getPresets()
            presets = response.data.presets.map { preset in
                PresetItem(
                    id: preset.id,
                    name: preset.name,
                    createdAt: nil  // API doesn't provide date currently
                )
            }
            isLoading = false
        } catch {
            print("Failed to load presets: \(error)")
            isLoading = false
            // Show empty state on error rather than blocking UI
        }
    }

    private func loadPreset(_ preset: PresetItem) async {
        guard let rest = appVM.rest else { return }

        do {
            try await rest.loadPreset(id: preset.id)
            currentPresetId = preset.id
            appVM.log("Loaded preset: \(preset.name)", category: "PRESET")

            // Refresh effect/palette state after loading preset
            await appVM.effects.loadEffects()
            await appVM.palettes.loadPalettes()
            _ = await appVM.parameters.loadParameters()
        } catch {
            print("Failed to load preset: \(error)")
            errorMessage = "Failed to load preset: \(error.localizedDescription)"
        }
    }

    private func savePreset(name: String, slot: Int?) async {
        guard let rest = appVM.rest else { return }

        do {
            try await rest.savePreset(name: name)
            appVM.log("Saved preset: \(name)", category: "PRESET")

            // Reload presets to show the new one
            await loadPresets()
        } catch {
            print("Failed to save preset: \(error)")
            errorMessage = "Failed to save preset: \(error.localizedDescription)"
        }
    }

    private func deletePreset(_ preset: PresetItem) async {
        guard let rest = appVM.rest else { return }

        do {
            try await rest.deletePreset(id: preset.id)
            appVM.log("Deleted preset: \(preset.name)", category: "PRESET")

            // Clear current preset if it was deleted
            if currentPresetId == preset.id {
                currentPresetId = nil
            }

            // Reload presets
            await loadPresets()
        } catch {
            print("Failed to delete preset: \(error)")
            errorMessage = "Failed to delete preset: \(error.localizedDescription)"
        }

        presetToDelete = nil
    }
}

// MARK: - PresetItem

struct PresetItem: Identifiable, Sendable {
    let id: Int
    let name: String
    let createdAt: Date?

    var formattedDate: String? {
        guard let date = createdAt else { return nil }
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        formatter.timeStyle = .short
        return formatter.string(from: date)
    }
}

// MARK: - PresetRow

struct PresetRow: View {
    let preset: PresetItem
    let isCurrentPreset: Bool
    let type: PresetType

    var body: some View {
        HStack(spacing: Spacing.md) {
            // Icon
            Image(systemName: type.icon)
                .font(.iconSmall)
                .foregroundStyle(isCurrentPreset ? Color.lwGold : Color.lwTextSecondary)
                .frame(width: 24)

            // Name and date
            VStack(alignment: .leading, spacing: Spacing.xs) {
                Text(preset.name)
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextPrimary)
                    .lineLimit(1)

                if let date = preset.formattedDate {
                    Text(date)
                        .font(.caption)
                        .foregroundStyle(Color.lwTextTertiary)
                } else {
                    Text("Slot \(preset.id)")
                        .font(.caption)
                        .foregroundStyle(Color.lwTextTertiary)
                }
            }

            Spacer()

            // Checkmark for current preset
            if isCurrentPreset {
                Image(systemName: "checkmark.circle.fill")
                    .font(.iconSmall)
                    .foregroundStyle(Color.lwGold)
            }
        }
        .padding(.vertical, Spacing.xs)
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(preset.name)\(isCurrentPreset ? ", currently loaded" : "")")
        .accessibilityHint("Tap to load this preset")
    }
}

// MARK: - SavePresetSheet

struct SavePresetSheet: View {
    @Environment(\.dismiss) private var dismiss

    let type: PresetType
    let onSave: (String, Int?) async -> Void

    @State private var presetName = ""
    @State private var selectedSlot: Int = 0
    @State private var isSaving = false
    @FocusState private var isNameFieldFocused: Bool

    // Slot options (0-9)
    private let slots = Array(0...9)

    var body: some View {
        NavigationStack {
            Form {
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
                    Text(type.savePrompt)
                        .font(.caption)
                        .foregroundStyle(Color.lwTextTertiary)
                }
                .listRowBackground(Color.lwCard)

                Section {
                    Picker("Slot", selection: $selectedSlot) {
                        ForEach(slots, id: \.self) { slot in
                            Text("Slot \(slot)").tag(slot)
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
            }
            .scrollContentBackground(.hidden)
            .background(Color.lwBase)
            .navigationTitle("Save Preset")
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
                // Auto-focus the name field
                isNameFieldFocused = true
            }
        }
        .presentationDetents([.medium])
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

// MARK: - Preview

#Preview("Presets - Effect (Loading)") {
    NavigationStack {
        PresetsView(type: .effect)
            .environment(AppViewModel())
    }
}

#Preview("Presets - Effect (Empty)") {
    let appVM = AppViewModel()
    return NavigationStack {
        PresetsView(type: .effect)
            .environment(appVM)
    }
}

#Preview("Presets - Zone") {
    NavigationStack {
        PresetsView(type: .zone)
            .environment(AppViewModel())
    }
}

#Preview("Save Preset Sheet") {
    SavePresetSheet(type: .effect) { name, slot in
        print("Save preset: \(name) to slot \(slot ?? -1)")
    }
}

#Preview("Preset Row - Normal") {
    List {
        PresetRow(
            preset: PresetItem(id: 1, name: "Evening Glow", createdAt: Date()),
            isCurrentPreset: false,
            type: .effect
        )
        .listRowBackground(Color.lwCard)
    }
    .listStyle(.insetGrouped)
    .scrollContentBackground(.hidden)
    .background(Color.lwBase)
}

#Preview("Preset Row - Current") {
    List {
        PresetRow(
            preset: PresetItem(id: 2, name: "Party Mode", createdAt: nil),
            isCurrentPreset: true,
            type: .zone
        )
        .listRowBackground(Color.lwCard)
    }
    .listStyle(.insetGrouped)
    .scrollContentBackground(.hidden)
    .background(Color.lwBase)
}
