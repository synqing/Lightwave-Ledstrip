//
//  EdgeMixerViewModel.swift
//  LightwaveOS
//
//  ViewModel for EdgeMixer post-processor controls.
//  Communicates exclusively via WebSocket (edge_mixer.get/set/save).
//  Follows ZoneViewModel WS pattern with ParametersViewModel echo-prevention.
//

import Foundation
import Observation

@MainActor
@Observable
class EdgeMixerViewModel {
    // MARK: - State

    var config: EdgeMixerConfig = EdgeMixerConfig()
    var ws: WebSocketService?

    // MARK: - Private

    private var pendingParams: Set<String> = []
    private var spreadDebounceTask: Task<Void, Never>?
    private var strengthDebounceTask: Task<Void, Never>?
    private var clearPendingTasks: [String: Task<Void, Never>] = [:]

    // MARK: - Discrete Setters (immediate, no debounce)

    func setMode(_ mode: EdgeMixerMode) {
        config.mode = mode
        Task { [weak self] in
            guard let ws = self?.ws else { return }
            await ws.send("edge_mixer.set", params: ["mode": mode.rawValue])
            await ws.send("edge_mixer.save")
        }
    }

    func setSpatial(_ spatial: EdgeMixerSpatial) {
        config.spatial = spatial
        Task { [weak self] in
            guard let ws = self?.ws else { return }
            await ws.send("edge_mixer.set", params: ["spatial": spatial.rawValue])
            await ws.send("edge_mixer.save")
        }
    }

    func setTemporal(_ temporal: EdgeMixerTemporal) {
        config.temporal = temporal
        Task { [weak self] in
            guard let ws = self?.ws else { return }
            await ws.send("edge_mixer.set", params: ["temporal": temporal.rawValue])
            await ws.send("edge_mixer.save")
        }
    }

    // MARK: - Slider Interaction (debounced 150ms)

    func sliderChanged(_ param: String, value: Int) {
        pendingParams.insert(param)

        switch param {
        case "spread":
            config.spread = value
            spreadDebounceTask?.cancel()
            spreadDebounceTask = Task { [weak self] in
                do {
                    try await Task.sleep(nanoseconds: 150_000_000)
                    guard !Task.isCancelled, let ws = self?.ws else { return }
                    await ws.send("edge_mixer.set", params: ["spread": value])
                    await ws.send("edge_mixer.save")
                } catch is CancellationError {
                } catch {
                    print("Error updating EdgeMixer spread: \(error)")
                }
            }

        case "strength":
            config.strength = value
            strengthDebounceTask?.cancel()
            strengthDebounceTask = Task { [weak self] in
                do {
                    try await Task.sleep(nanoseconds: 150_000_000)
                    guard !Task.isCancelled, let ws = self?.ws else { return }
                    await ws.send("edge_mixer.set", params: ["strength": value])
                    await ws.send("edge_mixer.save")
                } catch is CancellationError {
                } catch {
                    print("Error updating EdgeMixer strength: \(error)")
                }
            }

        default:
            break
        }
    }

    func sliderReleased(_ param: String) {
        clearPendingTasks[param]?.cancel()
        clearPendingTasks[param] = Task { [weak self] in
            try? await Task.sleep(nanoseconds: 1_000_000_000)
            guard !Task.isCancelled else { return }
            self?.pendingParams.remove(param)
            self?.clearPendingTasks.removeValue(forKey: param)
        }
    }

    // MARK: - WS Response Handling

    /// Handle direct response from edge_mixer.get or edge_mixer.set commands.
    func handleResponse(_ data: [String: Any]) {
        let dict = (data["data"] as? [String: Any]) ?? data
        updateConfig(from: dict)
    }

    /// Handle periodic status broadcast (fields prefixed with "edgeMixer").
    func updateFromStatus(_ status: [String: Any]) {
        if let raw = status["edgeMixerMode"] as? Int,
           let m = EdgeMixerMode(rawValue: raw),
           !pendingParams.contains("mode") {
            config.mode = m
        }
        if let v = status["edgeMixerSpread"] as? Int,
           !pendingParams.contains("spread") {
            config.spread = v
        }
        if let v = status["edgeMixerStrength"] as? Int,
           !pendingParams.contains("strength") {
            config.strength = v
        }
        if let raw = status["edgeMixerSpatial"] as? Int,
           let s = EdgeMixerSpatial(rawValue: raw),
           !pendingParams.contains("spatial") {
            config.spatial = s
        }
        if let raw = status["edgeMixerTemporal"] as? Int,
           let t = EdgeMixerTemporal(rawValue: raw),
           !pendingParams.contains("temporal") {
            config.temporal = t
        }
    }

    // MARK: - Load & Reset

    /// Fire edge_mixer.get — response arrives asynchronously via event stream.
    func loadConfig() {
        Task { [weak self] in
            guard let ws = self?.ws else { return }
            await ws.send("edge_mixer.get")
        }
    }

    func reset() {
        spreadDebounceTask?.cancel()
        strengthDebounceTask?.cancel()
        clearPendingTasks.values.forEach { $0.cancel() }
        clearPendingTasks.removeAll()
        pendingParams.removeAll()
        config = EdgeMixerConfig()
        ws = nil
    }

    // MARK: - Private

    private func updateConfig(from dict: [String: Any]) {
        if let raw = dict["mode"] as? Int,
           let m = EdgeMixerMode(rawValue: raw),
           !pendingParams.contains("mode") {
            config.mode = m
        }
        if let v = dict["spread"] as? Int,
           !pendingParams.contains("spread") {
            config.spread = v
        }
        if let v = dict["strength"] as? Int,
           !pendingParams.contains("strength") {
            config.strength = v
        }
        if let raw = dict["spatial"] as? Int,
           let s = EdgeMixerSpatial(rawValue: raw),
           !pendingParams.contains("spatial") {
            config.spatial = s
        }
        if let raw = dict["temporal"] as? Int,
           let t = EdgeMixerTemporal(rawValue: raw),
           !pendingParams.contains("temporal") {
            config.temporal = t
        }
    }
}
