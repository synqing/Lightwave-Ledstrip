//
//  PresetsViewModel.swift
//  LightwaveOS
//
//  Effect and zone preset management with REST API integration.
//  Supports save, load, delete operations for both preset types.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import Foundation
import Observation

// MARK: - PresetsViewModel

@MainActor
@Observable
class PresetsViewModel {
    // MARK: - Constants

    static let maxEffectPresets = 16
    static let maxZonePresets = 16

    // MARK: - State

    /// Loaded effect presets from device (summary data from list endpoint)
    var effectPresets: [EffectPresetSummary] = []

    /// Loaded zone presets from device
    var zonePresets: [ZonePresetSummary] = []

    /// Currently loaded effect preset ID (if any)
    var currentEffectPresetId: Int?

    /// Currently loaded zone preset ID (if any)
    var currentZonePresetId: Int?

    /// Loading indicator for async operations
    var isLoading: Bool = false

    /// Error message for display (cleared on next operation)
    var errorMessage: String?

    /// Success message for display (auto-dismissed after 2 seconds)
    var successMessage: String?

    // MARK: - Dependencies

    var restClient: RESTClient?

    // MARK: - Computed Properties

    /// Returns the next available effect preset slot (0-15), or nil if all slots are full
    var nextAvailableEffectSlot: Int? {
        let usedSlots = Set(effectPresets.map { $0.id })
        return (0..<Self.maxEffectPresets).first { !usedSlots.contains($0) }
    }

    /// Returns the next available zone preset slot (0-15), or nil if all slots are full
    var nextAvailableZoneSlot: Int? {
        let usedSlots = Set(zonePresets.map { $0.id })
        return (0..<Self.maxZonePresets).first { !usedSlots.contains($0) }
    }

    /// All occupied effect preset slots
    var occupiedEffectSlots: Set<Int> {
        Set(effectPresets.map { $0.id })
    }

    /// All occupied zone preset slots
    var occupiedZoneSlots: Set<Int> {
        Set(zonePresets.map { $0.id })
    }

    // MARK: - Effect Preset API Methods

    /// Load all effect presets from device
    func loadEffectPresets() async {
        guard let client = restClient else {
            errorMessage = "Not connected to device"
            return
        }

        isLoading = true
        errorMessage = nil
        defer { isLoading = false }

        do {
            let response = try await client.getEffectPresets()
            effectPresets = response.data.presets
            print("Loaded \(effectPresets.count) effect presets")
        } catch {
            errorMessage = "Failed to load effect presets: \(error.localizedDescription)"
            print("Effect presets load failed: \(error)")
        }
    }

    /// Load all zone presets from device
    func loadZonePresets() async {
        guard let client = restClient else {
            errorMessage = "Not connected to device"
            print("[PRESETS] loadZonePresets: No REST client")
            return
        }

        isLoading = true
        errorMessage = nil
        defer { isLoading = false }

        print("[PRESETS] loadZonePresets: Fetching from device...")

        do {
            let response = try await client.getZonePresets()
            zonePresets = response.data.presets
            print("[PRESETS] loadZonePresets: Loaded \(zonePresets.count) presets")
            for preset in zonePresets {
                print("[PRESETS]   - ID \(preset.id): \(preset.name) (\(preset.zoneCount) zones)")
            }
        } catch {
            errorMessage = "Failed to load zone presets: \(error.localizedDescription)"
            print("[PRESETS] loadZonePresets FAILED: \(error)")
        }
    }

    /// Load a specific effect preset by ID, applying it to the device
    func loadPreset(id: Int) async {
        guard let client = restClient else {
            errorMessage = "Not connected to device"
            return
        }

        isLoading = true
        errorMessage = nil
        defer { isLoading = false }

        do {
            try await client.loadEffectPreset(id: id)
            currentEffectPresetId = id

            if let preset = effectPresets.first(where: { $0.id == id }) {
                await showSuccess("Loaded '\(preset.name)'")
            } else {
                await showSuccess("Preset loaded")
            }
            print("Loaded effect preset \(id)")
        } catch {
            errorMessage = "Failed to load preset: \(error.localizedDescription)"
            print("Load effect preset failed: \(error)")
        }
    }

