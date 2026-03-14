//
//  AudioMetricsFrameTests.swift
//  LightwaveOSTests
//
//  Unit tests for binary frame parsing of 464-byte audio metrics frames.
//

import XCTest
@testable import LightwaveOS

final class AudioMetricsFrameTests: XCTestCase {

    // MARK: - Constants

    /// Expected frame size matches the struct constant.
    func testFrameSizeConstant() {
        XCTAssertEqual(AudioMetricsFrame.frameSize, 464,
                       "Frame size should be 464 bytes")
    }

    /// Magic bytes should be 0x00445541 (little-endian "AUD\0").
    func testMagicConstant() {
        XCTAssertEqual(AudioMetricsFrame.magic, 0x00445541,
                       "Magic should be 0x00445541")
    }

    // MARK: - Valid Frame Parsing

    /// A correctly formed 464-byte frame with valid magic parses successfully.
    func testValidFrameParsesSuccessfully() {
        let data = makeValidFrame()
        let frame = AudioMetricsFrame(data: data)

        XCTAssertNotNil(frame, "Valid 464-byte frame should parse successfully")
    }

    /// Parsed header fields should match the values written into the frame.
    func testParsedHeaderFieldsMatchInput() {
        let data = makeValidFrame(hopSeq: 42, timestamp: 5000)
        let frame = AudioMetricsFrame(data: data)

        XCTAssertEqual(frame?.hopSeq, 42)
        XCTAssertEqual(frame?.timestamp, 5000)
    }

    /// Parsed RMS and flux fields should match the values written.
    func testParsedRMSAndFluxFields() throws {
        let data = makeValidFrame(rms: 0.75, flux: 0.25, fastRMS: 0.9, fastFlux: 0.1)
        let frame = try XCTUnwrap(AudioMetricsFrame(data: data))

        XCTAssertEqual(frame.rms, 0.75, accuracy: Float(0.001))
        XCTAssertEqual(frame.flux, 0.25, accuracy: Float(0.001))
        XCTAssertEqual(frame.fastRMS, 0.9, accuracy: Float(0.001))
        XCTAssertEqual(frame.fastFlux, 0.1, accuracy: Float(0.001))
    }

    /// Frequency bands array should contain exactly 8 elements.
    func testBandsArrayCount() {
        let data = makeValidFrame()
        let frame = AudioMetricsFrame(data: data)

        XCTAssertEqual(frame?.bands.count, 8,
                       "Should have 8 frequency bands")
        XCTAssertEqual(frame?.heavyBands.count, 8,
                       "Should have 8 heavy frequency bands")
    }

    /// Chroma arrays should contain exactly 12 elements each.
    func testChromaArrayCount() {
        let data = makeValidFrame()
        let frame = AudioMetricsFrame(data: data)

        XCTAssertEqual(frame?.chroma.count, 12,
                       "Should have 12 chroma pitch classes")
        XCTAssertEqual(frame?.heavyChroma.count, 12,
                       "Should have 12 heavy chroma pitch classes")
    }

    /// Waveform array should contain exactly 128 normalised samples.
    func testWaveformArrayCount() {
        let data = makeValidFrame()
        let frame = AudioMetricsFrame(data: data)

        XCTAssertEqual(frame?.waveform.count, 128,
                       "Should have 128 waveform samples")
    }

    /// Beat tracking fields should be parsed from the correct offsets.
    func testBeatTrackingFields() throws {
        let data = makeValidFrame(bpmSmoothed: 140.0, tempoConfidence: 0.92,
                                  beatPhase01: 0.5, beatTick: 1, downbeatTick: 0)
        let frame = try XCTUnwrap(AudioMetricsFrame(data: data))

        XCTAssertEqual(frame.bpmSmoothed, 140.0, accuracy: Float(0.001))
        XCTAssertEqual(frame.tempoConfidence, 0.92, accuracy: Float(0.001))
        XCTAssertEqual(frame.beatPhase01, 0.5, accuracy: Float(0.001))
        XCTAssertEqual(frame.beatTick, 1)
        XCTAssertEqual(frame.downbeatTick, 0)
    }

    // MARK: - Invalid Frame Rejection

    /// Frame with wrong size (too short) should return nil.
    func testWrongSizeTooShortReturnsNil() {
        let data = Data(count: 463)
        let frame = AudioMetricsFrame(data: data)

        XCTAssertNil(frame, "Frame with 463 bytes should return nil")
    }

    /// Frame with wrong size (too long) should return nil.
    func testWrongSizeTooLongReturnsNil() {
        let data = Data(count: 465)
        let frame = AudioMetricsFrame(data: data)

        XCTAssertNil(frame, "Frame with 465 bytes should return nil")
    }

    /// Frame with correct size but wrong magic bytes should return nil.
    func testWrongMagicReturnsNil() {
        var data = Data(count: 464)
        data.withUnsafeMutableBytes { buffer in
            // Write incorrect magic
            buffer.storeBytes(of: UInt32(0xDEADBEEF), toByteOffset: 0, as: UInt32.self)
        }
        let frame = AudioMetricsFrame(data: data)

        XCTAssertNil(frame, "Frame with wrong magic bytes should return nil")
    }

    /// Empty data should return nil.
    func testEmptyDataReturnsNil() {
        let frame = AudioMetricsFrame(data: Data())

        XCTAssertNil(frame, "Empty data should return nil")
    }

    // MARK: - Mock Factory

