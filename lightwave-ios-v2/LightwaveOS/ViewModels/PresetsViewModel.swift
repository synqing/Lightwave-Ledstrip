//
//  PresetsViewModel.swift
//  LightwaveOS
//
//  Effect and scene preset management.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import Foundation
import Observation

@MainActor
@Observable
class PresetsViewModel {
    // MARK: - State

    var presets: [Preset] = []
    var currentPresetId: Int?

    // MARK: - Dependencies

    var restClient: RESTClient?

    // MARK: - API Methods

    func loadPresets() async {
        guard let client = restClient else { return }

        // Load presets via REST (not implemented in RESTClient yet)
        // For now, use empty list
        presets = []
        print("Load presets (not implemented)")
    }

    func loadPreset(id: Int) async {
        guard let client = restClient else { return }

        // Load preset via REST (not implemented in RESTClient yet)
        currentPresetId = id
        print("Load preset \(id) (not implemented)")
    }

    func saveCurrentAsPreset(name: String) async {
        guard let client = restClient else { return }

        // Save current state as preset via REST (not implemented in RESTClient yet)
        print("Save current as preset '\(name)' (not implemented)")

        // Reload presets
        await loadPresets()
    }

    func deletePreset(id: Int) async {
        guard let client = restClient else { return }

        // Delete preset via REST (not implemented in RESTClient yet)
        print("Delete preset \(id) (not implemented)")

        // Reload presets
        await loadPresets()
    }
}

// MARK: - Preset

struct Preset: Identifiable, Sendable {
    let id: Int
    let name: String
    let effectId: Int
    let paletteId: Int
    let parameters: [String: Int]?
    let createdAt: Date?

    var displayName: String {
        name
    }
}
