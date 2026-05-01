//
//  EffectParameterSheetTests.swift
//  LightwaveOSTests
//
//  Tests for Phase 2 Task P2-1: runtime parameter sheet for end-user effect tuning.
//  Covers:
//    1. Parameter-type → control-kind mapping (F-4 decision).
//    2. Visibility helper hiding parameters with `displayName == nil`.
//    3. Decoding of the `effects.parameters` GET envelope (already covered in
//       EffectParameterDecodingTests, but P2-1 spec mandates a sanity check here
//       since the sheet depends on the round-trip working end-to-end).
//    4. Encoding of the runtime SET body — `{effectId, parameters: {<name>: <value>}}`.
//
//  Per F-4: FLOAT → slider, INT → stepper, BOOL → toggle, ENUM → picker,
//  .unknown → fallback slider, displayName == nil → hidden.
//

import XCTest
@testable import LightwaveOS

final class EffectParameterSheetTests: XCTestCase {

    // MARK: - Helpers

    /// Build a parameter with whatever fields the test under inspection needs.
    /// Keeps test bodies focussed on the field actually being asserted.
    private func makeParameter(
        name: String = "contrast",
        displayName: String? = "Contrast",
        min: Float = 0.0,
        max: Float = 1.0,
        defaultValue: Float = 0.5,
        value: Float = 0.5,
        type: ParameterType? = .float
    ) throws -> EffectParameter {
        // Round-trip via JSON to construct the struct without exposing a public
        // memberwise initialiser. Keeps EffectParameter's surface unchanged.
        let typeFragment = type.map { ",\"type\":\($0.rawValue)" } ?? ""
        let displayFragment = displayName.map { ",\"displayName\":\"\($0)\"" } ?? ""
        let json = """
        {
            "name": "\(name)"\(displayFragment),
            "min": \(min),
            "max": \(max),
            "default": \(defaultValue),
            "value": \(value)\(typeFragment)
        }
        """.data(using: .utf8)!
        return try JSONDecoder().decode(EffectParameter.self, from: json)
    }

    // MARK: - Control-kind mapping (F-4)

    func test_parameter_with_FLOAT_type_maps_to_slider_control() throws {
        let param = try makeParameter(type: .float)
        XCTAssertEqual(param.controlKind(), .slider)
    }

    func test_parameter_with_INT_type_maps_to_stepper_control() throws {
        let param = try makeParameter(min: 1, max: 32, defaultValue: 8, value: 8, type: .int)
        XCTAssertEqual(param.controlKind(), .stepper)
    }

    func test_parameter_with_BOOL_type_maps_to_toggle_control() throws {
        let param = try makeParameter(min: 0, max: 1, defaultValue: 0, value: 1, type: .bool)
        XCTAssertEqual(param.controlKind(), .toggle)
    }

    func test_parameter_with_ENUM_type_maps_to_picker_control() throws {
        let param = try makeParameter(min: 0, max: 3, defaultValue: 0, value: 2, type: .enumerated)
        XCTAssertEqual(param.controlKind(), .picker)
    }

    func test_parameter_with_unknown_type_falls_back_to_slider() throws {
        // `.unknown` is the forward-compat sentinel — UI must still render
        // something sensible. F-4 defines fallback as a slider over [min, max].
        let param = try makeParameter(type: .unknown)
        XCTAssertEqual(param.controlKind(), .slider)
    }

    func test_parameter_with_nil_type_falls_back_to_slider() throws {
        // Legacy responses (pre-4398af3b) decode with `parameterType == nil`.
        // The sheet must still display them — same fallback as `.unknown`.
        let param = try makeParameter(type: nil)
        XCTAssertEqual(param.controlKind(), .slider)
    }

    // MARK: - Visibility helper

    func test_parameter_without_displayName_is_hidden() throws {
        let param = try makeParameter(displayName: nil)
        XCTAssertFalse(param.isVisible(),
                       "Parameters without a displayName must not be shown to end users.")
    }

    func test_parameter_with_displayName_is_visible() throws {
        let param = try makeParameter(displayName: "Contrast")
        XCTAssertTrue(param.isVisible())
    }

    func test_parameter_with_empty_displayName_is_hidden() throws {
        // Defensive — firmware may emit an empty string rather than omitting
        // the field. Both cases must hide the parameter.
        let param = try makeParameter(displayName: "")
        XCTAssertFalse(param.isVisible(),
                       "Empty displayName must be treated as hidden.")
    }

    func test_controlKind_for_hidden_parameter_returns_hidden() throws {
        let param = try makeParameter(displayName: nil)
        XCTAssertEqual(param.controlKind(), .hidden,
                       "Hidden parameters must report .hidden so the sheet can skip them.")
    }

    // MARK: - GET envelope decoding (round-trip sanity)

    func test_decodes_effect_parameters_get_response() throws {
        let json = """
        {
            "effectId": 4878,
            "name": "SbSpectralEnvelope",
            "hasParameters": true,
            "parameters": [
                {
                    "name": "contrast",
                    "displayName": "Contrast",
                    "min": 0.0,
                    "max": 3.0,
                    "default": 1.0,
                    "value": 1.0,
                    "type": 0
                }
            ]
        }
        """.data(using: .utf8)!

        let envelope = try JSONDecoder().decode(EffectParametersGet.self, from: json)
        XCTAssertEqual(envelope.effectId, 4878)
        XCTAssertEqual(envelope.name, "SbSpectralEnvelope")
        XCTAssertTrue(envelope.hasParameters)
        XCTAssertEqual(envelope.parameters.count, 1)

        let only = envelope.parameters[0]
        XCTAssertEqual(only.name, "contrast")
        XCTAssertEqual(only.displayName, "Contrast")
        XCTAssertEqual(only.min, 0.0, accuracy: 0.001)
        XCTAssertEqual(only.max, 3.0, accuracy: 0.001)
        XCTAssertEqual(only.defaultValue, 1.0, accuracy: 0.001)
        XCTAssertEqual(only.value, 1.0, accuracy: 0.001)
        XCTAssertEqual(only.parameterType, .float)
        XCTAssertEqual(only.controlKind(), .slider)
    }

    // MARK: - SET body encoding

    func test_encodes_set_runtime_parameter_body() throws {
        // The encoder helper must produce `{"effectId": <id>, "parameters": {"<name>": <value>}}`
        // verbatim — that's the contract documented in docs/protocol/k1-rest-contract.yaml.
        let body = RESTClient.encodeRuntimeParameterBody(
            effectId: 4878,
            name: "contrast",
            value: 1.5
        )

        // Re-parse the bytes to compare semantically rather than relying on key order.
        let decoded = try JSONSerialization.jsonObject(with: body) as? [String: Any]
        XCTAssertNotNil(decoded, "Body must be a JSON object")

        XCTAssertEqual(decoded?["effectId"] as? Int, 4878,
                       "Top-level effectId must be an integer matching the input")

        guard let params = decoded?["parameters"] as? [String: Any] else {
            return XCTFail("`parameters` must be a nested object keyed by name")
        }
        XCTAssertEqual(params.count, 1, "Only the named parameter must be present")

        // Numeric tolerance: JSON encodes 1.5 cleanly, but the value comes back
        // as an NSNumber — compare the Double form.
        let raw = params["contrast"]
        if let n = raw as? NSNumber {
            XCTAssertEqual(n.doubleValue, 1.5, accuracy: 0.0001)
        } else {
            XCTFail("Parameter value for `contrast` must be numeric, got \(String(describing: raw))")
        }
    }
}
