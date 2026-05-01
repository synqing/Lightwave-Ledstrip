//
//  STMVRMSStreamTests.swift
//  LightwaveOSTests
//
//  Phase 2 — Task P2-2: STM and VRMS subscription + binary/text frame decoding.
//
//  Wire formats, sourced from firmware:
//  - STM: 250-byte binary, magic 0xFD, layout per
//    `firmware-v3/src/network/webserver/StmStreamBroadcaster.h` lines 38-59
//    and `StmStreamBroadcaster.cpp` lines 78-115.
//      [0]    uint8   magic = 0xFD
//      [1]    uint8   ready flag (0/1)
//      [2..]  42 × float32 spectral bins (168 B)
//      [..]   16 × float32 temporal bands (64 B)
//      [..]   float32 spectralEnergy
//      [..]   float32 temporalEnergy
//      [..]   float32 spectralCentroid (bin-weighted)
//      [..]   float32 dominantBin (encoded as float32)
//
//  - VRMS: JSON text message, type "vrms.frame", per
//    `firmware-v3/src/network/WebServer.cpp` lines 884-924.
//      { "type": "vrms.frame",
//        "metrics": { dominantHue, colourVariance, spatialCentroid, symmetryScore,
//                     brightnessMean, brightnessVariance, temporalFreq, audioVisualCorr },
//        "timestamp": <uint32 millis> }
//

import XCTest
@testable import LightwaveOS

final class STMVRMSStreamTests: XCTestCase {

    // MARK: - WebSocketMessageType raw-value round-trips

    /// `stm.subscribe` and `stm.unsubscribe` round-trip through the raw-value parser.
    func testStmSubscribeCommandRoundTrips() {
        XCTAssertEqual(WebSocketMessageType(rawValue: "stm.subscribed"), .stmSubscribed)
        XCTAssertEqual(WebSocketMessageType(rawValue: "stm.unsubscribed"), .stmUnsubscribed)
        XCTAssertEqual(WebSocketMessageType.stmSubscribed.rawValue, "stm.subscribed")
        XCTAssertEqual(WebSocketMessageType.stmUnsubscribed.rawValue, "stm.unsubscribed")
    }

    /// `vrms.frame` and the subscribe/unsubscribe acknowledgements round-trip.
    func testVrmsSubscribeCommandRoundTrips() {
        XCTAssertEqual(WebSocketMessageType(rawValue: "vrms.subscribed"), .vrmsSubscribed)
        XCTAssertEqual(WebSocketMessageType(rawValue: "vrms.unsubscribed"), .vrmsUnsubscribed)
        XCTAssertEqual(WebSocketMessageType(rawValue: "vrms.frame"), .vrmsFrame)
        XCTAssertEqual(WebSocketMessageType.vrmsFrame.rawValue, "vrms.frame")
    }

    // MARK: - STMFrame binary decoding

    /// A hand-crafted 250-byte STM buffer with magic 0xFD decodes into the
    /// expected struct. Values are picked to be uniquely identifiable in each
    /// field so off-by-one offsets surface as test failures.
    func testDecodesStmBinaryFrame() {
        let frame = STMFrame.encodeForTesting(
            ready: true,
            spectral: makeRamp(count: STMFrame.spectralBinCount, start: 0.0, step: 0.1),
            temporal: makeRamp(count: STMFrame.temporalBandCount, start: 0.5, step: 0.05),
            spectralEnergy: 1.25,
            temporalEnergy: 2.5,
            spectralCentroid: 19.5,
            dominantBin: 12.0
        )

        XCTAssertEqual(frame.count, STMFrame.frameSize, "STM fixture must be exactly 250 bytes")

        guard let decoded = STMFrame(data: frame) else {
            XCTFail("STM decoder returned nil for a well-formed fixture")
            return
        }

        XCTAssertTrue(decoded.ready, "ready flag should round-trip true")
        XCTAssertEqual(decoded.spectral.count, STMFrame.spectralBinCount)
        XCTAssertEqual(decoded.temporal.count, STMFrame.temporalBandCount)

        // Check first/last spectral and temporal entries.
        XCTAssertEqual(decoded.spectral.first ?? -1, 0.0, accuracy: 1e-5)
        XCTAssertEqual(decoded.spectral.last ?? -1, 0.0 + 0.1 * Float(STMFrame.spectralBinCount - 1), accuracy: 1e-4)
        XCTAssertEqual(decoded.temporal.first ?? -1, 0.5, accuracy: 1e-5)
        XCTAssertEqual(decoded.temporal.last ?? -1, 0.5 + 0.05 * Float(STMFrame.temporalBandCount - 1), accuracy: 1e-4)

        // Scalar metrics.
        XCTAssertEqual(decoded.spectralEnergy, 1.25, accuracy: 1e-5)
        XCTAssertEqual(decoded.temporalEnergy, 2.5, accuracy: 1e-5)
        XCTAssertEqual(decoded.spectralCentroid, 19.5, accuracy: 1e-5)
        XCTAssertEqual(decoded.dominantBin, 12.0, accuracy: 1e-5)
    }

