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

    // MARK: - Lazy hydration (heap-stability mitigation)
    //
    // The palette list (limit=75) returns ~9 KB of JSON which, layered on top
    // of the effects fetch and other connect-time GETs, contributed to the
    // K1 V2 internal-heap fragmentation that latched heap-shedding. We now
    // skip the network round-trip on connect and hydrate lazily the first
    // time `loadPalettes()` is invoked from a UI surface — i.e. when the
    // user opens the palette picker. The default `PaletteStore.all` covers
    // every palette name and gradient at app start, so the picker has
    // something to render before the network call resolves.

    /// Set to `true` after the first network hydration completes (success or
    /// failure). Subsequent calls to `loadPalettes()` become no-ops, preventing
    /// the picker from re-fetching every time it's opened.
    @ObservationIgnored
    private var hasHydrated = false

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

    /// Hydrate the palette catalogue from the firmware. Called lazily by the
    /// palette picker the first time it is opened — NOT from the connect path
    /// (heap-stability mitigation, see `hasHydrated` above).
    ///
    /// Idempotent: subsequent invocations after a successful or failed
    /// hydration become no-ops, so re-opening the picker does not repeatedly
    /// hit the firmware. Callers that genuinely need a fresh fetch (e.g. a
    /// "refresh palettes" admin button) should use `forceReloadPalettes()`.
    func loadPalettes() async {
        guard !hasHydrated else { return }
        await performPaletteFetch()
    }

    /// Force a re-hydration ignoring `hasHydrated`. Reserved for explicit user
    /// gestures (pull-to-refresh, admin reload). Normal UI paths must use
    /// `loadPalettes()`.
    func forceReloadPalettes() async {
        await performPaletteFetch()
    }

    /// Internal: perform the actual REST fetch and merge it into the local
    /// catalogue. Updates `hasHydrated` so future `loadPalettes()` calls
    /// short-circuit.
    private func performPaletteFetch() async {
        guard let client = restClient else { return }

        // Mark the hydration as having taken place even before the response
        // returns — a second concurrent call entering through a different
        // call site must not double-fetch.
        hasHydrated = true

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

                // Override with API results, but preserve palette colours from defaults
                for palette in fromAPI {
                    let base = merged[palette.id]
                    merged[palette.id] = PaletteMetadata(
                        id: palette.id,
                        name: palette.name,
                        category: palette.category ?? base?.category,
                        colors: base?.colors
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
            // On failure clear `hasHydrated` so the picker retries on its next
            // open. The first connect-time call is suppressed by AppViewModel,
            // so the user-facing latency is bounded to a single picker-open
            // event.
            hasHydrated = false
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
