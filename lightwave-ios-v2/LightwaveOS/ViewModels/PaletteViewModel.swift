//
//  PaletteViewModel.swift
//  LightwaveOS
//
//  Palette selection and management, loading from API with fallback to defaults.
//  Includes 12-slot global favourites (shared across all palette pickers) with undo support.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import Foundation
import Observation
import UniformTypeIdentifiers
import CoreTransferable

// MARK: - Drag Payload

/// Shared drag payload for palette drag-and-drop across all views
struct PaletteDragItem: Codable, Transferable, Sendable, Hashable {
    let paletteId: Int
    let sourceSlot: Int?  // nil if dragged from grid, slot index if from favourites

    static var transferRepresentation: some TransferRepresentation {
        CodableRepresentation(contentType: .paletteDragItem)
    }
}

extension UTType {
    static let paletteDragItem = UTType(exportedAs: "com.spectrasynq.lightwaveos.paletteDragItem")
}

// MARK: - Undo State

/// Snapshot for one-step undo of favourite mutations
struct FavouriteUndoSnapshot: Sendable {
    let id: UUID
    let label: String
    let slots: [Int?]
}

@MainActor
@Observable
class PaletteViewModel {
    // MARK: - Constants

    static let favouriteSlotCount = 12

    // MARK: - State

    var allPalettes: [PaletteMetadata] = PaletteStore.all
    var currentPaletteId: Int = 0
    var searchText: String = ""
    var selectedCategory: String = "All"

    // MARK: - Favourites (12 fixed slots, shared globally)

    /// 12 fixed favourite slots shared across ALL palette pickers. nil = empty slot, Int = palette ID
    var favouriteSlots: [Int?] = Array(repeating: nil, count: 12)

    // MARK: - Undo

    /// Current undo snapshot (one-step only)
    private(set) var undoSnapshot: FavouriteUndoSnapshot?

    // MARK: - Dependencies

    var restClient: RESTClient?

    // MARK: - Computed Properties

    var currentPaletteName: String {
        allPalettes.first(where: { $0.id == currentPaletteId })?.name ?? "Unknown"
    }

    /// All unique categories from loaded palettes
    var categories: [String] {
        let cats = Set(allPalettes.compactMap { $0.category })
        return ["All"] + cats.sorted()
    }

    /// Filtered palettes by category and search text
    func filteredPalettes(category: String? = nil, searchText: String? = nil) -> [PaletteMetadata] {
        var palettes = allPalettes

        // Filter by category
        let cat = category ?? selectedCategory
        if cat != "All" {
            palettes = palettes.filter { $0.category == cat }
        }

        // Filter by search text
        let search = searchText ?? self.searchText
        if !search.isEmpty {
            palettes = palettes.filter {
                $0.name.localizedCaseInsensitiveContains(search) ||
                ($0.category ?? "").localizedCaseInsensitiveContains(search)
            }
        }

        return palettes
    }

    /// Grouped palettes by category
    var groupedPalettes: [(String, [PaletteMetadata])] {
        let filtered = filteredPalettes()
        let grouped = Dictionary(grouping: filtered) { $0.category ?? "Uncategorised" }
        return grouped.sorted { $0.key < $1.key }
            .map { ($0.key, $0.value.sorted { $0.name < $1.name }) }
    }

    // MARK: - API Methods

