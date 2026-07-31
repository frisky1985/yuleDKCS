import Foundation

// MARK: - ICCOA 协议适配器

/// ICCOA Digital Key 4.0 BLE 协议适配器
///
/// 参考: ICCOA.DK.TS.002 BLE 章节
/// 指令帧格式（简化骨架，完整实现按 ICCOA 规范）:
///   [0]     = command type
///   [1-2]   = session handle
///   [3-6]   = counter
///   [7...]  = encrypted payload (SM4)
final class ICCOABleAdapter: BleProtocolAdapter {

    let protocolType: YDKBleProtocolType = .iccoa

    func parseAdvertisement(_ advertisementData: [String: Any], rssi: Int) -> VehicleAdvertise? {
        guard let services = advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID],
              services.contains(YDKBleUUIDs.iccoaService) else {
            return nil
        }

        let mfrData = advertisementData[CBAdvertisementDataManufacturerDataKey] as? Data

        return VehicleAdvertise(
            vehicleId: "iccoa-" + (mfrData?.subdata(in: 2..<min(6, mfrData?.count ?? 0)).hexString ?? "unknown"),
            rssi: rssi,
            protocolType: YDKBleProtocolType.iccoa.rawValue,
            supportsUWB: false,
            manufacturerData: mfrData
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

    private func buildCommand(type: BleCommandType, session: SessionContext) throws -> Data {
        var frame = Data()
        frame.append(type.rawValue)
        frame.append(contentsOf: withUnsafeBytes(of: session.sessionHandle.bigEndian) { Array($0) })
        frame.append(contentsOf: withUnsafeBytes(of: session.counter.bigEndian) { Array($0) })
        frame.append(0x00) // payload 占位（SM4 加密）
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

private extension Data {
    var hexString: String {
        map { String(format: "%02x", $0) }.joined()
    }
}
