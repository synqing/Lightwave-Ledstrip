//
//  PaletteViewModel.swift
//  LightwaveOS
//
//  Palette selection and management, loading from API with fallback to defaults.
//  iOS 17+, Swift 6 with @Observable @MainActor.
//

import Foundation
import Observation

@MainActor
@Observable
class PaletteViewModel {
    // MARK: - State

    var allPalettes: [PaletteMetadata] = PaletteStore.all
    var currentPaletteId: Int = 0
    var searchText: String = ""
    var selectedCategory: String = "All"

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
        guard let client = restClient else { return }

        do {
            let response = try await client.getPalettes(limit: 100)
            let fromAPI = response.data.palettes.map { p in
                PaletteMetadata(id: p.id, name: p.name, category: p.category ?? "Palette")
            }

            if !fromAPI.isEmpty {
                // Merge: API overrides, defaults as fallback
                var merged: [Int: PaletteMetadata] = [:]

                // Start with defaults
                for palette in PaletteStore.all {
                    merged[palette.id] = palette
                }

                // Override with API results
                for palette in fromAPI {
                    merged[palette.id] = palette
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
}