    func loadPalettes() async {
        // Load persisted favourites first
        loadFavourites()

        guard let client = restClient else {
            // No client - use defaults and reconcile
            self.allPalettes = PaletteStore.all
            reconcileFavourites()
            return
        }

        do {
            let response = try await client.getPalettes(limit: 100)
            let fromAPI = response.data.palettes.map { p in
                PaletteMetadata(
                    id: p.id,
                    name: p.name,
                    // DON'T default to "Palette" here - let merge handle it
                    category: p.category,
                    colors: nil,
                    flags: PaletteFlags(
                        warm: p.flags?.warm,
                        cool: p.flags?.cool,
                        calm: p.flags?.calm,
                        vivid: p.flags?.vivid,
                        cvdFriendly: p.flags?.cvdFriendly,
                        whiteHeavy: p.flags?.whiteHeavy
                    ),
                    avgBrightness: p.avgBrightness,
                    maxBrightness: p.maxBrightness
                )
            }

            if !fromAPI.isEmpty {
                // Merge: API overrides, defaults as fallback
                var merged: [Int: PaletteMetadata] = [:]

                // Start with defaults (which have colours from Palettes_Master.json)
                for palette in PaletteStore.all {
                    merged[palette.id] = palette
                }

                // Override with API results, but preserve palette colours from defaults
                for palette in fromAPI {
                    let base = merged[palette.id]
                    merged[palette.id] = PaletteMetadata(
                        id: palette.id,
                        name: palette.name,
                        // Use API category if present, else base category, else "Palette"
                        category: palette.category ?? base?.category ?? "Palette",
                        colors: base?.colors,
                        flags: palette.flags ?? base?.flags,
                        avgBrightness: palette.avgBrightness ?? base?.avgBrightness,
                        maxBrightness: palette.maxBrightness ?? base?.maxBrightness
                    )
                }

                self.allPalettes = merged.values.sorted { $0.id < $1.id }
                print("Loaded \(allPalettes.count) palettes (API + defaults)")
            } else {
                // Fallback to defaults
                self.allPalettes = PaletteStore.all
                print("Using fallback palettes")
            }

        } catch {
            print("Palette load failed, using fallback: \(error)")
            self.allPalettes = PaletteStore.all
        }

        // Reconcile favourites after palettes are loaded
        reconcileFavourites()
    }

    func setPalette(id: Int) async {
        guard let client = restClient else { return }

        do {
            try await client.setPalette(id)

            // Optimistic update
            self.currentPaletteId = id
            print("Set palette to \(id): \(currentPaletteName)")

        } catch {
            print("Error setting palette: \(error)")
        }
    }

    func nextPalette() async {
        guard !allPalettes.isEmpty else { return }

        let currentIndex = allPalettes.firstIndex(where: { $0.id == currentPaletteId }) ?? -1
        let nextIndex = (currentIndex + 1) % allPalettes.count
        let nextPalette = allPalettes[nextIndex]

        await setPalette(id: nextPalette.id)
    }

    func previousPalette() async {
        guard !allPalettes.isEmpty else { return }

        let currentIndex = allPalettes.firstIndex(where: { $0.id == currentPaletteId }) ?? 0
        let prevIndex = currentIndex > 0 ? currentIndex - 1 : allPalettes.count - 1
        let prevPalette = allPalettes[prevIndex]

        await setPalette(id: prevPalette.id)
    }

    // MARK: - Favourites Mutations

    /// Save current state for undo before any mutation
    private func saveUndoSnapshot(label: String) {
        undoSnapshot = FavouriteUndoSnapshot(
            id: UUID(),
            label: label,
            slots: favouriteSlots
        )
    }

    /// Clear undo buffer (after toast dismissal or timeout)
    func clearFavouriteUndo() {
        undoSnapshot = nil
    }

    /// Restore from undo snapshot
    func undoFavouriteChange() {
        guard let snapshot = undoSnapshot else { return }
        favouriteSlots = snapshot.slots
        persistFavourites()
        undoSnapshot = nil
    }

    /// Pin palette to a specific slot (empty slot)
    /// - Returns: Undo label if mutation occurred
    @discardableResult
    func pinToSlot(paletteId: Int, slot: Int) -> String? {
        guard slot >= 0, slot < Self.favouriteSlotCount else { return nil }

        // Check for duplicate - if already in favourites, this becomes a move
        if let existingSlot = favouriteSlots.firstIndex(where: { $0 == paletteId }) {
            return swapSlots(from: existingSlot, to: slot)
        }

        saveUndoSnapshot(label: "Pin \(paletteName(for: paletteId))")
        favouriteSlots[slot] = paletteId
        persistFavourites()
        return undoSnapshot?.label
    }

