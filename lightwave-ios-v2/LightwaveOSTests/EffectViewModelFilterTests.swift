//
//  EffectViewModelFilterTests.swift
//  LightwaveOSTests
//
//  Phase 2 Task P2-5 — verifies the experimental-effect filter behaviour and
//  the `showExperimental` power-user toggle on `EffectViewModel`.
//
//  Firmware emits `isExperimental` per effect in `/api/v1/effects` (see
//  `firmware-v3/src/network/webserver/handlers/EffectHandlers.cpp:173` and
//  `PatternRegistry::isExperimental(eid)`). iOS hides experimental effects
//  from the picker by default; a power-user toggle reveals them with a visual
//  cue in the gallery.
//
//  Tests cover:
//    1. Legacy decode — `EffectsResponse` payloads that pre-date the
//       `isExperimental` field decode without throwing; effects default to
//       non-experimental.
//    2. Modern decode — payloads with mixed `isExperimental: true / false`
//       preserve the flag end-to-end (DTO → EffectMetadata → ViewModel).
//    3. Default filter behaviour — `filteredEffects()` excludes experimentals
//       when `showExperimental == false` (the default).
//    4. Power-user mode — `filteredEffects()` includes ALL effects when
//       `showExperimental == true`.
//    5. Toggle — `toggleShowExperimental()` flips the state.
//

import XCTest
@testable import LightwaveOS

@MainActor
final class EffectViewModelFilterTests: XCTestCase {

    // MARK: - Helpers

    /// Build a representative effect list: 3 experimental + 5 non-experimental.
    private func makeMixedEffects() -> [EffectMetadata] {
        return [
            // Non-experimental (5)
            EffectMetadata(id: 1, name: "LGP Interference", category: "Light Guide Plate",
                           isAudioReactive: false, isExperimental: false),
            EffectMetadata(id: 2, name: "LGP Holographic", category: "Light Guide Plate",
                           isAudioReactive: true, isExperimental: false),
            EffectMetadata(id: 3, name: "Beat Pulse", category: "Audio Reactive",
                           isAudioReactive: true, isExperimental: false),
            EffectMetadata(id: 4, name: "Starfield", category: "Particles",
                           isAudioReactive: false, isExperimental: false),
            EffectMetadata(id: 5, name: "Gentle Breathing", category: "Ambient",
                           isAudioReactive: false, isExperimental: false),
            // Experimental (3)
            EffectMetadata(id: 0x1300, name: "Spectral Flux", category: "Audio Reactive",
                           isAudioReactive: true, isExperimental: true),
            EffectMetadata(id: 0x1301, name: "Onset Cascade", category: "Audio Reactive",
                           isAudioReactive: true, isExperimental: true),
            EffectMetadata(id: 0x1302, name: "Centroid Bloom", category: "Audio Reactive",
                           isAudioReactive: true, isExperimental: true)
        ]
    }

    // MARK: - DTO decoding

    /// Legacy K1 firmware (pre-`isExperimental`) returns effect objects without
    /// the flag. Decoder MUST default `isExperimental` to false rather than throw.
    func testDecodesLegacyEffectListResponse() throws {
        let json = """
        {
            "success": true,
            "data": {
                "effects": [
                    { "id": 1, "name": "LGP Interference", "category": "Light Guide Plate", "isAudioReactive": false },
                    { "id": 2, "name": "Beat Pulse", "category": "Audio Reactive", "isAudioReactive": true }
                ],
                "total": 2
            }
        }
        """.data(using: .utf8)!

        let decoder = JSONDecoder()
        let response = try decoder.decode(EffectsResponse.self, from: json)

        XCTAssertEqual(response.data.effects.count, 2)
        // Legacy DTO field absent: must decode as nil (Optional) so backward
        // compatibility is preserved without forcing a default at the wire layer.
        for effect in response.data.effects {
            XCTAssertNil(effect.isExperimental,
                         "Legacy payload should decode `isExperimental` as nil (absent)")
        }
    }

    /// Modern K1 firmware emits `isExperimental` per effect. Mixed list MUST
    /// preserve the flag through to the DTO.
    func testDecodesModernEffectListWithExperimentals() throws {
        let json = """
        {
            "success": true,
            "data": {
                "effects": [
                    { "id": 1, "name": "LGP Interference", "category": "Light Guide Plate",
                      "isAudioReactive": false, "isExperimental": false },
                    { "id": 4864, "name": "Spectral Flux", "category": "Audio Reactive",
                      "isAudioReactive": true, "isExperimental": true },
                    { "id": 4865, "name": "Onset Cascade", "category": "Audio Reactive",
                      "isAudioReactive": true, "isExperimental": true }
                ],
                "total": 3
            }
        }
        """.data(using: .utf8)!

        let decoder = JSONDecoder()
        let response = try decoder.decode(EffectsResponse.self, from: json)

        XCTAssertEqual(response.data.effects.count, 3)
        XCTAssertEqual(response.data.effects[0].isExperimental, false)
        XCTAssertEqual(response.data.effects[1].isExperimental, true)
        XCTAssertEqual(response.data.effects[2].isExperimental, true)
    }

    // MARK: - filteredEffects() behaviour

    /// Default (non-power-user) view: experimental effects are hidden.
    /// Given 3 experimental + 5 non-experimental, default returns 5.
    func testFilteredEffectsExcludesExperimentalsByDefault() {
        let vm = EffectViewModel()
        vm.allEffects = makeMixedEffects()
        XCTAssertFalse(vm.showExperimental,
                       "showExperimental should default to false for safety")

        let filtered = vm.filteredEffects()

        XCTAssertEqual(filtered.count, 5,
                       "Default filter must exclude all 3 experimental effects")
        XCTAssertTrue(filtered.allSatisfy { !$0.isExperimental },
                      "No experimental effect should leak into the default view")
    }

    /// Power-user mode: with `showExperimental == true`, all effects are returned.
    /// Same 8-effect input → 8 outputs.
    func testFilteredEffectsIncludesAllWhenShowExperimentalTrue() {
        let vm = EffectViewModel()
        vm.allEffects = makeMixedEffects()
        vm.showExperimental = true

        let filtered = vm.filteredEffects()

        XCTAssertEqual(filtered.count, 8,
                       "Power-user mode must include all effects (5 + 3)")
        XCTAssertEqual(filtered.filter { $0.isExperimental }.count, 3,
                       "All 3 experimentals should be present when showExperimental == true")
    }

    /// Toggle helper flips the state and remains observable.
    func testToggleShowExperimentalFlipsState() {
        let vm = EffectViewModel()
        XCTAssertFalse(vm.showExperimental)

        vm.toggleShowExperimental()
        XCTAssertTrue(vm.showExperimental, "First toggle should flip false → true")

        vm.toggleShowExperimental()
        XCTAssertFalse(vm.showExperimental, "Second toggle should flip true → false")
    }
}
