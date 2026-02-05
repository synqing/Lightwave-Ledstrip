//
//  LightwaveOSApp.swift
//  LightwaveOS
//
//  App entry point for LightwaveOS v2.
//  Creates AppViewModel and injects it via environment.
//

import SwiftUI

@main
struct LightwaveOSApp: App {
    @State private var appVM = AppViewModel()
    @Environment(\.scenePhase) private var scenePhase

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environment(appVM)
                .environment(appVM.audio)
                .preferredColorScheme(ColorScheme.dark)
                .onChange(of: scenePhase) { oldPhase, newPhase in
                    handleScenePhaseChange(from: oldPhase, to: newPhase)
                }
        }
    }

    private func handleScenePhaseChange(from oldPhase: ScenePhase, to newPhase: ScenePhase) {
        switch newPhase {
        case .background:
            Task {
                await appVM.handleBackground()
            }

        case .active:
            if oldPhase == .background {
                Task {
                    await appVM.handleForeground()
                }
            }

        case .inactive:
            // Brief transition state — no action needed
            break

        @unknown default:
            break
        }
    }
}