    /// Load a specific zone preset by ID, applying it to the device
    func loadZonePreset(id: Int) async {
        guard let client = restClient else {
            errorMessage = "Not connected to device"
            return
        }

        isLoading = true
        errorMessage = nil
        defer { isLoading = false }

        do {
            try await client.loadZonePresetById(id: id)
            currentZonePresetId = id

            if let preset = zonePresets.first(where: { $0.id == id }) {
                await showSuccess("Loaded '\(preset.name)'")
            } else {
                await showSuccess("Zone preset loaded")
            }
            print("Loaded zone preset \(id)")
        } catch {
            errorMessage = "Failed to load zone preset: \(error.localizedDescription)"
            print("Load zone preset failed: \(error)")
        }
    }

    /// Save current effect configuration as a new preset
    /// - Parameters:
    ///   - name: User-friendly name for the preset
    ///   - slot: Slot ID (0-15) to save to; overwrites existing preset in slot
    func saveCurrentAsEffectPreset(name: String, slot: Int) async {
        guard let client = restClient else {
            errorMessage = "Not connected to device"
            return
        }

        guard slot >= 0 && slot < Self.maxEffectPresets else {
            errorMessage = "Invalid slot: must be 0-\(Self.maxEffectPresets - 1)"
            return
        }

        guard !name.trimmingCharacters(in: .whitespaces).isEmpty else {
            errorMessage = "Preset name cannot be empty"
            return
        }

        isLoading = true
        errorMessage = nil
        defer { isLoading = false }

        do {
            try await client.saveEffectPreset(slot: slot, name: name)
            await showSuccess("Saved '\(name)'")
            print("Saved effect preset '\(name)' to slot \(slot)")

            // Reload presets to reflect the new state
            await loadEffectPresets()
        } catch {
            errorMessage = "Failed to save preset: \(error.localizedDescription)"
            print("Save effect preset failed: \(error)")
        }
    }

    /// Save current zone configuration as a new preset
    /// - Parameters:
    ///   - name: User-friendly name for the preset
    ///   - slot: Slot ID (0-15) to save to; Note: firmware ignores slot and auto-assigns ID 10+
    func saveCurrentAsZonePreset(name: String, slot: Int) async {
        guard let client = restClient else {
            errorMessage = "Not connected to device"
            print("[PRESETS] saveZonePreset: No REST client")
            return
        }

        guard slot >= 0 && slot < Self.maxZonePresets else {
            errorMessage = "Invalid slot: must be 0-\(Self.maxZonePresets - 1)"
            print("[PRESETS] saveZonePreset: Invalid slot \(slot)")
            return
        }

        guard !name.trimmingCharacters(in: .whitespaces).isEmpty else {
            errorMessage = "Preset name cannot be empty"
            print("[PRESETS] saveZonePreset: Empty name")
            return
        }

        isLoading = true
        errorMessage = nil
        defer { isLoading = false }

        print("[PRESETS] saveZonePreset: Saving '\(name)' (slot hint: \(slot))...")

        do {
            try await client.saveZonePreset(slot: slot, name: name)
            await showSuccess("Saved '\(name)'")
            print("[PRESETS] saveZonePreset: Save succeeded, reloading presets...")

            // Reload presets to reflect the new state
            await loadZonePresets()
        } catch {
            errorMessage = "Failed to save: \(error.localizedDescription)"
            print("[PRESETS] saveZonePreset FAILED: \(error)")
        }
    }

    /// Delete an effect preset by ID
    func deleteEffectPreset(id: Int) async {
        guard let client = restClient else {
            errorMessage = "Not connected to device"
            return
        }

        isLoading = true
        errorMessage = nil
        defer { isLoading = false }

        let presetName = effectPresets.first(where: { $0.id == id })?.name

        do {
            try await client.deleteEffectPreset(id: id)

            // Clear current preset ID if we deleted the active one
            if currentEffectPresetId == id {
                currentEffectPresetId = nil
            }

            if let name = presetName {
                await showSuccess("Deleted '\(name)'")
            } else {
                await showSuccess("Preset deleted")
            }
            print("Deleted effect preset \(id)")

            // Reload presets to reflect the deletion
            await loadEffectPresets()
        } catch {
            errorMessage = "Failed to delete preset: \(error.localizedDescription)"
            print("Delete effect preset failed: \(error)")
        }
    }

