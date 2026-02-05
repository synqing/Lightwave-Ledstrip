//
//  ConnectionManager.swift
//  LightwaveOS
//
//  Single connection state machine actor.
//  Owns all connection logic, retry, network monitoring.
//  iOS 17+, Swift 6 with strict concurrency.
//
//  Created: 2026-02-06
//

import Foundation
import Network
import Observation
import SystemConfiguration.CaptiveNetwork

/// Connection state machine states
enum ConnectionState: Equatable, Sendable {
    case idle
    case targeting
    case handshakeHTTP
    case connectingWS
    case ready
    case backoff(attempt: Int, nextRetry: Date)
    case failed(FailureReason)

    /// Quick check for "working" states
    var isConnecting: Bool {
        switch self {
        case .targeting, .handshakeHTTP, .connectingWS:
            return true
        default:
            return false
        }
    }

    /// Quick check for ready state
    var isReady: Bool {
        if case .ready = self { return true }
        return false
    }
}

/// Delegate protocol for ConnectionManager events
@MainActor
protocol ConnectionManagerDelegate: AnyObject {
    /// Called when connection reaches ready state with established clients
    func connectionDidBecomeReady(rest: RESTClient, ws: WebSocketService)

    /// Called when connection fails or disconnects
    func connectionDidDisconnect()

    /// Called on every state change for observation
    func connectionStateDidChange(_ state: ConnectionState)
}

/// Single connection owner for the app
@MainActor
@Observable
final class ConnectionManager {
    // MARK: - Published State

    private(set) var state: ConnectionState = .idle
    private(set) var target: DeviceTarget?

    // MARK: - Delegate

    weak var delegate: ConnectionManagerDelegate?

    // MARK: - Dependencies (injected)

    private let ws: WebSocketService
    private let discovery: DeviceDiscoveryService

    // MARK: - Internal State

    private var rest: RESTClient?
    /// Network monitor is nonisolated(unsafe) to allow cleanup in deinit.
    /// NWPathMonitor.cancel() is thread-safe.
    nonisolated(unsafe) private var networkMonitor: NWPathMonitor?
    private var lastNetworkStatus: NWPath.Status?
    private var reconnectTask: Task<Void, Never>?
    private var wsEventTask: Task<Void, Never>?
    private var retryAttempt: Int = 0
    private var needsResync: Bool = false
    private var handshakeTask: Task<Void, Error>?

    // MARK: - Configuration

    private let maxRetries = 5
    private let baseBackoff: TimeInterval = 2.0
    private let maxBackoff: TimeInterval = 30.0
    private let httpTimeout: TimeInterval = 10.0
    private let wsTimeout: TimeInterval = 10.0

    // MARK: - Initialization

    init(ws: WebSocketService, discovery: DeviceDiscoveryService) {
        self.ws = ws
        self.discovery = discovery
    }

    deinit {
        // Network monitor cleanup - cancel directly since we can't call MainActor methods
        networkMonitor?.cancel()
    }

    // MARK: - Public API

    /// Connect to a device target
    func connect(target: DeviceTarget) async {
        // Cancel any existing connection attempt
        reconnectTask?.cancel()
        reconnectTask = nil
        wsEventTask?.cancel()
        wsEventTask = nil
        handshakeTask?.cancel()
        handshakeTask = nil

        self.target = target
        self.retryAttempt = 0

        await performConnection()
    }

    /// Disconnect and reset to idle
    func disconnect() async {
        reconnectTask?.cancel()
        reconnectTask = nil
        wsEventTask?.cancel()
        wsEventTask = nil
        handshakeTask?.cancel()
        handshakeTask = nil
        stopNetworkMonitor()

        await ws.gracefulDisconnect()
        rest = nil

        setState(.idle)
        delegate?.connectionDidDisconnect()
    }

    /// Handle app entering background
    func handleBackground() async {
        guard state.isReady else { return }
        needsResync = true
        await ws.gracefulDisconnect()
    }

    /// Handle app returning to foreground
    func handleForeground() async {
        guard needsResync, target != nil else { return }
        needsResync = false
        await performConnection()
    }

    /// Clear cached target and persisted data
    func clearTarget() {
        target = nil
        DeviceTarget.clearPersisted()
    }

    /// Force re-handshake (for diagnostics)
    func forceRehandshake() async {
        guard target != nil else { return }
        await ws.gracefulDisconnect()
        retryAttempt = 0
        await performConnection()
    }

    // MARK: - Connection Flow

