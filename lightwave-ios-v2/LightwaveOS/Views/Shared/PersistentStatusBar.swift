//
//  PersistentStatusBar.swift
//  LightwaveOS
//
//  Persistent status bar with expandable drawer showing connection details.
//  Design: Brand logo left, connection status right with chevron.
//  Drawer: IP, RSSI, WiFi, WebSocket status.
//

import SwiftUI

struct PersistentStatusBar: View {
    @Environment(AppViewModel.self) private var appVM
    var onTap: (() -> Void)?

    @State private var isDrawerOpen = false

    var body: some View {
        VStack(spacing: 0) {
            // Status bar
            Button {
                withAnimation(.spring(duration: 0.25)) {
                    isDrawerOpen.toggle()
                }
                onTap?()
            } label: {
                HStack(spacing: 12) {
                    // LEFT: Brand text
                    Text("LIGHTWAVEOS")
                        .font(.statusBrand)
                        .foregroundStyle(Color.lwTextPrimary)
                        .tracking(1.5)

                    Spacer()

                    // RIGHT: Connection status
                    connectionStatusView
                }
                .padding(.horizontal, 16)
                .padding(.vertical, 10)
                .background(statusBarBackground)
            }
            .buttonStyle(.plain)
            .accessibilityLabel(Text("Connection status"))
            .accessibilityValue(Text(statusText))

            // Expandable drawer
            if isDrawerOpen {
                drawerView
                    .transition(.move(edge: .top).combined(with: .opacity))
            }
        }
        .animation(.spring(duration: 0.25), value: isDrawerOpen)
    }

    // MARK: - Status Bar Components

    private var connectionStatusView: some View {
        HStack(spacing: 8) {
            // Connection indicator (Rive where available, fallback to native dot)
            connectionIndicator
                .shadow(color: dotGlowColor, radius: 8, x: 0, y: 0)

            // Status label
            Text(statusText)
                .font(.statusLabel)
                .foregroundStyle(Color.lwTextSecondary)

            // Chevron (rotates when drawer opens)
            Image(systemName: isDrawerOpen ? "chevron.down" : "chevron.right")
                .font(.iconTiny)
                .foregroundStyle(Color.lwTextTertiary)
                .rotationEffect(.degrees(isDrawerOpen ? 0 : 0))
        }
    }

    private var connectionIndicator: some View {
        ConnectionDot(state: appVM.connectionState)
            .frame(width: 16, height: 16)
    }

    private var statusBarBackground: some View {
        RoundedRectangle(cornerRadius: 12)
            .fill(Color.lwCard)
            .overlay {
                RoundedRectangle(cornerRadius: 12)
                    .strokeBorder(Color.white.opacity(0.25), lineWidth: 2)
            }
    }

    // MARK: - Drawer

    private var drawerView: some View {
        VStack(spacing: 0) {
            VStack(spacing: 12) {
                // IP Address row
                drawerRow(
                    label: "IP Address",
                    value: appVM.currentDevice?.cleanIP ?? "—",
                    valueColor: .lwTextPrimary
                )

                // RSSI row
                drawerRow(
                    label: "Signal Strength",
                    value: rssiText,
                    valueColor: rssiColor
                )

                // WiFi status row
                drawerRow(
                    label: "WiFi",
                    value: wifiStatusText,
                    valueColor: .lwTextSecondary
                )

                // WebSocket status row
                drawerRow(
                    label: "WebSocket",
                    value: wsStatusText,
                    valueColor: wsStatusColor
                )
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 12)
        }
        .background(drawerBackground)
    }

    @ViewBuilder
    private func drawerRow(label: String, value: String, valueColor: Color) -> some View {
        HStack {
            Text(label)
                .font(.cardLabel)
                .foregroundStyle(Color.lwTextSecondary)

            Spacer()

            Text(value)
                .font(.monospace)
                .foregroundStyle(valueColor)
        }
    }

    private var drawerBackground: some View {
        VStack(spacing: 0) {
            // Merge seamlessly with status bar (no top border)
            Color.lwElevated
        }
        .clipShape(
            UnevenRoundedRectangle(
                topLeadingRadius: 0,
                bottomLeadingRadius: 12,
                bottomTrailingRadius: 12,
                topTrailingRadius: 0
            )
        )
        .overlay {
            UnevenRoundedRectangle(
                topLeadingRadius: 0,
                bottomLeadingRadius: 12,
                bottomTrailingRadius: 12,
                topTrailingRadius: 0
            )
            .strokeBorder(Color.white.opacity(0.06), lineWidth: 1)
        }
    }

    // MARK: - Computed Properties

    private var statusText: String {
        switch appVM.connectionState {
        case .idle:
            return "Not Connected"
        case .targeting:
            return "Searching..."
        case .handshakeHTTP:
            return "Connecting..."
        case .connectingWS:
            return "Connecting..."
        case .backoff:
            return "Reconnecting..."
        case .ready:
            if appVM.currentDevice?.cleanIP == "192.168.4.1" {
                return "Connected (AP)"
            }
            return "Connected"
        case .failed:
            return "Reconnecting..."
        }
    }

    private var isErrorState: Bool {
        if case .failed = appVM.connectionState {
            return true
        }
        return false
    }

    private var dotGlowColor: Color {
        switch appVM.connectionState {
        case .ready:
            return Color.lwSuccess.opacity(0.4)
        case .targeting, .handshakeHTTP, .connectingWS, .backoff:
            return Color.lwGold.opacity(0.3)
        default:
            return Color.clear
        }
    }

    private var rssiText: String {
        if let rssi = appVM.deviceStatus?.network?.rssi {
            return "\(rssi) dBm"
        }
        return "—"
    }

    private var rssiColor: Color {
        guard let rssi = appVM.deviceStatus?.network?.rssi else {
            return Color.lwTextTertiary
        }
        if rssi >= -50 {
            return Color.lwSuccess
        }
        if rssi >= -70 {
            return Color.lwGold
        }
        return Color.lwError
    }

    private var wifiStatusText: String {
        if let connected = appVM.deviceStatus?.network?.connected {
            return connected ? "Connected" : "Disconnected"
        }
        return appVM.connectionState.isReady ? "Connected" : "Disconnected"
    }

    private var wsStatusText: String {
        appVM.wsConnected ? "Active" : "Inactive"
    }

    private var wsStatusColor: Color {
        appVM.wsConnected ? Color.lwSuccess : Color.lwTextTertiary
    }
}

// MARK: - Preview

// NOTE: Previews are disabled because connectionState is now a computed property
// from ConnectionManager and cannot be set directly on AppViewModel.
// To preview different states, use the ConnectionDot preview instead.

#Preview("Status Bar - Default") {
    VStack(spacing: 16) {
        PersistentStatusBar()
            .environment(AppViewModel())

        Spacer()
    }
    .padding(.horizontal, 16)
    .background(Color.lwBase)
}
