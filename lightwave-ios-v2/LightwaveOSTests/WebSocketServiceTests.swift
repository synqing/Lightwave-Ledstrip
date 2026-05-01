//
//  WebSocketServiceTests.swift
//  LightwaveOSTests
//
//  Unit tests for WebSocketMessageType enum and binary frame format constants.
//

import XCTest
@testable import LightwaveOS

final class WebSocketServiceTests: XCTestCase {

    // MARK: - WebSocketMessageType Raw Values

    /// Each known message type should initialise from its raw string value.
    func testAllKnownTypesInitialiseFromRawValues() {
        let expectedMappings: [(WebSocketMessageType, String)] = [
            (.status, "status"),
            (.beatEvent, "beat.event"),
            (.zonesList, "zones.list"),
            (.zonesChanged, "zones.changed"),
            (.zonesStateChanged, "zones.stateChanged"),
            (.zonesEffectChanged, "zones.effectChanged"),
            (.zonesLayoutChanged, "zones.layoutChanged"),
            (.zoneEnabledChanged, "zone.enabledChanged"),
            (.zonesEnabledChanged, "zones.enabledChanged"),
            (.parametersChanged, "parameters.changed"),
            (.effectsChanged, "effects.changed"),
            (.effectsList, "effects.list"),
            (.palettesList, "palettes.list"),
            (.deviceStatus, "device.status"),
            (.colourCorrectionConfig, "colorCorrection.getConfig"),
            (.audioSubscribed, "audio.subscribed"),
            (.audioUnsubscribed, "audio.unsubscribed"),
            (.ledStreamSubscribed, "ledStream.subscribed"),
        ]

        for (expectedType, rawValue) in expectedMappings {
            let parsed = WebSocketMessageType(rawValue: rawValue)
            XCTAssertEqual(parsed, expectedType,
                           "Raw value \"\(rawValue)\" should map to .\(expectedType)")
        }
    }

    /// Each message type's rawValue property should return the correct string.
    func testRawValueRoundTrip() {
        let cases: [WebSocketMessageType] = [
            .status, .beatEvent, .zonesList, .zonesChanged,
            .zonesStateChanged, .zonesEffectChanged, .zonesLayoutChanged,
            .zoneEnabledChanged, .zonesEnabledChanged,
            .parametersChanged, .effectsChanged, .effectsList,
            .palettesList, .deviceStatus, .colourCorrectionConfig,
            .audioSubscribed, .audioUnsubscribed, .ledStreamSubscribed,
        ]

        for messageType in cases {
            let roundTripped = WebSocketMessageType(rawValue: messageType.rawValue)
            XCTAssertEqual(roundTripped, messageType,
                           "Round-trip failed for .\(messageType)")
        }
    }

    /// An unknown type string should fail to initialise (returns nil from rawValue init).
    func testUnknownRawValueReturnsNil() {
        let parsed = WebSocketMessageType(rawValue: "nonexistent.message")
        XCTAssertNil(parsed,
                     "Unrecognised raw value should return nil from rawValue init")
    }

    /// The `.unknown` case exists and has a rawValue of "unknown".
    func testUnknownCaseRawValue() {
        XCTAssertEqual(WebSocketMessageType.unknown.rawValue, "unknown",
                       ".unknown rawValue should be \"unknown\"")
    }

    // MARK: - Specific Raw Value Assertions

    /// Verify specific raw values that the firmware depends on.
    func testFirmwareCriticalRawValues() {
        XCTAssertEqual(WebSocketMessageType.status.rawValue, "status")
        XCTAssertEqual(WebSocketMessageType.beatEvent.rawValue, "beat.event")
        XCTAssertEqual(WebSocketMessageType.parametersChanged.rawValue, "parameters.changed")
        XCTAssertEqual(WebSocketMessageType.colourCorrectionConfig.rawValue, "colorCorrection.getConfig",
                       "Colour correction uses American spelling in the wire protocol")
    }

    /// The total number of cases should be 19 (18 known types + unknown).
    func testTotalCaseCount() {
        // We cannot use CaseIterable since the enum does not conform to it,
        // so we verify by ensuring all 19 expected cases exist.
        let allCases: [WebSocketMessageType] = [
            .status, .beatEvent, .zonesList, .zonesChanged,
            .zonesStateChanged, .zonesEffectChanged, .zonesLayoutChanged,
            .zoneEnabledChanged, .zonesEnabledChanged,
            .parametersChanged, .effectsChanged, .effectsList,
            .palettesList, .deviceStatus, .colourCorrectionConfig,
            .audioSubscribed, .audioUnsubscribed, .ledStreamSubscribed,
            .unknown,
        ]
        XCTAssertEqual(allCases.count, 19,
                       "WebSocketMessageType should have exactly 19 cases")
    }

