import Foundation

// MARK: - CCC 协议适配器

/// CCC Digital Key v4.0 BLE 协议适配器
///
/// 指令帧格式基于仓库内参考实现 (embedded/ccc_protocol), 详细分析见同目录
/// `CCC_FRAME_ANALYSIS.md`:
/// - 帧头: `ble_frame_header_t` — [msg_type(1)][msg_id(1)][payload_len(2,BE)][reserved(1)]
/// - 控制指令: 消息类型 = AUTH_REQUEST (0x20), 载荷经安全提供者封装
/// - 响应: AUTH_RESPONSE (0x21) / STATE_NOTIFY (0x40) / ERROR (0xFF)
final class CCCBleAdapter: BleProtocolAdapter {

    let protocolType: YDKBleProtocolType = .ccc

    /// 消息安全提供者。
    /// ⚠️ TODO(防幻觉): 真实加密需 CCC-TS-101 Reader Protocol 安全通道细节
    /// (参考实现存在 AES-CCM 与 AES-256-GCM 冲突, 见 CCC_FRAME_ANALYSIS.md §5)。
    /// 当前默认透传实现仅用于单测/联调, 生产接入前必须替换。
    private let security: CCCMessageSecurityProviding

    init(security: CCCMessageSecurityProviding = CCCNullMessageSecurity()) {
        self.security = security
    }

    // MARK: 广告包解析

    /// 解析 CCC 车辆广告包:
    /// 1. 校验广播包含 CCC Digital Key Service (0xFFD1);
    /// 2. 解析 Manufacturer Specific Data (Apple 0x004C, iBeacon 风格) → vehicleId;
    /// 3. 无法确认 vehicleId 时返回 nil (不伪造标识)。
    func parseAdvertisement(_ advertisementData: [String: Any], rssi: Int) -> VehicleAdvertise? {
        let extracted = YDKAdvertisementParser.extract(from: advertisementData)

        // 1. service UUID 过滤: 必须包含 CCC Digital Key Service
        guard extracted.serviceUUIDs.contains(YDKBleUUIDs.cccService) else {
            return nil
        }

        // 2. manufacturer data → vehicleId (iBeacon 风格, 参考 ble_kw47a.c LP 广播)
        guard let manufacturerData = extracted.manufacturerData,
              let mfr = YDKAdvertisementParser.parseManufacturerData(manufacturerData),
              let vehicleId = YDKAdvertisementParser.cccVehicleID(from: mfr) else {
            return nil
        }

        // TODO-verify: UWB 能力位在 CCC 规范中位于配对/OOB 数据 (ccc_nfc_oob_data_t.capability),
        // 广告包不携带; 此处恒为 false, 待 OOB 配对后更新。
        return VehicleAdvertise(
            vehicleId: vehicleId,
            rssi: rssi,
            protocolType: YDKBleProtocolType.ccc.rawValue,
            supportsUWB: false,
            manufacturerData: manufacturerData
        )
    }

    // MARK: 指令构建

    func buildUnlockCommand(keyId: String, session: SessionContext) throws -> Data {
        try buildCommand(type: .unlock, keyId: keyId, session: session)
    }

    func buildLockCommand(keyId: String, session: SessionContext) throws -> Data {
        try buildCommand(type: .lock, keyId: keyId, session: session)
    }

    func buildStartEngineCommand(keyId: String, session: SessionContext) throws -> Data {
        try buildCommand(type: .engineOn, keyId: keyId, session: session)
    }

    /// 构建控制指令帧:
    /// 1. 会话层明文载荷 (CCCControlPayload, 布局见其文档注释);
    /// 2. 经安全提供者封装 (TODO: 生产需真实加密+签名);
    /// 3. 包装为 CCC 帧头 (AUTH_REQUEST), msg_id 取自 session.counter 低 8 位。
    private func buildCommand(type: BleCommandType, keyId: String, session: SessionContext) throws -> Data {
        let plaintext = CCCControlPayload.build(subcommand: type, session: session, keyId: keyId)
        let protected = try security.encrypt(plaintext)
        let frame = CCCCommandFrame(
            messageType: CCCMessageType.authRequest.rawValue,
            messageID: UInt8(truncatingIfNeeded: session.counter),
            payload: protected
        )
        return frame.data
    }

    // MARK: 响应解析

    /// 解析指令响应。优先按 CCC 帧解析 (AUTH_RESPONSE 0x21 / ERROR 0xFF);
    /// 非帧格式 (如 <5 字节的裸状态字节) 回退到 [0]=status 的旧语义。
    func parseCommandResponse(_ data: Data) throws -> CommandResult {
        guard let frame = CCCCommandFrame(data: data) else {
            // 回退: 裸 status 字节 (旧联调格式)
            guard data.count >= 1 else {
                return CommandResult(success: false, errorCode: -1, errorMessage: "empty response")
            }
            let status = data[0]
            if status == 0x00 {
                return CommandResult(success: true)
            }
            return CommandResult(success: false, errorCode: Int32(status),
                                 errorMessage: "CCC command failed: 0x\(String(format: "%02X", status))")
        }

        switch frame.messageType {
        case CCCMessageType.authResponse.rawValue,
             CCCMessageType.pairResponse.rawValue:
            guard let status = frame.payload.first else {
                return CommandResult(success: false, errorCode: -1, errorMessage: "empty response payload")
            }
            if status == 0x00 {
                return CommandResult(success: true)
            }
            return CommandResult(success: false, errorCode: Int32(status),
                                 errorMessage: "CCC command failed: 0x\(String(format: "%02X", status))")

        case CCCMessageType.error.rawValue:
            let code = frame.payload.first ?? 0xFF
            return CommandResult(success: false, errorCode: Int32(code),
                                 errorMessage: "CCC error message: 0x\(String(format: "%02X", code))")

        default:
            return CommandResult(success: false, errorCode: -1,
                                 errorMessage: "unexpected message type 0x\(String(format: "%02X", frame.messageType))")
        }
    }

    /// 解析车辆状态。优先按 CCC 帧解析 (STATE_NOTIFY 0x40);
    /// 非帧格式 (FFD5 直接读) 回退到 [locked][engineOn][batteryPct] 布局。
    /// TODO-verify: 状态载荷字段布局需对照 CCC-TS-101 Vehicle Status 定义确认。
    func parseVehicleStatus(_ data: Data) throws -> VehicleStatus {
        if let frame = CCCCommandFrame(data: data),
           frame.messageType == CCCMessageType.stateNotify.rawValue,
           frame.payload.count >= 3 {
            let p = frame.payload
            return VehicleStatus(locked: p[0] != 0, engineOn: p[1] != 0, batteryPct: Int32(p[2]))
        }

        // 回退: 裸状态字节 (占位假设, TODO-verify)
        guard data.count >= 3 else {
            throw YDKError.internal_("invalid status response")
        }
        return VehicleStatus(
            locked: data[0] != 0,
            engineOn: data[1] != 0,
            batteryPct: Int32(data[2])
        )
    }
}