    /// The static `.mock` property should return a valid frame.
    func testMockReturnsNonNil() {
        let mock = AudioMetricsFrame.mock

        // The mock is constructed with a force-unwrap, so if it exists we are fine.
        // Verify key fields match the values written in the mock factory.
        XCTAssertEqual(mock.hopSeq, 1,
                       "Mock hopSeq should be 1")
        XCTAssertEqual(mock.timestamp, 1000,
                       "Mock timestamp should be 1000")
    }

    /// Mock should have expected RMS and flux values.
    func testMockRMSAndFluxValues() {
        let mock = AudioMetricsFrame.mock

        XCTAssertEqual(mock.rms, Float(0.5), accuracy: Float(0.001))
        XCTAssertEqual(mock.flux, Float(0.3), accuracy: Float(0.001))
        XCTAssertEqual(mock.fastRMS, Float(0.6), accuracy: Float(0.001))
        XCTAssertEqual(mock.fastFlux, Float(0.4), accuracy: Float(0.001))
    }

    /// Mock beat tracking fields should match the factory values.
    func testMockBeatTrackingValues() {
        let mock = AudioMetricsFrame.mock

        XCTAssertEqual(mock.bpmSmoothed, Float(120.0), accuracy: Float(0.001),
                       "Mock BPM should be 120")
        XCTAssertEqual(mock.tempoConfidence, Float(0.85), accuracy: Float(0.001),
                       "Mock tempo confidence should be 0.85")
        XCTAssertEqual(mock.beatPhase01, Float(0.25), accuracy: Float(0.001),
                       "Mock beat phase should be 0.25")
        XCTAssertEqual(mock.beatTick, 0,
                       "Mock beatTick should be 0")
        XCTAssertEqual(mock.downbeatTick, 1,
                       "Mock downbeatTick should be 1")
    }

    /// Mock should have correct array dimensions.
    func testMockArrayDimensions() {
        let mock = AudioMetricsFrame.mock

        XCTAssertEqual(mock.bands.count, 8)
        XCTAssertEqual(mock.heavyBands.count, 8)
        XCTAssertEqual(mock.chroma.count, 12)
        XCTAssertEqual(mock.heavyChroma.count, 12)
        XCTAssertEqual(mock.waveform.count, 128)
    }

    /// Mock waveform should be all zeroes (Int16(0) / Int16.max == 0.0).
    func testMockWaveformIsZero() {
        let mock = AudioMetricsFrame.mock

        for (index, sample) in mock.waveform.enumerated() {
            XCTAssertEqual(sample, Float(0.0), accuracy: Float(0.001),
                           "Waveform sample \(index) should be zero")
        }
    }

    // MARK: - Helpers

    /// Builds a valid 464-byte frame with configurable field values.
    private func makeValidFrame(
        hopSeq: UInt32 = 1,
        timestamp: UInt32 = 1000,
        rms: Float = 0.5,
        flux: Float = 0.3,
        fastRMS: Float = 0.6,
        fastFlux: Float = 0.4,
        bpmSmoothed: Float = 120.0,
        tempoConfidence: Float = 0.85,
        beatPhase01: Float = 0.25,
        beatTick: UInt8 = 0,
        downbeatTick: UInt8 = 1
    ) -> Data {
        var data = Data(count: AudioMetricsFrame.frameSize)

        data.withUnsafeMutableBytes { buffer in
            buffer.storeBytes(of: AudioMetricsFrame.magic, toByteOffset: 0, as: UInt32.self)
            buffer.storeBytes(of: hopSeq, toByteOffset: 4, as: UInt32.self)
            buffer.storeBytes(of: timestamp, toByteOffset: 8, as: UInt32.self)
            buffer.storeBytes(of: rms, toByteOffset: 12, as: Float.self)
            buffer.storeBytes(of: flux, toByteOffset: 16, as: Float.self)
            buffer.storeBytes(of: fastRMS, toByteOffset: 20, as: Float.self)
            buffer.storeBytes(of: fastFlux, toByteOffset: 24, as: Float.self)

            // Write frequency bands (8 floats at offset 28)
            for i in 0..<8 {
                buffer.storeBytes(of: Float(0.1 * Float(i)), toByteOffset: 28 + i * 4, as: Float.self)
            }
            // Heavy bands (8 floats at offset 60)
            for i in 0..<8 {
                buffer.storeBytes(of: Float(0.05 * Float(i)), toByteOffset: 60 + i * 4, as: Float.self)
            }
            // Chroma (12 floats at offset 92)
            for i in 0..<12 {
                buffer.storeBytes(of: Float(0.08 * Float(i)), toByteOffset: 92 + i * 4, as: Float.self)
            }
            // Heavy chroma (12 floats at offset 140)
            for i in 0..<12 {
                buffer.storeBytes(of: Float(0.04 * Float(i)), toByteOffset: 140 + i * 4, as: Float.self)
            }
            // Waveform (128 Int16 samples at offset 192)
            for i in 0..<128 {
                buffer.storeBytes(of: Int16(0), toByteOffset: 192 + i * 2, as: Int16.self)
            }

            buffer.storeBytes(of: bpmSmoothed, toByteOffset: 448, as: Float.self)
            buffer.storeBytes(of: tempoConfidence, toByteOffset: 452, as: Float.self)
            buffer.storeBytes(of: beatPhase01, toByteOffset: 456, as: Float.self)
            buffer.storeBytes(of: beatTick, toByteOffset: 460, as: UInt8.self)
            buffer.storeBytes(of: downbeatTick, toByteOffset: 461, as: UInt8.self)
        }

        return data
    }
}