    private func performConnection() async {
        guard let currentTarget = target else {
            setState(.failed(.noTarget))
            delegate?.connectionDidDisconnect()
            return
        }

        // 1. We already have an IP (DeviceTarget.lastKnownIP is non-optional)
        setState(.targeting)

        // 2. HTTP Handshake
        setState(.handshakeHTTP)

        do {
            let deviceInfo = try await performHTTPHandshake(target: currentTarget)

            // 3. Verify identity (currently skipped - firmware doesn't return UUID)
            try verifyIdentity(response: deviceInfo, target: currentTarget)

            // 4. Update target with confirmed identity and persist
            var updatedTarget = currentTarget
            updatedTarget.displayName = updatedTarget.displayName ?? "LightwaveOS (\(currentTarget.lastKnownIP))"
            // NOTE: Can't set deviceID without UUID from firmware
            self.target = updatedTarget
            updatedTarget.persist()

            // 5. WebSocket connection
            setState(.connectingWS)

            try await performWSConnection(target: updatedTarget)

            // 6. Success - create REST client and transition to ready
            let restClient = RESTClient(host: updatedTarget.lastKnownIP, port: updatedTarget.port)
            self.rest = restClient

            setState(.ready)
            startNetworkMonitor()

            // Notify delegate on main actor
            delegate?.connectionDidBecomeReady(rest: restClient, ws: ws)

            // Reset retry counter on successful connection
            retryAttempt = 0

        } catch let error as FailureReason {
            await handleConnectionFailure(reason: error)
        } catch {
            // Unexpected error - wrap it
            await handleConnectionFailure(reason: .httpHandshakeFailed(statusCode: nil, message: error.localizedDescription))
        }
    }

    private func handleConnectionFailure(reason: FailureReason) async {
        // Check if cancellation occurred
        if Task.isCancelled {
            print("[CONN] Connection attempt cancelled")
            return
        }

        // Determine if we should retry
        let shouldRetry = reason.isRetriable && retryAttempt < maxRetries

        if shouldRetry {
            scheduleRetry(reason: reason)
        } else {
            if retryAttempt >= maxRetries {
                setState(.failed(.maxRetries(attempts: retryAttempt)))
            } else {
                setState(.failed(reason))
            }
            delegate?.connectionDidDisconnect()
        }
    }

    private func scheduleRetry(reason: FailureReason) {
        retryAttempt += 1

        // Calculate backoff with jitter
        let delay = min(baseBackoff * pow(2.0, Double(retryAttempt - 1)), maxBackoff)
        let jitter = Double.random(in: -0.5...0.5)
        let finalDelay = max(delay + jitter, 1.0) // Minimum 1 second

        let nextRetry = Date().addingTimeInterval(finalDelay)
        setState(.backoff(attempt: retryAttempt, nextRetry: nextRetry))

        print("[CONN] Retry \(retryAttempt)/\(maxRetries) scheduled in \(String(format: "%.1f", finalDelay))s after: \(reason)")

        // Schedule retry task
        reconnectTask = Task { [weak self] in
            try? await Task.sleep(nanoseconds: UInt64(finalDelay * 1_000_000_000))

            guard !Task.isCancelled else { return }

            await self?.performConnection()
        }
    }

    // MARK: - HTTP Handshake

    private func performHTTPHandshake(target: DeviceTarget) async throws -> DeviceInfoResponse {
        // Create temporary REST client for handshake
        let tempRest = RESTClient(host: target.lastKnownIP, port: target.port)

        // Create timeout task
        return try await withThrowingTaskGroup(of: DeviceInfoResponse.self) { group in
            // Add actual request
            group.addTask {
                try await tempRest.getDeviceInfo()
            }

            // Add timeout
            group.addTask {
                try await Task.sleep(nanoseconds: UInt64(self.httpTimeout * 1_000_000_000))
                throw FailureReason.httpHandshakeFailed(statusCode: nil, message: "Handshake timeout after \(self.httpTimeout)s")
            }

            // Return first result, cancel the other
            guard let result = try await group.next() else {
                throw FailureReason.httpHandshakeFailed(statusCode: nil, message: "No response from device")
            }

            group.cancelAll()
            return result
        }
    }

    // MARK: - Identity Verification

    private func verifyIdentity(response: DeviceInfoResponse, target: DeviceTarget) throws {
        // NOTE: Firmware /api/v1/device/info doesn't return UUID
        // Identity verification is not currently possible
        // TODO: Add UUID to firmware endpoint, then enable verification
        // For now, accept any device that responds with valid DeviceInfoResponse
    }

    // MARK: - WebSocket Connection

    private var wsEventStream: AsyncStream<WebSocketService.Event>?

