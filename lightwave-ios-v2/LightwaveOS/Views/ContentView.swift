//
//  ContentView.swift
//  LightwaveOS
//
//  Root view with TabView + PersistentStatusBar.
//  Four main tabs: Play | Zones | Audio | Device.
//

import SwiftUI

struct ContentView: View {
    @Environment(AppViewModel.self) private var appVM
    @State private var selectedTab: AppTab = .play
    @State private var showConnectionSheet = false

    enum AppTab: String, CaseIterable, Identifiable {
        case play, zones, audio, device

        var id: String { rawValue }

        var title: String {
            switch self {
            case .play: return "Play"
            case .zones: return "Zones"
            case .audio: return "Audio"
            case .device: return "Device"
            }
        }

        var icon: String {
            switch self {
            case .play: return "play.circle.fill"
            case .zones: return "rectangle.split.3x1"
            case .audio: return "waveform"
            case .device: return "gearshape"
            }
        }
    }

    var body: some View {
        VStack(spacing: 0) {
            // Persistent status bar (always visible, above TabView)
            PersistentStatusBar(onTap: {
                showConnectionSheet = true
            })
            .padding(.horizontal, 8)

            // Main content area with TabView
            TabView(selection: $selectedTab) {
                PlayTab()
                    .tabItem {
                        Label(AppTab.play.title, systemImage: AppTab.play.icon)
                    }
                    .tag(AppTab.play)

                ZonesTab()
                    .tabItem {
                        Label(AppTab.zones.title, systemImage: AppTab.zones.icon)
                    }
                    .tag(AppTab.zones)

                AudioTab()
                    .tabItem {
                        Label(AppTab.audio.title, systemImage: AppTab.audio.icon)
                    }
                    .tag(AppTab.audio)

                DeviceTab()
                    .tabItem {
                        Label(AppTab.device.title, systemImage: AppTab.device.icon)
                    }
                    .tag(AppTab.device)
            }
            .tint(.lwGold)
        }
        .background(Color.lwBase)
        .sheet(isPresented: $showConnectionSheet) {
            ConnectionSheet()
        }
        .onAppear {
            // Show connection sheet if not connected
            if appVM.connectionState != .connected {
                showConnectionSheet = true
            }
        }
        .onChange(of: appVM.connectionState) { _, newState in
            // Auto-show connection sheet when disconnected
            if newState == .disconnected {
                showConnectionSheet = true
            } else if newState == .connected {
                showConnectionSheet = false
            }
        }
    }
}

// MARK: - Preview

#Preview("Content View") {
    ContentView()
        .environment(AppViewModel())
}
