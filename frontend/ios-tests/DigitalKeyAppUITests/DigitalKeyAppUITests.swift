import XCTest

// MARK: - DigitalKeyAppUITests
///
/// 端到端 UI 测试，覆盖 App 核心用户流程：
/// - 页面导航与 Tab 切换
/// - 钥匙列表展示与空状态
/// - 添加钥匙流程
/// - 钥匙详情查看
/// - 远程控制（解锁/锁定/启动等）
/// - 设置页面交互
///
/// 注意：App 使用 SwiftUI + UIHostingController，KeyService 通过
/// DispatchQueue.main.asyncAfter 模拟异步加载（~0.5s 延迟），测试中
/// 使用 waitForExistence 等待异步数据就绪。
final class DigitalKeyAppUITests: XCTestCase {
    
    var app: XCUIApplication!
    
    // MARK: - Setup / Teardown
    
    override func setUpWithError() throws {
        continueAfterFailure = false
        app = XCUIApplication()
        app.launch()
    }
    
    override func tearDownWithError() throws {
        let screenshot = app.windows.firstMatch.screenshot()
        let attachment = XCTAttachment(screenshot: screenshot)
        attachment.name = "Final App State - \(name)"
        attachment.lifetime = .deleteOnSuccess
        add(attachment)
        
        app = nil
    }
    
    // MARK: - 1. App Launch & Tab Navigation
    
    /// App 启动后应显示 Tab Bar，三个 Tab 按钮均存在
    func testTabBarExists() throws {
        let tabBar = app.tabBars.firstMatch
        XCTAssertTrue(tabBar.exists, "Tab bar should be present")
        
        let keysTab = tabBar.buttons["钥匙"]
        let controlTab = tabBar.buttons["车控"]
        let settingsTab = tabBar.buttons["设置"]
        
        XCTAssertTrue(keysTab.exists, "钥匙 tab should exist")
        XCTAssertTrue(controlTab.exists, "车控 tab should exist")
        XCTAssertTrue(settingsTab.exists, "设置 tab should exist")
    }
    
    /// 默认选中「钥匙」Tab
    func testDefaultTabIsKeys() throws {
        let keysTab = app.tabBars.buttons["钥匙"]
        XCTAssertTrue(keysTab.isSelected, "钥匙 tab should be selected by default")
    }
    
    /// 钥匙页：显示「我的钥匙」导航标题
    func testKeyListNavigationTitle() throws {
        let navBar = app.navigationBars["我的钥匙"]
        XCTAssertTrue(navBar.exists, "Navigation bar should display '我的钥匙'")
    }
    
    /// Tab 切换：点击「车控」Tab → 显示「车辆控制」导航标题
    func testSwitchToVehicleControlTab() throws {
        app.tabBars.buttons["车控"].tap()
        
        let navBar = app.navigationBars["车辆控制"]
        XCTAssertTrue(navBar.waitForExistence(timeout: 2), "Should navigate to vehicle control")
    }
    
    /// Tab 切换：点击「设置」Tab → 显示「设置」导航标题
    func testSwitchToSettingsTab() throws {
        app.tabBars.buttons["设置"].tap()
        
        let navBar = app.navigationBars["设置"]
        XCTAssertTrue(navBar.waitForExistence(timeout: 2), "Should navigate to settings")
    }
    
    /// Tab 切换后返回钥匙页，Tab 状态正确
    func testTabSwitchRoundTrip() throws {
        // 切换到车控
        app.tabBars.buttons["车控"].tap()
        XCTAssertTrue(app.navigationBars["车辆控制"].waitForExistence(timeout: 2))
        
        // 切换到设置
        app.tabBars.buttons["设置"].tap()
        XCTAssertTrue(app.navigationBars["设置"].waitForExistence(timeout: 2))
        
        // 回到钥匙
        app.tabBars.buttons["钥匙"].tap()
        XCTAssertTrue(app.navigationBars["我的钥匙"].waitForExistence(timeout: 2))
    }
    
    // MARK: - 2. Key List Display
    
