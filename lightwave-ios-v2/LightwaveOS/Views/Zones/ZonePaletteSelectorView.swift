//
//  ZonePaletteSelectorView.swift
//  LightwaveOS
//
//  Zone-scoped palette selector using SHARED global 12-slot favourites.
//

import SwiftUI
import UniformTypeIdentifiers

struct ZonePaletteSelectorView: View {
    @Environment(AppViewModel.self) private var appVM
    @Environment(\.dismiss) private var dismiss

    let zoneId: Int

    @State private var searchText = ""
    @State private var selectedFilter = "All"
    @State private var selectedFlags: Set<PaletteFlagKey> = []

    // Undo toast state
    @State private var undoToastLabel: String?
    @State private var undoToastTask: Task<Void, Never>?

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

                // Undo toast overlay
                if let label = undoToastLabel {
                    VStack {
                        Spacer()
                        UndoToast(label: label) {
                            appVM.palettes.undoFavouriteChange()
                            dismissUndoToast()
                        } onDismiss: {
                            dismissUndoToast()
                        }
                        .padding(.bottom, 20)
                    }
                    .transition(.move(edge: .bottom).combined(with: .opacity))
                    .animation(.spring(duration: 0.35, bounce: 0.1), value: undoToastLabel)
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

    private func showUndoToast(label: String) {
        undoToastTask?.cancel()
        undoToastLabel = label
        undoToastTask = Task { @MainActor in
            try? await Task.sleep(nanoseconds: 4_000_000_000)
            guard !Task.isCancelled else { return }
            dismissUndoToast()
        }
    }

    private func dismissUndoToast() {
        undoToastTask?.cancel()
        undoToastLabel = nil
        appVM.palettes.clearFavouriteUndo()
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

            // MARK: SHARED Favourites Grid (3×4 = 12 slots)
            favouritesGrid
        }
        .padding(.horizontal, 16)
        .padding(.top, 12)
        .padding(.bottom, 16)
        .background(Color.lwBase)
    }

    // MARK: - Favourites Grid (SHARED - same as main palette selector)

    private var favouritesGrid: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("FAVOURITES")
                .font(.subGroupHeader)
                .foregroundStyle(Color.lwTextSecondary)
                .tracking(1.5)

