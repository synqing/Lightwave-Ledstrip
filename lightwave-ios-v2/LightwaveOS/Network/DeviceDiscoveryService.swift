//
//  DeviceDiscoveryService.swift
//  LightwaveOS
//
//  mDNS/Bonjour device discovery actor using Network.framework.
//  Discovers LightwaveOS devices on local network via _http._tcp and _lightwaveos._tcp services.
//  iOS 17+, Swift 6 with strict concurrency.
//

import Foundation
import Network
import NetworkExtension

/// Actor-based device discovery service for LightwaveOS devices.
/// Provides reactive device updates via AsyncStream and eliminates data races
/// present in the previous @unchecked Sendable implementation.
@available(iOS 17.0, *)
actor DeviceDiscoveryService {
    // MARK: - Actor-Isolated State

    private(set) var discoveredDevices: [DeviceInfo] = []
    private(set) var isSearching: Bool = false

    private var browser: NWBrowser?
    private var pendingResolutions: [String: Task<Void, Never>] = [:]
    private var streamContinuation: AsyncStream<[DeviceInfo]>.Continuation?

    // MARK: - Non-Isolated State

    private let browserQueue = DispatchQueue(label: "com.lightwaveos.discovery", qos: .userInitiated)
    private var probeTask: Task<Void, Never>?

    // Known IP for v2 AP gateway (Portable Mode)
    private static let lightwaveAPIP = "192.168.4.1"

    // MARK: - Public Interface

    /// Start discovery and return a stream of device updates.
    /// Each yielded array contains the current complete list of discovered devices.
    ///
    /// - Returns: AsyncStream that yields device lists whenever devices are added/removed.
    func startDiscovery() -> AsyncStream<[DeviceInfo]> {
        // Stop any existing discovery
        if browser != nil {
            stopDiscoveryInternal()
        }

        isSearching = true

        // Use makeStream() to avoid actor isolation issues in AsyncStream closure
        let (stream, continuation) = AsyncStream<[DeviceInfo]>.makeStream()
        self.streamContinuation = continuation

        // Yield initial empty state
        continuation.yield([])

        // Create and configure browser
        let parameters = NWParameters()
        parameters.includePeerToPeer = false

        let newBrowser = NWBrowser(for: .bonjour(type: "_http._tcp", domain: nil), using: parameters)
        self.browser = newBrowser

        // Set up state handler
        newBrowser.stateUpdateHandler = { [weak self] state in
            guard let self = self else { return }
            Task { await self.handleBrowserState(state) }
        }

        // Set up results handler
        newBrowser.browseResultsChangedHandler = { [weak self] results, changes in
            guard let self = self else { return }
            Task { await self.handleBrowseChanges(changes) }
        }

        // Start browsing
        newBrowser.start(queue: browserQueue)

        // Schedule HTTP probe fallback (works without entitlement)
        self.probeTask = Task {
            // First: check if we're on the v2 AP (instant if entitlement available)
            if let apIP = await self.detectLightwaveAP() {
                if let device = await self.probeDevice(ip: apIP) {
                    await self.addDevice(device)
                    return
                }
            }

            // Wait 5 seconds for Bonjour to find something
            try? await Task.sleep(for: .seconds(5))
            guard !Task.isCancelled else { return }

            // If Bonjour found nothing, probe known IPs
            if await self.discoveredDevices.isEmpty {
                if let device = await self.probeDevice(ip: Self.lightwaveAPIP) {
                    await self.addDevice(device)
                }
            }
        }

        // Handle stream termination
        continuation.onTermination = { [weak self] _ in
            guard let self = self else { return }
            Task { await self.stopDiscoveryInternal() }
        }

        return stream
    }

    /// Stop device discovery and clean up resources.
    func stopDiscovery() {
        stopDiscoveryInternal()
        streamContinuation?.finish()
        streamContinuation = nil
    }

    // MARK: - Private Implementation

    private func stopDiscoveryInternal() {
        probeTask?.cancel()
        probeTask = nil
        browser?.cancel()
        browser = nil
        cancelAllResolutions()
        isSearching = false
        discoveredDevices.removeAll()
    }

    // MARK: - Browser State Handling

    private func handleBrowserState(_ state: NWBrowser.State) {
        switch state {
        case .ready:
            isSearching = true

        case .failed(let error):
            print("Device discovery failed: \(error.localizedDescription)")
            isSearching = false

        case .cancelled:
            isSearching = false

        default:
            break
        }
    }

    // MARK: - Browse Results Handling

    private func handleBrowseChanges(_ changes: Set<NWBrowser.Result.Change>) {
        for change in changes {
            switch change {
            case .added(let result):
                handleAddedResult(result)

            case .removed(let result):
                handleRemovedResult(result)

            case .changed(old: _, new: let result, flags: _):
                handleAddedResult(result)

            @unknown default:
                break
            }
        }
    }

    private func handleAddedResult(_ result: NWBrowser.Result) {
        guard Self.isLightwaveDevice(result) else { return }

        let deviceId = Self.generateDeviceId(from: result)

        // Cancel any existing resolution for this device
        pendingResolutions[deviceId]?.cancel()

        // Start new resolution
        let task = Task {
            if let deviceInfo = await resolveEndpoint(result) {
                await addDevice(deviceInfo)
            }
        }

        pendingResolutions[deviceId] = task
    }

    private func handleRemovedResult(_ result: NWBrowser.Result) {
        let deviceId = Self.generateDeviceId(from: result)
        pendingResolutions[deviceId]?.cancel()
        pendingResolutions.removeValue(forKey: deviceId)

        removeDevice(withId: deviceId)
    }

    // MARK: - Device Filtering

    private static func isLightwaveDevice(_ result: NWBrowser.Result) -> Bool {
        switch result.endpoint {
        case .service(let name, _, _, _):
            let lowercasedName = name.lowercased()
            return lowercasedName.contains("lightwaveos") || lowercasedName.contains("lightwave")

        default:
            return false
        }
    }

    // MARK: - Endpoint Resolution

    private func resolveEndpoint(_ result: NWBrowser.Result) async -> DeviceInfo? {
        let connection = NWConnection(to: result.endpoint, using: .tcp)

        // Use a class with atomic check-and-set to prevent double-resume
        final class ResumedFlag: @unchecked Sendable {
            private let lock = NSLock()
            private var _value = false

            /// Atomically check if not resumed and set to resumed.
            /// Returns true if this call successfully claimed the resume, false if already resumed.
            func tryResume() -> Bool {
                lock.lock()
                defer { lock.unlock() }
                guard !_value else { return false }
                _value = true
                return true
            }
        }

        let resumed = ResumedFlag()

        return await withCheckedContinuation { continuation in
            connection.stateUpdateHandler = { state in
                switch state {
                case .ready:
                    guard resumed.tryResume() else { return }
                    let info = Self.extractDeviceInfo(from: connection, result: result)
                    connection.cancel()
                    continuation.resume(returning: info)

                case .failed, .cancelled:
                    guard resumed.tryResume() else { return }
                    continuation.resume(returning: nil)

                default:
                    break
                }
            }

            connection.start(queue: self.browserQueue)

            // Timeout after 5 seconds
            self.browserQueue.asyncAfter(deadline: .now() + 5) {
                guard resumed.tryResume() else { return }
                connection.cancel()
                continuation.resume(returning: nil)
            }
        }
    }

    // MARK: - Device Info Extraction

    /// Extract device information from an active NWConnection.
    /// This is a static method to avoid actor isolation issues in the continuation callback.
    private static func extractDeviceInfo(from connection: NWConnection, result: NWBrowser.Result) -> DeviceInfo? {
        guard case .service(let name, _, _, _) = result.endpoint else {
            return nil
        }

        var ipAddress: String?
        var port: Int = 80
        var hostname = name

        if let path = connection.currentPath {
            if let endpoint = path.remoteEndpoint {
                switch endpoint {
                case .hostPort(let host, let hostPort):
                    port = Int(hostPort.rawValue)

                    switch host {
                    case .ipv4(let address):
                        ipAddress = "\(address)".components(separatedBy: "%").first
                    case .ipv6(let address):
                        ipAddress = "\(address)".components(separatedBy: "%").first
                    case .name(let hostName, _):
                        hostname = hostName
                    @unknown default:
                        break
                    }

                default:
                    break
                }
            }
        }

        guard let ip = ipAddress else {
            return nil
        }

        return DeviceInfo(
            hostname: hostname,
            ipAddress: ip,
            port: port
        )
    }

    // MARK: - Device Management

    private func addDevice(_ device: DeviceInfo) {
        if let index = discoveredDevices.firstIndex(where: { $0.id == device.id }) {
            // Update existing device
            discoveredDevices[index] = device
        } else {
            // Add new device
            discoveredDevices.append(device)
        }

        // Yield updated list
        streamContinuation?.yield(discoveredDevices)
    }

    private func removeDevice(withId id: String) {
        discoveredDevices.removeAll { $0.id == id }

        // Yield updated list
        streamContinuation?.yield(discoveredDevices)
    }

    // MARK: - HTTP Probe

    /// Probe a device at a specific IP via HTTP GET /api/v1/device/info.
    /// Returns DeviceInfo if the device responds within the timeout.
    func probeDevice(ip: String, port: Int = 80) async -> DeviceInfo? {
        guard let url = URL(string: "http://\(ip):\(port)/api/v1/device/info") else {
            return nil
        }

        var request = URLRequest(url: url)
        request.timeoutInterval = 2.0
        request.httpMethod = "GET"

        do {
            let (data, response) = try await URLSession.shared.data(for: request)
            guard let httpResponse = response as? HTTPURLResponse,
                  httpResponse.statusCode == 200 else {
                return nil
            }

            // Parse hostname from response if available
            var hostname = "lightwaveos"
            if let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
               let dataDict = json["data"] as? [String: Any],
               let name = dataDict["hostname"] as? String {
                hostname = name
            }

            return DeviceInfo(hostname: hostname, ipAddress: ip, port: port)
        } catch {
            return nil
        }
    }

    /// Check if currently connected to a LightwaveOS AP via SSID.
    /// Returns the v2 device IP if on the AP, nil otherwise.
    /// Requires "Access WiFi Information" entitlement + location permission.
    /// Falls back gracefully (returns nil) when unavailable.
    private func detectLightwaveAP() async -> String? {
        await withCheckedContinuation { continuation in
            NEHotspotNetwork.fetchCurrent { network in
                if let ssid = network?.ssid,
                   ssid.localizedCaseInsensitiveContains("lightwaveos") {
                    continuation.resume(returning: Self.lightwaveAPIP)
                } else {
                    continuation.resume(returning: nil)
                }
            }
        }
    }

    // MARK: - Utilities

    private static func generateDeviceId(from result: NWBrowser.Result) -> String {
        switch result.endpoint {
        case .service(let name, let type, let domain, _):
            return "\(name).\(type).\(domain ?? "local")"
        default:
            return UUID().uuidString
        }
    }

    private func cancelAllResolutions() {
        pendingResolutions.values.forEach { $0.cancel() }
        pendingResolutions.removeAll()
    }
}
