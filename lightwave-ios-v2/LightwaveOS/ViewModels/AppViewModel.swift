//
//  AppViewModel.swift
//  LightwaveOS
//
//  Root ViewModel managing connection lifecycle and child ViewModels.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import Foundation
import Observation

/// App mode enum tracking the active tab
enum AppMode: String, Sendable {
    case play
    case zones
    case audio
    case device
}

@MainActor
@Observable
class AppViewModel {
    // MARK: - Connection State

    enum ConnectionState: Equatable {
        case disconnected
        case discovering
        case connecting
        case connected
        case error(String)
    }

    var connectionState: ConnectionState = .disconnected
    var currentDevice: DeviceInfo?
    var currentMode: AppMode = .play

    // MARK: - LED Stream State

    var ledData: [UInt8] = Array(repeating: 0, count: 960) // 320 LEDs × 3 bytes (RGB)
    var isLEDStreamActive: Bool = false

    // MARK: - Device Discovery State

    var discoveredDevices: [DeviceInfo] = []
    var isDiscoverySearching: Bool = false

    // MARK: - Child ViewModels

    var effects: EffectViewModel
    var palettes: PaletteViewModel
    var parameters: ParametersViewModel
    var zones: ZoneViewModel
    var audio: AudioViewModel
    var transition: TransitionViewModel
    var colourCorrection: ColourCorrectionViewModel

    // MARK: - Network

    private(set) var rest: RESTClient?
    let ws: WebSocketService
    let discovery: DeviceDiscoveryService

    // MARK: - Debug Log

    var debugLog: [DebugEntry] = []
    private let maxLogEntries = 100

    // MARK: - Zone Refresh Throttling

    private var zoneRefreshTask: Task<Void, Never>?
    private var lastZoneRefresh: Date = .distantPast

    // MARK: - Initialization

    init() {
        self.effects = EffectViewModel()
        self.palettes = PaletteViewModel()
        self.parameters = ParametersViewModel()
        self.zones = ZoneViewModel()
        self.audio = AudioViewModel()
        self.transition = TransitionViewModel()
        self.colourCorrection = ColourCorrectionViewModel()
        self.ws = WebSocketService()
        self.discovery = DeviceDiscoveryService()
    }

    // MARK: - Connection Management

    func connect(to device: DeviceInfo) async {
        log("Connecting to \(device.displayName) at \(device.cleanIP)...", category: "CONN")
        connectionState = .connecting
        currentDevice = device

        await connectManual(ip: device.cleanIP, port: device.port)
    }

    func connectManual(ip: String, port: Int = 80) async {
        // Strip interface scope suffix if present
        let cleanIP = ip.components(separatedBy: "%").first ?? ip
        connectionState = .connecting
        log("Manual connection to \(cleanIP):\(port)", category: "CONN")

        // Create REST client
        let client = RESTClient(host: cleanIP, port: port)
        self.rest = client

        do {
            // Test connectivity with device info endpoint
            log("Testing connectivity...", category: "CONN")
            let response = try await client.getDeviceInfo()

            guard response.success else {
                throw NSError(domain: "LightwaveOS", code: -1, userInfo: [NSLocalizedDescriptionKey: "Device returned failure response"])
            }

            log("Device info received: \(response.data.firmware)", category: "CONN")

            // Connect WebSocket
            log("Connecting WebSocket...", category: "WS")
            guard let wsURL = URL(string: "ws://\(cleanIP):\(port)/ws") else {
                throw NSError(domain: "LightwaveOS", code: -2, userInfo: [NSLocalizedDescriptionKey: "Invalid WebSocket URL"])
            }
            let stream = await ws.connect(to: wsURL)
            consumeWebSocketEvents(stream)

            // Subscribe to LED stream so firmware sends binary frames
            await ws.subscribeLEDStream()

            // Subscribe to audio metrics stream
            await ws.subscribeAudioStream()

            // Inject REST client into child ViewModels
            effects.restClient = client
            palettes.restClient = client
            parameters.restClient = client
            zones.restClient = client
            audio.restClient = client
            transition.restClient = client
            colourCorrection.restClient = client

            // Load initial state
            log("Loading effects list...", category: "INIT")
            await effects.loadEffects()

            log("Loading palettes list...", category: "INIT")
            await palettes.loadPalettes()

            log("Loading parameters...", category: "INIT")
            await parameters.loadParameters()

            log("Loading zones...", category: "INIT")
            await zones.loadZones()

            log("Loading colour correction...", category: "INIT")
            await colourCorrection.loadConfig()

            // Connection successful
            connectionState = .connected
            log("Connected successfully", category: "CONN")

        } catch {
            log("Connection failed: \(error.localizedDescription)", category: "ERROR")
            connectionState = .error(error.localizedDescription)
            rest = nil
        }
    }

