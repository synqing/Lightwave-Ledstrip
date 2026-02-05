//
//  ConnectionDot.swift
//  LightwaveOS
//
//  Status indicator dot for device connection state.
//

import SwiftUI

struct ConnectionDot: View {
    var connectionState: ConnectionState

    @SwiftUI.State private var pulseOpacity: Double = 1.0
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    // MARK: - Initialisers

    init(state: ConnectionState) {
        self.connectionState = state
    }

    /// Convenience initialiser for backwards compatibility
    init(isConnected: Bool, isConnecting: Bool = false) {
        if isConnecting {
            self.connectionState = .targeting
        } else if isConnected {
            self.connectionState = .ready
        } else {
            self.connectionState = .idle
        }
    }

    var body: some View {
        Circle()
            .fill(statusColor)
            .frame(width: 8, height: 8)
            .opacity(isAnimating && !reduceMotion ? pulseOpacity : 1.0)
            .onAppear {
                if isAnimating && !reduceMotion {
                    startPulsing()
                }
            }
            .onChange(of: connectionState) { _, newState in
                if mapToDisplayState(newState) == .connecting && !reduceMotion {
                    startPulsing()
                } else {
                    pulseOpacity = 1.0
                }
            }
    }

    // MARK: - Display State

    /// Simplified display states for the connection indicator
    enum DisplayState {
        case connected
        case connecting
        case disconnected
        case error
    }

    private var displayState: DisplayState {
        mapToDisplayState(connectionState)
    }

    private func mapToDisplayState(_ state: ConnectionState) -> DisplayState {
        switch state {
        case .ready:
            return .connected
        case .targeting, .handshakeHTTP, .connectingWS, .backoff:
            return .connecting
        case .idle:
            return .disconnected
        case .failed:
            return .error
        }
    }

    // MARK: - Helpers

    private var isAnimating: Bool {
        displayState == .connecting
    }

    private var statusColor: Color {
        switch displayState {
        case .connected:
            return Color.lwSuccess
        case .connecting:
            return Color.lwGold
        case .disconnected, .error:
            return Color.lwError
        }
    }

    private func startPulsing() {
        guard !reduceMotion else { return }

        withAnimation(.easeInOut(duration: 0.8).repeatForever(autoreverses: true)) {
            pulseOpacity = 0.3
        }
    }
}

// MARK: - Preview

#Preview("Connection States") {
    VStack(spacing: Spacing.lg) {
        LWCard(title: "Connection States") {
            VStack(alignment: .leading, spacing: Spacing.md) {
                HStack {
                    ConnectionDot(state: .ready)
                    Text("Ready (Connected)")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .idle)
                    Text("Idle (Disconnected)")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .targeting)
                    Text("Targeting...")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .handshakeHTTP)
                    Text("HTTP Handshake...")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .connectingWS)
                    Text("Connecting WebSocket...")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .backoff(attempt: 2, nextRetry: Date().addingTimeInterval(5)))
                    Text("Backoff (Retry 2)")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(state: .failed(.noTarget))
                    Text("Failed (Error)")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }
            }
        }

        LWCard(title: "Convenience Initialisers") {
            VStack(alignment: .leading, spacing: Spacing.md) {
                HStack {
                    ConnectionDot(isConnected: true)
                    Text("isConnected: true")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(isConnected: false)
                    Text("isConnected: false")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(isConnected: false, isConnecting: true)
                    Text("isConnecting: true")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }
            }
        }

        LWCard(title: "In Status Bar") {
            HStack(spacing: Spacing.sm) {
                ConnectionDot(state: .ready)
                Text("Connected")
                    .font(.cardLabel)
                    .foregroundStyle(Color.lwTextPrimary)

                Text("•")
                    .foregroundStyle(Color.lwTextTertiary)

                Text("LGP Holographic")
                    .font(.cardLabel)
                    .foregroundStyle(Color.lwTextSecondary)

                Text("•")
                    .foregroundStyle(Color.lwTextTertiary)

                HStack(spacing: 4) {
                    BeatPulse(isBeating: true, isDownbeat: false, confidence: 0.9)
                    Text("124 BPM")
                        .font(.cardLabel)
                        .foregroundStyle(Color.lwGold)
                }
            }
        }
    }
    .padding(Spacing.lg)
    .background(Color.lwBase)
}
