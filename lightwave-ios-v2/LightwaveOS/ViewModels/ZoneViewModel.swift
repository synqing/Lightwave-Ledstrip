//
//  ZoneViewModel.swift
//  LightwaveOS
//
//  Multi-zone composition management for dual-strip LED controller.
//  Handles zone enable/disable, preset loading, per-zone effect/palette/blend/speed.
//

import Foundation
import Observation

@MainActor
@Observable
class ZoneViewModel {
    // MARK: - Constants

    private static let centerLeft = 79
    private static let centerRight = 80
    private static let minZoneWidth = 4

    // MARK: - State

    var zonesEnabled: Bool = false
    var zoneCount: Int = 2
    var zones: [ZoneConfig] = []
    var segments: [ZoneSegment] = []
    var presets: [ZonePreset] = []

    /// Boundary state (derived from segments, drives split sliders)
    var boundary0: Int = 55  // b0: inner/middle boundary (default 2-zone even)
    var boundary1: Int = 30  // b1: middle/outer boundary (default 3-zone even)

    // MARK: - Network

    var restClient: RESTClient?
    var ws: WebSocketService?

    // MARK: - Pending State (debounce tracking)

    private var pendingZoneSpeeds: Set<Int> = []
    private var pendingZoneBrightness: Set<Int> = []
    private var speedDebounceTasks: [Int: Task<Void, Never>] = [:]
    private var brightnessDebounceTasks: [Int: Task<Void, Never>] = [:]
    private var clearSpeedPendingTasks: [Int: Task<Void, Never>] = [:]
    private var clearBrightnessPendingTasks: [Int: Task<Void, Never>] = [:]
    private var splitDebounceTask: Task<Void, Never>?

    // MARK: - API Methods

    func toggleZones() async {
        guard let client = restClient else { return }
        let newState = !zonesEnabled

        do {
            try await client.setZonesEnabled(newState)
            self.zonesEnabled = newState
            print("Zones \(newState ? "enabled" : "disabled")")
        } catch {
            print("Error toggling zones: \(error)")
        }
    }

    func setZoneEffect(zoneId: Int, effectId: Int, effectName: String? = nil) async {
        // Optimistic local update
        if let index = zones.firstIndex(where: { $0.id == zoneId }) {
            zones[index].effectId = effectId
            if let name = effectName {
                zones[index].effectName = name
            }
        }

        // WebSocket path (more reliable for zone controls)
        if let ws = ws {
            await ws.send("zone.setEffect", params: [
                "zoneId": zoneId,
                "effectId": effectId
            ])
        }

        guard let client = restClient else { return }

        do {
            try await client.setZoneEffect(zoneId: zoneId, effectId: effectId)
            print("Set zone \(zoneId) effect to \(effectId)")
        } catch {
            print("Error setting zone effect: \(error)")
        }
    }

    func setZonePalette(zoneId: Int, paletteId: Int, paletteName: String? = nil) async {
        // Optimistic local update
        if let index = zones.firstIndex(where: { $0.id == zoneId }) {
            zones[index].paletteId = paletteId
            if let name = paletteName {
                zones[index].paletteName = name
            }
        }

        // WebSocket path (more reliable for zone controls)
        if let ws = ws {
            await ws.send("zone.setPalette", params: [
                "zoneId": zoneId,
                "paletteId": paletteId
            ])
        }

        guard let client = restClient else { return }

        do {
            try await client.setZonePalette(zoneId: zoneId, paletteId: paletteId)
            print("Set zone \(zoneId) palette to \(paletteId)")
        } catch {
            print("Error setting zone palette: \(error)")
        }
    }

    func setZoneBlend(zoneId: Int, blendMode: Int, blendModeName: String? = nil) async {
        // Optimistic local update
        if let index = zones.firstIndex(where: { $0.id == zoneId }) {
            zones[index].blendMode = blendMode
            if let name = blendModeName {
                zones[index].blendModeName = name
            }
        }

        // WebSocket path (more reliable for zone controls)
        if let ws = ws {
            await ws.send("zone.setBlend", params: [
                "zoneId": zoneId,
                "blendMode": blendMode
            ])
        }

        guard let client = restClient else { return }

        do {
            try await client.setZoneBlendMode(zoneId: zoneId, blendMode: blendMode)
            print("Set zone \(zoneId) blend mode to \(blendMode)")
        } catch {
            print("Error setting zone blend: \(error)")
        }
    }