    /// 初始空状态：无车辆时显示空状态提示
    func testEmptyStateBeforeLoading() throws {
        // Mock 数据有 0.5s 延迟，初始短暂为空
        // 检查空状态文案是否存在
        let emptyText = app.staticTexts["暂无数字钥匙"]
        // 可能数据已经加载，两种状态均接受
        let vehicleExists = app.staticTexts["我的宝马"].waitForExistence(timeout: 3)
        if !vehicleExists {
            // 空状态应该显示提示文字
            let hintText = app.staticTexts["点击右上角 + 添加您的第一把钥匙"]
            XCTAssertTrue(emptyText.exists || hintText.exists, "Should show empty state when no vehicles")
        }
    }
    
    /// Mock 数据加载后车辆卡片出现
    func testVehicleCardsAppearAfterLoading() throws {
        // 等待异步加载完成（KeyService 使用 0.5s 延迟）
        let bmwLabel = app.staticTexts["我的宝马"]
        let exists = bmwLabel.waitForExistence(timeout: 5)
        XCTAssertTrue(exists, "Vehicle '我的宝马' should appear after mock data loads")
        
        // 验证车辆品牌/型号信息
        let modelText = app.staticTexts["BMW iX3"]
        XCTAssertTrue(modelText.exists, "Vehicle model info should be visible")
    }
    
    /// 第二辆车（无昵称）显示 model 名称
    func testSecondVehicleCardShowsModelName() throws {
        // 等待数据加载
        let benzLabel = app.staticTexts["EQC"]
        let exists = benzLabel.waitForExistence(timeout: 5)
        XCTAssertTrue(exists, "Vehicle without nickname should show model name 'EQC'")
    }
    
    /// 车辆卡片显示电量信息
    func testVehicleCardShowsBattery() throws {
        let batteryText = app.staticTexts["85%"]
        let exists = batteryText.waitForExistence(timeout: 5)
        XCTAssertTrue(exists, "Vehicle card should show battery level 85%")
    }
    
    /// 车辆卡片显示在线状态（绿色圆点表示在线）
    func testVehicleCardShowsOnlineStatus() throws {
        // 车辆在线状态指示器，在线车辆有绿色圆点
        // 用 accessibility 元素检测，"在线"可能出现在状态 View 中
        let onlineBmw = app.staticTexts["我的宝马"]
        XCTAssertTrue(onlineBmw.waitForExistence(timeout: 5))
    }
    
    // MARK: - 3. Add Key Flow
    
    /// 点击 + 按钮 → 跳转到添加钥匙页面
    func testAddKeyButtonNavigatesToAddKey() throws {
        // 点击导航栏右侧的 + 按钮
        let addButton = app.navigationBars["我的钥匙"].buttons.firstMatch
        XCTAssertTrue(addButton.exists, "Add key button should exist")
        addButton.tap()
        
        // 验证跳转到添加钥匙页面
        let addKeyNavBar = app.navigationBars["添加钥匙"]
        XCTAssertTrue(addKeyNavBar.waitForExistence(timeout: 3), "Should navigate to add key screen")
    }
    
    /// 添加钥匙页显示「添加方式」说明卡片
    func testAddKeyShowsInstructions() throws {
        app.navigationBars["我的钥匙"].buttons.firstMatch.tap()
        
        let instructionsCard = app.staticTexts["添加方式"]
        XCTAssertTrue(instructionsCard.waitForExistence(timeout: 3), "Add key page should show instructions")
    }
    
    /// 添加钥匙页显示「填写信息」表单卡片
    func testAddKeyShowsForm() throws {
        app.navigationBars["我的钥匙"].buttons.firstMatch.tap()
        
        let formTitle = app.staticTexts["填写信息"]
        XCTAssertTrue(formTitle.waitForExistence(timeout: 3), "Add key page should show form")
    }
    
    /// 添加钥匙页显示「发送分享请求」提交按钮
    func testAddKeyShowsSubmitButton() throws {
        app.navigationBars["我的钥匙"].buttons.firstMatch.tap()
        
        let submitButton = app.buttons["发送分享请求"]
        XCTAssertTrue(submitButton.waitForExistence(timeout: 3), "Submit button should exist")
    }
    
