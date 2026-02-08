//
//  ConnectionDotSnapshotTests.swift
//  LightwaveOSTests
//
//  Snapshot tests for ConnectionDot component showing all connection states.
//  Animations are disabled during testing for deterministic snapshots.
//

import XCTest
import SwiftUI
@testable import LightwaveOS

final class ConnectionDotSnapshotTests: SnapshotTestCase {

    // MARK: - Individual Connection States

    func testConnectionStateIdle() {
        let view = HStack {
            ConnectionDot(state: .idle)
            Text("Idle")
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 200,
            height: 80,
            named: "connection-idle"
        )
    }

    func testConnectionStateTargeting() {
        let view = HStack {
            ConnectionDot(state: .targeting)
            Text("Targeting")
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 200,
            height: 80,
            named: "connection-targeting"
        )
    }

    func testConnectionStateHandshakeHTTP() {
        let view = HStack {
            ConnectionDot(state: .handshakeHTTP)
            Text("HTTP Handshake")
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 250,
            height: 80,
            named: "connection-handshake-http"
        )
    }

    func testConnectionStateConnectingWS() {
        let view = HStack {
            ConnectionDot(state: .connectingWS)
            Text("WebSocket Connect")
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 280,
            height: 80,
            named: "connection-connecting-ws"
        )
    }

    func testConnectionStateReady() {
        let view = HStack {
            ConnectionDot(state: .ready)
            Text("Connected")
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 200,
            height: 80,
            named: "connection-ready"
        )
    }

    func testConnectionStateBackoff() {
        let view = HStack {
            ConnectionDot(state: .backoff(attempt: 2, nextRetry: Date().addingTimeInterval(5)))
            Text("Backoff (Degraded)")
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 280,
            height: 80,
            named: "connection-backoff"
        )
    }

    func testConnectionStateFailed() {
        let view = HStack {
            ConnectionDot(state: .failed(.noTarget))
            Text("Failed")
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 200,
            height: 80,
            named: "connection-failed"
        )
    }

    // MARK: - Display States Overview

    func testAllDisplayStates() {
        let view = LWCard(title: "5 Display States") {
            VStack(alignment: .leading, spacing: Spacing.md) {
                HStack {
                    ConnectionDot(state: .targeting)
                    Text("Searching...")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .connectingWS)
                    Text("Connecting...")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .ready)
                    Text("Connected")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .backoff(attempt: 2, nextRetry: Date().addingTimeInterval(5)))
                    Text("Degraded")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .idle)
                    Text("Offline")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }
            }
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 375,
            height: 300,
            named: "all-display-states"
        )
    }

    func testAllConnectionStates() {
        let view = LWCard(title: "All Connection States") {
            VStack(alignment: .leading, spacing: Spacing.md) {
                HStack {
                    ConnectionDot(state: .ready)
                    Text("Ready")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .idle)
                    Text("Idle")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .targeting)
                    Text("Targeting")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .handshakeHTTP)
                    Text("HTTP Handshake")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .connectingWS)
                    Text("WebSocket Connect")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .backoff(attempt: 2, nextRetry: Date()))
                    Text("Backoff")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .failed(.noTarget))
                    Text("Failed")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }
            }
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 375,
            height: 400,
            named: "all-connection-states"
        )
    }

    // MARK: - In Context (Status Bar)

    func testConnectionDotInStatusBar() {
        let view = HStack(spacing: Spacing.sm) {
            ConnectionDot(state: .ready)
            Text("Connected")
                .font(.cardLabel)
                .foregroundStyle(Color.lwTextPrimary)

            Text("•")
                .foregroundStyle(Color.lwTextTertiary)

            Text("LGP Holographic")
                .font(.cardLabel)
                .foregroundStyle(Color.lwTextSecondary)
        }
        .padding(Spacing.md)
        .background(Color.lwCard)
        .cornerRadius(8)
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 375,
            height: 100,
            named: "connection-in-status-bar"
        )
    }

    func testConnectionDotCompactLayout() {
        let view = HStack(spacing: Spacing.xs) {
            ConnectionDot(state: .ready)
            Text("OK")
                .font(.caption)
                .foregroundStyle(Color.lwSuccess)
        }
        .padding(Spacing.md)

        assertSnapshot(
            of: view,
            width: 100,
            height: 60,
            named: "connection-compact"
        )
    }

    // MARK: - Backward Compatibility

    func testConnectionDotBackwardCompatibleConnected() {
        let view = HStack {
            ConnectionDot(isConnected: true, isConnecting: false)
            Text("Connected (Legacy)")
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 280,
            height: 80,
            named: "connection-legacy-connected"
        )
    }

    func testConnectionDotBackwardCompatibleConnecting() {
        let view = HStack {
            ConnectionDot(isConnected: false, isConnecting: true)
            Text("Connecting (Legacy)")
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 280,
            height: 80,
            named: "connection-legacy-connecting"
        )
    }

    func testConnectionDotBackwardCompatibleDisconnected() {
        let view = HStack {
            ConnectionDot(isConnected: false, isConnecting: false)
            Text("Disconnected (Legacy)")
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
        }
        .padding(Spacing.lg)

        assertSnapshot(
            of: view,
            width: 280,
            height: 80,
            named: "connection-legacy-disconnected"
        )
    }
}