    func setZoneSpeed(zoneId: Int, speed: Int) {
        guard restClient != nil || ws != nil else { return }

        // Mark as pending to ignore WS updates
        pendingZoneSpeeds.insert(zoneId)

        // Update local state immediately
        if let index = zones.firstIndex(where: { $0.id == zoneId }) {
            zones[index].speed = speed
        }

        // Cancel previous debounce task for this zone
        speedDebounceTasks[zoneId]?.cancel()

        // Debounce the network update (150ms)
        speedDebounceTasks[zoneId] = Task {
            do {
                try await Task.sleep(nanoseconds: 150_000_000)
                guard !Task.isCancelled else { return }

                if let ws = ws {
                    await ws.send("zones.update", params: [
                        "zoneId": zoneId,
                        "speed": speed
                    ])
                }
                if let client = restClient {
                    try await client.setZoneSpeed(zoneId: zoneId, speed: speed)
                }
                print("Updated zone \(zoneId) speed to \(speed)")

                // Clear pending flag after 1 second
                clearSpeedPendingTasks[zoneId]?.cancel()
                clearSpeedPendingTasks[zoneId] = Task {
                    try? await Task.sleep(nanoseconds: 1_000_000_000)
                    guard !Task.isCancelled else { return }
                    pendingZoneSpeeds.remove(zoneId)
                    clearSpeedPendingTasks.removeValue(forKey: zoneId)
                    speedDebounceTasks.removeValue(forKey: zoneId)
                }

            } catch is CancellationError {
                // Cancelled by newer update
            } catch {
                print("Error updating zone speed: \(error)")
            }
        }
    }

    func setZoneBrightness(zoneId: Int, brightness: Int) {
        guard restClient != nil || ws != nil else { return }

        // Mark as pending to ignore WS updates
        pendingZoneBrightness.insert(zoneId)

        // Update local state immediately
        if let index = zones.firstIndex(where: { $0.id == zoneId }) {
            zones[index].brightness = brightness
        }

        // Cancel previous debounce task for this zone
        brightnessDebounceTasks[zoneId]?.cancel()

        // Debounce the network update (150ms)
        brightnessDebounceTasks[zoneId] = Task {
            do {
                try await Task.sleep(nanoseconds: 150_000_000)
                guard !Task.isCancelled else { return }

                if let ws = ws {
                    await ws.send("zones.update", params: [
                        "zoneId": zoneId,
                        "brightness": brightness
                    ])
                }
                if let client = restClient {
                    try await client.setZoneBrightness(zoneId: zoneId, brightness: brightness)
                }
                print("Updated zone \(zoneId) brightness to \(brightness)")

                // Clear pending flag after 1 second
                clearBrightnessPendingTasks[zoneId]?.cancel()
                clearBrightnessPendingTasks[zoneId] = Task {
                    try? await Task.sleep(nanoseconds: 1_000_000_000)
                    guard !Task.isCancelled else { return }
                    pendingZoneBrightness.remove(zoneId)
                    clearBrightnessPendingTasks.removeValue(forKey: zoneId)
                    brightnessDebounceTasks.removeValue(forKey: zoneId)
                }

            } catch is CancellationError {
                // Cancelled by newer update
            } catch {
                print("Error updating zone brightness: \(error)")
            }
        }
    }

    // MARK: - Boundary Model (centre-origin symmetric zone splits)

    /// Extract boundary values from current segments
    func updateBoundariesFromSegments() {
        guard segments.count >= 2 else { return }
        let z0 = segments.first(where: { $0.zoneId == 0 })
        boundary0 = z0?.s1LeftStart ?? 55

        if segments.count >= 3 {
            let z1 = segments.first(where: { $0.zoneId == 1 })
            boundary1 = z1?.s1LeftStart ?? 30
        }
    }

    /// Create a symmetric zone segment from left-side parameters
    private func makeSeg(zoneId: Int, leftStart: Int, leftEnd: Int) -> ZoneSegment {
        let leftSize = leftEnd - leftStart + 1
        let dist = Self.centerLeft - leftEnd
        let rightStart = Self.centerRight + dist
        let rightEnd = rightStart + leftSize - 1
        return ZoneSegment(
            zoneId: zoneId,
            s1LeftStart: leftStart,
            s1LeftEnd: leftEnd,
            s1RightStart: rightStart,
            s1RightEnd: rightEnd
        )
    }