            // 3 columns × 4 rows = 12 slots
            LazyVGrid(
                columns: [
                    GridItem(.flexible(), spacing: 8),
                    GridItem(.flexible(), spacing: 8),
                    GridItem(.flexible(), spacing: 8)
                ],
                spacing: 8
            ) {
                ForEach(0..<12, id: \.self) { slot in
                    ZoneFavouriteSlotCell(
                        zoneId: zoneId,
                        slot: slot,
                        paletteId: appVM.palettes.favouriteSlots[slot],
                        palette: appVM.palettes.favouriteSlots[slot].flatMap { appVM.palettes.palette(for: $0) },
                        isCurrentPalette: appVM.palettes.favouriteSlots[slot] == currentPaletteId
                    ) { palette in
                        // Tap to select for this zone
                        selectPalette(palette)
                    } onDrop: { item in
                        if let label = appVM.palettes.handleDrop(item: item, ontoSlot: slot) {
                            showUndoToast(label: label)
                        }
                    } onRemove: {
                        if let label = appVM.palettes.removeFromFavourites(slot: slot) {
                            showUndoToast(label: label)
                        }
                    }
                }
            }
        }
        .padding(.top, 8)
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
                            ZonePaletteCell(
                                zoneId: zoneId,
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

// MARK: - Zone Favourite Slot Cell (uses shared global favourites)

struct ZoneFavouriteSlotCell: View {
    let zoneId: Int
    let slot: Int
    let paletteId: Int?
    let palette: PaletteMetadata?
    let isCurrentPalette: Bool
    let onSelect: (PaletteMetadata) -> Void
    let onDrop: (PaletteDragItem) -> Void
    let onRemove: () -> Void

    var body: some View {
        Group {
            if let palette {
                // Filled slot - draggable
                filledSlotView(palette: palette)
            } else {
                // Empty slot - drop target only
                emptySlotView
            }
        }
        .dropDestination(for: PaletteDragItem.self) { items, _ in
            guard let item = items.first else { return false }
            onDrop(item)
            return true
        }
    }

    private func filledSlotView(palette: PaletteMetadata) -> some View {
        VStack(spacing: 0) {
            PaletteGradientSwatch(colors: palette.gradientColors)
                .frame(height: 28)
                .frame(maxWidth: .infinity)

            Text(palette.shortName)
                .font(.system(size: 9, weight: .medium))
                .foregroundStyle(Color.lwTextSecondary)
                .lineLimit(1)
                .truncationMode(.tail)
                .padding(.horizontal, 4)
                .padding(.vertical, 4)
                .frame(maxWidth: .infinity, alignment: .center)
        }
        .background(Color.lwCard)
        .clipShape(RoundedRectangle(cornerRadius: 8))
        .overlay(
            RoundedRectangle(cornerRadius: 8)
                .strokeBorder(isCurrentPalette ? Color.zoneColor(zoneId) : Color.lwElevated, lineWidth: isCurrentPalette ? 2 : 1)
        )
        .shadow(
            color: isCurrentPalette ? Color.zoneColor(zoneId).opacity(0.3) : Color.clear,
            radius: isCurrentPalette ? 6 : 0
        )
        // Remove button as overlay - separate hit target
        .overlay(alignment: .topTrailing) {
            Button(role: .destructive) {
                onRemove()
            } label: {
                Image(systemName: "xmark.circle.fill")
                    .font(.caption)
                    .foregroundStyle(Color.lwTextTertiary)
                    .frame(width: 24, height: 24)
                    .background(
                        Circle()
                            .fill(Color.lwCard.opacity(0.8))
                    )
                    .contentShape(Circle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel(Text("Remove favourite slot \(slot + 1)"))
        }
        // Tap to select for zone
        .onTapGesture { onSelect(palette) }
        // Drag from this slot
        .contentShape(.dragPreview, RoundedRectangle(cornerRadius: 8))
        .draggable(PaletteDragItem(paletteId: palette.id, sourceSlot: slot))
    }

    private var emptySlotView: some View {
        RoundedRectangle(cornerRadius: 8)
            .strokeBorder(Color.lwElevated.opacity(0.5), style: StrokeStyle(lineWidth: 1, dash: [4, 3]))
            .frame(height: 48)
            .background(Color.lwCard.opacity(0.3))
            .clipShape(RoundedRectangle(cornerRadius: 8))
            .overlay {
                Image(systemName: "plus")
                    .font(.system(size: 12, weight: .medium))
                    .foregroundStyle(Color.lwTextTertiary.opacity(0.5))
            }
    }
}

// MARK: - Zone Palette Cell (Draggable)

struct ZonePaletteCell: View {
    @Environment(AppViewModel.self) private var appVM

    let zoneId: Int
    let palette: PaletteMetadata
    let isSelected: Bool
    let onTap: () -> Void

    var body: some View {
        // Outer container for dragging - NO competing gestures
        VStack(spacing: 0) {
            // Gradient swatch - primary drag handle
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
                .strokeBorder(isSelected ? Color.zoneColor(zoneId) : Color.clear, lineWidth: 2)
        )
        .shadow(
            color: isSelected ? Color.zoneColor(zoneId).opacity(0.3) : Color.clear,
            radius: isSelected ? 8 : 0,
            x: 0,
            y: 0
        )
        // Menu button as overlay - separate hit target
        .overlay(alignment: .topTrailing) {
            Menu {
                if let emptySlot = appVM.palettes.firstEmptySlot() {
                    Button {
                        _ = appVM.palettes.pinToSlot(paletteId: palette.id, slot: emptySlot)
                    } label: {
                        Label("Add to Favourites", systemImage: "star")
                    }
                }

                Menu {
                    ForEach(0..<12, id: \.self) { slot in
                        Button {
                            if appVM.palettes.favouriteSlots[slot] == nil {
                                _ = appVM.palettes.pinToSlot(paletteId: palette.id, slot: slot)
                            } else {
                                _ = appVM.palettes.replaceSlot(paletteId: palette.id, slot: slot)
                            }
                        } label: {
                            let slotLabel = "Slot \(slot + 1)"
                            if let current = appVM.palettes.favouriteSlots[slot] {
                                Text("\(slotLabel): \(appVM.palettes.paletteName(for: current))")
                            } else {
                                Text("\(slotLabel): Empty")
                            }
                        }
                    }
                } label: {
                    Label("Pin to Slot…", systemImage: "square.grid.3x4")
                }

                Divider()

                Button {
                    onTap()
                } label: {
                    Label("Select for Zone", systemImage: "checkmark.circle")
                }
            } label: {
                Image(systemName: "ellipsis.circle")
                    .font(.iconSmall)
                    .foregroundStyle(Color.lwTextTertiary)
                    .frame(width: 32, height: 32)
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
        }
        // Tap gesture for selection
        .onTapGesture { onTap() }
        // Drag uses the whole cell shape
        .contentShape(.dragPreview, RoundedRectangle(cornerRadius: 10))
        .draggable(PaletteDragItem(paletteId: palette.id, sourceSlot: nil))
        .accessibilityLabel(Text(palette.name))
        .accessibilityHint(Text("Tap to select, or drag to favourites"))
        .accessibilityAddTraits(isSelected ? .isSelected : [])
    }
}
