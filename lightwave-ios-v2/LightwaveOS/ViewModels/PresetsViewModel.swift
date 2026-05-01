//
//  PresetsViewModel.swift
//  LightwaveOS
//
//  Phase 2 (P2-3) — Effect and zone preset CRUD surface.
//
//  Reads (list) go via REST so the VM gets the firmware's authoritative
//  list shape. Mutations (saveCurrent / load / delete) go via WebSocket so
//  the firmware's broadcast confirmations (`effectPresets.saved`,
//  `effectPresets.deleted`, `zonePresets.saved`, `zonePresets.deleted`)
//  drive every connected client to refresh in lockstep.
//
//  iOS 17+, Swift 6 with `@MainActor @Observable`. British English comments.
//

import Foundation
import Observation

// MARK: - Test seam

/// Subset of `WebSocketService.send(_:params:)` that the preset VM dispatches
/// against. Production code passes the live `WebSocketService` actor; tests
/// supply a recording double. Defined here (not in `WebSocketService.swift`)
/// to keep the actor surface area minimal — see the `// MARK: Phase 2 —
/// preset CRUD WS commands` block in WebSocketService.swift for the
/// conformance.
///
/// The parameter map is `[String: any Sendable]` (not `[String: Any]`) so the
/// type checker can prove the dictionary is safe to cross actor boundaries
/// under Swift 6 strict concurrency. All preset-command parameters are
/// `Int` / `String` and trivially conform to `Sendable`.
@available(iOS 17.0, *)
protocol PresetWebSocketCommanding: Sendable {
    /// Dispatch a WebSocket command with optional parameter map.
    func sendCommand(_ command: String, params: [String: any Sendable]) async
}

// MARK: - PresetsViewModel

@available(iOS 17.0, *)
@MainActor
@Observable
final class PresetsViewModel {

    // MARK: - Public state

    /// Latest list of user-saved effect presets fetched from
    /// `GET /api/v1/effect-presets`.
    var effectPresets: [EffectPresetSummary] = []

    /// Latest list of zone presets (built-in plus user-saved) fetched from
    /// `GET /api/v1/zone-presets`.
    var zonePresets: [ZonePresetSummary] = []

    /// `true` whilst a list-load is in flight. UI binds a spinner to this.
    var loading: Bool = false

    /// Most recent error string, or `nil` after a successful fetch / mutation.
    var error: String?

    // MARK: - REST list-load

    /// Fetch the full effect-preset list. Updates `effectPresets`, `loading`,
    /// and `error` regardless of outcome.
    func loadEffectPresets(client: RESTClient) async {
        loading = true
        error = nil
        do {
            let response: EffectPresetsListResponse = try await client.getEffectPresets()
            effectPresets = response.data.presets
        } catch {
            self.error = "Failed to load effect presets: \(error.localizedDescription)"
            // Leave the previous list in place so a transient blip does not
            // erase the user's view.
        }
        loading = false
    }

    /// Fetch the full zone-preset list (built-in plus user-saved).
    func loadZonePresets(client: RESTClient) async {
        loading = true
        error = nil
        do {
            let response: ZonePresetsListResponse = try await client.getZonePresets()
            zonePresets = response.data.presets
        } catch {
            self.error = "Failed to load zone presets: \(error.localizedDescription)"
        }
        loading = false
    }

    /// Convenience that fetches both lists. Used by the view on appear and
    /// by the broadcast-driven refresh path.
    func loadAll(client: RESTClient) async {
        await loadEffectPresets(client: client)
        await loadZonePresets(client: client)
    }

    // MARK: - Effect preset WS commands

    /// Save the current live effect configuration into a slot.
    /// Dispatches `effectPresets.saveCurrent` per the firmware contract
    /// (`firmware-v3/src/network/webserver/ws/WsEffectPresetCommands.cpp`).
    func saveCurrentEffectPreset(slot: Int, name: String, ws: PresetWebSocketCommanding) async {
        await ws.sendCommand("effectPresets.saveCurrent", params: [
            "slot": slot,
            "name": name
        ])
    }

    /// Apply a stored effect preset to the renderer. Dispatches
    /// `effectPresets.load`. The firmware echoes back `effectPresets.loaded`
    /// — listened for by AppViewModel and forwarded to the relevant child
    /// ViewModels in Phase 3.
    func loadEffectPreset(id: Int, ws: PresetWebSocketCommanding) async {
        await ws.sendCommand("effectPresets.load", params: ["id": id])
    }

    /// Delete an effect preset slot. Dispatches `effectPresets.delete`.
    func deleteEffectPreset(id: Int, ws: PresetWebSocketCommanding) async {
        await ws.sendCommand("effectPresets.delete", params: ["id": id])
    }

    // MARK: - Zone preset WS commands

    /// Save the current live zone configuration into a slot. Dispatches
    /// `zonePresets.saveCurrent` per
    /// `firmware-v3/src/network/webserver/ws/WsZonePresetCommands.cpp`.
    func saveCurrentZonePreset(slot: Int, name: String, ws: PresetWebSocketCommanding) async {
        await ws.sendCommand("zonePresets.saveCurrent", params: [
            "slot": slot,
            "name": name
        ])
    }

    /// Apply a stored zone preset. Dispatches `zonePresets.load`.
    func loadZonePreset(id: Int, ws: PresetWebSocketCommanding) async {
        await ws.sendCommand("zonePresets.load", params: ["id": id])
    }

    /// Delete a user-saved zone preset slot. Dispatches `zonePresets.delete`.
    /// Built-in presets cannot be deleted; the firmware will reject the
    /// request — UI must not offer the delete affordance for `builtin == true`
    /// rows.
    func deleteZonePreset(id: Int, ws: PresetWebSocketCommanding) async {
        await ws.sendCommand("zonePresets.delete", params: ["id": id])
    }

    // MARK: - Broadcast handlers

    /// Handle the inbound `effectPresets.saved` broadcast: refresh the list
    /// so this client and every other connected client sees the new slot.
    /// Phase 3 will wire this to the AppViewModel WS event consumer; tests
    /// invoke it directly.
    func handleEffectPresetsSavedBroadcast(client: RESTClient) async {
        await loadEffectPresets(client: client)
    }

    /// Mirror of `handleEffectPresetsSavedBroadcast(client:)` for the
    /// `effectPresets.deleted` broadcast.
    func handleEffectPresetsDeletedBroadcast(client: RESTClient) async {
        await loadEffectPresets(client: client)
    }
}