    /// A correctly-sized buffer with the wrong magic byte must NOT decode as STM.
    func testStmRejectsWrongMagic() {
        var bytes = STMFrame.encodeForTesting(
            ready: false,
            spectral: Array(repeating: 0, count: STMFrame.spectralBinCount),
            temporal: Array(repeating: 0, count: STMFrame.temporalBandCount),
            spectralEnergy: 0,
            temporalEnergy: 0,
            spectralCentroid: 0,
            dominantBin: 0
        )
        bytes[0] = 0xAA  // Corrupt the magic byte.
        XCTAssertNil(STMFrame(data: bytes), "Wrong magic must reject")
    }

    // MARK: - VRMSFrame JSON decoding

    /// A `vrms.frame` JSON envelope decodes into the expected struct.
    func testDecodesVrmsJsonFrame() throws {
        let json = """
        {
          "type": "vrms.frame",
          "metrics": {
            "dominantHue": 192.0,
            "colourVariance": 0.123,
            "spatialCentroid": 159.5,
            "symmetryScore": 0.875,
            "brightnessMean": 144.0,
            "brightnessVariance": 1234,
            "temporalFreq": 0.045,
            "audioVisualCorr": -0.250
          },
          "timestamp": 4242
        }
        """

        guard let data = json.data(using: .utf8) else {
            XCTFail("Could not encode test JSON")
            return
        }

        let envelope = try JSONDecoder().decode(VRMSEnvelope.self, from: data)
        let frame = envelope.metrics

        XCTAssertEqual(envelope.timestamp, 4242)
        XCTAssertEqual(frame.dominantHue, 192.0, accuracy: 1e-5)
        XCTAssertEqual(frame.colourVariance, 0.123, accuracy: 1e-5)
        XCTAssertEqual(frame.spatialCentroid, 159.5, accuracy: 1e-5)
        XCTAssertEqual(frame.symmetryScore, 0.875, accuracy: 1e-5)
        XCTAssertEqual(frame.brightnessMean, 144.0, accuracy: 1e-5)
        XCTAssertEqual(frame.brightnessVariance, 1234, accuracy: 1e-5)
        XCTAssertEqual(frame.temporalFreq, 0.045, accuracy: 1e-5)
        XCTAssertEqual(frame.audioVisualCorr, -0.250, accuracy: 1e-5)
    }

    // MARK: - Unknown frame fall-through

    /// A binary frame with an unknown magic byte (not 0xFE LED, not 0xFD STM, not
    /// 0x00445541 audio metrics) must NOT crash the STM decoder. It returns nil.
    func testUnknownBinaryFrameFallsThrough() {
        var bytes = [UInt8](repeating: 0x00, count: STMFrame.frameSize)
        bytes[0] = 0x42  // Arbitrary non-magic byte.
        let data = Data(bytes)
        XCTAssertNil(STMFrame(data: data), "Unknown magic must not decode as STM")
    }

    /// STM decoder must reject buffers whose length differs from `frameSize`.
    func testStmRejectsWrongLength() {
        let shortBytes = Data([0xFD, 0x01, 0x00, 0x00])
        XCTAssertNil(STMFrame(data: shortBytes), "Short buffer must not decode")

        let longBytes = Data(repeating: 0xFD, count: STMFrame.frameSize + 16)
        XCTAssertNil(STMFrame(data: longBytes), "Oversized buffer must not decode")
    }

    // MARK: - Helpers

    private func makeRamp(count: Int, start: Float, step: Float) -> [Float] {
        (0..<count).map { start + Float($0) * step }
    }
}