    /// Convert boundary values to zone segments
    func segmentsFromBoundaries() -> [ZoneSegment] {
        switch zoneCount {
        case 1:
            return [makeSeg(zoneId: 0, leftStart: 0, leftEnd: Self.centerLeft)]
        case 2:
            return [
                makeSeg(zoneId: 0, leftStart: boundary0, leftEnd: Self.centerLeft),
                makeSeg(zoneId: 1, leftStart: 0, leftEnd: boundary0 - 1)
            ]
        case 3:
            return [
                makeSeg(zoneId: 0, leftStart: boundary0, leftEnd: Self.centerLeft),
                makeSeg(zoneId: 1, leftStart: boundary1, leftEnd: boundary0 - 1),
                makeSeg(zoneId: 2, leftStart: 0, leftEnd: boundary1 - 1)
            ]
        default:
            return []
        }
    }

    /// Enforce MIN_ZONE_WIDTH constraints with cascade push
    private func validateBoundaries() {
        // b0: inner zone needs >= minZoneWidth LEDs per side, outer also needs >= minZoneWidth
        boundary0 = max(Self.minZoneWidth, min(boundary0, Self.centerLeft - Self.minZoneWidth + 1))

        if zoneCount == 3 {
            // b1 must leave room for middle zone (>= minZoneWidth) and outer zone (>= minZoneWidth)
            boundary1 = max(Self.minZoneWidth, min(boundary1, boundary0 - Self.minZoneWidth))
        }
    }

    /// Update a zone boundary from split slider drag (with cascade push + debounced API call)
    func setZoneSplit(boundaryIndex: Int, value: Int) {
        if boundaryIndex == 0 {
            boundary0 = value
        } else {
            boundary1 = value
        }

        // Cascade push: enforce constraints
        validateBoundaries()

        // Update local segments immediately (drives LED strip + zone card metadata)
        segments = segmentsFromBoundaries()

        // Debounce firmware API call (150ms)
        splitDebounceTask?.cancel()
        splitDebounceTask = Task {
            do {
                try await Task.sleep(nanoseconds: 150_000_000)
                guard !Task.isCancelled else { return }

                let layoutZones: [[String: Int]] = segments.map { seg in
                    [
                        "zoneId": seg.zoneId,
                        "s1LeftStart": seg.s1LeftStart,
                        "s1LeftEnd": seg.s1LeftEnd,
                        "s1RightStart": seg.s1RightStart,
                        "s1RightEnd": seg.s1RightEnd
                    ]
                }
                try await restClient?.setZoneLayout(zones: layoutZones)
            } catch is CancellationError {
                // Cancelled by newer drag
            } catch {
                print("Error setting zone layout: \(error)")
            }
        }
    }

    /// Reset splits to even distribution (triggered by centre marker tap)
    func resetSplitsToEven() {
        switch zoneCount {
        case 2:
            boundary0 = 55  // inner=25 LEDs/side, outer=55 LEDs/side
        case 3:
            boundary0 = 65  // inner=15/side
            boundary1 = 30  // middle=35/side, outer=30/side
        default:
            return
        }

        segments = segmentsFromBoundaries()

        // Send to firmware immediately (no debounce for tap reset)
        Task {
            let layoutZones: [[String: Int]] = segments.map { seg in
                [
                    "zoneId": seg.zoneId,
                    "s1LeftStart": seg.s1LeftStart,
                    "s1LeftEnd": seg.s1LeftEnd,
                    "s1RightStart": seg.s1RightStart,
                    "s1RightEnd": seg.s1RightEnd
                ]
            }
            try? await restClient?.setZoneLayout(zones: layoutZones)
        }
    }

    func loadPreset(presetId: Int, using ws: WebSocketService) async {
        await ws.send("zone.loadPreset", params: ["presetId": presetId])
    }

