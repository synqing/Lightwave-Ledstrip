//
//  LightwaveOSUITests.swift
//  LightwaveOSUITests
//
//  Minimal UI test placeholder for the LightwaveOS app.
//

import XCTest

final class LightwaveOSUITests: XCTestCase {

    /// Verify the application launches without crashing.
    @MainActor
    func testAppLaunches() throws {
        let app = XCUIApplication()
        app.launch()
        XCTAssertTrue(app.exists)
    }
}
