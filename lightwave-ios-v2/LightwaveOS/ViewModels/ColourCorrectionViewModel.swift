//
//  ColourCorrectionViewModel.swift
//  LightwaveOS
//
//  Colour correction configuration management.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//
//  Network path: WebSocket only.
//
//  REST endpoints `/api/v1/colorCorrection/{config,save}` are stubs that
//  return 404 NOT_IMPLEMENTED on the firmware (see
//  `firmware-v3/src/network/webserver/handlers/ColorCorrectionHandlers.cpp:14-30`).
//  The working surface is `colorCorrection.getConfig` / `setConfig` over WS,
//  delegating to `ColorCorrectionEngine::getInstance()` (see
//  `firmware-v3/src/network/webserver/ws/WsColorCommands.cpp:247-352`).
//  Replies are unicast — the parent `AppViewModel` routes them through
//  `handleConfigUpdate(_:)`.
//

import Foundation
import Observation

@MainActor
@Observable
class ColourCorrectionViewModel {
    // MARK: - State

    var config: ColourCorrectionConfig = ColourCorrectionConfig()

    // MARK: - Dependencies

    /// WebSocket actor injected by `AppViewModel.connectManual`. The viewmodel
    /// uses WS for both reads and writes; REST is dead per the file header.
    var ws: WebSocketService?

    /// Weak parent reference for in-app diagnostic logging. Set by
    /// `AppViewModel.init` so child events surface in DebugLogView.
    weak var appVM: AppViewModel?

    // MARK: - Debounce / pending guard

    /// In-flight save task — cancelled by the next debounced control change.
    private var saveDebounceTask: Task<Void, Never>?

    /// True between issuing a `setConfig` send and receiving the unicast
    /// reply, used to suppress WS-driven state overwrite while a save is in
    /// flight (mirrors the pattern in `ParametersViewModel`).
    private var pendingSave: Bool = false

    // MARK: - Computed Properties (slider/picker bindings)

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

    // MARK: - Network methods (WS only)

    /// Request the firmware's live ColorCorrectionEngine state. Reply arrives
    /// as `.colourCorrectionConfigUpdated` and is routed through
    /// `handleConfigUpdate(_:)` by `AppViewModel.consumeWebSocketEvents`.
    /// Call on connect AND every time the panel opens, since the firmware
    /// does NOT broadcast colour-correction state changes (per
    /// `docs/protocol/k1-ws-contract.yaml:2032-2076` — unicast only).
    func loadConfig() async {
        guard let ws = ws else {
            appVM?.log("loadConfig skipped — ws is nil", category: "CC-ERROR")
            return
        }
        appVM?.log("WS colorCorrection.getConfig — sending", category: "CC")
        await ws.sendColourCorrectionGetConfig()
    }

    /// Push the full config atomically. Firmware applies + replies with the
    /// new state, which arrives via `.colourCorrectionConfigUpdated` and
    /// flushes `pendingSave`.
    func saveConfig() async {
        guard let ws = ws else {
            appVM?.log("saveConfig skipped — ws is nil", category: "CC-ERROR")
            return
        }
        pendingSave = true
        appVM?.log(
            "WS colorCorrection.setConfig — gamma=\(config.gammaEnabled)/\(String(format: "%.2f", config.gammaValue)) ae=\(config.autoExposureEnabled)/target=\(config.autoExposureTarget) brownGuard=\(config.brownGuardrailEnabled) mode=\(config.mode.rawValue)",
            category: "CC"
        )
        await ws.sendColourCorrectionSetConfig(
            gammaEnabled: config.gammaEnabled,
            gammaValue: Float(config.gammaValue),
            autoExposureEnabled: config.autoExposureEnabled,
            autoExposureTarget: config.autoExposureTarget,
            brownGuardrailEnabled: config.brownGuardrailEnabled,
            mode: config.mode.rawValue
        )
    }

    /// Apply a `colorCorrection.{getConfig|setConfig}` unicast reply payload
    /// to local state. Both reply shapes are identical: the full
    /// ColorCorrectionEngine config under either the top-level keys or a
    /// nested `data` object (firmware shape is currently top-level — see
    /// `WsColorCommands.cpp:247-352`).
    ///
    /// Suppresses overwrite while `pendingSave` is true so a slider drag's
    /// optimistic local state is not stomped by a stale earlier reply.
    func handleConfigUpdate(_ payload: [String: Any]) {
        let data = (payload["data"] as? [String: Any]) ?? payload

        if pendingSave {
            // Latest send is the authoritative one; clear the guard so future
            // pushes (e.g. from another client like Tab5) re-apply normally.
            pendingSave = false
            appVM?.log("CC reply received during pendingSave — guard cleared, local state retained", category: "CC")
            return
        }

        if let v = data["gammaEnabled"] as? Bool {
            config.gammaEnabled = v
        }
        if let v = data["gammaValue"] as? Double {
            config.gammaValue = v
        } else if let v = data["gammaValue"] as? Int {
            config.gammaValue = Double(v)
        }
        if let v = data["autoExposureEnabled"] as? Bool {
            config.autoExposureEnabled = v
        }
        if let v = data["autoExposureTarget"] as? Int {
            config.autoExposureTarget = v
        } else if let v = data["autoExposureTarget"] as? Double {
            config.autoExposureTarget = Int(v)
        }
        if let v = data["brownGuardrailEnabled"] as? Bool {
            config.brownGuardrailEnabled = v
        }
        if let v = data["mode"] as? Int, let m = CCMode(rawValue: v) {
            config.mode = m
        } else if let v = data["mode"] as? String, let m = CCMode(rawValue: ccModeRawValue(forName: v) ?? -1) {
            config.mode = m
        }

        appVM?.log(
            "CC state refreshed from WS — gamma=\(config.gammaEnabled)/\(String(format: "%.2f", config.gammaValue)) ae=\(config.autoExposureEnabled) brownGuard=\(config.brownGuardrailEnabled) mode=\(config.mode.rawValue)",
            category: "CC"
        )
    }

    // MARK: - Private Helpers

    private func debouncedSave() {
        saveDebounceTask?.cancel()

        saveDebounceTask = Task { [weak self] in
            do {
                try await Task.sleep(nanoseconds: 150_000_000) // 150ms — project-wide slider floor
                guard !Task.isCancelled else { return }
                await self?.saveConfig()
            } catch is CancellationError {
                // Cancelled by newer update — drop silently.
            } catch {
                self?.appVM?.log("CC debounce error: \(error.localizedDescription)", category: "CC-ERROR")
            }
        }
    }

    /// Translate a firmware mode name string ("OFF"/"GAMMA"/"AUTO_EXPOSURE"/
    /// "BOTH" — see `WsColorCommands.cpp:247-294`) to the iOS `CCMode` raw
    /// value. Used as a defensive fallback when firmware sends a string.
    private func ccModeRawValue(forName name: String) -> Int? {
        switch name.uppercased() {
        case "OFF": return 0
        case "GAMMA": return 1
        case "AUTO_EXPOSURE": return 2
        case "BOTH": return 3
        default: return nil
        }
    }
}