    func loadZones() async {
        guard let client = restClient else { return }

        do {
            let response = try await client.getZones()

            self.zonesEnabled = response.data.enabled
            self.zoneCount = response.data.zoneCount
            self.zones = response.data.zones.map { zone in
                ZoneConfig(
                    id: zone.id,
                    enabled: zone.enabled ?? true,
                    effectId: zone.effectId,
                    effectName: zone.effectName,
                    brightness: zone.brightness ?? 255,
                    speed: zone.speed,
                    paletteId: zone.paletteId,
                    paletteName: zone.paletteName,
                    blendMode: zone.blendMode,
                    blendModeName: zone.blendModeName
                )
            }
            if let segs = response.data.segments {
                self.segments = segs.map { s in
                    ZoneSegment(
                        zoneId: s.zoneId,
                        s1LeftStart: s.s1LeftStart,
                        s1LeftEnd: s.s1LeftEnd,
                        s1RightStart: s.s1RightStart,
                        s1RightEnd: s.s1RightEnd
                    )
                }
            }
            if let presetsData = response.data.presets {
                self.presets = presetsData.map { ZonePreset(id: $0.id, name: $0.name) }
            }
            // Sync boundary state from loaded segments
            updateBoundariesFromSegments()

            print("Loaded \(zones.count) zones, \(segments.count) segments, enabled: \(zonesEnabled)")
        } catch {
            print("Error loading zones: \(error)")
        }
    }

    // MARK: - WebSocket Updates

    func handleZoneUpdate(_ data: [String: Any]) {
        if let zonesData = data["zones"] as? [[String: Any]] {
            self.zones = zonesData.compactMap { zone in
                guard let id = zone["id"] as? Int else { return nil }
                let effectId = zone["effectId"] as? Int ?? 0
                let speed = zone["speed"] as? Int ?? 15
                let paletteId = zone["paletteId"] as? Int ?? 0
                let blendMode = zone["blendMode"] as? Int ?? 0
                return ZoneConfig(
                    id: id,
                    enabled: zone["enabled"] as? Bool ?? true,
                    effectId: effectId,
                    effectName: zone["effectName"] as? String,
                    brightness: zone["brightness"] as? Int ?? 255,
                    speed: speed,
                    paletteId: paletteId,
                    paletteName: zone["paletteName"] as? String,
                    blendMode: blendMode,
                    blendModeName: zone["blendModeName"] as? String
                )
            }
        }

        if let segs = data["segments"] as? [[String: Any]] {
            self.segments = segs.compactMap { seg in
                guard
                    let zoneId = seg["zoneId"] as? Int,
                    let s1LeftStart = seg["s1LeftStart"] as? Int,
                    let s1LeftEnd = seg["s1LeftEnd"] as? Int,
                    let s1RightStart = seg["s1RightStart"] as? Int,
                    let s1RightEnd = seg["s1RightEnd"] as? Int
                else { return nil }

                return ZoneSegment(
                    zoneId: zoneId,
                    s1LeftStart: s1LeftStart,
                    s1LeftEnd: s1LeftEnd,
                    s1RightStart: s1RightStart,
                    s1RightEnd: s1RightEnd
                )
            }
            updateBoundariesFromSegments()
        }

        if let enabled = data["enabled"] as? Bool {
            zonesEnabled = enabled
        }

        if let count = (data["zoneCount"] as? Int) ?? (data["count"] as? Int) {
            zoneCount = count
        }

        if let zoneId = data["zoneId"] as? Int {
            guard let index = zones.firstIndex(where: { $0.id == zoneId }) else { return }

            let current = data["current"] as? [String: Any]

            if let effectId = (current?["effectId"] as? Int) ?? (data["effectId"] as? Int) {
                zones[index].effectId = effectId
            }

            if let effectName = (current?["effectName"] as? String) ?? (data["effectName"] as? String) {
                zones[index].effectName = effectName
            }

            if let speed = (current?["speed"] as? Int) ?? (data["speed"] as? Int),
               !pendingZoneSpeeds.contains(zoneId) {
                zones[index].speed = speed
            }

            if let brightness = (current?["brightness"] as? Int) ?? (data["brightness"] as? Int),
               !pendingZoneBrightness.contains(zoneId) {
                zones[index].brightness = brightness
            }

            if let paletteId = (current?["paletteId"] as? Int) ?? (data["paletteId"] as? Int) {
                zones[index].paletteId = paletteId
            }

            if let paletteName = (current?["paletteName"] as? String) ?? (data["paletteName"] as? String) {
                zones[index].paletteName = paletteName
            }

            if let blendMode = (current?["blendMode"] as? Int) ?? (data["blendMode"] as? Int) {
                zones[index].blendMode = blendMode
            }

            if let blendModeName = (current?["blendModeName"] as? String) ?? (data["blendModeName"] as? String) {
                zones[index].blendModeName = blendModeName
            }
        }
    }
}