    /// Delete a zone preset by ID
    func deleteZonePreset(id: Int) async {
        guard let client = restClient else {
            errorMessage = "Not connected to device"
            return
        }

        isLoading = true
        errorMessage = nil
        defer { isLoading = false }

        let presetName = zonePresets.first(where: { $0.id == id })?.name

        do {
            try await client.deleteZonePreset(id: id)

            // Clear current preset ID if we deleted the active one
            if currentZonePresetId == id {
                currentZonePresetId = nil
            }

            if let name = presetName {
                await showSuccess("Deleted '\(name)'")
            } else {
                await showSuccess("Zone preset deleted")
            }
            print("Deleted zone preset \(id)")

            // Reload presets to reflect the deletion
            await loadZonePresets()
        } catch {
            errorMessage = "Failed to delete zone preset: \(error.localizedDescription)"
            print("Delete zone preset failed: \(error)")
        }
    }

    // MARK: - Convenience Methods

    /// Load all presets (both effect and zone)
    func loadAllPresets() async {
        await withTaskGroup(of: Void.self) { group in
            group.addTask { await self.loadEffectPresets() }
            group.addTask { await self.loadZonePresets() }
        }
    }

    /// Get effect preset by ID
    func effectPreset(id: Int) -> EffectPresetSummary? {
        effectPresets.first(where: { $0.id == id })
    }

    /// Get zone preset by ID
    func zonePreset(id: Int) -> ZonePresetSummary? {
        zonePresets.first(where: { $0.id == id })
    }

    /// Check if an effect preset slot is occupied
    func isEffectSlotOccupied(_ slot: Int) -> Bool {
        effectPresets.contains { $0.id == slot }
    }

    /// Check if a zone preset slot is occupied
    func isZoneSlotOccupied(_ slot: Int) -> Bool {
        zonePresets.contains { $0.id == slot }
    }

    /// Clear error message
    func clearError() {
        errorMessage = nil
    }

    /// Clear success message
    func clearSuccess() {
        successMessage = nil
    }

    // MARK: - Private Helpers

    /// Show success message with auto-dismiss after 2 seconds
    private func showSuccess(_ message: String) async {
        successMessage = message

        // Auto-dismiss after 2 seconds
        Task { @MainActor in
            try? await Task.sleep(for: .seconds(2))
            // Only clear if message hasn't changed
            if self.successMessage == message {
                self.successMessage = nil
            }
        }
    }
}

// MARK: - Preview Support

#if DEBUG
extension PresetsViewModel {
    /// Preview instance with sample data
    static var preview: PresetsViewModel {
        let vm = PresetsViewModel()
        vm.effectPresets = [
            EffectPresetSummary(
                id: 0,
                name: "Sunset Glow",
                effectId: 5,
                paletteId: 12,
                timestamp: Int(Date().timeIntervalSince1970)
            ),
            EffectPresetSummary(
                id: 1,
                name: "Ocean Waves",
                effectId: 1,
                paletteId: 3,
                timestamp: Int(Date().timeIntervalSince1970 - 3600)
            ),
            EffectPresetSummary(
                id: 2,
                name: "Night Club",
                effectId: 8,
                paletteId: 7,
                timestamp: Int(Date().timeIntervalSince1970 - 86400)
            )
        ]
        vm.zonePresets = [
            ZonePresetSummary(
                id: 0,
                name: "Dual Split",
                zoneCount: 2,
                timestamp: Int(Date().timeIntervalSince1970)
            ),
            ZonePresetSummary(
                id: 1,
                name: "Triple Rings",
                zoneCount: 3,
                timestamp: Int(Date().timeIntervalSince1970 - 7200)
            )
        ]
        vm.currentEffectPresetId = 0
        return vm
    }

    /// Empty preview instance
    static var emptyPreview: PresetsViewModel {
        PresetsViewModel()
    }
}
#endif