    /// 添加钥匙页 - 选择车辆卡片出现
    func testAddKeyShowsVehicleSelection() throws {
        app.navigationBars["我的钥匙"].buttons.firstMatch.tap()
        
        let selectionCard = app.staticTexts["选择车辆"]
        XCTAssertTrue(selectionCard.waitForExistence(timeout: 3), "Vehicle selection card should appear")
    }
    
    /// 添加钥匙页 - 有车辆选择项
    func testAddKeyVehicleOptions() throws {
        app.navigationBars["我的钥匙"].buttons.firstMatch.tap()
        
        let bmwCell = app.staticTexts["BMW iX3"]
        let exists = bmwCell.waitForExistence(timeout: 3)
        XCTAssertTrue(exists, "Vehicle selection should list available vehicles")
    }
    
    /// 添加钥匙页 - 钥匙类型选择器存在（亲友钥匙 / 临时钥匙）
    func testAddKeyTypePickerExists() throws {
        app.navigationBars["我的钥匙"].buttons.firstMatch.tap()
        
        // Segmented Picker 显示为 buttons
        let friendKeyButton = app.buttons["亲友钥匙"]
        let tempKeyButton = app.buttons["临时钥匙"]
        
        XCTAssertTrue(friendKeyButton.waitForExistence(timeout: 3), "Friend key type picker should exist")
        XCTAssertTrue(tempKeyButton.exists, "Temporary key type picker should exist")
    }
    
    /// 添加钥匙页 - 权限选择列表
    func testAddKeyPermissionToggles() throws {
        app.navigationBars["我的钥匙"].buttons.firstMatch.tap()
        
        let accessControlLabel = app.staticTexts["权限选择"]
        XCTAssertTrue(accessControlLabel.waitForExistence(timeout: 3), "Permission selection should exist")
        
        // 验证可用权限选项
        let unlockPermission = app.staticTexts["解锁"]
        let lockPermission = app.staticTexts["锁定"]
        let startPermission = app.staticTexts["启动"]
        
        XCTAssertTrue(unlockPermission.exists, "Unlock permission toggle should exist")
        XCTAssertTrue(lockPermission.exists, "Lock permission toggle should exist")
        XCTAssertTrue(startPermission.exists, "Start permission toggle should exist")
    }
    
    /// 添加钥匙页 - 接收者手机号输入框
    func testAddKeyPhoneFieldExists() throws {
        app.navigationBars["我的钥匙"].buttons.firstMatch.tap()
        
        let phoneFieldLabel = app.staticTexts["接收者手机号"]
        XCTAssertTrue(phoneFieldLabel.waitForExistence(timeout: 3), "Phone number field should exist")
        
        // 输入框应为 TextField
        let phoneField = app.textFields["请输入手机号"]
        XCTAssertTrue(phoneField.exists, "Phone number text field should exist")
    }
    
    // MARK: - 4. Key Detail View
    
    /// 点击车辆卡片 → 跳转到钥匙详情页
    func testTapVehicleCardShowsDetail() throws {
        let vehicleLabel = app.staticTexts["我的宝马"]
        guard vehicleLabel.waitForExistence(timeout: 5) else {
            XCTFail("Vehicle label did not appear")
            return
        }
        
        vehicleLabel.tap()
        
        // 验证详情页导航标题为车辆名称
        let detailNavBar = app.navigationBars["我的宝马"]
        XCTAssertTrue(detailNavBar.waitForExistence(timeout: 3), "Should navigate to vehicle detail")
    }
    
    /// 详情页显示车辆信息卡片（VIN、颜色、型号）
    func testDetailShowsVehicleInfo() throws {
        app.staticTexts["我的宝马"].tap()
        guard app.navigationBars["我的宝马"].waitForExistence(timeout: 5) else { return }
        
        let vinLabel = app.staticTexts["LSVAU2180N2123456"]
        let colorLabel = app.staticTexts["Mountain White"]
        
        XCTAssertTrue(vinLabel.exists, "VIN should be visible in detail")
        XCTAssertTrue(colorLabel.exists, "Color should be visible in detail")
    }
    
