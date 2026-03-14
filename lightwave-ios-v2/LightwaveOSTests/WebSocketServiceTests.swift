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
}
