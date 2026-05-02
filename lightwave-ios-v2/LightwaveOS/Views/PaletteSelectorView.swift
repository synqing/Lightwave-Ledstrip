//
//  PaletteSelectorView.swift
//  LightwaveOS
//
//  3-column grid palette selector with sticky search bar and filter chips.
//  Matches ui-overhaul-mockup.html design spec.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import SwiftUI

struct PaletteSelectorView: View {
    @Environment(AppViewModel.self) private var appVM
    @Environment(\.dismiss) private var dismiss

    @State private var searchText = ""
    @State private var selectedFilter = "All"

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

        return palettes
    }

    private var groupedFilteredPalettes: [(category: String, palettes: [PaletteMetadata])] {
        let grouped = Dictionary(grouping: filteredPalettes) { $0.displayCategory }
        return grouped.sorted { $0.key < $1.key }
            .map { (category: $0.key, palettes: $0.value.sorted { $0.id < $1.id }) }
    }

    private func isSelected(_ palette: PaletteMetadata) -> Bool {
        palette.id == appVM.palettes.currentPaletteId
    }

    // MARK: - Methods

    private func selectPalette(_ palette: PaletteMetadata) {
        Task {
            await appVM.palettes.setPalette(id: palette.id)
            dismiss()
        }
    }
}

// MARK: - Filter Chip

struct FilterChip: View {
    let title: String
    let isSelected: Bool
    let onTap: () -> Void

    var body: some View {
        Button(action: onTap) {
            Text(title)
                .font(.pillLabel)
                .tracking(1.0)
                .foregroundStyle(isSelected ? Color.lwBase : Color.lwTextSecondary)
                .padding(.horizontal, 12)
                .padding(.vertical, 6)
                .background(isSelected ? Color.lwGold : Color.clear)
                .overlay(
                    Capsule()
                        .strokeBorder(isSelected ? Color.lwGold : Color.lwElevated, lineWidth: 1)
                )
                .clipShape(Capsule())
        }
        .buttonStyle(.plain)
        .accessibilityAddTraits(isSelected ? .isSelected : [])
    }
}

// MARK: - Palette Cell

struct PaletteCell: View {
    let palette: PaletteMetadata
    let isSelected: Bool
    let onTap: () -> Void

    var body: some View {
        Button(action: onTap) {
            VStack(spacing: 0) {
                // Gradient swatch
                PaletteGradientSwatch(colors: palette.gradientColors)
                    .frame(height: 40)
                    .frame(maxWidth: .infinity)

                // Palette name
                Text(palette.name)
                    .font(.microLabel)
                    .foregroundStyle(Color.lwTextSecondary)
                    .lineLimit(1)
                    .truncationMode(.tail)
                    .padding(.horizontal, 5)
                    .padding(.vertical, 6)
                    .frame(maxWidth: .infinity, alignment: .center)
            }
            .background(Color.lwCard)
            .clipShape(RoundedRectangle(cornerRadius: 10))
            .overlay(
                RoundedRectangle(cornerRadius: 10)
                    .strokeBorder(isSelected ? Color.lwGold : Color.clear, lineWidth: 2)
            )
            .shadow(
                color: isSelected ? Color.lwGold.opacity(0.3) : Color.clear,
                radius: isSelected ? 8 : 0,
                x: 0,
                y: 0
            )
        }
        .buttonStyle(.plain)
        .accessibilityLabel(Text(palette.name))
        .accessibilityHint(Text("Select palette"))
        .accessibilityAddTraits(isSelected ? .isSelected : [])
    }
}

// MARK: - Palette Gradient Swatch

struct PaletteGradientSwatch: View {
    let colors: [Color]

    var body: some View {
        if colors.count > 1 {
            LinearGradient(
                colors: colors,
                startPoint: .leading,
                endPoint: .trailing
            )
        } else {
            (colors.first ?? Color.gray)
        }
    }
}

// MARK: - Preview

