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

    @State private var isSharePresented = false

    private var logText: String {
        appVM.debugLog
            .map { "[\($0.category)] \(formatFullTimestamp($0.timestamp)): \($0.message)" }
            .joined(separator: "\n")
    }

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
                Button {
                    isSharePresented = true
                } label: {
                    Image(systemName: "square.and.arrow.up")
                        .foregroundStyle(Color.lwGold)
                }
            }
        }
        .sheet(isPresented: $isSharePresented) {
            ShareView(activityItems: [exportFileURL()])
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

    private func formatFullTimestamp(_ date: Date) -> String {
        let formatter = ISO8601DateFormatter()
        formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        return formatter.string(from: date)
    }

    private func exportFileURL() -> URL {
        let filename = "lightwaveos-debuglog-\(Date().ISO8601Format()).txt"
        let url = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent(filename)

        do {
            try logText.write(to: url, atomically: true, encoding: .utf8)
        } catch {
        }

        return url
    }
}

private struct ShareView: UIViewControllerRepresentable {
    let activityItems: [Any]

    func makeUIViewController(context: Context) -> UIActivityViewController {
        UIActivityViewController(activityItems: activityItems, applicationActivities: nil)
    }

    func updateUIViewController(_ uiViewController: UIActivityViewController, context: Context) {}
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
