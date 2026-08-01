// yuleDKCS SDK iOS 集成示例（最小骨架）
//
// 只展示 API 调用面，不复制 SDK 内部逻辑。
// 所有调用与 mobile/ios/Sources 下的公开 API 签名一一对应。
// 验证: swiftc -parse Sources/DemoApp/main.swift
//
// 注意: 本示例为可解析的调用面演示，真机运行需:
//   - 车厂 App 工程（Xcode）以 target 依赖方式引入 SDK 三个产物
//   - Info.plist 声明蓝牙后台/NFC/UWB 权限（见 SDK-INTEGRATION-GUIDE.md §2.11）

import Foundation
import YDKHubClient      // YDKHubClient / SDKConfig / YDKShareFlow / YDKMailboxClient
import YDKKeyManager     // YDKKeyManager / YDKKeyManagerDelegate / YDKOfflineAuthorizer
import YDKBLEManager     // YDKBLEManager / YDKUWBManaging / YDKNFCManaging / 模型

// MARK: - 钥匙变更回调（YDKKeyManagerDelegate）

final class DemoKeyDelegate: YDKKeyManagerDelegate {
    func keyManager(_ manager: YDKKeyManager, didDetectChanges changes: [KeyChange]) {
        for change in changes {
            print("钥匙变更 \(change.keyId): \(change.type.rawValue)")
        }
    }

    func keyManager(_ manager: YDKKeyManager, syncDidFailWith error: Error) {
        print("钥匙同步失败: \(error.localizedDescription)")
    }
}

// MARK: - 主流程（初始化 / 扫描 / 解锁 / 分享 / 离线授权）

let config = SDKConfig(
    hubEndpoint: "hub.yuletech.com",
    hubPort: 8080,
    platform: .iOS,
    enableLogging: true
)
let hubClient = YDKHubClient(config: config)
hubClient.setToken("session-token-from-oem-server")   // token 由车厂 Server 签发

let keyManager = YDKKeyManager(hubClient: hubClient, enableLogging: true)
let keyDelegate = DemoKeyDelegate()
keyManager.delegate = keyDelegate

let bleManager = YDKBLEManager(
    enableLogging: true,
    backgroundRestoreIdentifier: "com.yourcompany.dkcs.ble"   // 可选: 启用后台状态恢复
)
bleManager.connectionChangeHandler = { state in
    // 0=disconnected 1=scanning 2=connecting 3=connected
    print("BLE 连接状态: \(state)")
}

