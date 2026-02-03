//
//  ColourCorrectionViewModel.swift
//  LightwaveOS
//
//  Colour correction configuration management.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import Foundation
import Observation

@MainActor
@Observable
class ColourCorrectionViewModel {
    // MARK: - State

    var config: ColourCorrectionConfig = ColourCorrectionConfig()

    // MARK: - Dependencies

    var restClient: RESTClient?

    // MARK: - Debounce

    private var saveDebounceTask: Task<Void, Never>?

    // MARK: - Computed Properties

    var gammaEnabled: Bool {
        get { config.gammaEnabled }
        set {
            config.gammaEnabled = newValue
            debouncedSave()
        }
    }

    var gammaValue: Double {
        get { config.gammaValue }
        set {
            config.gammaValue = newValue
            debouncedSave()
        }
    }

    var autoExposureEnabled: Bool {
        get { config.autoExposureEnabled }
        set {
            config.autoExposureEnabled = newValue
            debouncedSave()
        }
    }

    var autoExposureTarget: Int {
        get { config.autoExposureTarget }
        set {
            config.autoExposureTarget = newValue
            debouncedSave()
        }
    }

    var brownGuardrailEnabled: Bool {
        get { config.brownGuardrailEnabled }
        set {
            config.brownGuardrailEnabled = newValue
            debouncedSave()
        }
    }

    var mode: CCMode {
        get { config.mode }
        set {
            config.mode = newValue
            debouncedSave()
        }
    }

    // MARK: - API Methods

    func loadConfig() async {
        guard let client = restClient else { return }

        do {
            let response = try await client.getColourCorrection()
            config.gammaEnabled = response.data.gammaEnabled ?? config.gammaEnabled
            config.gammaValue = Double(response.data.gammaValue ?? Float(config.gammaValue))
            config.autoExposureEnabled = response.data.autoExposureEnabled ?? config.autoExposureEnabled
            config.autoExposureTarget = response.data.autoExposureTarget ?? config.autoExposureTarget
            config.brownGuardrailEnabled = response.data.brownGuardrailEnabled ?? config.brownGuardrailEnabled
            if let modeRaw = response.data.mode, let mode = CCMode(rawValue: modeRaw) {
                config.mode = mode
            }
        } catch {
            print("Error loading colour correction: \(error)")
        }
    }

    func saveConfig() async {
        guard let client = restClient else { return }

        do {
            try await client.setColourCorrection(
                gammaEnabled: config.gammaEnabled,
                gammaValue: Float(config.gammaValue),
                autoExposureEnabled: config.autoExposureEnabled,
                autoExposureTarget: config.autoExposureTarget,
                brownGuardrailEnabled: config.brownGuardrailEnabled,
                mode: config.mode.rawValue
            )
        } catch {
            print("Error saving colour correction: \(error)")
        }
    }

    // MARK: - Private Helpers

    private func debouncedSave() {
        saveDebounceTask?.cancel()

        saveDebounceTask = Task {
            do {
                try await Task.sleep(nanoseconds: 150_000_000) // 150ms
                guard !Task.isCancelled else { return }

                await saveConfig()

            } catch is CancellationError {
                // Cancelled by newer update
            } catch {
                print("Error saving colour correction: \(error)")
            }
        }
    }
}
