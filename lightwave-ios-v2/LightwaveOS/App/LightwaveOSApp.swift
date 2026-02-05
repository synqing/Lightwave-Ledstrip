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

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environment(appVM)
                .environment(appVM.audio)
                .preferredColorScheme(ColorScheme.dark)
        }
    }
}
