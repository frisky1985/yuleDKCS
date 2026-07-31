import Foundation

// MARK: - CCC 协议适配器

/// CCC Digital Key v4.0 BLE 协议适配器
///
/// 指令帧格式按 CCC-TS-101 v4.0.0 Table 19-19, 详细分析见同目录
/// `CCC_FRAME_ANALYSIS.md`:
/// - 帧头: `[message_header(1)][payload_header(1)][length(2,BE)]` — 4 字节
/// - 控制指令: SE 消息 (Message Type=0x01) + DK_APDU_RQ (0x0B), 载荷经安全提供者封装
/// - 响应: DK_APDU_RS (0x0C)
final class CCCBleAdapter: BleProtocolAdapter {

    let protocolType: YDKBleProtocolType = .ccc

    /// 消息安全提供者。
    /// 真实实现: `CCCSecureChannel` (GPC_SPE_014 SCP03: AES-128 + CMAC-AES-128),
    /// 依据 `CCC_FRAME_ANALYSIS.md` §5 (2026-07-31 规范裁决)。
    /// 默认透传实现仅用于单测/联调, 生产接入前必须替换。
    private let security: CCCMessageSecurityProviding

    init(security: CCCMessageSecurityProviding = CCCNullMessageSecurity()) {
        self.security = security
    }

    // MARK: 广告包解析

    /// 解析 CCC 车辆广告包 (CCC-TS-101 v4.0.0 §19.2.1.3):
    /// 1. 校验广播包含 CCC_DK_UUID (0xFFF5);
    /// 2. 优先解析规范 Service Data (0x21, CCCServiceDataIntent UUID) → intent/brand;
    ///    回退解析 iBeacon 风格厂商数据 (R3.0 存量设备) → vehicleId;
    /// 3. vehicleId 仅在可确认时返回 (不伪造标识)。
    func parseAdvertisement(_ advertisementData: [String: Any], rssi: Int) -> VehicleAdvertise? {
        let extracted = YDKAdvertisementParser.extract(from: advertisementData)

        // 1. service UUID 过滤: 必须包含 CCC Digital Key Service (0xFFF5)
        guard extracted.serviceUUIDs.contains(YDKBleUUIDs.cccService) else {
            return nil
        }

        // 2a. 规范格式: Service Data 128bit UUID (0x21) → brand identifier
        let structures = YDKAdvertisementParser.parseADStructures(from: extracted.manufacturerData ?? Data())
        let serviceData = YDKAdvertisementParser.cccServiceData(from: structures)

        // 2b. 回退: iBeacon 风格厂商数据 (R3.0 存量联调设备)
        let legacyVehicleID: String?
        if let manufacturerData = extracted.manufacturerData,
           let mfr = YDKAdvertisementParser.parseManufacturerData(manufacturerData) {
            legacyVehicleID = YDKAdvertisementParser.cccVehicleID(from: mfr)
        } else {
            legacyVehicleID = nil
        }

        // 3. vehicleId: 规范 Service Data 无唯一车辆 UUID, 用 brand identifier 作为
        //    广告层标识 (前缀 ccc- 区分协议); R3.0 兼容路径用 20B proximity UUID。
        let vehicleId: String
        if let sd = serviceData {
            vehicleId = "ccc-" + YDKAdvertisementParser.hexString(sd.brandIdentifier)
        } else if let legacy = legacyVehicleID {
            vehicleId = legacy
        } else {
            return nil
        }

        // TODO-verify: UWB 能力位在 CCC 规范中位于配对/OOB 数据 (ccc_nfc_oob_data_t.capability),
        // 广告包不携带; 此处恒为 false, 待 OOB 配对后更新。
        return VehicleAdvertise(
            vehicleId: vehicleId,
            rssi: rssi,
            protocolType: YDKBleProtocolType.ccc.rawValue,
            supportsUWB: false,
            manufacturerData: extracted.manufacturerData
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
    /// 2. 经安全提供者封装 (生产: CCCSecureChannel SCP03 加密);
    /// 3. 包装为规范 4 字节帧头: SE 消息 (0x01) + DK_APDU_RQ (0x0B)。
    private func buildCommand(type: BleCommandType, keyId: String, session: SessionContext) throws -> Data {
        let plaintext = CCCControlPayload.build(subcommand: type, session: session, keyId: keyId)
        let protected = try security.encrypt(plaintext)
        let frame = CCCCommandFrame(
            messageType: CCCMessageType.se.rawValue,
            messageID: CCCApduMessageID.dkApduRq.rawValue,
            payload: protected
        )
        return frame.data
    }

    // MARK: 响应解析

    /// 解析指令响应。优先按规范帧解析 (SE 消息 + DK_APDU_RS 0x0C);
    /// 非帧格式 (如 <4 字节的裸状态字节) 回退到 [0]=status 的旧语义。
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

        // 规范响应: SE 消息 (0x01) + DK_APDU_RS (0x0C), payload 首字节为 APDU SW1
        // (0x90 = 成功, 其他为错误码)。兼容 R3.0 参考实现的 authResponse/pairResponse 类型。
        switch frame.messageType {
        case CCCMessageType.se.rawValue:
            guard frame.messageID == CCCApduMessageID.dkApduRs.rawValue else {
                return CommandResult(success: false, errorCode: -1,
                                     errorMessage: "unexpected SE message ID 0x\(String(format: "%02X", frame.messageID))")
            }
            guard let sw1 = frame.payload.first else {
                return CommandResult(success: false, errorCode: -1, errorMessage: "empty response payload")
            }
            if sw1 == 0x90 || sw1 == 0x00 {
                return CommandResult(success: true)
            }
            return CommandResult(success: false, errorCode: Int32(sw1),
                                 errorMessage: "CCC APDU failed: SW=0x\(String(format: "%02X", sw1))")

        default:
            // R3.0 兼容: authResponse/pairResponse payload[0]==0x00 成功; error 0xFF
            guard let status = frame.payload.first else {
                return CommandResult(success: false, errorCode: -1, errorMessage: "empty response payload")
            }
            if frame.messageType == 0xFF {
                return CommandResult(success: false, errorCode: Int32(status),
                                     errorMessage: "CCC error message: 0x\(String(format: "%02X", status))")
            }
            if status == 0x00 {
                return CommandResult(success: true)
            }
            return CommandResult(success: false, errorCode: Int32(status),
                                 errorMessage: "CCC command failed: 0x\(String(format: "%02X", status))")
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
