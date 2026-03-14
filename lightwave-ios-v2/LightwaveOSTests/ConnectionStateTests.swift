//
//  ConnectionStateTests.swift
//  LightwaveOSTests
//
//  Unit tests for AppViewModel.ConnectionState enum behaviour.
//

import XCTest
@testable import LightwaveOS

final class ConnectionStateTests: XCTestCase {

    // MARK: - Case Existence

    /// Verify all expected cases can be instantiated without error.
    func testAllCasesExist() {
        let disconnected: AppViewModel.ConnectionState = .disconnected
        let discovering: AppViewModel.ConnectionState = .discovering
        let connecting: AppViewModel.ConnectionState = .connecting
        let connected: AppViewModel.ConnectionState = .connected
        let error: AppViewModel.ConnectionState = .error("test")

        // If any case were missing, the compiler would reject this file.
        // Use XCTAssertNotNil on a boxed optional to silence unused-variable warnings.
        XCTAssertNotNil(Optional(disconnected))
        XCTAssertNotNil(Optional(discovering))
        XCTAssertNotNil(Optional(connecting))
        XCTAssertNotNil(Optional(connected))
        XCTAssertNotNil(Optional(error))
    }

    // MARK: - Equatable Conformance

    /// Two identical simple cases should be equal.
    func testEquatableSimpleCases() {
        XCTAssertEqual(
            AppViewModel.ConnectionState.disconnected,
            AppViewModel.ConnectionState.disconnected
        )
        XCTAssertEqual(
            AppViewModel.ConnectionState.discovering,
            AppViewModel.ConnectionState.discovering
        )
        XCTAssertEqual(
            AppViewModel.ConnectionState.connecting,
            AppViewModel.ConnectionState.connecting
        )
        XCTAssertEqual(
            AppViewModel.ConnectionState.connected,
            AppViewModel.ConnectionState.connected
        )
    }

    /// Different simple cases should not be equal.
    func testNotEqualDifferentCases() {
        XCTAssertNotEqual(
            AppViewModel.ConnectionState.disconnected,
            AppViewModel.ConnectionState.connected
        )
        XCTAssertNotEqual(
            AppViewModel.ConnectionState.discovering,
            AppViewModel.ConnectionState.connecting
        )
    }

    /// Error cases with the same message should be equal.
    func testErrorCaseEqualWithSameMessage() {
        XCTAssertEqual(
            AppViewModel.ConnectionState.error("timeout"),
            AppViewModel.ConnectionState.error("timeout")
        )
    }

    /// Error cases with different messages should not be equal.
    func testErrorCaseNotEqualWithDifferentMessages() {
        XCTAssertNotEqual(
            AppViewModel.ConnectionState.error("timeout"),
            AppViewModel.ConnectionState.error("connection refused")
        )
    }

    /// Error case should not equal a simple case.
    func testErrorCaseNotEqualToSimpleCase() {
        XCTAssertNotEqual(
            AppViewModel.ConnectionState.error("disconnected"),
            AppViewModel.ConnectionState.disconnected
        )
    }

    // MARK: - Error Associated Value

    /// Error case carries the correct associated String value.
    func testErrorAssociatedValue() {
        let state = AppViewModel.ConnectionState.error("Network unreachable")

        if case .error(let message) = state {
            XCTAssertEqual(message, "Network unreachable")
        } else {
            XCTFail("Expected .error case with associated value")
        }
    }

    /// Error case with an empty string is still valid.
    func testErrorWithEmptyString() {
        let state = AppViewModel.ConnectionState.error("")

        if case .error(let message) = state {
            XCTAssertTrue(message.isEmpty,
                          "Empty error message should be preserved")
        } else {
            XCTFail("Expected .error case")
        }
    }
}
