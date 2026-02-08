//
//  LightwaveOSUITests.swift
//  LightwaveOSUITests
//
//  Placeholder for UI tests.
//

import XCTest

final class LightwaveOSUITests: XCTestCase {
    override func setUpWithError() throws {
        continueAfterFailure = false
    }

    func testAppLaunches() throws {
        let app = XCUIApplication()
        app.launch()

        // Verify app launched successfully
        XCTAssertTrue(app.wait(for: .runningForeground, timeout: 5))
    }
}
