//
//  ConnectionSheet.swift
//  LightwaveOS
//
//  Modal sheet for device discovery and manual IP connection.
//  Ported from v1 with minimal changes.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import SwiftUI
import Network

struct ConnectionSheet: View {
    @Environment(AppViewModel.self) private var appVM
    @Environment(\.dismiss) private var dismiss

    @State private var manualIP: String = ""
    @State private var manualPort: String = "80"
    @State private var isConnecting = false
    @State private var errorMessage: String?
    @State private var showAPInstructions = false
    @State private var apMonitor: NWPathMonitor?

    private let enableDiscoveryTasks: Bool

    init(enableDiscoveryTasks: Bool = true) {
        self.enableDiscoveryTasks = enableDiscoveryTasks
    }

    var body: some View {
        NavigationStack {
            ZStack {
                Color.lwBase.ignoresSafeArea()

                ScrollView {
                    VStack(spacing: Spacing.md) {
                        // Title
                        VStack(spacing: 2) {
                            Text("Connect to LightwaveOS")
                                .font(.sheetTitle)
                                .fontWeight(.bold)
                                .foregroundStyle(Color.lwTextPrimary)

                            Text("Select a discovered device or enter IP manually")
                                .font(.caption)
                                .foregroundStyle(Color.lwTextSecondary)
                                .multilineTextAlignment(.center)
                        }
                        .padding(.top, Spacing.sm)

                        // Error message
                        if let error = errorMessage {
                            LWCard {
                                HStack {
                                    Image(systemName: "exclamationmark.triangle.fill")
                                        .font(.caption)
                                        .foregroundStyle(Color.lwError)
                                    Text(error)
                                        .font(.caption)
                                        .foregroundStyle(Color.lwTextPrimary)
                                }
                            }
                        }

                        // Section 1: Discovered Devices
                        LWCard(title: "Discovered Devices") {
                            if appVM.discoveredDevices.isEmpty {
                                VStack(spacing: Spacing.sm) {
                                    ProgressView()
                                        .tint(Color.lwGold)
                                        .controlSize(.small)
                                        .frame(width: 120, height: 72)

                                    Text("Searching...")
                                        .font(.caption)
                                        .foregroundStyle(Color.lwTextSecondary)

                                    VStack(spacing: Spacing.xs) {
                                        if showAPInstructions {
                                            VStack(spacing: Spacing.xs) {
                                                Text("Join the AP network first:")
                                                    .font(.caption)
                                                    .fontWeight(.semibold)
                                                    .foregroundStyle(Color.lwGold)

                                                VStack(alignment: .leading, spacing: 2) {
                                                    Text("Network: **LightwaveOS-AP**")
                                                    Text("Password: **SpectraSynq**")
                                                }
                                                .font(.caption)
                                                .foregroundStyle(Color.lwTextPrimary)

                                                Text("Open Settings > Wi-Fi, join the network, then return here. Connection will start automatically.")
                                                    .font(.caption2)
                                                    .foregroundStyle(Color.lwTextSecondary)
                                                    .multilineTextAlignment(.center)
                                            }
                                            .padding(Spacing.sm)
                                            .background(Color.lwElevated)
                                            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.nested))
                                        } else {
                                            Text("AP-only mode? Tap Quick Connect below.")
                                                .font(.caption)
                                                .foregroundStyle(Color.lwTextSecondary)
                                                .multilineTextAlignment(.center)
                                        }

                                        Button {
                                            connectToApQuick()
                                        } label: {
                                            HStack {
                                                if isConnecting {
                                                    ProgressView()
                                                        .tint(Color.lwBase)
                                                        .controlSize(.small)
                                                } else {
                                                    Image(systemName: "wifi")
                                                }
                                                Text(showAPInstructions ? "Retry Quick Connect" : "Quick Connect: LightwaveOS-AP")
                                            }
                                            .font(.bodyValue)
                                            .foregroundStyle(Color.lwBase)
                                            .frame(maxWidth: .infinity)
                                            .padding(.vertical, Spacing.sm)
                                            .background(Color.lwGold)
                                            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.nested))
                                        }
                                        .disabled(isConnecting)
                                        .opacity(isConnecting ? 0.5 : 1.0)
                                    }
                                }
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, Spacing.sm)
                            } else {
                                VStack(spacing: Spacing.xs) {
                                    ForEach(appVM.discoveredDevices) { device in
                                        DeviceRowView(device: device) {
                                            connectToDevice(device)
                                        }
                                    }
                                }
                            }
                        }

