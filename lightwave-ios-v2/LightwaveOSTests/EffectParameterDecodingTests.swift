//
//  EffectParameterDecodingTests.swift
//  LightwaveOSTests
//
//  Forward-compatibility tests for the per-effect-parameter response shape.
//
//  Firmware commit 4398af3b (2026-03-24) added a numeric `type` field to each
//  parameter object in the `effects.parameters` response. The on-the-wire shape
//  is documented in `docs/protocol/k1-ws-contract.yaml`:
//
//      array of {name, displayName, min, max, default, value, type}
//      type: uint8 enum — 0=FLOAT, 1=INT, 2=BOOL, 3=ENUM
//
//  iOS' decoder must:
//    - decode legacy payloads (no `type` field) without throwing — `parameterType`
//      becomes nil
//    - decode current payloads (with numeric `type`) into a strongly-typed enum
//    - tolerate unknown numeric codes (firmware may grow new types) by mapping
//      them to `.unknown` instead of throwing
//
//  Note: the parity-phase-1 plan referenced `parameterType: "FLOAT"` (string),
//  but the firmware codec actually emits `type: <uint8>` (numeric). Tests here
//  match firmware reality, not the planning doc.
//

import XCTest
@testable import LightwaveOS

final class EffectParameterDecodingTests: XCTestCase {

    // MARK: - Legacy shape (pre-4398af3b)

    /// Parameter objects emitted by old firmware lack the `type` field entirely.
    /// Decoder must succeed and surface `parameterType == nil`.
    func testDecodesLegacyParameterShape() throws {
        let json = """
        {
            "name": "contrast",
            "displayName": "Contrast",
            "min": 0.0,
            "max": 3.0,
            "default": 1.0,
            "value": 1.0
        }
        """.data(using: .utf8)!

        let decoder = JSONDecoder()
        let param = try decoder.decode(EffectParameter.self, from: json)

        XCTAssertEqual(param.name, "contrast")
        XCTAssertEqual(param.displayName, "Contrast")
        XCTAssertEqual(param.min, 0.0, accuracy: 0.001)
        XCTAssertEqual(param.max, 3.0, accuracy: 0.001)
        XCTAssertEqual(param.defaultValue, 1.0, accuracy: 0.001)
        XCTAssertEqual(param.value, 1.0, accuracy: 0.001)
        XCTAssertNil(param.parameterType,
                     "Legacy payload (no `type` field) must decode to nil parameterType")
    }

    // MARK: - Current shape (post-4398af3b) — typed payloads

    /// Parameter objects from current firmware include a numeric `type` field.
    /// Each documented code (0=FLOAT, 1=INT, 2=BOOL, 3=ENUM) must map to its
    /// corresponding enum case.
    func testDecodesTypedParameterShapeFloat() throws {
        let json = """
        {
            "name": "contrast",
            "displayName": "Contrast",
            "min": 0.0,
            "max": 3.0,
            "default": 1.0,
            "value": 1.0,
            "type": 0
        }
        """.data(using: .utf8)!

        let decoder = JSONDecoder()
        let param = try decoder.decode(EffectParameter.self, from: json)

        XCTAssertEqual(param.parameterType, .float)
    }

    func testDecodesTypedParameterShapeInt() throws {
        let json = """
        {
            "name": "stepCount",
            "displayName": "Step Count",
            "min": 1.0,
            "max": 32.0,
            "default": 8.0,
            "value": 8.0,
            "type": 1
        }
        """.data(using: .utf8)!

        let param = try JSONDecoder().decode(EffectParameter.self, from: json)
        XCTAssertEqual(param.parameterType, .int)
    }

    func testDecodesTypedParameterShapeBool() throws {
        let json = """
        {
            "name": "mirrorEnabled",
            "displayName": "Mirror",
            "min": 0.0,
            "max": 1.0,
            "default": 0.0,
            "value": 1.0,
            "type": 2
        }
        """.data(using: .utf8)!

        let param = try JSONDecoder().decode(EffectParameter.self, from: json)
        XCTAssertEqual(param.parameterType, .bool)
    }

    func testDecodesTypedParameterShapeEnum() throws {
        let json = """
        {
            "name": "blendMode",
            "displayName": "Blend Mode",
            "min": 0.0,
            "max": 3.0,
            "default": 0.0,
            "value": 2.0,
            "type": 3
        }
        """.data(using: .utf8)!

        let param = try JSONDecoder().decode(EffectParameter.self, from: json)
        XCTAssertEqual(param.parameterType, .enumerated)
    }

    // MARK: - Forward compatibility — unknown type codes

    /// If firmware grows a new parameter-type code that iOS doesn't know about
    /// (e.g. uint8 = 99), the decoder must tolerate it by mapping to `.unknown`
    /// rather than throwing. This protects iOS from breaking the moment firmware
    /// extends the enum.
    func testDecodesUnknownTypeSafely() throws {
        let json = """
        {
            "name": "experimental",
            "displayName": "Experimental",
            "min": 0.0,
            "max": 1.0,
            "default": 0.0,
            "value": 0.5,
            "type": 99
        }
        """.data(using: .utf8)!

        let decoder = JSONDecoder()
        XCTAssertNoThrow(try decoder.decode(EffectParameter.self, from: json),
                         "Unknown type codes must NOT throw — forward compatibility")

        let param = try decoder.decode(EffectParameter.self, from: json)
        XCTAssertEqual(param.parameterType, .unknown,
                       "Unknown type codes must map to .unknown")
    }

    // MARK: - Envelope decoding

    /// The full `effects.parameters` envelope (effectId, name, hasParameters,
    /// parameters[]) must decode end-to-end including a mix of typed and legacy
    /// parameter objects in the same response (a real-world scenario when the
    /// firmware emits typed entries but the iOS client encounters a transient
    /// or partial response).
    func testDecodesFullEffectParametersEnvelope() throws {
        let json = """
        {
            "effectId": 4878,
            "name": "Spectral Envelope",
            "hasParameters": true,
            "parameters": [
                {
                    "name": "contrast",
                    "displayName": "Contrast",
                    "min": 0.0,
                    "max": 3.0,
                    "default": 1.0,
                    "value": 1.5,
                    "type": 0
                },
                {
                    "name": "legacyFloat",
                    "displayName": "Legacy",
                    "min": 0.0,
                    "max": 1.0,
                    "default": 0.5,
                    "value": 0.5
                }
            ]
        }
        """.data(using: .utf8)!

        let envelope = try JSONDecoder().decode(EffectParametersGet.self, from: json)
        XCTAssertEqual(envelope.effectId, 4878)
        XCTAssertEqual(envelope.name, "Spectral Envelope")
        XCTAssertTrue(envelope.hasParameters)
        XCTAssertEqual(envelope.parameters.count, 2)
        XCTAssertEqual(envelope.parameters[0].parameterType, .float)
        XCTAssertNil(envelope.parameters[1].parameterType)
    }
}
