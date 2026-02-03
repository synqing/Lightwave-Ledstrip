//
//  DebugLogView.swift
//  LightwaveOS
//
//  Full-screen debug log viewer with colour-coded categories.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import SwiftUI

struct DebugLogView: View {
    @Environment(AppViewModel.self) private var appVM

    var body: some View {
        Group {
            if appVM.debugLog.isEmpty {
                VStack {
                    Spacer()
                    Text("No log entries")
                        .font(.bodyValue)
                        .foregroundStyle(Color.lwTextTertiary)
                    Spacer()
                }
            } else {
                ScrollViewReader { proxy in
                    ScrollView {
                        LazyVStack(alignment: .leading, spacing: 2) {
                            ForEach(appVM.debugLog) { entry in
                                logEntryView(entry)
                                    .id(entry.id)
                            }
                        }
                        .padding(Spacing.md)
                    }
                    .onChange(of: appVM.debugLog.count) { _, _ in
                        if let last = appVM.debugLog.last {
                            withAnimation {
                                proxy.scrollTo(last.id, anchor: .bottom)
                            }
                        }
                    }
                }
            }
        }
        .background(Color.lwBase)
        .navigationTitle("Debug Log")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button("Clear") { appVM.clearLog() }
                    .foregroundStyle(Color.lwGold)
            }
        }
    }

    // MARK: - Log Entry View

    private func logEntryView(_ entry: DebugEntry) -> some View {
        Text("[\(entry.category)] \(entry.message)")
            .font(.monospace)
            .foregroundStyle(categoryColor(entry.category))
    }

    // MARK: - Category Colour Mapping

    private func categoryColor(_ category: String) -> Color {
        switch category {
        case "ERROR": return .lwError
        case "CONN": return .lwGold
        case "WS": return .lwCyan
        default: return .lwTextSecondary
        }
    }
}

// MARK: - Preview

#Preview("Debug Log View - Empty") {
    NavigationStack {
        DebugLogView()
            .environment(AppViewModel())
    }
}

#Preview("Debug Log View - With Entries") {
    let appVM = AppViewModel()
    appVM.debugLog = [
        DebugEntry(timestamp: Date(), category: "INFO", message: "App launched"),
        DebugEntry(timestamp: Date(), category: "CONN", message: "Searching for devices..."),
        DebugEntry(timestamp: Date(), category: "CONN", message: "Device found: 192.168.1.101"),
        DebugEntry(timestamp: Date(), category: "WS", message: "WebSocket connected"),
        DebugEntry(timestamp: Date(), category: "ERROR", message: "Connection timeout")
    ]

    return NavigationStack {
        DebugLogView()
            .environment(appVM)
    }
}
