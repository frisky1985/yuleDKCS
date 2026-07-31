import Foundation

// MARK: - ICCE 协议适配器

/// ICCE Digital Key BLE 协议适配器 (T/CA 110-2020)
///
/// ICCE 使用挑战-响应认证 (0xFEFD AuthChallenge) + 控制指令 (0xFEFE ControlCmd)。
///
/// ⚠️ 指令帧 (2b-F): 车端参考实现 `embedded/icce_protocol` 定义了帧格式,
/// 当前 buildCommand 仍为占位, 需按参考 + SM4 加密实现 (TODO: 2b-F)。
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

    // ⚠️ TODO (2b-F): 按 ICCE 参考实现帧格式 + SM4 加密实现, 当前为占位。
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
        return CommandResult(success: false, errorCode: Int32(status), errorMessage: "ICCE command failed: 0x\(String(format: "%02X", status))")
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