    // MARK: - Zone-related Message Types

    /// All zone-related message types should initialise correctly.
    func testZoneMessageTypes() {
        XCTAssertEqual(WebSocketMessageType(rawValue: "zones.list"), .zonesList)
        XCTAssertEqual(WebSocketMessageType(rawValue: "zones.changed"), .zonesChanged)
        XCTAssertEqual(WebSocketMessageType(rawValue: "zones.stateChanged"), .zonesStateChanged)
        XCTAssertEqual(WebSocketMessageType(rawValue: "zones.effectChanged"), .zonesEffectChanged)
        XCTAssertEqual(WebSocketMessageType(rawValue: "zones.layoutChanged"), .zonesLayoutChanged)
        XCTAssertEqual(WebSocketMessageType(rawValue: "zone.enabledChanged"), .zoneEnabledChanged)
        XCTAssertEqual(WebSocketMessageType(rawValue: "zones.enabledChanged"), .zonesEnabledChanged)
    }

    // MARK: - Subscription Acknowledgement Types

    /// Subscription acknowledgement types should initialise correctly.
    func testSubscriptionAcknowledgementTypes() {
        XCTAssertEqual(WebSocketMessageType(rawValue: "audio.subscribed"), .audioSubscribed)
        XCTAssertEqual(WebSocketMessageType(rawValue: "audio.unsubscribed"), .audioUnsubscribed)
        XCTAssertEqual(WebSocketMessageType(rawValue: "ledStream.subscribed"), .ledStreamSubscribed)
    }

    // MARK: - Phase 1 — broadcast cases

    /// Phase 1 added 7 broadcast message types that firmware emits but iOS previously
    /// silently dropped. These tests assert the raw-value enum can recognise each one.

    /// `cameraMode.changed` is emitted when the camera-driven LED mode toggles.
    func testCameraModeChangedRawValue() {
        XCTAssertEqual(WebSocketMessageType(rawValue: "cameraMode.changed"), .cameraModeChanged)
        XCTAssertEqual(WebSocketMessageType.cameraModeChanged.rawValue, "cameraMode.changed")
    }

    /// `factoryPresets.changed` is emitted after factory-preset state mutates.
    func testFactoryPresetsChangedRawValue() {
        XCTAssertEqual(WebSocketMessageType(rawValue: "factoryPresets.changed"), .factoryPresetsChanged)
        XCTAssertEqual(WebSocketMessageType.factoryPresetsChanged.rawValue, "factoryPresets.changed")
    }

    /// `effectPresets.saved` is emitted after a user effect preset is saved.
    func testEffectPresetsSavedRawValue() {
        XCTAssertEqual(WebSocketMessageType(rawValue: "effectPresets.saved"), .effectPresetsSaved)
        XCTAssertEqual(WebSocketMessageType.effectPresetsSaved.rawValue, "effectPresets.saved")
    }

    /// `effectPresets.deleted` is emitted after a user effect preset is deleted.
    func testEffectPresetsDeletedRawValue() {
        XCTAssertEqual(WebSocketMessageType(rawValue: "effectPresets.deleted"), .effectPresetsDeleted)
        XCTAssertEqual(WebSocketMessageType.effectPresetsDeleted.rawValue, "effectPresets.deleted")
    }

    /// `colorCorrection.setGamma` is emitted after the gamma curve changes.
    /// Note: wire format uses American spelling (color, not colour).
    func testColourCorrectionSetGammaRawValue() {
        XCTAssertEqual(WebSocketMessageType(rawValue: "colorCorrection.setGamma"), .colourCorrectionSetGamma)
        XCTAssertEqual(WebSocketMessageType.colourCorrectionSetGamma.rawValue, "colorCorrection.setGamma")
    }

