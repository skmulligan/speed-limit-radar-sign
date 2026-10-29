import XCTest

final class NavigationTests: XCTestCase {
  func testOfflineScreensAndUnavailableBluetooth() {
    let app = XCUIApplication()
    app.launchArguments = ["--ui-testing"]
    app.launch()
    XCTAssertTrue(app.navigationBars["Live radar"].waitForExistence(timeout: 10))
    XCTAssertTrue(app.staticTexts["No current speed"].exists)
    app.tabBars.buttons["History"].tap()
    XCTAssertTrue(app.navigationBars["History"].exists)
    XCTAssertTrue(app.staticTexts["No episodes"].exists)
    app.tabBars.buttons["Trends"].tap()
    XCTAssertTrue(app.navigationBars["Trends"].exists)
    app.tabBars.buttons["Device"].tap()
    XCTAssertFalse(app.buttons["Sync now"].isEnabled)
    app.buttons["Find nearby sign"].tap()
    XCTAssertTrue(app.alerts["Street Speed"].waitForExistence(timeout: 2))
    app.alerts.buttons["OK"].tap()
    app.tabBars.buttons["Radar"].tap()
    let attachment = XCTAttachment(screenshot: app.screenshot())
    attachment.name = "Radar disconnected"
    attachment.lifetime = .keepAlways
    add(attachment)
  }
}