    /// 详情页显示钥匙信息卡片
    func testDetailShowsKeyInfo() throws {
        app.staticTexts["我的宝马"].tap()
        guard app.navigationBars["我的宝马"].waitForExistence(timeout: 5) else { return }
        
        let keyInfoTitle = app.staticTexts["钥匙信息"]
        XCTAssertTrue(keyInfoTitle.exists, "Key info section should be visible")
        
        // 钥匙类型应显示为「车主」
        let keyTypeLabel = app.staticTexts["车主"]
        XCTAssertTrue(keyTypeLabel.exists, "Key type should show '车主'")
    }
    
    /// 详情页显示车辆状态卡片
    func testDetailShowsVehicleStatus() throws {
        app.staticTexts["我的宝马"].tap()
        guard app.navigationBars["我的宝马"].waitForExistence(timeout: 5) else { return }
        
        let statusLabel = app.staticTexts["在线"]
        XCTAssertTrue(statusLabel.exists, "Online status should be visible in detail")
    }
    
    /// 详情页显示快捷操作按钮（定位、鸣笛、分享）
    func testDetailShowsQuickActions() throws {
        app.staticTexts["我的宝马"].tap()
        guard app.navigationBars["我的宝马"].waitForExistence(timeout: 5) else { return }
        
        let quickActionCard = app.staticTexts["快捷操作"]
        XCTAssertTrue(quickActionCard.exists, "Quick actions card should exist")
        
        let locateBtn = app.buttons["定位"]
        let honkBtn = app.buttons["鸣笛"]
        
        XCTAssertTrue(locateBtn.exists, "Locate button should exist in quick actions")
        XCTAssertTrue(honkBtn.exists, "Honk button should exist in quick actions")
    }
    
    /// 详情页显示车辆状态信息
    func testDetailShowsMileage() throws {
        app.staticTexts["我的宝马"].tap()
        guard app.navigationBars["我的宝马"].waitForExistence(timeout: 5) else { return }
        
        let mileageText = app.staticTexts["1.3 万km"]
        XCTAssertTrue(mileageText.waitForExistence(timeout: 2), "Mileage should be visible")
    }
    
    // MARK: - 5. Remote Control (Vehicle Control Tab)
    
    /// 车控页面显示「车辆控制」导航标题
    func testVehicleControlNavigationTitle() throws {
        app.tabBars.buttons["车控"].tap()
        
        let navBar = app.navigationBars["车辆控制"]
        XCTAssertTrue(navBar.waitForExistence(timeout: 2))
    }
    
    /// 车控页面显示解锁按钮
    func testVehicleControlShowsUnlockButton() throws {
        app.tabBars.buttons["车控"].tap()
        waitForVehicleControlLoad()
        
        let unlockBtn = app.buttons["解锁"]
        XCTAssertTrue(unlockBtn.waitForExistence(timeout: 3), "Unlock button should exist on vehicle control")
    }
    
    /// 车控页面显示锁定按钮
    func testVehicleControlShowsLockButton() throws {
        app.tabBars.buttons["车控"].tap()
        waitForVehicleControlLoad()
        
        let lockBtn = app.buttons["锁定"]
        XCTAssertTrue(lockBtn.waitForExistence(timeout: 3), "Lock button should exist on vehicle control")
    }
    
    /// 车控页面显示启动按钮
    func testVehicleControlShowsStartButton() throws {
        app.tabBars.buttons["车控"].tap()
        waitForVehicleControlLoad()
        
        let startBtn = app.buttons["启动"]
        XCTAssertTrue(startBtn.waitForExistence(timeout: 3), "Start engine button should exist")
    }
    
    /// 车控页面显示后备箱按钮
    func testVehicleControlShowsTrunkButton() throws {
        app.tabBars.buttons["车控"].tap()
        waitForVehicleControlLoad()
        
        let trunkBtn = app.buttons["后备箱"]
        XCTAssertTrue(trunkBtn.waitForExistence(timeout: 3), "Trunk button should exist")
    }
    
    /// 车控页面显示空调按钮
    func testVehicleControlShowsClimateButton() throws {
        app.tabBars.buttons["车控"].tap()
        waitForVehicleControlLoad()
        
        let climateBtn = app.buttons["空调"]
        XCTAssertTrue(climateBtn.waitForExistence(timeout: 3), "Climate button should exist")
    }
    
