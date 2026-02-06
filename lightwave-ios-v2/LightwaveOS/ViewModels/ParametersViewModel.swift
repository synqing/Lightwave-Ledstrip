//
//  ParametersViewModel.swift
//  LightwaveOS
//
//  Visual parameters management with debounced updates.
//  Port from v1 — the debounce logic is solid, kept as-is.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import Foundation
import Observation

@MainActor
@Observable
class ParametersViewModel {
    // MARK: - Parameters

    var brightness: Double = 128
    var speed: Double = 15
    var mood: Double = 128
    var fadeAmount: Double = 20
    var hue: Double = 0
    var saturation: Double = 255
    var intensity: Double = 128
    var complexity: Double = 128
    var variation: Double = 0
    var paletteId: Int? = nil

    // MARK: - Dependencies

    var restClient: RESTClient?

    // MARK: - Internal State (Pending flags pattern)

    private var pendingParams: Set<String> = []
    private var debounceTasks: [String: Task<Void, Never>] = [:]
    private var clearPendingTasks: [String: Task<Void, Never>] = [:]

    // MARK: - Disconnect Cleanup

    func disconnect() {
        // Cancel all in-flight debounce and pending-clear tasks
        for task in debounceTasks.values { task.cancel() }
        debounceTasks.removeAll()
        for task in clearPendingTasks.values { task.cancel() }
        clearPendingTasks.removeAll()
        pendingParams.removeAll()
        restClient = nil
    }

    // MARK: - Slider Interaction

    func sliderChanged(_ param: String, value: Double) {
        // Update local value immediately for responsive UI
        setValue(param, to: value)

        // Mark as pending to ignore WS updates
        pendingParams.insert(param)

        // Cancel previous debounce task for THIS parameter only
        debounceTasks[param]?.cancel()

        // Debounce the network update (150ms) per parameter
        debounceTasks[param] = Task {
            do {
                try await Task.sleep(nanoseconds: 150_000_000) // 150ms

                guard !Task.isCancelled else { return }
                guard let client = restClient else { return }

                // Send update
                try await client.setParameters([param: Int(value)])

                debounceTasks.removeValue(forKey: param)

            } catch is CancellationError {
                // Cancelled by newer update, this is expected
            } catch {
                print("Error updating parameter \(param): \(error)")
            }
        }
    }

    func sliderReleased(_ param: String) {
        // Clear pending flag after a short delay to allow final WS update to be ignored
        clearPendingTasks[param]?.cancel()

        clearPendingTasks[param] = Task {
            try? await Task.sleep(nanoseconds: 1_000_000_000) // 1 second
            guard !Task.isCancelled else { return }
            pendingParams.remove(param)
            clearPendingTasks.removeValue(forKey: param)
        }
    }

    // MARK: - WebSocket Updates

    func updateFromStatus(_ status: [String: Any]) {
        // Only update parameters that aren't currently being dragged
        for (key, value) in status {
            guard !pendingParams.contains(key) else { continue }

            if let doubleValue = value as? Double {
                setValue(key, to: doubleValue)
            } else if let intValue = value as? Int {
                setValue(key, to: Double(intValue))
            }
        }
    }

    // MARK: - API Methods

    func loadParameters() async -> Int? {
        guard let client = restClient else { return nil }

        do {
            let response = try await client.getParameters()

            brightness = Double(response.data.brightness)
            speed = Double(response.data.speed)
            mood = Double(response.data.mood ?? 128)
            fadeAmount = Double(response.data.fadeAmount ?? 20)
            hue = Double(response.data.hue ?? 0)
            saturation = Double(response.data.saturation ?? 255)
            intensity = Double(response.data.intensity ?? 128)
            complexity = Double(response.data.complexity ?? 128)
            variation = Double(response.data.variation ?? 0)
            paletteId = response.data.paletteId

            print("Loaded parameters")
            return paletteId

        } catch {
            print("Error loading parameters: \(error)")
            return nil
        }
    }

    // MARK: - Private Helpers

    private func setValue(_ param: String, to value: Double) {
        switch param {
        case "brightness":
            brightness = value
        case "speed":
            speed = value
        case "mood":
            mood = value
        case "fadeAmount":
            fadeAmount = value
        case "hue":
            hue = value
        case "saturation":
            saturation = value
        case "intensity":
            intensity = value
        case "complexity":
            complexity = value
        case "variation":
            variation = value
        default:
            break
        }
    }
}
