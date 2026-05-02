//
//  DeviceTab.swift
//  LightwaveOS
//
//  Settings and diagnostics tab using NavigationStack + List.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import SwiftUI

struct DeviceTab: View {
    @Environment(AppViewModel.self) private var appVM

    var body: some View {
        NavigationStack {
            List {
                // Section: Device
                Section("Device") {
                    DeviceInfoRow(
                        label: "Firmware Version",
                        value: appVM.deviceInfo?.firmware ?? "—"
                    )

                    DeviceInfoRow(
                        label: "Uptime",
                        value: uptimeText
                    )

                    DeviceInfoRow(
                        label: "FPS",
                        value: fpsText,
                        valueColor: fpsColor
                    )

                    DeviceInfoRow(
                        label: "Free Heap",
                        value: freeHeapText
                    )

                    DeviceInfoRow(
                        label: "AP Clients",
                        value: wsClientsText
                    )
                }
                .listRowBackground(Color.lwCard)

                // Section: Presets
                Section("Presets") {
                    NavigationLink {
                        PresetsView(type: .effect)
                    } label: {
                        Label("Effect Presets", systemImage: "star.fill")
                            .foregroundStyle(Color.lwTextPrimary)
                    }

                    NavigationLink {
                        PresetsView(type: .zone)
                    } label: {
                        Label("Zone Presets", systemImage: "square.split.2x2")
                            .foregroundStyle(Color.lwTextPrimary)
                    }

                    NavigationLink {
                        ShowsView()
                    } label: {
                        Label("Shows", systemImage: "play.circle")
                            .foregroundStyle(Color.lwTextPrimary)
                    }
                }
                .listRowBackground(Color.lwCard)

                // Section: Network
                Section("Network") {
                    HStack {
                        Text("Connection Status")
                            .foregroundStyle(Color.lwTextSecondary)
                        Spacer()
                        ConnectionDot(state: appVM.connectionState)
                        Text(connectionStatusText)
                            .font(.caption)
                            .foregroundStyle(Color.lwTextPrimary)
                    }

                    DeviceInfoRow(
                        label: "Device IP",
                        value: appVM.currentDevice?.ipAddress ?? "Not connected"
                    )

                    Button {
                        Task {
                            if let device = appVM.currentDevice {
                                await appVM.connect(to: device)
                            }
                        }
                    } label: {
                        HStack {
                            Image(systemName: "arrow.clockwise")
                            Text("Reconnect")
                        }
                        .foregroundStyle(Color.lwGold)
                    }
                }
                .listRowBackground(Color.lwCard)

                // Section: Debug
                Section("Debug") {
                    NavigationLink {
                        DebugLogView()
                    } label: {
                        Label("Debug Log", systemImage: "list.bullet.rectangle")
                            .foregroundStyle(Color.lwTextPrimary)
                    }

                    NavigationLink {
                        WebSocketInspectorView()
                    } label: {
                        Label("WebSocket Inspector", systemImage: "antenna.radiowaves.left.and.right")
                            .foregroundStyle(Color.lwTextPrimary)
                    }
                }
                .listRowBackground(Color.lwCard)

                // Section: About
                Section("About") {
                    DeviceInfoRow(
                        label: "App Version",
                        value: Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "1.0"
                    )

                    DeviceInfoRow(
                        label: "Build Number",
                        value: Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "1"
                    )
                }
                .listRowBackground(Color.lwCard)
            }
            .listStyle(.insetGrouped)
            .scrollContentBackground(.hidden)
            .background(Color.lwBase)
            .navigationTitle("Device")
        }
    }

    // MARK: - Computed Properties

    private var connectionStatusText: String {
        switch appVM.connectionState {
        case .connected:
            return "Connected"
        case .connecting:
            return "Connecting..."
        case .disconnected:
            return "Disconnected"
        case .discovering:
            return "Discovering..."
        case .error(let message):
            return "Error: \(message)"
        }
    }

    // K1 V2 is AP-ONLY (hard architectural rule — see firmware
    // CLAUDE.md). The firmware gates `network.rssi` behind
    // `WiFi.status() == WL_CONNECTED` which is permanently false on K1,
    // so the legacy "WiFi RSSI" row was always rendering "—". Replaced
    // with the AP-side connected-client count which firmware does
    // populate via `wsClients`.
    private var wsClientsText: String {
        guard let count = appVM.deviceStatus?.wsClients else { return "—" }
        return count == 1 ? "1 client" : "\(count) clients"
    }

    private var uptimeText: String {
        guard let seconds = appVM.deviceStatus?.uptime else { return "—" }
        let hours = seconds / 3600
        let minutes = (seconds % 3600) / 60
        let secs = seconds % 60
        if hours > 0 {
            return "\(hours)h \(minutes)m"
        }
        if minutes > 0 {
            return "\(minutes)m \(secs)s"
        }
        return "\(secs)s"
    }

    private var fpsText: String {
        guard let fps = appVM.deviceStatus?.fps else { return "—" }
        return String(format: "%.0f", fps)
    }

    private var fpsColor: Color {
        guard let fps = appVM.deviceStatus?.fps else { return .lwTextTertiary }
        if fps >= 100 { return .lwSuccess }
        if fps >= 60 { return .lwGold }
        return .lwError
    }

    private var freeHeapText: String {
        // Firmware reports `freeHeap` in BYTES (ESP.getFreeHeap()).
        // Convert to KB / MB for human display. K1 V2 typical baseline
        // is ~150 KB; rendering anything as "MB" almost always means
        // PSRAM pool, never internal heap.
        guard let heap = appVM.deviceStatus?.freeHeap else { return "—" }
        if heap >= 1_048_576 {
            let mb = Double(heap) / 1_048_576.0
            return String(format: "%.1f MB", mb)
        }
        if heap >= 1024 {
            let kb = heap / 1024
            return "\(kb) KB"
        }
        return "\(heap) B"
    }
}

// MARK: - Device Info Row

struct DeviceInfoRow: View {
    let label: String
    let value: String
    var valueColor: Color = .lwTextPrimary

    var body: some View {
        HStack {
            Text(label)
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextSecondary)

            Spacer()

            Text(value)
                .font(.bodyValue)
                .foregroundStyle(valueColor)
                .monospacedDigit()
        }
    }
}

// MARK: - Preview

#Preview("Device Tab") {
    DeviceTab()
        .environment(AppViewModel())
}