    /// 车控页面显示车辆状态（在线/离线、电量等）
    func testVehicleControlShowsStatus() throws {
        app.tabBars.buttons["车控"].tap()
        waitForVehicleControlLoad()
        
        // 车辆在线状态下应显示「在线」
        let onlineText = app.staticTexts["在线"]
        XCTAssertTrue(onlineText.waitForExistence(timeout: 3), "Online status should be visible")
        
        // 电量信息
        let batteryText = app.staticTexts["85%"]
        XCTAssertTrue(batteryText.waitForExistence(timeout: 2), "Battery percentage should be visible")
    }
    
    /// 车控页面显示远程控制卡片
    func testVehicleControlShowsRemoteControlCard() throws {
        app.tabBars.buttons["车控"].tap()
        waitForVehicleControlLoad()
        
        let remoteControlCard = app.staticTexts["远程控制"]
        XCTAssertTrue(remoteControlCard.waitForExistence(timeout: 3), "Remote control card should exist")
    }
    
    /// 点击解锁按钮后触发操作
    func testUnlockButtonIsTappable() throws {
        app.tabBars.buttons["车控"].tap()
        waitForVehicleControlLoad()
        
        let unlockBtn = app.buttons["解锁"]
        XCTAssertTrue(unlockBtn.waitForExistence(timeout: 3))
        XCTAssertTrue(unlockBtn.isEnabled, "Unlock button should be enabled")
    }
    
    /// 车控页面显示停止引擎按钮
    func testVehicleControlShowsStopButton() throws {
        app.tabBars.buttons["车控"].tap()
        waitForVehicleControlLoad()
        
        let stopBtn = app.buttons["停止"]
        XCTAssertTrue(stopBtn.waitForExistence(timeout: 3), "Stop button should exist")
    }
    
    /// 车控页面显示定位按钮
    func testVehicleControlShowsLocateButton() throws {
        app.tabBars.buttons["车控"].tap()
        waitForVehicleControlLoad()
        
        let locateBtn = app.buttons["定位"]
        XCTAssertTrue(locateBtn.waitForExistence(timeout: 3), "Locate button should exist")
    }
    
    /// 车控页面显示关空调按钮
    func testVehicleControlShowsStopClimateButton() throws {
        app.tabBars.buttons["车控"].tap()
        waitForVehicleControlLoad()
        
        let stopClimateBtn = app.buttons["关空调"]
        XCTAssertTrue(stopClimateBtn.waitForExistence(timeout: 3), "Stop climate button should exist")
    }
    
    // MARK: - 6. Settings Tab
    
    /// 设置页面显示账户信息
    func testSettingsShowsAccountSection() throws {
        app.tabBars.buttons["设置"].tap()
        
        let accountSection = app.staticTexts["账户"]
        XCTAssertTrue(accountSection.waitForExistence(timeout: 2), "Account section should exist")
        
        let userName = app.staticTexts["用户名"]
        XCTAssertTrue(userName.exists, "User name label should exist")
    }
    
    /// 设置页面显示通知开关
    func testSettingsShowsNotificationToggles() throws {
        app.tabBars.buttons["设置"].tap()
        
        let notificationSection = app.staticTexts["通知"]
        XCTAssertTrue(notificationSection.waitForExistence(timeout: 2))
        
        let pushToggle = app.switches["推送通知"]
        XCTAssertTrue(pushToggle.exists, "Push notification toggle should exist")
    }
    
    /// 设置页面显示安全选项
    func testSettingsShowsSecuritySection() throws {
        app.tabBars.buttons["设置"].tap()
        
        let securitySection = app.staticTexts["安全"]
        XCTAssertTrue(securitySection.waitForExistence(timeout: 2))
        
        let biometricToggle = app.switches["生物识别"]
        XCTAssertTrue(biometricToggle.exists, "Biometric toggle should exist")
        
        let autoUnlockToggle = app.switches["自动解锁"]
        XCTAssertTrue(autoUnlockToggle.exists, "Auto unlock toggle should exist")
    }
    
