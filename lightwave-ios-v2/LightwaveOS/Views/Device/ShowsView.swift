//
//  ShowsView.swift
//  LightwaveOS
//
//  Show playback and management.
//
//  STUB: To be implemented with show list and playback controls.
//

import SwiftUI

struct ShowsView: View {
    var body: some View {
        List {
            Section {
                Text("No shows available")
                    .font(.caption)
                    .foregroundStyle(Color.lwTextTertiary)
            }
            .listRowBackground(Color.lwCard)
        }
        .listStyle(.insetGrouped)
        .scrollContentBackground(.hidden)
        .background(Color.lwBase)
        .navigationTitle("Shows")
        .navigationBarTitleDisplayMode(.inline)
    }
}

// MARK: - Preview

#Preview("Shows View") {
    NavigationStack {
        ShowsView()
    }
}
