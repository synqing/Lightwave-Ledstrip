//
//  DeviceInfo.swift
//  LightwaveOS
//
//  Represents a discovered Lightwave device on the network.
//

import Foundation

/// Represents a discovered Lightwave ESP32-S3 device
struct DeviceInfo: Codable, Sendable, Identifiable, Hashable {
    /// Unique identifier (uses IP address)
    var id: String { ipAddress }

    /// mDNS hostname (e.g., "lightwaveos.local")
    let hostname: String

    /// IPv4 address (e.g., "192.168.1.101")
    let ipAddress: String

    /// HTTP port (default: 80)
    let port: Int

    /// Timestamp of last successful connection
    var lastSeen: Date

    /// Clean IP without interface scope suffix (e.g., strips "%en0")
    var cleanIP: String {
        ipAddress.components(separatedBy: "%").first ?? ipAddress
    }

    /// Base URL for REST API endpoints
    var baseURL: URL {
        URL(string: "http://\(cleanIP):\(port)") ?? URL(string: "http://localhost")!
    }

    /// WebSocket URL for real-time updates
    var wsURL: URL {
        URL(string: "ws://\(cleanIP):\(port)/ws") ?? URL(string: "ws://localhost/ws")!
    }

    /// Display name for UI
    var displayName: String {
        hostname.replacingOccurrences(of: ".local", with: "")
    }

    // MARK: - Initialization

    init(hostname: String, ipAddress: String, port: Int = 80, lastSeen: Date = Date()) {
        self.hostname = hostname
        self.ipAddress = ipAddress
        self.port = port
        self.lastSeen = lastSeen
    }
}

// MARK: - Hashable Conformance

extension DeviceInfo {
    func hash(into hasher: inout Hasher) {
        hasher.combine(ipAddress)
        hasher.combine(port)
    }

    static func == (lhs: DeviceInfo, rhs: DeviceInfo) -> Bool {
        lhs.ipAddress == rhs.ipAddress && lhs.port == rhs.port
    }
}

// MARK: - Sample Data

#if DEBUG
extension DeviceInfo {
    static let preview = DeviceInfo(
        hostname: "lightwaveos.local",
        ipAddress: "192.168.1.101",
        port: 80,
        lastSeen: Date()
    )

    static let k1Prototype = DeviceInfo(
        hostname: "lightwaveos.local",
        ipAddress: "192.168.1.101",
        port: 80,
        lastSeen: Date()
    )
}
#endif
