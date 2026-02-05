//
//  FailureReason.swift
//  LightwaveOS
//
//  Typed failure taxonomy for connection failures.
//  Every failure is classifiable - no "unknown" bucket.
//  iOS 17+, Swift 6 with strict concurrency.
//

import Foundation

/// Classified reasons for connection failure.
/// Used by ConnectionManager state machine and observability logging.
/// Conforms to Error for use in throws/catch patterns.
enum FailureReason: Error, Equatable, Sendable, CustomStringConvertible {
    // MARK: - Permission Failures

    /// Local Network permission denied by user or system
    case permissionDenied

    // MARK: - Network Failures

    /// No network route to target IP (WiFi off, wrong network, etc.)
    case noRoute

    /// Network request timed out
    case timeout

    /// DNS resolution failed (for hostname targets)
    case dnsFailed

    /// Network path became unsatisfied (WiFi lost mid-connection)
    case networkUnsatisfied

    // MARK: - Discovery Failures

    /// Bonjour discovery returned no results within timeout
    case bonjourNoResults

    /// No target available (no persisted device, no AP detected, no discovery results)
    case noTarget

    // MARK: - Handshake Failures

    /// HTTP handshake to /api/v1/device/info failed
    case httpHandshakeFailed(statusCode: Int?, message: String)

    /// WebSocket handshake failed or connection rejected
    case wsHandshakeFailed(message: String)

    /// WebSocket receive loop terminated unexpectedly
    case wsReceiveFailed(message: String)

    // MARK: - Identity Failures

    /// Device at target IP has different UUID than expected
    case deviceMismatch(expected: String, actual: String)

    // MARK: - Retry Exhaustion

    /// Maximum retry attempts exceeded
    case maxRetries(attempts: Int)

    /// User cancelled the connection attempt
    case cancelled

    // MARK: - CustomStringConvertible

    var description: String {
        switch self {
        case .permissionDenied:
            return "Local Network permission denied"
        case .noRoute:
            return "No network route to device"
        case .timeout:
            return "Connection timed out"
        case .dnsFailed:
            return "DNS resolution failed"
        case .networkUnsatisfied:
            return "Network connection lost"
        case .bonjourNoResults:
            return "No devices found via Bonjour"
        case .noTarget:
            return "No device target available"
        case .httpHandshakeFailed(let statusCode, let message):
            if let code = statusCode {
                return "HTTP handshake failed (\(code)): \(message)"
            }
            return "HTTP handshake failed: \(message)"
        case .wsHandshakeFailed(let message):
            return "WebSocket handshake failed: \(message)"
        case .wsReceiveFailed(let message):
            return "WebSocket connection lost: \(message)"
        case .deviceMismatch(let expected, let actual):
            return "Device mismatch: expected \(expected), found \(actual)"
        case .maxRetries(let attempts):
            return "Connection failed after \(attempts) attempts"
        case .cancelled:
            return "Connection cancelled"
        }
    }

    // MARK: - User-Facing Messages

    /// Short user-facing error title
    var userTitle: String {
        switch self {
        case .permissionDenied:
            return "Permission Required"
        case .noRoute, .networkUnsatisfied:
            return "No Network"
        case .timeout:
            return "Connection Timeout"
        case .dnsFailed:
            return "DNS Error"
        case .bonjourNoResults, .noTarget:
            return "No Devices Found"
        case .httpHandshakeFailed:
            return "Device Not Responding"
        case .wsHandshakeFailed, .wsReceiveFailed:
            return "Connection Lost"
        case .deviceMismatch:
            return "Wrong Device"
        case .maxRetries:
            return "Connection Failed"
        case .cancelled:
            return "Cancelled"
        }
    }

    /// User-facing error description with actionable guidance
    var userMessage: String {
        switch self {
        case .permissionDenied:
            return "LightwaveOS needs Local Network permission. Go to Settings → Privacy & Security → Local Network and enable LightwaveOS."
        case .noRoute:
            return "Cannot reach the device. Check that your iPhone is connected to the same WiFi network as the device, or connect to the LightwaveOS-AP network."
        case .networkUnsatisfied:
            return "WiFi connection was lost. Check your network connection and try again."
        case .timeout:
            return "The device is taking too long to respond. It may be busy or out of range."
        case .dnsFailed:
            return "Could not resolve device hostname. Try connecting via IP address instead."
        case .bonjourNoResults:
            return "No LightwaveOS devices found on this network. Make sure the device is powered on and connected to WiFi."
        case .noTarget:
            return "No device to connect to. Use the connection sheet to discover or enter a device."
        case .httpHandshakeFailed:
            return "The device is not responding. Check that it's powered on and try again."
        case .wsHandshakeFailed:
            return "Could not establish real-time connection. The device may be busy with another client."
        case .wsReceiveFailed:
            return "Lost connection to the device. Attempting to reconnect..."
        case .deviceMismatch:
            return "Connected to a different device than expected. This may happen if the device IP changed."
        case .maxRetries:
            return "Could not connect after multiple attempts. Check the device and network, then try again."
        case .cancelled:
            return "Connection was cancelled."
        }
    }

    // MARK: - Retry Eligibility

    /// Whether this failure should trigger automatic retry
    var shouldRetry: Bool {
        switch self {
        case .permissionDenied, .cancelled, .deviceMismatch, .maxRetries, .noTarget:
            return false
        case .noRoute, .timeout, .dnsFailed, .networkUnsatisfied,
             .bonjourNoResults, .httpHandshakeFailed, .wsHandshakeFailed, .wsReceiveFailed:
            return true
        }
    }

    // MARK: - Logging Category

    /// Category for structured logging
    var logCategory: String {
        switch self {
        case .permissionDenied:
            return "PERM"
        case .noRoute, .networkUnsatisfied:
            return "NET"
        case .timeout:
            return "TIMEOUT"
        case .dnsFailed:
            return "DNS"
        case .bonjourNoResults, .noTarget:
            return "DISC"
        case .httpHandshakeFailed:
            return "HTTP"
        case .wsHandshakeFailed, .wsReceiveFailed:
            return "WS"
        case .deviceMismatch:
            return "IDENTITY"
        case .maxRetries:
            return "RETRY"
        case .cancelled:
            return "USER"
        }
    }
}