    func disconnect() async {
        log("Disconnecting...", category: "CONN")
        await ws.disconnect()
        rest = nil
        currentDevice = nil
        connectionState = .disconnected

        // Reset state
        effects.currentEffectId = 0
        effects.currentEffectName = ""
        palettes.currentPaletteId = 0
        audio.reset()
        ledData = Array(repeating: 0, count: 960)
        isLEDStreamActive = false
    }

    // MARK: - Device Discovery

    /// Start device discovery and consume updates.
    /// Updates discoveredDevices and isDiscoverySearching from the actor stream.
    func startDeviceDiscovery() async {
        let stream = await discovery.startDiscovery()

        Task { @MainActor [weak self] in
            for await devices in stream {
                guard let self = self else { break }
                self.discoveredDevices = devices
                self.isDiscoverySearching = await self.discovery.isSearching
            }
        }
    }

    /// Stop device discovery.
    func stopDeviceDiscovery() async {
        await discovery.stopDiscovery()
        discoveredDevices = []
        isDiscoverySearching = false
    }

    // MARK: - WebSocket Event Stream

    private func consumeWebSocketEvents(_ stream: AsyncStream<WebSocketService.Event>) {
        Task { @MainActor [weak self] in
            for await event in stream {
                guard let self = self else { break }
                switch event {
                case .status(let payload):
                    // Update current effect
                    if let effectId = payload.data["currentEffect"] as? Int {
                        self.effects.currentEffectId = effectId
                        if let effect = self.effects.allEffects.first(where: { $0.id == effectId }) {
                            self.effects.currentEffectName = effect.name
                        }
                    }

                    // Update current palette
                    if let paletteId = payload.data["currentPalette"] as? Int {
                        self.palettes.currentPaletteId = paletteId
                    }

                    // Update parameters
                    self.parameters.updateFromStatus(payload.data)

                    // Update audio
                    if let bpm = payload.data["bpm"] as? Double,
                       let confidence = payload.data["bpmConfidence"] as? Double {
                        self.audio.updateFromStatus(bpm: bpm, confidence: confidence)
                    }

                    self.log("Status update received", category: "WS")

                case .beat(let payload):
                    self.audio.handleBeatEvent(payload.data)

                case .audioMetrics(let frame):
                    self.audio.handleMetricsFrame(frame)

                case .zoneUpdate(let payload):
                    self.zones.handleZoneUpdate(payload.data)
                    self.log("Zone update received", category: "WS")
                    self.throttledZoneRefresh()

                case .parameterUpdate(let payload):
                    self.parameters.updateFromStatus(payload.data)
                    self.log("Parameter update received", category: "WS")

                case .ledData(let data):
                    guard data.count == 960 else {
                        self.log("Invalid LED data: \(data.count) bytes", category: "WS")
                        return
                    }
                    self.ledData = [UInt8](data)
                    if !self.isLEDStreamActive {
                        self.isLEDStreamActive = true
                        self.log("LED stream started", category: "WS")
                    }

                case .connected:
                    self.log("WebSocket connected", category: "WS")

                case .disconnected(let error):
                    self.log("WebSocket disconnected: \(error?.localizedDescription ?? "clean")", category: "WS")
                }
            }
        }
    }

    // MARK: - Zone Refresh Throttling

    /// Throttle zone REST refreshes to at most once every 2 seconds.
    /// Applies delta from WS immediately; defers full REST reload.
    private func throttledZoneRefresh() {
        guard Date().timeIntervalSince(lastZoneRefresh) > 2.0 else { return }
        zoneRefreshTask?.cancel()
        zoneRefreshTask = Task { [weak self] in
            guard let self = self else { return }
            try? await Task.sleep(for: .seconds(0.5))
            guard !Task.isCancelled else { return }
            await self.zones.loadZones()
            self.lastZoneRefresh = Date()
        }
    }

    // MARK: - Debug Logging

    func log(_ message: String, category: String = "APP") {
        let entry = DebugEntry(
            timestamp: Date(),
            category: category,
            message: message
        )

        debugLog.append(entry)

        // Trim old entries
        if debugLog.count > maxLogEntries {
            debugLog.removeFirst(debugLog.count - maxLogEntries)
        }

        // Also print to console in debug builds
        #if DEBUG
        print("[\(category)] \(message)")
        #endif
    }

    func clearLog() {
        debugLog.removeAll()
    }
}

// MARK: - Debug Entry

struct DebugEntry: Identifiable, Sendable {
    let id = UUID()
    let timestamp: Date
    let category: String
    let message: String
}
