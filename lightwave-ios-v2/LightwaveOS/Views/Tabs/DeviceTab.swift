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
                        label: "WiFi RSSI",
                        value: wifiRssiText,
                        valueColor: wifiRssiColor
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

    private var wifiRssiText: String {
        if let rssi = appVM.deviceStatus?.network?.rssi {
            return "\(rssi) dBm"
        }
        return "—"
    }

    private var wifiRssiColor: Color {
        guard let rssi = appVM.deviceStatus?.network?.rssi else {
            return .lwTextTertiary
        }
        if rssi >= -50 {
            return .lwSuccess
        }
        if rssi >= -70 {
            return .lwGold
        }
        return .lwError
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
        guard let heap = appVM.deviceStatus?.freeHeap else { return "—" }
        if heap >= 1024 {
            let mb = Double(heap) / 1024.0
            return String(format: "%.1f MB", mb)
        }
        return "\(heap) KB"
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

// MARK: - Placeholder: WebSocket Inspector

struct WebSocketInspectorView: View {
    var body: some View {
        VStack {
            Text("WebSocket Inspector")
                .font(.effectTitle)
                .foregroundStyle(Color.lwTextPrimary)

            Text("Coming soon")
                .font(.caption)
                .foregroundStyle(Color.lwTextSecondary)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(Color.lwBase)
        .navigationTitle("WebSocket Inspector")
        .navigationBarTitleDisplayMode(.inline)
    }
}

// MARK: - Preview

#Preview("Device Tab") {
    DeviceTab()
        .environment(AppViewModel())
}
