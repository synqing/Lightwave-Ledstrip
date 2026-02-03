//
//  WebSocketService.swift
//  LightwaveOS
//
//  Actor-based WebSocket service for real-time communication with ESP32 device.
//  Handles JSON text messages and binary LED stream data.
//  Auto-reconnect with exponential backoff.
//  iOS 17+, Swift 6 with strict concurrency. British English comments.
//

import Foundation

// MARK: - WebSocket Message Types

enum WebSocketMessageType: String {
    case status
    case beatEvent = "beat.event"
    case zonesList = "zones.list"
    case zonesChanged = "zones.changed"
    case zonesStateChanged = "zones.stateChanged"
    case zonesEffectChanged = "zones.effectChanged"
    case zonesLayoutChanged = "zones.layoutChanged"
    case zoneEnabledChanged = "zone.enabledChanged"
    case parametersChanged = "parameters.changed"
    case effectsChanged = "effects.changed"
    case effectsList = "effects.list"
    case palettesList = "palettes.list"
    case deviceStatus = "device.status"
    case colourCorrectionConfig = "colorCorrection.getConfig"
    case unknown
}

// MARK: - WebSocket Service Actor

@available(iOS 17.0, *)
actor WebSocketService {
    // MARK: - Event Stream Types

    /// Event stream for WebSocket state and messages
    enum Event: Sendable {
        case status(WebSocketPayload)
        case beat(WebSocketPayload)
        case zoneUpdate(WebSocketPayload)
        case parameterUpdate(WebSocketPayload)
        case ledData(Data)
        case audioMetrics(AudioMetricsFrame)
        case connected
        case disconnected(Error?)
    }

    /// Sendable wrapper for [String: Any] JSON payloads
    struct WebSocketPayload: @unchecked Sendable {
        let data: [String: Any]
    }

    // MARK: - Public State

    private(set) var isConnected: Bool = false

    // MARK: - Private State

    private var webSocketTask: URLSessionWebSocketTask?
    private var session: URLSession?
    private var shouldReconnect = true
    private var reconnectAttempts = 0
    private var reconnectTask: Task<Void, Never>?
    private var receiveTask: Task<Void, Never>?
    private var monitorTask: Task<Void, Never>?

    private let initialBackoff: TimeInterval = 3.0
    private let maxBackoff: TimeInterval = 30.0
    private let backoffMultiplier: Double = 2.0

    private var currentURL: URL?
    private var eventContinuation: AsyncStream<Event>.Continuation?

    // MARK: - Initialization

    init() {
        let config = URLSessionConfiguration.default
        config.timeoutIntervalForRequest = 30.0
        self.session = URLSession(configuration: config)
    }

    // MARK: - Public Methods

    /// Connect to the WebSocket server and return an event stream
    func connect(to url: URL) -> AsyncStream<Event> {
        // Guard double-connect: clean up any existing connection first
        if webSocketTask != nil || eventContinuation != nil {
            disconnect()
        }

        currentURL = url
        shouldReconnect = true
        reconnectAttempts = 0

        return AsyncStream<Event> { continuation in
            self.eventContinuation = continuation
            self.performConnect()
        }
    }

    /// Disconnect from the WebSocket server
    func disconnect() {
        shouldReconnect = false
        reconnectTask?.cancel()
        reconnectTask = nil
        receiveTask?.cancel()
        receiveTask = nil
        monitorTask?.cancel()
        monitorTask = nil
        closeConnection()
        eventContinuation?.finish()
        eventContinuation = nil
    }

    /// Send a command with optional parameters
    func send(_ command: String, params: [String: Any] = [:]) {
        guard webSocketTask != nil else {
            print("WS send dropped (not connected): \(command)")
            return
        }
        var message: [String: Any] = ["type": command]
        message.merge(params) { _, new in new }
        sendRaw(message)
    }

    /// Subscribe to LED stream
    func subscribeLEDStream() {
        send("ledStream.subscribe")
    }

    /// Unsubscribe from LED stream
    func unsubscribeLEDStream() {
        send("ledStream.unsubscribe")
    }

    /// Subscribe to audio metrics stream
    func subscribeAudioStream() {
        send("audio.subscribe")
    }

    /// Unsubscribe from audio metrics stream
    func unsubscribeAudioStream() {
        send("audio.unsubscribe")
    }

    /// Trigger a transition effect
    func triggerTransition(type: Int, duration: Int? = nil, toEffect: Int? = nil) {
        var params: [String: Any] = ["type": type]
        if let duration = duration {
            params["duration"] = duration
        }
        if let toEffect = toEffect {
            params["toEffect"] = toEffect
        }
        send("transition.trigger", params: params)
    }

    // MARK: - Private Connection Management

    private func performConnect() {
        guard let url = currentURL else { return }

        closeConnection()

        guard let session = session else { return }

        let task = session.webSocketTask(with: url)
        webSocketTask = task

        task.resume()
        isConnected = true
        reconnectAttempts = 0

        // Yield connected event
        eventContinuation?.yield(.connected)

        // Start receiving messages
        receiveTask = Task {
            await receiveMessages()
        }

        // Start connection monitoring
        monitorTask = Task {
            await monitorConnection()
        }
    }

    private func closeConnection() {
        webSocketTask?.cancel(with: .goingAway, reason: nil)
        webSocketTask = nil

        if isConnected {
            isConnected = false
            eventContinuation?.yield(.disconnected(nil))
        }
    }

    // MARK: - Message Receiving

    private func receiveMessages() async {
        while let task = webSocketTask, task.state == .running {
            do {
                let message = try await task.receive()
                handleMessage(message)
            } catch {
                handleConnectionError(error)
                break
            }
        }
    }

    private func handleMessage(_ message: URLSessionWebSocketTask.Message) {
        switch message {
        case .string(let text):
            handleTextMessage(text)

        case .data(let data):
            handleBinaryMessage(data)

        @unknown default:
            break
        }
    }

    private func handleTextMessage(_ text: String) {
        guard let data = text.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return
        }

        let messageType = (json["type"] as? String).flatMap { WebSocketMessageType(rawValue: $0) } ?? .unknown
        let payload = WebSocketPayload(data: json)

        switch messageType {
        case .status:
            eventContinuation?.yield(.status(payload))

        case .beatEvent:
            eventContinuation?.yield(.beat(payload))

        case .zonesList, .zonesChanged, .zonesStateChanged, .zonesEffectChanged, .zonesLayoutChanged, .zoneEnabledChanged:
            eventContinuation?.yield(.zoneUpdate(payload))

        case .parametersChanged:
            eventContinuation?.yield(.parameterUpdate(payload))

        case .deviceStatus, .effectsChanged, .effectsList, .palettesList, .colourCorrectionConfig:
            // These are handled via REST API, not event stream
            break

        case .unknown:
            break
        }
    }

    private func handleBinaryMessage(_ data: Data) {
        // Audio metrics frame: 464 bytes, magic 0x00445541
        if data.count == AudioMetricsFrame.frameSize {
            let magic = data.withUnsafeBytes { $0.load(as: UInt32.self) }
            if magic == AudioMetricsFrame.magic {
                if let frame = AudioMetricsFrame(data: data) {
                    eventContinuation?.yield(.audioMetrics(frame))
                    return
                }
            }
        }

        // LED stream validation: 961 bytes, magic byte 0xFE
        guard data.count == 961, data.first == 0xFE else {
            return
        }

        // Pass 960 bytes (320 RGB triplets) after magic byte
        eventContinuation?.yield(.ledData(Data(data.dropFirst())))
    }

    // MARK: - Message Sending

    private func sendRaw(_ message: [String: Any]) {
        guard let data = try? JSONSerialization.data(withJSONObject: message),
              let text = String(data: data, encoding: .utf8) else {
            return
        }

        Task {
            await sendText(text)
        }
    }

    private func sendText(_ text: String) async {
        do {
            try await webSocketTask?.send(.string(text))
        } catch {
            // Connection error will be handled by receive loop or monitor
        }
    }

    // MARK: - Connection Monitoring & Reconnection

    private func monitorConnection() async {
        // Periodically ping to detect disconnection
        while shouldReconnect {
            try? await Task.sleep(nanoseconds: 10_000_000_000) // 10 seconds

            guard let task = webSocketTask, task.state == .running else {
                // Task is not running — trigger reconnect
                if shouldReconnect {
                    isConnected = false
                    scheduleReconnect()
                }
                return
            }

            task.sendPing { [weak self] error in
                if let error = error {
                    Task {
                        await self?.handleConnectionError(error)
                    }
                }
            }
        }
    }

    private func handleConnectionError(_ error: Error) {
        isConnected = false
        eventContinuation?.yield(.disconnected(error))

        if shouldReconnect {
            scheduleReconnect()
        }
    }

    private func scheduleReconnect() {
        reconnectTask?.cancel()

        let backoff = min(
            initialBackoff * pow(backoffMultiplier, Double(reconnectAttempts)),
            maxBackoff
        )

        reconnectAttempts += 1

        reconnectTask = Task {
            try? await Task.sleep(for: .seconds(backoff))

            guard !Task.isCancelled, shouldReconnect else { return }

            performConnect()
        }
    }
}
