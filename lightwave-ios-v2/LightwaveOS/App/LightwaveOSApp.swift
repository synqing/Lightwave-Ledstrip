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
#if DEBUG
            let env = ProcessInfo.processInfo.environment
            if env["LW_EXPORT_DESIGN_PNGS"] == "1"
                || ProcessInfo.processInfo.arguments.contains("LW_EXPORT_DESIGN_PNGS") {
                DesignReferenceExportView()
                    .environment(appVM)
                    .environment(appVM.audio)
                    .preferredColorScheme(.dark)
            } else if env["LW_SHOW_RIVE_GALLERY"] == "1"
                || ProcessInfo.processInfo.arguments.contains("LW_SHOW_RIVE_GALLERY") {
                RiveReferenceGallery()
                    .environment(appVM)
                    .environment(appVM.audio)
                    .preferredColorScheme(.dark)
            } else {
                ContentView()
                    .environment(appVM)
                    .environment(appVM.audio)
                    .preferredColorScheme(.dark)
            }
#else
            ContentView()
                .environment(appVM)
                .environment(appVM.audio)
                .preferredColorScheme(.dark)
#endif
        }
    }
}