    private func performWSConnection(target: DeviceTarget) async throws {
        guard let wsURL = target.webSocketURL else {
            throw FailureReason.wsHandshakeFailed(message: "Invalid WebSocket URL")
        }

        // Connect WebSocket - this returns the event stream
        let eventStream = await ws.connect(to: wsURL)

        // Wait for connection confirmation with timeout
        try await withThrowingTaskGroup(of: Void.self) { group in
            group.addTask { [weak self] in
                guard let self = self else { return }

                for await event in eventStream {
                    switch event {
                    case .connected:
                        return  // Success
                    case .disconnected(let error):
                        throw FailureReason.wsHandshakeFailed(message: error?.localizedDescription ?? "Disconnected")
                    default:
                        continue
                    }
                }
                throw FailureReason.wsHandshakeFailed(message: "Stream ended unexpectedly")
            }

            // Timeout task
            group.addTask {
                try await Task.sleep(nanoseconds: UInt64(self.wsTimeout * 1_000_000_000))
                throw FailureReason.wsHandshakeFailed(message: "WebSocket timeout")
            }

            try await group.next()
            group.cancelAll()
        }

        // Store the stream and start monitoring after connection
        self.wsEventStream = eventStream
        startWSEventMonitoring(stream: eventStream)
    }

    private func startWSEventMonitoring(stream: AsyncStream<WebSocketService.Event>) {
        wsEventTask = Task { [weak self] in
            for await event in stream {
                guard !Task.isCancelled, let self = self else { return }

                switch event {
                case .disconnected(let error):
                    print("[CONN] WebSocket disconnected: \(error?.localizedDescription ?? "clean")")
                    await self.handleWSDisconnection()
                default:
                    break
                }
            }
        }
    }

    private func handleWSDisconnection() async {
        guard state.isReady else { return }

        print("[CONN] WebSocket lost while ready - attempting reconnect")

        // Transition to backoff and retry
        retryAttempt = 0
        await performConnection()
    }

    // MARK: - Network Monitor

    private func startNetworkMonitor() {
        stopNetworkMonitor() // Clean up any existing monitor

        let monitor = NWPathMonitor(requiredInterfaceType: .wifi)
        self.networkMonitor = monitor

        monitor.pathUpdateHandler = { [weak self] path in
            Task { @MainActor [weak self] in
                guard let self = self else { return }

                let newStatus = path.status
                let oldStatus = self.lastNetworkStatus
                self.lastNetworkStatus = newStatus

                // Detect network state changes
                switch (oldStatus, newStatus) {
                case (.unsatisfied, .satisfied), (nil, .satisfied):
                    // Network became available
                    print("[CONN] Network became available")

                    // If we're in failed or idle state, attempt reconnect
                    if case .failed = self.state, self.target != nil {
                        print("[CONN] Triggering reconnect after network recovery")
                        self.retryAttempt = 0
                        await self.performConnection()
                    }

                case (.satisfied, .unsatisfied):
                    // Network lost
                    print("[CONN] Network lost")

                case (.requiresConnection, .satisfied):
                    // Captive portal resolved
                    print("[CONN] Captive portal resolved")

                default:
                    // No significant change
                    break
                }
            }
        }

        let queue = DispatchQueue(label: "com.lightwave.connectionmanager.network", qos: .utility)
        monitor.start(queue: queue)
    }

    private func stopNetworkMonitor() {
        networkMonitor?.cancel()
        networkMonitor = nil
        lastNetworkStatus = nil
    }

    // MARK: - State Management

    private func setState(_ newState: ConnectionState) {
        let oldState = state
        state = newState

        // Log transition
        logStateTransition(from: oldState, to: newState)

        // Notify delegate
        delegate?.connectionStateDidChange(newState)
    }

    // MARK: - Logging

    private func logStateTransition(from oldState: ConnectionState, to newState: ConnectionState) {
        let timestamp = ISO8601DateFormatter().string(from: Date())
        let oldStateStr = stateDescription(oldState)
        let newStateStr = stateDescription(newState)
        print("[CONN] \(timestamp) \(oldStateStr) -> \(newStateStr)")
    }

    private func stateDescription(_ state: ConnectionState) -> String {
        switch state {
        case .idle:
            return "idle"
        case .targeting:
            return "targeting"
        case .handshakeHTTP:
            return "handshakeHTTP"
        case .connectingWS:
            return "connectingWS"
        case .ready:
            return "ready"
        case .backoff(let attempt, let nextRetry):
            let formatter = DateFormatter()
            formatter.dateFormat = "HH:mm:ss"
            return "backoff(attempt: \(attempt), nextRetry: \(formatter.string(from: nextRetry)))"
        case .failed(let reason):
            return "failed(\(reason))"
        }
    }
}

// MARK: - FailureReason Extensions

extension FailureReason {
    /// Determines if this failure reason should trigger a retry
    var isRetriable: Bool {
        // Delegate to the FailureReason's own shouldRetry property
        return self.shouldRetry
    }
}