#Preview("Palette Selector") {
    PaletteSelectorView()
        .environment({
            let vm = AppViewModel()
            // Create mock palettes with gradient colours for preview
            vm.palettes.allPalettes = [
                PaletteMetadata(
                    id: 0,
                    name: "Sunset Real",
                    category: "Artistic",
                    colors: [
                        PaletteColor(position: 0.0, r: 255, g: 0, b: 0),
                        PaletteColor(position: 0.5, r: 255, g: 128, b: 0),
                        PaletteColor(position: 1.0, r: 255, g: 255, b: 0)
                    ]
                ),
                PaletteMetadata(
                    id: 1,
                    name: "Rivendell",
                    category: "Artistic",
                    colors: [
                        PaletteColor(position: 0.0, r: 0, g: 100, b: 0),
                        PaletteColor(position: 0.5, r: 50, g: 150, b: 50),
                        PaletteColor(position: 1.0, r: 100, g: 200, b: 100)
                    ]
                ),
                PaletteMetadata(
                    id: 2,
                    name: "Ocean Breeze",
                    category: "Artistic",
                    colors: [
                        PaletteColor(position: 0.0, r: 0, g: 50, b: 100),
                        PaletteColor(position: 0.5, r: 0, g: 100, b: 200),
                        PaletteColor(position: 1.0, r: 100, g: 150, b: 255)
                    ]
                ),
                PaletteMetadata(
                    id: 33,
                    name: "Vik",
                    category: "Scientific",
                    colors: [
                        PaletteColor(position: 0.0, r: 0, g: 32, b: 64),
                        PaletteColor(position: 0.5, r: 128, g: 64, b: 32),
                        PaletteColor(position: 1.0, r: 255, g: 192, b: 128)
                    ]
                ),
                PaletteMetadata(
                    id: 34,
                    name: "Tokyo",
                    category: "Scientific",
                    colors: [
                        PaletteColor(position: 0.0, r: 100, g: 0, b: 100),
                        PaletteColor(position: 0.5, r: 150, g: 50, b: 150),
                        PaletteColor(position: 1.0, r: 200, g: 100, b: 200)
                    ]
                ),
                PaletteMetadata(
                    id: 57,
                    name: "Viridis",
                    category: "LGP-Optimised",
                    colors: [
                        PaletteColor(position: 0.0, r: 68, g: 1, b: 84),
                        PaletteColor(position: 0.5, r: 33, g: 145, b: 140),
                        PaletteColor(position: 1.0, r: 253, g: 231, b: 37)
                    ]
                ),
                PaletteMetadata(
                    id: 58,
                    name: "Plasma",
                    category: "LGP-Optimised",
                    colors: [
                        PaletteColor(position: 0.0, r: 13, g: 8, b: 135),
                        PaletteColor(position: 0.5, r: 180, g: 50, b: 130),
                        PaletteColor(position: 1.0, r: 240, g: 249, b: 33)
                    ]
                ),
                PaletteMetadata(
                    id: 59,
                    name: "Inferno",
                    category: "LGP-Optimised",
                    colors: [
                        PaletteColor(position: 0.0, r: 0, g: 0, b: 4),
                        PaletteColor(position: 0.5, r: 188, g: 55, b: 84),
                        PaletteColor(position: 1.0, r: 252, g: 255, b: 164)
                    ]
                )
            ]
            vm.palettes.currentPaletteId = 0
            return vm
        }())
}

#Preview("Palette Selector - Selected") {
    PaletteSelectorView()
        .environment({
            let vm = AppViewModel()
            vm.palettes.allPalettes = [
                PaletteMetadata(
                    id: 5,
                    name: "Analogous 1",
                    category: "Artistic",
                    colors: [
                        PaletteColor(position: 0.0, r: 255, g: 100, b: 0),
                        PaletteColor(position: 0.5, r: 255, g: 150, b: 50),
                        PaletteColor(position: 1.0, r: 255, g: 200, b: 100)
                    ]
                ),
                PaletteMetadata(
                    id: 33,
                    name: "Vik",
                    category: "Scientific",
                    colors: [
                        PaletteColor(position: 0.0, r: 0, g: 32, b: 64),
                        PaletteColor(position: 0.5, r: 128, g: 64, b: 32),
                        PaletteColor(position: 1.0, r: 255, g: 192, b: 128)
                    ]
                )
            ]
            vm.palettes.currentPaletteId = 5
            return vm
        }())
}
