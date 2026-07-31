import Foundation

// MARK: - ICCE 协议适配器

/// ICCE Digital Key BLE 协议适配器 (T/CA 110-2020)
///
/// ICCE 使用挑战-响应认证 (0xFEFD AuthChallenge) + 控制指令 (0xFEFE ControlCmd)
final class ICCEBleAdapter: BleProtocolAdapter {

    let protocolType: YDKBleProtocolType = .icce

    func parseAdvertisement(_ advertisementData: [String: Any], rssi: Int) -> VehicleAdvertise? {
        guard let services = advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID],
              services.contains(YDKBleUUIDs.icceService) else {
            return nil
        }

        let mfrData = advertisementData[CBAdvertisementDataManufacturerDataKey] as? Data

        return VehicleAdvertise(
            vehicleId: "icce-" + (mfrData?.subdata(in: 2..<min(6, mfrData?.count ?? 0)).hexString ?? "unknown"),
            rssi: rssi,
            protocolType: YDKBleProtocolType.icce.rawValue,
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

private extension Data {
    var hexString: String {
        map { String(format: "%02x", $0) }.joined()
    }
}