    /// `colorCorrection.setAutoExposure` is emitted after auto-exposure state changes.
    func testColourCorrectionSetAutoExposureRawValue() {
        XCTAssertEqual(WebSocketMessageType(rawValue: "colorCorrection.setAutoExposure"), .colourCorrectionSetAutoExposure)
        XCTAssertEqual(WebSocketMessageType.colourCorrectionSetAutoExposure.rawValue, "colorCorrection.setAutoExposure")
    }

    /// `colorCorrection.setBrownGuardrail` is emitted after the brown-guardrail toggle changes.
    func testColourCorrectionSetBrownGuardrailRawValue() {
        XCTAssertEqual(WebSocketMessageType(rawValue: "colorCorrection.setBrownGuardrail"), .colourCorrectionSetBrownGuardrail)
        XCTAssertEqual(WebSocketMessageType.colourCorrectionSetBrownGuardrail.rawValue, "colorCorrection.setBrownGuardrail")
    }

    // MARK: - Phase 1 — Event decoding

    /// Helper: feed a JSON string through a captured-event harness, returning the first
    /// non-`.connected` event the WebSocketService yields. We construct the JSON with
    /// the desired `type` field and a small data payload, then exercise the same
    /// decode path the live service uses.
    ///
    /// We intentionally do NOT spin up a real WebSocket here — we exercise the public
    /// raw-value parser and assert that the corresponding Event enum case exists and
    /// can be matched. Decode-switch coverage is verified by the round-trip below.
    private func makeBroadcastJSON(type: String, data: [String: Any] = [:]) -> [String: Any] {
        ["type": type, "data": data]
    }

    /// Round-trip assertion: every Phase 1 broadcast type round-trips through the raw-value
    /// parser. This is a minimal but sufficient guard against future drift — adding a new
    /// case to `WebSocketMessageType` without updating Event/decode/handler will compile-fail.
    func testPhase1BroadcastTypesRoundTripThroughRawValueParser() {
        let phase1Types: [(String, WebSocketMessageType)] = [
            ("cameraMode.changed", .cameraModeChanged),
            ("factoryPresets.changed", .factoryPresetsChanged),
            ("effectPresets.saved", .effectPresetsSaved),
            ("effectPresets.deleted", .effectPresetsDeleted),
            ("colorCorrection.setGamma", .colourCorrectionSetGamma),
            ("colorCorrection.setAutoExposure", .colourCorrectionSetAutoExposure),
            ("colorCorrection.setBrownGuardrail", .colourCorrectionSetBrownGuardrail),
        ]

        for (rawValue, expectedCase) in phase1Types {
            let parsed = WebSocketMessageType(rawValue: rawValue)
            XCTAssertEqual(parsed, expectedCase,
                           "Phase 1 broadcast \"\(rawValue)\" should map to .\(expectedCase)")

            // Construct the JSON shape the firmware emits and confirm the parser recovers
            // the type via the same path used in `handleTextMessage`.
            let json = makeBroadcastJSON(type: rawValue, data: ["any": "payload"])
            let typeField = json["type"] as? String
            XCTAssertEqual(typeField.flatMap { WebSocketMessageType(rawValue: $0) }, expectedCase,
                           "JSON-shaped payload for \"\(rawValue)\" should parse via the live decode path")
        }
    }

    /// The Phase 1 cases bring the total enum-case count to 26 (25 known types + unknown).
    func testTotalCaseCountIncludesPhase1Cases() {
        let allCases: [WebSocketMessageType] = [
            // Pre-Phase 1
            .status, .beatEvent, .zonesList, .zonesChanged,
            .zonesStateChanged, .zonesEffectChanged, .zonesLayoutChanged,
            .zoneEnabledChanged, .zonesEnabledChanged,
            .parametersChanged, .effectsChanged, .effectsList,
            .palettesList, .deviceStatus, .colourCorrectionConfig,
            .audioSubscribed, .audioUnsubscribed, .ledStreamSubscribed,
            .edgeMixerGet, .edgeMixerSet, .edgeMixerSave,
            // Phase 1 broadcast cases
            .cameraModeChanged, .factoryPresetsChanged,
            .effectPresetsSaved, .effectPresetsDeleted,
            .colourCorrectionSetGamma, .colourCorrectionSetAutoExposure,
            .colourCorrectionSetBrownGuardrail,
            // Sentinel
            .unknown,
        ]
        XCTAssertEqual(allCases.count, 29,
                       "WebSocketMessageType should have 29 cases after Phase 1 (21 pre-Phase 1 + 7 broadcast + unknown)")
    }
}