                        // Section 2: Manual Connection
                        LWCard(title: "Manual Connection") {
                            VStack(spacing: Spacing.sm) {
                                VStack(alignment: .leading, spacing: 2) {
                                    Text("IP Address")
                                        .font(.cardLabel)
                                        .foregroundStyle(Color.lwTextSecondary)
                                        .textCase(.uppercase)

                                    TextField("192.168.1.101", text: $manualIP)
                                        .textFieldStyle(.plain)
                                        .font(.bodyValue)
                                        .padding(Spacing.sm)
                                        .background(Color.lwElevated)
                                        .foregroundStyle(Color.lwTextPrimary)
                                        .clipShape(RoundedRectangle(cornerRadius: CornerRadius.nested))
                                        .keyboardType(.decimalPad)
                                        .autocorrectionDisabled()
                                }

                                VStack(alignment: .leading, spacing: 2) {
                                    Text("Port")
                                        .font(.cardLabel)
                                        .foregroundStyle(Color.lwTextSecondary)
                                        .textCase(.uppercase)

                                    TextField("80", text: $manualPort)
                                        .textFieldStyle(.plain)
                                        .font(.bodyValue)
                                        .padding(Spacing.sm)
                                        .background(Color.lwElevated)
                                        .foregroundStyle(Color.lwTextPrimary)
                                        .clipShape(RoundedRectangle(cornerRadius: CornerRadius.nested))
                                        .keyboardType(.numberPad)
                                        .autocorrectionDisabled()
                                }

                                Button {
                                    connectManually()
                                } label: {
                                    HStack {
                                        if isConnecting {
                                            ProgressView()
                                                .tint(Color.lwBase)
                                                .controlSize(.small)
                                        } else {
                                            Image(systemName: "arrow.right.circle.fill")
                                        }
                                        Text(isConnecting ? "Connecting..." : "Connect")
                                    }
                                    .font(.bodyValue)
                                    .foregroundStyle(Color.lwBase)
                                    .frame(maxWidth: .infinity)
                                    .padding(.vertical, Spacing.sm)
                                    .background(Color.lwGold)
                                    .clipShape(RoundedRectangle(cornerRadius: CornerRadius.nested))
                                }
                                .disabled(isConnecting || manualIP.isEmpty)
                                .opacity((isConnecting || manualIP.isEmpty) ? 0.5 : 1.0)
                            }
                        }
                    }
                    .padding(Spacing.md)
                }
            }
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Close") {
                        dismiss()
                    }
                    .foregroundStyle(Color.lwGold)
                }
            }
        }
        .task {
            guard enableDiscoveryTasks else { return }
            await appVM.startDeviceDiscovery()
        }
        .onDisappear {
            stopAPMonitor()
            guard enableDiscoveryTasks else { return }
            Task {
                await appVM.stopDeviceDiscovery()
            }
        }
        .onChange(of: appVM.connectionState) { _, newState in
            if newState == .connected {
                dismiss()
            }
        }
    }

    // MARK: - Helper Methods

    private func connectToDevice(_ device: DeviceInfo) {
        isConnecting = true
        errorMessage = nil

        Task {
            await appVM.connect(to: device)

            await MainActor.run {
                isConnecting = false
                if case .error(let message) = appVM.connectionState {
                    errorMessage = message
                }
                // dismiss happens via onChange(of: appVM.connectionState) above
            }
        }
    }

    private func connectManually() {
        let ip = manualIP.trimmingCharacters(in: .whitespaces)
        guard !ip.isEmpty else { return }
        let port = Int(manualPort) ?? 80

        isConnecting = true
        errorMessage = nil

        Task {
            await appVM.connectManual(ip: ip, port: port)

            await MainActor.run {
                isConnecting = false
                if case .error(let message) = appVM.connectionState {
                    errorMessage = message
                }
            }
        }
    }

    private func connectToApQuick() {
        let ip = "192.168.4.1"
        let port = 80

        isConnecting = true
        errorMessage = nil

        Task {
            await appVM.connectManual(ip: ip, port: port)

            await MainActor.run {
                isConnecting = false
                if case .error = appVM.connectionState {
                    // Probe failed — user is probably not on the AP network yet.
                    // Show join instructions and start monitoring for WiFi changes.
                    showAPInstructions = true
                    errorMessage = "Cannot reach 192.168.4.1 — join the AP network first."
                    startAPMonitor()
                }
            }
        }
    }

    /// Monitor WiFi changes — when the user joins the AP in Settings and returns,
    /// auto-retry the Quick Connect probe.
    private func startAPMonitor() {
        stopAPMonitor()
        let monitor = NWPathMonitor(requiredInterfaceType: .wifi)
        self.apMonitor = monitor

        monitor.pathUpdateHandler = { [self] path in
            Task { @MainActor in
                if path.status == .satisfied {
                    // WiFi changed — user may have joined the AP. Retry.
                    self.stopAPMonitor()
                    self.connectToApQuick()
                }
            }
        }

        monitor.start(queue: DispatchQueue(label: "com.lightwaveos.apmonitor", qos: .utility))
    }

    private func stopAPMonitor() {
        apMonitor?.cancel()
        apMonitor = nil
    }
}

// MARK: - Device Row View

struct DeviceRowView: View {
    let device: DeviceInfo
    let onTap: () -> Void

    var body: some View {
        Button(action: onTap) {
            VStack(alignment: .leading, spacing: 2) {
                HStack {
                    Text(device.displayName)
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextPrimary)

                    Spacer()

                    Image(systemName: "chevron.right")
                        .font(.caption)
                        .foregroundStyle(Color.lwGold)
                }

                Text(device.ipAddress)
                    .font(.caption)
                    .foregroundStyle(Color.lwTextSecondary)
                    .monospacedDigit()
            }
            .padding(Spacing.sm)
            .background(Color.lwElevated)
            .clipShape(RoundedRectangle(cornerRadius: CornerRadius.nested))
        }
        .buttonStyle(.plain)
    }
}

// MARK: - Mock Data

#if DEBUG
private let mockDiscoveredDevices: [DeviceInfo] = [
    DeviceInfo(hostname: "lightwaveos.local", ipAddress: "192.168.1.101", port: 80),
    DeviceInfo(hostname: "lightwaveos-k1.local", ipAddress: "192.168.1.102", port: 80)
]
#endif

// MARK: - Preview

#Preview("Connection Sheet") {
    struct PreviewWrapper: View {
        @State private var isPresented = true

        var body: some View {
            Text("Background")
                .sheet(isPresented: $isPresented) {
                    ConnectionSheet()
                        .environment(AppViewModel())
                }
        }
    }

    return PreviewWrapper()
}
