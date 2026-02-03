//
//  ConnectionDot.swift
//  LightwaveOS
//
//  Status indicator dot for device connection state.
//

import SwiftUI

struct ConnectionDot: View {
    var state: AppViewModel.ConnectionState

    @State private var pulseOpacity: Double = 1.0
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    // Convenience initialisers for backwards compatibility
    init(state: AppViewModel.ConnectionState) {
        self.state = state
    }

    init(isConnected: Bool, isConnecting: Bool = false) {
        if isConnecting {
            self.state = .connecting
        } else if isConnected {
            self.state = .connected
        } else {
            self.state = .disconnected
        }
    }

    var body: some View {
        Circle()
            .fill(statusColor)
            .frame(width: 8, height: 8)
            .opacity(isConnecting && !reduceMotion ? pulseOpacity : 1.0)
            .onAppear {
                if isConnecting && !reduceMotion {
                    startPulsing()
                }
            }
            .onChange(of: state) { _, newState in
                if isStateConnecting(newState) && !reduceMotion {
                    startPulsing()
                } else {
                    pulseOpacity = 1.0
                }
            }
    }

    // MARK: - Helpers

    private var isConnecting: Bool {
        isStateConnecting(state)
    }

    private func isStateConnecting(_ state: AppViewModel.ConnectionState) -> Bool {
        switch state {
        case .connecting, .discovering:
            return true
        default:
            return false
        }
    }

    private var statusColor: Color {
        switch state {
        case .connected:
            return Color.lwSuccess
        case .connecting, .discovering:
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
                    ConnectionDot(isConnected: true)
                    Text("Connected")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(isConnected: false)
                    Text("Disconnected")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                HStack {
                    ConnectionDot(isConnected: false, isConnecting: true)
                    Text("Connecting...")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)
                }
            }
        }

        LWCard(title: "In Status Bar") {
            HStack(spacing: Spacing.sm) {
                ConnectionDot(isConnected: true)
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