func demo() async throws {
    let vehicleId = "LSVX0000000000001"

    // ── 钥匙管理 ──
    let bindResp = try await hubClient.bindKey(vehicleId: vehicleId)
    print("绑定成功 keyId=\(bindResp.keyId)")

    let keys = try await hubClient.listKeys(status: "ACTIVE")
    print("有效钥匙 \(keys.count) 把")
    let key = try await hubClient.getKey(keyId: bindResp.keyId)
    print("钥匙状态: \(key.status)")

    // ── 远程控车 ──
    let resp = try await hubClient.remoteUnlock(vehicleId: vehicleId)
    if resp.resultCode == 0 {
        print("远程解锁已受理 cmdId=\(resp.cmdId ?? "")")
    }
    try await hubClient.remoteLock(vehicleId: vehicleId)
    try await hubClient.remoteStart(vehicleId: vehicleId)
    try await hubClient.remoteStop(vehicleId: vehicleId)

    // ── BLE 扫描 / 连接 / 本地解锁 ──
    let vehicles = try await bleManager.scanVehicles(timeout: 10)
    guard let nearby = vehicles.first else {
        print("未扫描到车辆")
        return
    }
    print("发现车辆 \(nearby.vehicleId) rssi=\(nearby.rssi) supportsUWB=\(nearby.supportsUWB)")

    try await bleManager.connectVehicle(vehicleId: nearby.vehicleId)

    // 离线授权裁决（Hub 不可达时 BLE 解锁的前置检查）
    if let verdict = keyManager.authorizeOfflineUse(keyId: bindResp.keyId) {
        guard verdict.allowed else {
            print("离线解锁被拒: \(verdict.reason?.rawValue ?? "unknown")")
            return
        }
    }

    try await bleManager.unlock(vehicleId: nearby.vehicleId)
    try await bleManager.lock(vehicleId: nearby.vehicleId)
    try await bleManager.startEngine(vehicleId: nearby.vehicleId)
    let status = try await bleManager.readVehicleStatus(vehicleId: nearby.vehicleId)
    print("车辆状态 locked=\(status.locked) battery=\(status.batteryPct)%")
    try await bleManager.disconnect()

    // ── 分享: Hub 分享码 ──
    let share = try await hubClient.createShare(
        keyId: bindResp.keyId,
        toVendor: "XIAOMI",
        validUntil: 1_900_000_000_000,
        maxUses: 0
    )
    if let code = share.shareCode {
        print("分享码: \(code)")
    }

    // 接收方接受分享
    let sharedKey = try await hubClient.acceptShare(shareCode: "123456")
    print("接受分享成功 keyId=\(sharedKey.keyId)")

    // 取消分享
    try await hubClient.cancelShare(shareId: share.shareId)

    // ── 分享: CCC Mailbox（CCC 协议专用）──
    let flow = YDKShareFlow(hubEndpoint: "hub.yuletech.com", port: 8080)
    let sharingURL = try await flow.shareKeyViaMailbox(
        payload: Data([0x01, 0x02, 0x03]),          // 钱包层生成的加密钥匙材料
        senderVendor: "APPLE",
        senderDeviceId: "iphone-001",
        host: "hub.yuletech.com:8080"
    )
    print("Mailbox 分享 URL: \(sharingURL)")

    let content = try await flow.acceptSharedKeyViaMailbox(
        urlString: sharingURL,
        updaterDeviceId: "xiaomi-001",
        keySigningPayload: Data([0x10]),            // 钱包层 KeySigning 材料
        importPayload: Data([0x11])                 // 钱包层 Import 确认
    )
    print("Mailbox 内容 \(content.payload?.count ?? 0) 字节")

    try await flow.senderCancelMailboxShare(mailboxId: "mb-xxx", updaterDeviceId: "iphone-001")

    // ── Push 触发的增量同步 ──
    _ = try await keyManager.handleKeyStatusPush(keyId: bindResp.keyId)

    // ── UWB 测距（真机 iOS 14+）──
    if #available(iOS 14.0, *) {
        let uwb = YDKNIUWBManager()
        uwb.rangingResultHandler = { m in
            print("UWB 距离 \(m.distance)m")
        }
        try uwb.injectPeerDiscoveryToken(data: Data([0xAA, 0xBB]))   // 车端 token（BLE 下发）
        _ = try uwb.exportLocalDiscoveryToken()                       // 本端 token（BLE 上行）
        try await uwb.startRanging(vehicleId: vehicleId)
        uwb.stopRanging()
    }

    // 无硬件/调试: Mock 测距
    let mockUwb = YDKMockUWBManager()
    mockUwb.rangingResultHandler = { m in print("Mock UWB 距离 \(m.distance)m") }
    try await mockUwb.startRanging(vehicleId: vehicleId)
    mockUwb.stopRanging()

    // ── NFC 备用解锁 ──
    let nfc = YDKCoreNFCManager(expectedTagId: "04A1B2C3D4E5F6")   // 可选绑定
    let tagInfo = try await nfc.readVehicleTag()
    print("NFC 标签 vehicleId=\(tagInfo.vehicleId) tagId=\(tagInfo.tagId)")
    try await nfc.sendCommandViaNFC(command: .unlock)   // .lock / .startEngine

    // ── 登出清理 ──
    hubClient.clearToken()
    keyManager.clearCache()
    bleManager.disconnect()
    hubClient.shutdown()
}

// 顶层入口（SE-0343: main.swift 支持顶层 await）
try await demo()
