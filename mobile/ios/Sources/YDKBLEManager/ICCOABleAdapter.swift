import Foundation

// MARK: - ICCOA 协议适配器

/// ICCOA Digital Key BLE 协议适配器
///
/// 参考: ICCOA.DK.TS.002 BLE 章节 (知识库: docs/certification/iccoa-spec.md)
///
/// ⚠️ 指令帧 (2b-F, 由 Android worker 主导): 车端参考实现
/// `embedded/iccoa_protocol/src/iccoa/dk30/iccoa_dk30.c` 定义了 DK3.0 线格式:
/// ```
///   [SOP(1)][cmd_id(1)][seq_num(2,LE)][payload_len(2,LE)][payload][checksum(1)][EOP(1)]
/// ```
/// 当前 buildCommand 仍为占位, 需按上述格式 + SM4 加密实现 (TODO: 2b-F)。
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

    // ⚠️ TODO (2b-F): 按 ICCOA DK3.0 线格式 + SM4 加密实现, 当前为占位。
    private func buildCommand(type: BleCommandType, session: SessionContext) throws -> Data {
        var frame = Data()
        frame.append(type.rawValue)
        frame.append(contentsOf: withUnsafeBytes(of: session.sessionHandle.bigEndian) { Array($0) })
        frame.append(contentsOf: withUnsafeBytes(of: session.counter.bigEndian) { Array($0) })
        frame.append(0x00) // payload 占位 (SM4 加密, TODO: 2b-F)
        return frame
    }

    func parseCommandResponse(_ data: Data) throws -> CommandResult {
        guard data.count >= 1 else {
            return CommandResult(success: false, errorCode: -1, errorMessage: "empty response")
        }
        let status = data[0]
        if status == 0x00 {
            return CommandResult(success: true)
        }
        return CommandResult(success: false, errorCode: Int32(status), errorMessage: "ICCOA command failed: 0x\(String(format: "%02X", status))")
    }

    func parseVehicleStatus(_ data: Data) throws -> VehicleStatus {
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
