//
//  ConnectivityTortureTest.swift
//  LightwaveOS
//
//  Internal diagnostic screen for connectivity testing.
//  Shows connection state, target info, retry status, and provides test controls.
//  iOS 17+, Swift 6.
//

import SwiftUI

struct ConnectivityTortureTest: View {
    @Environment(AppViewModel.self) private var appVM

    @State private var exportMessage: String?
    @State private var stateEntryTime: Date = Date()

    var body: some View {
        NavigationStack {
            List {
                // Section 1: Current State
                connectionStateSection

                // Section 2: Target Info
                deviceTargetSection

                // Section 3: Network Status
                networkStatusSection

                // Section 4: Controls
                testControlsSection

                // Section 5: Recent Logs
                recentLogsSection
            }
            .listStyle(.insetGrouped)
            .scrollContentBackground(.hidden)
            .background(Color.lwBase)
            .navigationTitle("Connectivity Test")
            .navigationBarTitleDisplayMode(.inline)
            .onChange(of: connectionStateString) { _, _ in
                stateEntryTime = Date()
            }
            .overlay {
                if let message = exportMessage {
                    exportToast(message)
                }
            }
        }
    }

    // MARK: - Connection State Section

    private var connectionStateSection: some View {
        Section {
            LabeledContent("State") {
                Text(connectionStateString)
                    .font(.monospace)
                    .foregroundStyle(connectionStateColour)
            }

            if let details = connectionStateDetails {
                LabeledContent("Details") {
                    Text(details)
                        .font(.monospace)
                        .foregroundStyle(Color.lwTextSecondary)
                }
            }

            LabeledContent("Time in State") {
                Text(timeInState)
                    .font(.monospace)
                    .foregroundStyle(Color.lwTextSecondary)
            }
        } header: {
            Text("Connection State")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwGold)
        }
        .listRowBackground(Color.lwCard)
    }

    // MARK: - Device Target Section

    private var deviceTargetSection: some View {
        Section {
            if let device = appVM.currentDevice {
                LabeledContent("IP") {
                    Text(device.cleanIP)
                        .font(.monospace)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                LabeledContent("Port") {
                    Text("\(device.port)")
                        .font(.monospace)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                LabeledContent("Device ID") {
                    Text(device.id.isEmpty ? "Unknown" : device.id)
                        .font(.monospace)
                        .foregroundStyle(Color.lwTextSecondary)
                }

                LabeledContent("Display Name") {
                    Text(device.displayName)
                        .font(.monospace)
                        .foregroundStyle(Color.lwTextPrimary)
                }
            } else if let ip = appVM.currentIP {
                LabeledContent("IP") {
                    Text(ip)
                        .font(.monospace)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                LabeledContent("Port") {
                    Text("80")
                        .font(.monospace)
                        .foregroundStyle(Color.lwTextPrimary)
                }

                Text("No device info available")
                    .font(.caption)
                    .foregroundStyle(Color.lwTextTertiary)
            } else {
                Text("No target")
                    .font(.bodyValue)
                    .foregroundStyle(Color.lwTextTertiary)
            }
        } header: {
            Text("Device Target")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwGold)
        }
        .listRowBackground(Color.lwCard)
    }

    // MARK: - Network Status Section

    private var networkStatusSection: some View {
        Section {
            LabeledContent("WiFi Status") {
                Text(wifiStatusString)
                    .font(.monospace)
                    .foregroundStyle(wifiStatusColour)
            }

            LabeledContent("WebSocket") {
                Text(appVM.wsConnected ? "Connected" : "Disconnected")
                    .font(.monospace)
                    .foregroundStyle(appVM.wsConnected ? Color.lwSuccess : Color.lwError)
            }

            LabeledContent("LED Stream") {
                Text(appVM.isLEDStreamActive ? "Active" : "Inactive")
                    .font(.monospace)
                    .foregroundStyle(appVM.isLEDStreamActive ? Color.lwSuccess : Color.lwTextTertiary)
            }

            if let ip = appVM.currentIP {
                LabeledContent("Mode") {
                    Text(ip == "192.168.4.1" ? "AP (Direct)" : "STA (Router)")
                        .font(.monospace)
                        .foregroundStyle(Color.lwTextSecondary)
                }
            }
        } header: {
            Text("Network")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwGold)
        }
        .listRowBackground(Color.lwCard)
    }

    // MARK: - Test Controls Section

    private var testControlsSection: some View {
        Section {
            Button {
                Task {
                    await appVM.connectManual(ip: "192.168.4.1", port: 80)
                }
            } label: {
                Label("Connect to AP", systemImage: "antenna.radiowaves.left.and.right")
            }
            .foregroundStyle(Color.lwGold)

            Button {
                Task {
                    await appVM.disconnect()
                }
            } label: {
                Label("Disconnect", systemImage: "xmark.circle")
            }
            .foregroundStyle(Color.lwError)

            Button {
                clearCachedIP()
            } label: {
                Label("Clear Cached IP", systemImage: "trash")
            }
            .foregroundStyle(Color.lwTextSecondary)

            Button {
                appVM.attemptReconnection()
            } label: {
                Label("Attempt Reconnection", systemImage: "arrow.clockwise")
            }
            .foregroundStyle(Color.lwCyan)

            Button {
                exportLogs()
            } label: {
                Label("Export Logs", systemImage: "square.and.arrow.up")
            }
            .foregroundStyle(Color.lwGold)
        } header: {
            Text("Test Controls")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwGold)
        }
        .listRowBackground(Color.lwCard)
    }

    // MARK: - Recent Logs Section

    private var recentLogsSection: some View {
        Section {
            let filteredLogs = appVM.debugLog
                .filter { ["CONN", "WS", "NET", "UDP", "REST", "ERROR"].contains($0.category) }
                .suffix(20)

            if filteredLogs.isEmpty {
                Text("No connectivity logs")
                    .font(.caption)
                    .foregroundStyle(Color.lwTextTertiary)
            } else {
                ForEach(Array(filteredLogs)) { entry in
                    logEntryView(entry)
                }
            }
        } header: {
            Text("Recent Logs")
                .font(.sectionHeader)
                .foregroundStyle(Color.lwGold)
        }
        .listRowBackground(Color.lwCard)
    }

    // MARK: - Log Entry View

    private func logEntryView(_ entry: DebugEntry) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            HStack {
                Text("[\(entry.category)]")
                    .font(.monospaceSmall)
                    .foregroundStyle(categoryColour(entry.category))

                Spacer()

                Text(formatTimestamp(entry.timestamp))
                    .font(.monospaceSmall)
                    .foregroundStyle(Color.lwTextTertiary)
            }

            Text(entry.message)
                .font(.monospace)
                .foregroundStyle(Color.lwTextSecondary)
                .lineLimit(2)
        }
        .padding(.vertical, 2)
    }

    // MARK: - Export Toast

    private func exportToast(_ message: String) -> some View {
        VStack {
            Spacer()
            Text(message)
                .font(.bodyValue)
                .foregroundStyle(Color.lwTextPrimary)
                .padding(.horizontal, Spacing.lg)
                .padding(.vertical, Spacing.md)
                .background(Color.lwElevated)
                .clipShape(RoundedRectangle(cornerRadius: CornerRadius.nested))
                .ambientShadow()
                .padding(.bottom, Spacing.lg)
        }
        .transition(.move(edge: .bottom).combined(with: .opacity))
        .animation(.easeInOut, value: exportMessage)
    }

    // MARK: - Computed Properties

    private var connectionStateString: String {
        switch appVM.connectionState {
        case .idle:
            return "Idle"
        case .targeting:
            return "Targeting..."
        case .handshakeHTTP:
            return "HTTP Handshake..."
        case .connectingWS:
            return "Connecting WS..."
        case .ready:
            return "Ready"
        case .backoff(let attempt, _):
            return "Backoff (attempt \(attempt))..."
        case .failed(let reason):
            return "Failed: \(reason.userTitle)"
        }
    }

    private var connectionStateDetails: String? {
        switch appVM.connectionState {
        case .failed(let reason):
            return reason.userMessage
        default:
            return nil
        }
    }

    private var connectionStateColour: Color {
        switch appVM.connectionState {
        case .idle:
            return .lwTextTertiary
        case .targeting, .handshakeHTTP, .connectingWS:
            return .lwGold
        case .ready:
            return .lwSuccess
        case .backoff:
            return .lwCyan
        case .failed:
            return .lwError
        }
    }

    private var timeInState: String {
        let elapsed = Date().timeIntervalSince(stateEntryTime)
        if elapsed < 60 {
            return String(format: "%.1fs", elapsed)
        } else if elapsed < 3600 {
            return String(format: "%.1fm", elapsed / 60)
        } else {
            return String(format: "%.1fh", elapsed / 3600)
        }
    }

    private var wifiStatusString: String {
        switch appVM.connectionState {
        case .ready:
            return "Connected"
        case .targeting, .handshakeHTTP, .connectingWS, .backoff:
            return "Active"
        case .idle:
            return "Idle"
        case .failed:
            return "Error"
        }
    }

    private var wifiStatusColour: Color {
        switch appVM.connectionState {
        case .ready:
            return .lwSuccess
        case .targeting, .handshakeHTTP, .connectingWS, .backoff:
            return .lwGold
        case .idle:
            return .lwTextTertiary
        case .failed:
            return .lwError
        }
    }

    // MARK: - Helper Methods

    private func categoryColour(_ category: String) -> Color {
        switch category {
        case "ERROR": return .lwError
        case "CONN": return .lwGold
        case "WS": return .lwCyan
        case "NET": return .lwSuccess
        case "UDP": return .lwZone2
        case "REST": return .lwZone1
        default: return .lwTextSecondary
        }
    }

    private func formatTimestamp(_ date: Date) -> String {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss"
        return formatter.string(from: date)
    }

    private func clearCachedIP() {
        UserDefaults.standard.removeObject(forKey: "lightwaveos.lastDeviceIP")
        UserDefaults.standard.removeObject(forKey: "lightwaveos.lastDevicePort")
        showExportMessage("Cached IP cleared")
    }

    private func exportLogs() {
        let logs = appVM.debugLog
            .map { "[\($0.category)] \(formatFullTimestamp($0.timestamp)): \($0.message)" }
            .joined(separator: "\n")

        let filename = "lightwaveos-logs-\(Date().ISO8601Format()).txt"
        let url = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent(filename)

        do {
            try logs.write(to: url, atomically: true, encoding: .utf8)
            showExportMessage("Logs exported to Documents")
        } catch {
            showExportMessage("Export failed: \(error.localizedDescription)")
        }
    }

    private func formatFullTimestamp(_ date: Date) -> String {
        let formatter = ISO8601DateFormatter()
        formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        return formatter.string(from: date)
    }

    private func showExportMessage(_ message: String) {
        withAnimation {
            exportMessage = message
        }

        Task {
            try? await Task.sleep(for: .seconds(2))
            await MainActor.run {
                withAnimation {
                    exportMessage = nil
                }
            }
        }
    }
}

// MARK: - Preview

#Preview("Connectivity Torture Test - Disconnected") {
    ConnectivityTortureTest()
        .environment(AppViewModel())
}

#Preview("Connectivity Torture Test - With Logs") {
    let appVM = AppViewModel()
    appVM.debugLog = [
        DebugEntry(timestamp: Date(), category: "CONN", message: "Connecting to 192.168.4.1..."),
        DebugEntry(timestamp: Date(), category: "WS", message: "WebSocket connected"),
        DebugEntry(timestamp: Date(), category: "UDP", message: "LED stream started"),
        DebugEntry(timestamp: Date(), category: "NET", message: "WiFi restored"),
        DebugEntry(timestamp: Date(), category: "ERROR", message: "Connection timeout")
    ]

    return ConnectivityTortureTest()
        .environment(appVM)
}
