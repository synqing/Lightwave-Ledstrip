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
    var deviceStatus: DeviceStatusResponse.DeviceStatus?
    var deviceInfo: DeviceInfoResponse.DeviceInfo?
    var wsConnected: Bool = false

    // MARK: - Network

    private(set) var rest: RESTClient?
    let ws: WebSocketService
    let udpReceiver: UDPStreamReceiver
    let discovery: DeviceDiscoveryService

    // MARK: - Debug Log

    var debugLog: [DebugEntry] = []
    private let maxLogEntries = 100

    // MARK: - Zone Refresh Throttling

    private var zoneRefreshTask: Task<Void, Never>?
    private var lastZoneRefresh: Date = .distantPast
    private var statusPollTask: Task<Void, Never>?
    private var streamSubscriptionTask: Task<Void, Never>?
    private var udpFallbackActive = false
    private var udpSubscribeStart: Date?
    private let udpFallbackDelaySeconds: TimeInterval = 3.0
    private var udpHealthTask: Task<Void, Never>?

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
        self.udpReceiver = UDPStreamReceiver()
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

        // Start UDP listener before subscribing (firmware will send to our port)
        udpReceiver.start()

        // Wire UDP frame handlers
        udpReceiver.onLedFrame = { [weak self] bytes in
            guard let self = self else { return }
            // Parse v1 frame: skip 4-byte header, extract RGB for both strips
            guard bytes.count >= 966 else { return }
            // Extract raw RGB data from dual-strip format
            // Strip 0: bytes 5..484 (stripID + 160*3 RGB)
            // Strip 1: bytes 486..965 (stripID + 160*3 RGB)
            var ledData = [UInt8](repeating: 0, count: 960)
            // Strip 0 RGB data starts at offset 5 (header=4, stripID=1)
            for i in 0..<480 {
                ledData[i] = bytes[5 + i]
            }
            // Strip 1 RGB data starts at offset 486 (4 + 1 + 480 + 1)
            for i in 0..<480 {
                ledData[480 + i] = bytes[486 + i]
            }
            self.ledData = ledData
            if !self.isLEDStreamActive {
                self.isLEDStreamActive = true
                self.log("LED stream started (UDP)", category: "UDP")
            }
        }

        udpReceiver.onAudioFrame = { [weak self] data in
            guard let self = self else { return }
            // Parse binary audio metrics frame
            if let frame = AudioMetricsFrame(data: data) {
                self.audio.handleMetricsFrame(frame)
            }
        }

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
            self.deviceInfo = response.data

            // Connect WebSocket
            log("Connecting WebSocket...", category: "WS")
            guard let wsURL = URL(string: "ws://\(cleanIP):\(port)/ws") else {
                throw NSError(domain: "LightwaveOS", code: -2, userInfo: [NSLocalizedDescriptionKey: "Invalid WebSocket URL"])
            }
            let stream = await ws.connect(to: wsURL)
            consumeWebSocketEvents(stream)

            // Inject REST client into child ViewModels
            effects.restClient = client
            palettes.restClient = client
            parameters.restClient = client
            zones.restClient = client
            zones.ws = ws
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

            log("Loading audio tuning...", category: "INIT")
            await audio.loadAudioTuning()

            startDeviceStatusPolling()

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
        statusPollTask?.cancel()
        statusPollTask = nil
        streamSubscriptionTask?.cancel()
        streamSubscriptionTask = nil
        udpReceiver.stop()
        deviceStatus = nil
        deviceInfo = nil
        wsConnected = false

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
                    self.wsConnected = true
                    self.udpFallbackActive = false
                    self.udpSubscribeStart = nil
                    self.startStreamSubscriptions()
                    self.startUdpHealthMonitor()

                case .disconnected(let error):
                    self.log("WebSocket disconnected: \(error?.localizedDescription ?? "clean")", category: "WS")
                    self.wsConnected = false
                    self.streamSubscriptionTask?.cancel()
                    self.streamSubscriptionTask = nil
                    self.udpHealthTask?.cancel()
                    self.udpHealthTask = nil
                    self.isLEDStreamActive = false
                    self.audio.audioFrameCount = 0
                    self.udpReceiver.reset()
                    self.udpFallbackActive = false
                    self.udpSubscribeStart = nil
                }
            }
        }
    }

    // MARK: - UDP Health Monitor

    /// If UDP dies mid-session (common under weak WiFi), fall back to WS streaming without spamming re-subscribes.
    private func startUdpHealthMonitor() {
        udpHealthTask?.cancel()
        udpHealthTask = Task { @MainActor [weak self] in
            guard let self else { return }

            var lastLed = self.udpReceiver.ledFrameCount
            var lastAudio = self.udpReceiver.audioFrameCount
            var lastProgress = Date()

            while !Task.isCancelled && self.wsConnected {
                try? await Task.sleep(for: .seconds(1))
                guard !Task.isCancelled else { break }

                if self.udpFallbackActive {
                    continue
                }

                let ledNow = self.udpReceiver.ledFrameCount
                let audioNow = self.udpReceiver.audioFrameCount

                if ledNow != lastLed || audioNow != lastAudio {
                    lastLed = ledNow
                    lastAudio = audioNow
                    lastProgress = Date()
                    continue
                }

                // No progress for a while -> fallback.
                if Date().timeIntervalSince(lastProgress) >= self.udpFallbackDelaySeconds {
                    self.udpFallbackActive = true
                    await self.ws.subscribeLEDStreamWS()
                    await self.ws.subscribeAudioStreamWS()
                    self.log("UDP stalled mid-session, falling back to WS streaming", category: "UDP")
                }
            }
        }
    }

    // MARK: - Stream Subscription Retry

    private func startStreamSubscriptions() {
        streamSubscriptionTask?.cancel()
        streamSubscriptionTask = Task { @MainActor [weak self] in
            guard let self else { return }
            var attempt = 0
            let maxAttempts = 8
            let udpReady = await self.udpReceiver.waitUntilReady(timeout: 2.0)

            if !udpReady {
                self.log("UDP listener not ready, falling back to WS streaming", category: "UDP")
                self.udpFallbackActive = true
                await self.ws.subscribeLEDStreamWS()
                await self.ws.subscribeAudioStreamWS()
                return
            }

            while !Task.isCancelled && attempt < maxAttempts {
                if self.udpFallbackActive {
                    break
                }

                let udpLedActive = self.udpReceiver.ledFrameCount > 0
                let udpAudioActive = self.udpReceiver.audioFrameCount > 0

                // Always subscribe on first attempt — firmware is idempotent for re-subscribes
                let needsAudio = attempt == 0 || !udpAudioActive
                let needsLED = attempt == 0 || !udpLedActive

                if !needsAudio && !needsLED {
                    break
                }

                attempt += 1

                if needsLED {
                    await self.ws.subscribeLEDStream()
                }
                if needsAudio {
                    await self.ws.subscribeAudioStream()
                }

                if self.udpSubscribeStart == nil {
                    self.udpSubscribeStart = Date()
                }

                self.log("UDP stream subscribe attempt \(attempt)", category: "UDP")

                if let startedAt = self.udpSubscribeStart,
                   Date().timeIntervalSince(startedAt) >= self.udpFallbackDelaySeconds {
                    let ledIdle = self.udpReceiver.ledFrameCount == 0
                    let audioIdle = self.udpReceiver.audioFrameCount == 0
                    if ledIdle || audioIdle {
                        self.udpFallbackActive = true
                        if ledIdle {
                            await self.ws.subscribeLEDStreamWS()
                        }
                        if audioIdle {
                            await self.ws.subscribeAudioStreamWS()
                        }
                        self.log("UDP idle after \(Int(self.udpFallbackDelaySeconds))s, falling back to WS streaming", category: "UDP")
                        break
                    }
                }

                try? await Task.sleep(for: .seconds(1))
            }

            if !Task.isCancelled && !self.udpFallbackActive {
                if self.udpReceiver.audioFrameCount == 0 {
                    self.log("UDP audio stream still idle after subscribe attempts", category: "UDP")
                }
                if self.udpReceiver.ledFrameCount == 0 {
                    self.log("UDP LED stream still idle after subscribe attempts", category: "UDP")
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

    // MARK: - Device Status Polling

    private func startDeviceStatusPolling() {
        statusPollTask?.cancel()
        statusPollTask = Task { [weak self] in
            guard let self else { return }
            while !Task.isCancelled {
                await self.loadDeviceStatus()
                try? await Task.sleep(for: .seconds(2))
            }
        }
    }

    private func loadDeviceStatus() async {
        guard let client = rest else { return }
        do {
            let response = try await client.getDeviceStatus()
            self.deviceStatus = response.data
        } catch {
            // Keep last known status; avoid spamming logs
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