    /// Replace palette in filled slot
    /// - Returns: Undo label if mutation occurred
    @discardableResult
    func replaceSlot(paletteId: Int, slot: Int) -> String? {
        guard slot >= 0, slot < Self.favouriteSlotCount else { return nil }

        // Check for duplicate - if already in favourites at different slot, swap instead
        if let existingSlot = favouriteSlots.firstIndex(where: { $0 == paletteId }), existingSlot != slot {
            return swapSlots(from: existingSlot, to: slot)
        }

        let replacedName = favouriteSlots[slot].flatMap { paletteName(for: $0) } ?? "slot"
        saveUndoSnapshot(label: "Replace \(replacedName)")
        favouriteSlots[slot] = paletteId
        persistFavourites()
        return undoSnapshot?.label
    }

    /// Swap two favourite slots (reorder within favourites)
    /// - Returns: Undo label if mutation occurred
    @discardableResult
    func swapSlots(from: Int, to: Int) -> String? {
        guard from >= 0, from < Self.favouriteSlotCount,
              to >= 0, to < Self.favouriteSlotCount,
              from != to else { return nil }

        saveUndoSnapshot(label: "Move favourite")
        let temp = favouriteSlots[from]
        favouriteSlots[from] = favouriteSlots[to]
        favouriteSlots[to] = temp
        persistFavourites()
        return undoSnapshot?.label
    }

    /// Remove palette from favourites
    /// - Returns: Undo label if mutation occurred
    @discardableResult
    func removeFromFavourites(slot: Int) -> String? {
        guard slot >= 0, slot < Self.favouriteSlotCount,
              let paletteId = favouriteSlots[slot] else { return nil }

        saveUndoSnapshot(label: "Remove \(paletteName(for: paletteId))")
        favouriteSlots[slot] = nil
        persistFavourites()
        return undoSnapshot?.label
    }

    /// Handle drop from grid or favourites onto a favourite slot
    /// - Returns: Undo label if mutation occurred
    @discardableResult
    func handleDrop(item: PaletteDragItem, ontoSlot: Int) -> String? {
        // If dragged from favourites, this is a swap/move
        if let sourceSlot = item.sourceSlot {
            return swapSlots(from: sourceSlot, to: ontoSlot)
        }

        // Dragged from grid - pin or replace
        if favouriteSlots[ontoSlot] == nil {
            return pinToSlot(paletteId: item.paletteId, slot: ontoSlot)
        } else {
            return replaceSlot(paletteId: item.paletteId, slot: ontoSlot)
        }
    }

    /// Find first empty slot, or nil if all slots filled
    func firstEmptySlot() -> Int? {
        favouriteSlots.firstIndex(where: { $0 == nil })
    }

    // MARK: - Reconciliation

    /// Called after loadPalettes() to nil out invalid favourite IDs
    func reconcileFavourites() {
        let validIds = Set(allPalettes.map { $0.id })
        var changed = false

        for i in 0..<favouriteSlots.count {
            if let id = favouriteSlots[i], !validIds.contains(id) {
                favouriteSlots[i] = nil
                changed = true
            }
        }

        if changed {
            persistFavourites()
            print("Reconciled favourites: removed invalid palette IDs")
        }
    }

    // MARK: - Persistence

    private static let favouritesKey = "palette_favourites"

    func loadFavourites() {
        if let data = UserDefaults.standard.data(forKey: Self.favouritesKey),
           let decoded = try? JSONDecoder().decode([Int?].self, from: data) {
            favouriteSlots = decoded.count == Self.favouriteSlotCount
                ? decoded
                : Array(repeating: nil, count: Self.favouriteSlotCount)
        }
    }

    func persistFavourites() {
        if let data = try? JSONEncoder().encode(favouriteSlots) {
            UserDefaults.standard.set(data, forKey: Self.favouritesKey)
        }
    }

    // MARK: - Helpers

    func paletteName(for id: Int) -> String {
        allPalettes.first(where: { $0.id == id })?.name ?? "Palette \(id)"
    }

    func palette(for id: Int) -> PaletteMetadata? {
        allPalettes.first(where: { $0.id == id })
    }

    /// Check if palette is in favourites
    func isInFavourites(_ paletteId: Int) -> Bool {
        favouriteSlots.contains(paletteId)
    }

    /// Reset state on disconnect
    func disconnect() {
        allPalettes = PaletteStore.all
        currentPaletteId = 0
        // Preserve favourites across disconnects
    }
}