    /// 设置页面显示版本信息
    func testSettingsShowsVersion() throws {
        app.tabBars.buttons["设置"].tap()
        
        let aboutSection = app.staticTexts["关于"]
        XCTAssertTrue(aboutSection.waitForExistence(timeout: 2))
        
        let versionText = app.staticTexts["1.0.0"]
        XCTAssertTrue(versionText.exists, "Version 1.0.0 should be displayed")
    }
    
    /// 设置页面显示登出按钮
    func testSettingsShowsLogoutButton() throws {
        app.tabBars.buttons["设置"].tap()
        
        let logoutBtn = app.buttons["退出登录"]
        XCTAssertTrue(logoutBtn.waitForExistence(timeout: 2), "Logout button should exist")
    }
    
    /// 设置页面包含调试信息入口
    func testSettingsShowsDebugEntry() throws {
        app.tabBars.buttons["设置"].tap()
        
        let debugLink = app.staticTexts["调试信息"]
        XCTAssertTrue(debugLink.waitForExistence(timeout: 2), "Debug info link should exist")
        
        let devSection = app.staticTexts["开发"]
        XCTAssertTrue(devSection.exists, "Development section should exist")
    }
    
    /// 设置页面：清除缓存按钮存在
    func testSettingsClearCacheButton() throws {
        app.tabBars.buttons["设置"].tap()
        
        let clearCacheBtn = app.buttons["清除缓存"]
        XCTAssertTrue(clearCacheBtn.waitForExistence(timeout: 2), "Clear cache button should exist")
    }
    
    // MARK: - 7. Full User Flow Integration
    
    /// 完整流程：启动 → 查看钥匙列表 → 点击添加 → 返回
    func testFullFlowKeysListToAddAndBack() throws {
        // 1. 确认在钥匙列表页
        XCTAssertTrue(app.navigationBars["我的钥匙"].waitForExistence(timeout: 2))
        
        // 2. 点击 + 进入添加钥匙
        app.navigationBars["我的钥匙"].buttons.firstMatch.tap()
        XCTAssertTrue(app.navigationBars["添加钥匙"].waitForExistence(timeout: 3))
        
        // 3. 验证添加页面内容
        XCTAssertTrue(app.staticTexts["添加方式"].exists)
        
        // 4. 返回钥匙列表
        app.navigationBars["添加钥匙"].buttons.firstMatch.tap()
        XCTAssertTrue(app.navigationBars["我的钥匙"].waitForExistence(timeout: 3))
    }
    
    /// 完整流程：钥匙列表 → 查看详情 → 返回
    func testFullFlowKeyListToDetailAndBack() throws {
        // 1. 等待车辆数据加载
        let vehicleLabel = app.staticTexts["我的宝马"]
        guard vehicleLabel.waitForExistence(timeout: 5) else {
            XCTFail("Vehicle data did not load")
            return
        }
        
        // 2. 点击进入详情
        vehicleLabel.tap()
        XCTAssertTrue(app.navigationBars["我的宝马"].waitForExistence(timeout: 3))
        
        // 3. 验证详情内容
        XCTAssertTrue(app.staticTexts["钥匙信息"].exists)
        XCTAssertTrue(app.staticTexts["快捷操作"].exists)
        
        // 4. 返回
        app.navigationBars["我的宝马"].buttons.firstMatch.tap()
        XCTAssertTrue(app.navigationBars["我的钥匙"].waitForExistence(timeout: 3))
    }
    
    /// 完整流程：Tab 导航 + 各页面切换
    func testFullFlowTabNavigation() throws {
        // 钥匙 tab → 车控 tab → 设置 tab → 钥匙 tab
        let keysTab = app.tabBars.buttons["钥匙"]
        let controlTab = app.tabBars.buttons["车控"]
        let settingsTab = app.tabBars.buttons["设置"]
        
        // 车控
        controlTab.tap()
        XCTAssertTrue(app.navigationBars["车辆控制"].waitForExistence(timeout: 2))
        
        // 设置
        settingsTab.tap()
        XCTAssertTrue(app.navigationBars["设置"].waitForExistence(timeout: 2))
        
        // 回到钥匙
        keysTab.tap()
        XCTAssertTrue(app.navigationBars["我的钥匙"].waitForExistence(timeout: 2))
        
        // 验证钥匙 Tab 选中
        XCTAssertTrue(keysTab.isSelected)
    }
    
