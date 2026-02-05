//
//  DeviceTarget.swift
//  LightwaveOS
//
//  Canonical target model for device connection.
//  Tracks stable identity + last-known network coordinates.
//  iOS 17+, Swift 6 with strict concurrency.
//

import Foundation

/// Represents a target device for connection.
/// Persisted to UserDefaults for fast reconnection on app launch.
struct DeviceTarget: Codable, Equatable, Sendable {
    /// Stable device identifier (UUID from firmware /api/v1/device/info)
    var deviceID: String

    /// Last known IP address (IPv4 or IPv6, without interface scope suffix)
    var lastKnownIP: String

    /// HTTP/WebSocket port (default 80)
    var port: Int

    /// Mode hint from last connection (AP or STA)
    var modeHint: Mode?

    /// Friendly name for display (hostname or user-assigned)
    var displayName: String?

    enum Mode: String, Codable, Sendable {
        case ap = "AP"
        case sta = "STA"
    }

    // MARK: - Computed Properties

    /// WebSocket URL for this target
    var webSocketURL: URL? {
        URL(string: "ws://\(lastKnownIP):\(port)/ws")
    }

    /// REST base URL for this target
    var restBaseURL: URL? {
        URL(string: "http://\(lastKnownIP):\(port)/api/v1/")
    }

    /// Device info endpoint URL for identity verification
    var deviceInfoURL: URL? {
        URL(string: "http://\(lastKnownIP):\(port)/api/v1/device/info")
    }

    // MARK: - Persistence Keys

    private static let persistenceKey = "lightwaveos.deviceTarget"

    // MARK: - Persistence

    /// Save target to UserDefaults
    func persist() {
        if let data = try? JSONEncoder().encode(self) {
            UserDefaults.standard.set(data, forKey: Self.persistenceKey)
        }
    }

    /// Load persisted target from UserDefaults
    static func loadPersisted() -> DeviceTarget? {
        guard let data = UserDefaults.standard.data(forKey: persistenceKey),
              let target = try? JSONDecoder().decode(DeviceTarget.self, from: data) else {
            return nil
        }
        return target
    }

    /// Clear persisted target
    static func clearPersisted() {
        UserDefaults.standard.removeObject(forKey: persistenceKey)
    }

    // MARK: - Factory Methods

    /// Create target from manual IP entry (no device ID yet - will be verified on connect)
    static func manual(ip: String, port: Int = 80) -> DeviceTarget {
        DeviceTarget(
            deviceID: "", // Will be populated after identity verification
            lastKnownIP: ip.components(separatedBy: "%").first ?? ip, // Strip interface scope
            port: port,
            modeHint: ip == "192.168.4.1" ? .ap : nil,
            displayName: nil
        )
    }

    /// Create target for AP mode quick connect
    static var apMode: DeviceTarget {
        DeviceTarget(
            deviceID: "",
            lastKnownIP: "192.168.4.1",
            port: 80,
            modeHint: .ap,
            displayName: "LightwaveOS-AP"
        )
    }

    /// Create target from discovered DeviceInfo
    static func from(_ deviceInfo: DeviceInfo) -> DeviceTarget {
        DeviceTarget(
            deviceID: "", // Will be populated after identity verification
            lastKnownIP: deviceInfo.cleanIP,
            port: deviceInfo.port,
            modeHint: deviceInfo.cleanIP == "192.168.4.1" ? .ap : .sta,
            displayName: deviceInfo.displayName
        )
    }

    // MARK: - Mutation

    /// Update target with verified identity from device
    mutating func updateIdentity(deviceID: String, mode: Mode?, displayName: String?) {
        self.deviceID = deviceID
        if let mode = mode {
            self.modeHint = mode
        }
        if let name = displayName {
            self.displayName = name
        }
    }

    /// Update IP address (e.g., from Bonjour rediscovery)
    mutating func updateIP(_ newIP: String) {
        self.lastKnownIP = newIP.components(separatedBy: "%").first ?? newIP
    }
}
