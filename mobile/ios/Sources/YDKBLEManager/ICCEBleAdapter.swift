import Foundation
import CryptoKit
import YDKHubClient

// MARK: - ICCE 协议适配器

/// ICCE Digital Key BLE 协议适配器 (T/CA 110-2020)
///
/// 指令帧 control_command_t (事实来源: embedded/icce_protocol/docs/module_design.md
/// §3.1.4:275-291, 裁决 AD-5), 共 38 字节:
/// ```
///   [0]     command_type  命令类型 (0x01 解锁 / 0x02 闭锁 / 0x03 启动 / 0x04 熄火 / 0x05 尾门 / 0x06 状态查询)
///   [1]     target        目标设备 (0x00 = 车辆主体)
///   [2-5]   user_id       用户 ID (大端 BE u32)
///   [6-37]  hmac          命令 HMAC (32 字节)
/// ```
///
/// 枚举映射 (裁决 AD-4, 映射表 §2.2, 适配器内部转换):
///   BleCommandType.unlock→0x01 / lock→0x02 / engineOn→0x03 / engineOff→0x04 / status→0x06(QUERY)
///
/// 安全 (裁决 AD-6/AD-7):
/// - hmac[32] = HMAC-SHA256(会话密钥, 命令体前 6 字节 command_type..user_id)
///   (crypto_engine.c crypto_hmac_sha256; ⚠️ 覆盖范围标注: 待真机确认);
///   会话密钥未协商 → 零填充 hmac + 注释 (仅调试/预认证阶段, 生产禁止)。
/// - 会话加密 = SM4-CBC (PKCS#7): 密钥取 sessionKey 前 16 字节, IV 未协商时全零 (仅调试),
///   由 [encryptCommand] 提供 — 传输层在会话加密态调用, 指令结构本身为 38 字节明文。
final class ICCEBleAdapter: BleProtocolAdapter {

    let protocolType: YDKBleProtocolType = .icce

    /// 解析 ICCE 车辆广告包:
    /// 1. 校验广播包含 ICCE Digital Key Service (0xFEFA);
    /// 2. vehicleId 取自广播设备名 (参考实现广播仅含 Flags + Service UUID + Local Name,
    ///    无 manufacturer data — 见 ble_manager.c ble_start_advertising)。
    func parseAdvertisement(_ advertisementData: [String: Any], rssi: Int) -> VehicleAdvertise? {
        let extracted = YDKAdvertisementParser.extract(from: advertisementData)
        guard extracted.serviceUUIDs.contains(YDKBleUUIDs.icceService) else {
            return nil
        }

        let mfr = extracted.manufacturerData.flatMap(YDKAdvertisementParser.parseManufacturerData)
        guard let vehicleId = YDKAdvertisementParser.icceVehicleID(from: mfr, localName: extracted.localName) else {
            return nil
        }

        return VehicleAdvertise(
            vehicleId: vehicleId,
            rssi: rssi,
            protocolType: YDKBleProtocolType.icce.rawValue,
            supportsUWB: false,
            manufacturerData: extracted.manufacturerData
        )
    }

    func buildUnlockCommand(keyId: String, session: SessionContext) throws -> Data {
        try buildCommand(type: .unlock, session: session)
    }

    func buildLockCommand(keyId: String, session: SessionContext) throws -> Data {
        try buildCommand(type: .lock, session: session)
    }

    func buildStartEngineCommand(keyId: String, session: SessionContext) throws -> Data {
        try buildCommand(type: .engineOn, session: session)
    }