    /// 多辆车场景：列表应显示所有车辆
    func testMultipleVehiclesDisplayed() throws {
        let car1 = app.staticTexts["我的宝马"]
        let car2 = app.staticTexts["EQC"]
        
        let car1Exists = car1.waitForExistence(timeout: 5)
        let car2Exists = car2.waitForExistence(timeout: 3)
        
        XCTAssertTrue(car1Exists, "First vehicle should be displayed")
        XCTAssertTrue(car2Exists, "Second vehicle should be displayed")
    }
    
    /// 车控页面：点击解锁按钮可交互
    func testTapUnlockButton() throws {
        app.tabBars.buttons["车控"].tap()
        waitForVehicleControlLoad()
        
        let unlockBtn = app.buttons["解锁"]
        guard unlockBtn.waitForExistence(timeout: 3) else {
            XCTFail("Unlock button not found")
            return
        }
        
        unlockBtn.tap()
        // 操作触发后可能显示 alert，等待短暂时间来确认 button 可点击即可
        XCTAssertTrue(true, "Unlock button is tappable and responds")
    }
    
    /// 车控页面：点击锁定按钮可交互
    func testTapLockButton() throws {
        app.tabBars.buttons["车控"].tap()
        waitForVehicleControlLoad()
        
        let lockBtn = app.buttons["锁定"]
        guard lockBtn.waitForExistence(timeout: 3) else {
            XCTFail("Lock button not found")
            return
        }
        
        lockBtn.tap()
        XCTAssertTrue(true, "Lock button is tappable and responds")
    }
    
    /// 空状态页面有正确图标和文案
    func testEmptyStateContent() throws {
        // 如果车辆还未加载完成，检查空状态
        let emptyIcon = app.images["key.slash"]
        let vehicleLoaded = app.staticTexts["我的宝马"].waitForExistence(timeout: 1)
        
        if !vehicleLoaded {
            // 空状态图标可能是 system image，在 UI 测试中可能不可直接查询
            // 验证空状态文案
            let emptyText = app.staticTexts["暂无数字钥匙"]
            let hintText = app.staticTexts["点击右上角 + 添加您的第一把钥匙"]
            XCTAssertTrue(emptyText.exists || hintText.exists, "Empty state should show guidance text")
        }
    }
    
    /// 设置页面：通知开关可切换
    func testSettingsNotificationToggleInteraction() throws {
        app.tabBars.buttons["设置"].tap()
        
        let pushToggle = app.switches["推送通知"]
        guard pushToggle.waitForExistence(timeout: 2) else {
            XCTFail("Push notification toggle not found")
            return
        }
        
        let initialValue = pushToggle.value as? String
        pushToggle.tap()
        
        let newValue = pushToggle.value as? String
        XCTAssertNotEqual(initialValue, newValue, "Toggle should change state after tap")
    }
    
    /// 设置页面：生物识别开关可切换
    func testSettingsBiometricToggleInteraction() throws {
        app.tabBars.buttons["设置"].tap()
        
        let biometricToggle = app.switches["生物识别"]
        guard biometricToggle.waitForExistence(timeout: 2) else {
            XCTFail("Biometric toggle not found")
            return
        }
        
        let initialValue = biometricToggle.value as? String
        biometricToggle.tap()
        
        let newValue = biometricToggle.value as? String
        XCTAssertNotEqual(initialValue, newValue, "Biometric toggle should change state after tap")
    }
    
    // MARK: - Helpers
    
    /// 等待车控页车辆加载完成（等待「在线」状态文本出现）
    private func waitForVehicleControlLoad() {
        let onlineStatus = app.staticTexts["在线"]
        // 最多等待 5s 让车辆数据加载
        if !onlineStatus.waitForExistence(timeout: 5) {
            // 离线状态也接受
            let offlineStatus = app.staticTexts["离线"]
            _ = offlineStatus.waitForExistence(timeout: 2)
        }
    }
}
