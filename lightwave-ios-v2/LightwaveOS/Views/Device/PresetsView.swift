//
//  PresetsView.swift
//  LightwaveOS
//
//  Effect and zone preset management.
//
//  STUB: To be implemented with preset list and save/load functionality.
//

import SwiftUI

struct PresetsView: View {
    enum PresetType {
        case effect
        case zone
    }

    let type: PresetType

    var body: some View {
        List {
            Section {
                Text("No presets saved")
                    .font(.caption)
                    .foregroundStyle(Color.lwTextTertiary)
            }
            .listRowBackground(Color.lwCard)
        }
        .listStyle(.insetGrouped)
        .scrollContentBackground(.hidden)
        .background(Color.lwBase)
        .navigationTitle(type == .effect ? "Effect Presets" : "Zone Presets")
        .navigationBarTitleDisplayMode(.inline)
    }
}

// MARK: - Preview

#Preview("Presets - Effect") {
    NavigationStack {
        PresetsView(type: .effect)
    }
}

#Preview("Presets - Zone") {
    NavigationStack {
        PresetsView(type: .zone)
    }
}
