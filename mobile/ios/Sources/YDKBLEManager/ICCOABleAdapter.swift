import Foundation
import YDKHubClient

// MARK: - ICCOA 协议适配器

/// ICCOA Digital Key BLE 协议适配器 (DK3.0)
///
/// 指令帧 (裁决 AD-2/AD-3, 事实来源: embedded/iccoa_protocol/src/iccoa/dk30/iccoa_dk30.c
/// + include/iccoa_digital_key.h):
/// ```
///   [SOP(1) 0xAA][CMD_ID(1)][SEQ(2,LE)][LEN(2,LE)][PAYLOAD][XOR校验(1)][EOP(1) 0x55]
/// ```
/// - CTRL_REQ (0x20) payload = [cmd(1)][param(1)] (dk30.c:90-103, payload_len >= 2)
/// - XOR 校验覆盖 CMD_ID+SEQ+LEN+PAYLOAD, 不含 SOP (dk30.c:131-132)
///
/// ⚠️ 无应用层加密 (裁决 AD-1): 链路加密由 BLE LE Secure Connections 负责,
/// 禁止在 ICCOA 指令上做 SM4 — 本适配器不调用 Sm4。
///
/// 枚举映射 (裁决 AD-4, 映射表 §2.2, 适配器内部转换):
///   BleCommandType.unlock→0x02 / lock→0x01 / engineOn→0x03 / engineOff→0x04 / status→0x05
final class ICCOABleAdapter: BleProtocolAdapter {

    let protocolType: YDKBleProtocolType = .iccoa

    /// 解析 ICCOA 车辆广告包:
    /// 1. 校验广播包含 ICCOA Digital Key Service (0xFEF5);
    /// 2. TODO-verify: ICCOA BLE 广告中 vehicleId 的编码未知 (参考实现无广播数据定义),
    ///    暂不猜测 → 返回 nil, 待规范确认 (见 YDKAdvertisementParser.iccoaVehicleID)。
    func parseAdvertisement(_ advertisementData: [String: Any], rssi: Int) -> VehicleAdvertise? {
        let extracted = YDKAdvertisementParser.extract(from: advertisementData)
        guard extracted.serviceUUIDs.contains(YDKBleUUIDs.iccoaService) else {
            return nil
        }

        let mfr = extracted.manufacturerData.flatMap(YDKAdvertisementParser.parseManufacturerData)
        guard let vehicleId = YDKAdvertisementParser.iccoaVehicleID(from: mfr, localName: extracted.localName) else {
            return nil
        }

        return VehicleAdvertise(
            vehicleId: vehicleId,
            rssi: rssi,
            protocolType: YDKBleProtocolType.iccoa.rawValue,
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

    /// 构造 ICCOA DK3.0 CTRL_REQ 帧 (2b-F):
    ///   payload = [wireCmd(1)][param(1)=0x00], 明文 (无应用层 SM4, AD-1);
    ///   seq = session.counter 低 16 位 (小端);
    ///   keyId 不参与线格式 (AD-3 废除 keyId/counter 内部约定结构)。
    private func buildCommand(type: BleCommandType, session: SessionContext) throws -> Data {
        let wireCmd = Self.wireControlCode(for: type)
        let payload = Data([wireCmd, 0x00])
        let seqNum = UInt16(session.counter & 0xFFFF)
        return IcocaFrame.build(cmdId: IcocaFrame.cmdCtrlRequest, seqNum: seqNum, payload: payload)
    }

    /// 通用 BleCommandType → ICCOA wire 控制码 (裁决 AD-4, 映射表 §2.2;
    /// 枚举定义见 iccoa_digital_key.h:155-167)
    static func wireControlCode(for type: BleCommandType) -> UInt8 {
        switch type {
        case .unlock:   return IcocaFrame.ctrlUnlock    // 0x02
        case .lock:     return IcocaFrame.ctrlLock      // 0x01
        case .engineOn: return IcocaFrame.ctrlEngineOn  // 0x03
        case .engineOff: return IcocaFrame.ctrlEngineOff // 0x04
        case .status:   return IcocaFrame.ctrlTrunkOpen // 0x05 (CTRL_REQ payload 复用, 映射表)
        }
    }

    func parseCommandResponse(_ data: Data) throws -> CommandResult {
        guard !data.isEmpty else {
            return CommandResult(success: false, errorCode: -1, errorMessage: "empty response")
        }

        // 响应为 DK3.0 帧 (CTRL_RSP 0x21), payload 首字节为状态 (dk30.c:102);
        // 兼容裸状态字节 (旧联调格式)。
        let status: UInt8
        if data[data.startIndex] == IcocaFrame.sop {
            guard let frame = IcocaFrame.parse(data) else {
                return CommandResult(success: false, errorCode: -1, errorMessage: "invalid ICCOA frame")
            }
            status = frame.payload.first ?? 0x00
        } else {
            status = data[data.startIndex]
        }

        if status == 0x00 {
            return CommandResult(success: true)
        }
        return CommandResult(success: false, errorCode: Int32(status), errorMessage: "ICCOA command failed: 0x\(String(format: "%02X", status))")
    }

    func parseVehicleStatus(_ data: Data) throws -> VehicleStatus {
        // 状态可经 STATUS_NOTIFY (0x30) 帧或裸 3 字节下发
        let payload: Data
        if !data.isEmpty && data[data.startIndex] == IcocaFrame.sop {
            guard let frame = IcocaFrame.parse(data) else {
                throw YDKError.internal_("invalid status frame")
            }
            payload = frame.payload
        } else {
            payload = data
        }
        guard payload.count >= 3 else {
            throw YDKError.internal_("invalid status response")
        }
        return VehicleStatus(
            locked: payload[payload.startIndex] != 0,
            engineOn: payload[payload.startIndex + 1] != 0,
            batteryPct: Int32(payload[payload.startIndex + 2])
        )
    }
}
