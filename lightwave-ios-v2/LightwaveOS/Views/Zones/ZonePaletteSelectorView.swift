//
//  ZonePaletteSelectorView.swift
//  LightwaveOS
//
//  Zone-scoped palette selector matching the main palette grid layout.
//

import SwiftUI

struct ZonePaletteSelectorView: View {
    @Environment(AppViewModel.self) private var appVM
    @Environment(\.dismiss) private var dismiss

    let zoneId: Int

    @State private var searchText = ""
    @State private var selectedFilter = "All"
    @State private var selectedFlags: Set<PaletteFlagKey> = []

    var body: some View {
        NavigationStack {
            ZStack {
                Color.lwBase.ignoresSafeArea()

                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 0, pinnedViews: [.sectionHeaders]) {
                        // Sticky header section
                        Section {
                            // Grid content
                            gridContent
                        } header: {
                            stickyHeader
                        }
                    }
                }
            }
            .navigationTitle("SELECT PALETTE")
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
    }

    // MARK: - Sticky Header

    private var stickyHeader: some View {
        VStack(spacing: 12) {
            HStack {
                ZoneSelectorChip(title: "ZONE \(zoneId + 1)", colour: Color.zoneColor(zoneId))
                Spacer()
            }
            .padding(.bottom, 4)

            // Search bar
            HStack {
                Image(systemName: "magnifyingglass")
                    .font(.iconSmall)
                    .foregroundStyle(Color.lwTextSecondary)

                TextField("Search palettes...", text: $searchText)
                    .font(.hapticLabel)
                    .foregroundStyle(Color.lwTextPrimary)
                    .autocorrectionDisabled()

                if !searchText.isEmpty {
                    Button(action: { searchText = "" }) {
                        Image(systemName: "xmark.circle.fill")
                            .font(.iconSmall)
                            .foregroundStyle(Color.lwTextTertiary)
                    }
                }
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 10)
            .background(Color.lwCard)
            .overlay(
                RoundedRectangle(cornerRadius: 12)
                    .strokeBorder(searchText.isEmpty ? Color.lwElevated : Color.lwGold, lineWidth: 1)
            )
            .clipShape(RoundedRectangle(cornerRadius: 12))

            // Filter chips
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 6) {
                    FilterChip(title: "All", isSelected: selectedFilter == "All") {
                        selectedFilter = "All"
                    }

                    ForEach(uniqueCategories, id: \.self) { category in
                        FilterChip(title: category, isSelected: selectedFilter == category) {
                            selectedFilter = category
                        }
                    }
                }
            }

            // Flag chips
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 6) {
                    if !selectedFlags.isEmpty {
                        FlagChip(title: "Clear flags", count: nil, isSelected: false) {
                            selectedFlags.removeAll()
                        }
                    }
                    ForEach(PaletteFlagKey.allCases) { flag in
                        FlagChip(
                            title: flag.label,
                            count: countForFlag(flag),
                            isSelected: selectedFlags.contains(flag)
                        ) {
                            toggleFlag(flag)
                        }
                    }
                }
            }
        }
        .padding(.horizontal, 16)
        .padding(.top, 12)
        .padding(.bottom, 16)
        .background(Color.lwBase)
    }

    // MARK: - Grid Content

    private var gridContent: some View {
        VStack(alignment: .leading, spacing: 12) {
            ForEach(groupedFilteredPalettes, id: \.category) { group in
                VStack(alignment: .leading, spacing: 4) {
                    // Category header
                    Text(group.category.uppercased())
                        .font(.subGroupHeader)
                        .foregroundStyle(Color.lwTextSecondary)
                        .tracking(1.5)
                        .padding(.horizontal, 16)
                        .padding(.top, 12)
                        .padding(.bottom, 4)

                    // 3-column grid
                    LazyVGrid(
                        columns: [
                            GridItem(.flexible()),
                            GridItem(.flexible()),
                            GridItem(.flexible())
                        ],
                        spacing: 8
                    ) {
                        ForEach(group.palettes) { palette in
                            PaletteCell(
                                palette: palette,
                                isSelected: isSelected(palette)
                            ) {
                                selectPalette(palette)
                            }
                        }
                    }
                    .padding(.horizontal, 16)
                }
            }

            // Bottom padding
            Spacer(minLength: 16)
        }
    }

    // MARK: - Computed Properties

    private var allPalettes: [PaletteMetadata] {
        appVM.palettes.allPalettes
    }

    private var uniqueCategories: [String] {
        let categories = Set(allPalettes.compactMap { $0.category })
        return categories.sorted()
    }

    private var filteredPalettes: [PaletteMetadata] {
        var palettes = allPalettes

        // Filter by search text
        if !searchText.isEmpty {
            palettes = palettes.filter {
                $0.name.localizedCaseInsensitiveContains(searchText)
            }
        }

        // Filter by category
        if selectedFilter != "All" {
            palettes = palettes.filter { $0.category == selectedFilter }
        }

        // Filter by flags (all selected flags must match)
        if !selectedFlags.isEmpty {
            palettes = palettes.filter { palette in
                selectedFlags.allSatisfy { palette.hasFlag($0) }
            }
        }

        return palettes
    }

    private var groupedFilteredPalettes: [(category: String, palettes: [PaletteMetadata])] {
        let grouped = Dictionary(grouping: filteredPalettes) { $0.displayCategory }
        return grouped.sorted { $0.key < $1.key }
            .map { (category: $0.key, palettes: $0.value.sorted { $0.id < $1.id }) }
    }

    private func isSelected(_ palette: PaletteMetadata) -> Bool {
        palette.id == currentPaletteId
    }

    private var currentPaletteId: Int? {
        appVM.zones.zones.first(where: { $0.id == zoneId })?.paletteId
    }

    // MARK: - Methods

    private func selectPalette(_ palette: PaletteMetadata) {
        Task {
            await appVM.zones.setZonePalette(
                zoneId: zoneId,
                paletteId: palette.id,
                paletteName: palette.name
            )
            dismiss()
        }
    }

    private func toggleFlag(_ flag: PaletteFlagKey) {
        if selectedFlags.contains(flag) {
            selectedFlags.remove(flag)
        } else {
            selectedFlags.insert(flag)
        }
    }

    /// Count of palettes that have this flag among the current search+category+flags filtered set
    private func countForFlag(_ flag: PaletteFlagKey) -> Int {
        filteredPalettes.filter { $0.hasFlag(flag) }.count
    }
}

// MARK: - Zone Chip

private struct ZoneSelectorChip: View {
    let title: String
    let colour: Color

    var body: some View {
        Text(title)
            .font(.subGroupHeader)
            .tracking(1.2)
            .foregroundStyle(colour)
            .padding(.horizontal, 12)
            .padding(.vertical, 6)
            .background(
                Capsule()
                    .fill(Color.lwCard)
                    .overlay(
                        Capsule()
                            .strokeBorder(colour.opacity(0.8), lineWidth: 1.5)
                    )
            )
    }
}