    /// 构造 ICCE control_command_t (38 字节明文结构, 裁决 AD-5):
    /// `[command_type(1)][target(1)=0x00][user_id(BE u32)][hmac(32)]`
    ///
    /// - 会话密钥存在 → hmac 为真实 HMAC-SHA256 (非零);
    /// - 未协商密钥 → hmac 零填充 (仅调试, B1.5 约定)。
    func buildControlCommand(type: BleCommandType, session: SessionContext) -> Data {
        // 命令体前 6 字节 [command_type][target][user_id BE] — HMAC 覆盖范围 (AD-6)
        var body = Data(capacity: 6)
        body.append(Self.wireCommandCode(for: type))
        body.append(0x00) // target: 车辆主体
        body.append(UInt8((session.userId >> 24) & 0xFF))
        body.append(UInt8((session.userId >> 16) & 0xFF))
        body.append(UInt8((session.userId >> 8) & 0xFF))
        body.append(UInt8(session.userId & 0xFF))

        var command = Data(capacity: 38)
        command.append(body)
        command.append(hmacSHA256(sessionKey: session.sessionKey, body: body))
        return command
    }

    /// 构造完整控制指令 = 38 字节明文 control_command_t (hmac 真实/零填充见上)。
    /// 会话加密 (SM4-CBC, AD-7) 由传输层经 [encryptCommand] 应用, 不在此展开。
    private func buildCommand(type: BleCommandType, session: SessionContext) throws -> Data {
        buildControlCommand(type: type, session: session)
    }

    /// hmac[32] = HMAC-SHA256(会话密钥, 命令体前 6 字节) (裁决 AD-6;
    /// ⚠️ 覆盖范围 command_type..user_id 标注: 待真机确认)。
    /// 会话密钥未协商 → 32 字节零填充 (仅调试/预认证阶段, 生产禁止)。
    private func hmacSHA256(sessionKey: Data?, body: Data) -> Data {
        guard let sessionKey = sessionKey, !sessionKey.isEmpty else {
            return Data(repeating: 0, count: 32)
        }
        let mac = HMAC<SHA256>.authenticationCode(
            for: body,
            using: SymmetricKey(data: sessionKey)
        )
        return Data(mac)
    }

    /// 会话加密 (裁决 AD-7): SM4-CBC + PKCS#7。
    /// - 密钥 = sessionKey 前 16 字节 (security_auth.h session_key[32] 取前 16B, KEY_TYPE_SM4);
    /// - IV = sessionIv ?? 全零 (未协商时全零仅调试);
    /// - 无会话密钥 → 原样返回明文 (仅调试/预认证阶段)。
    func encryptCommand(_ command: Data, session: SessionContext) throws -> Data {
        guard let sessionKey = session.sessionKey, !sessionKey.isEmpty else {
            return command
        }
        let sm4Key = Array(sessionKey.prefix(Sm4.keySize))
        let iv = session.sessionIv.map(Array.init) ?? Sm4.zeroIV
        let padded = try Sm4.pkcs7Pad(Array(command))
        return Data(try Sm4.cbcEncrypt(key: sm4Key, iv: iv, plain: padded))
    }

    /// 通用 BleCommandType → ICCE wire 命令码 (裁决 AD-4, 映射表 §2.2;
    /// 枚举定义见 module_design.md §3.1.4 command_type_t)
    static func wireCommandCode(for type: BleCommandType) -> UInt8 {
        switch type {
        case .unlock:    return 0x01 // CMD_UNLOCK_DOOR
        case .lock:      return 0x02 // CMD_LOCK_DOOR
        case .engineOn:  return 0x03 // CMD_START_ENGINE
        case .engineOff: return 0x04 // CMD_STOP_ENGINE
        case .status:    return 0x06 // CMD_QUERY_STATUS
        }
    }

    func parseCommandResponse(_ data: Data) throws -> CommandResult {
        guard !data.isEmpty else {
            return CommandResult(success: false, errorCode: -1, errorMessage: "empty response")
        }
        let status = data[data.startIndex]
        if status == 0x00 {
            return CommandResult(success: true)
        }
        return CommandResult(success: false, errorCode: Int32(status), errorMessage: "ICCE command failed: 0x\(String(format: "%02X", status))")
    }

    func parseVehicleStatus(_ data: Data) throws -> VehicleStatus {
        guard data.count >= 3 else {
            throw YDKError.internal_("invalid status response")
        }
        return VehicleStatus(
            locked: data[data.startIndex] != 0,
            engineOn: data[data.startIndex + 1] != 0,
            batteryPct: Int32(data[data.startIndex + 2])
        )
    }
}
