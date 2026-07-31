import Foundation

// MARK: - CCC 协议适配器

/// CCC Digital Key v4.0 BLE 协议适配器
///
/// 指令帧格式（简化骨架，完整实现按 CCC-TS-101 Reader Protocol 章节）:
///   [0]     = command type (0x01 unlock / 0x02 lock / ...)
///   [1-2]   = session handle (big endian)
///   [3-6]   = message counter (big endian)
///   [7...]  = payload
final class CCCBleAdapter: BleProtocolAdapter {

    let protocolType: YDKBleProtocolType = .ccc

    // MARK: 广告包解析

    func parseAdvertisement(_ advertisementData: [String: Any], rssi: Int) -> VehicleAdvertise? {
        // CCC 车辆广告包: 广播 service UUID 0xFFD1 + manufacturer data 携带车辆标识
        guard let services = advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID],
              services.contains(YDKBleUUIDs.cccService) else {
            return nil
        }

        // manufacturer data: [0-1] company id, [2] protocol version, [3...] vehicle id bytes
        let mfrData = advertisementData[CBAdvertisementDataManufacturerDataKey] as? Data
        let supportsUWB = advertisementData[CBAdvertisementDataIsConnectable] as? Bool ?? false

        // 从 manufacturer data 或 name 提取 vehicleId（简化：占位逻辑）
        let vehicleId = extractVehicleId(from: mfrData) ?? "unknown-vehicle"

        return VehicleAdvertise(
            vehicleId: vehicleId,
            rssi: rssi,
            protocolType: YDKBleProtocolType.ccc.rawValue,
            supportsUWB: supportsUWB,
            manufacturerData: mfrData
        )
    }

    private func extractVehicleId(from data: Data?) -> String? {
        guard let data = data, data.count >= 6 else { return nil }
        // 简化: 取 data[2...5] 的 hex 作为 vehicle id 占位
        return data.subdata(in: 2..<min(6, data.count)).hexString
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

    private func buildCommand(type: BleCommandType, keyId: String, session: SessionContext) throws -> Data {
        var frame = Data()
        frame.append(type.rawValue)
        frame.append(contentsOf: withUnsafeBytes(of: session.sessionHandle.bigEndian) { Array($0) })
        frame.append(contentsOf: withUnsafeBytes(of: session.counter.bigEndian) { Array($0) })
        // payload 占位（真实实现需按 CCC Reader Protocol 加密 + 签名）
        frame.append(0x00)
        return frame
    }

    // MARK: 响应解析

    func parseCommandResponse(_ data: Data) throws -> CommandResult {
        guard data.count >= 1 else {
            return CommandResult(success: false, errorCode: -1, errorMessage: "empty response")
        }
        // [0] = status: 0x00 success, 其他 = error
        let status = data[0]
        if status == 0x00 {
            return CommandResult(success: true)
        }
        return CommandResult(success: false, errorCode: Int32(status), errorMessage: "CCC command failed: 0x\(String(format: "%02X", status))")
    }

    func parseVehicleStatus(_ data: Data) throws -> VehicleStatus {
        guard data.count >= 3 else {
            throw YDKError.internal_("invalid status response")
        }
        let lockStatus = data[0] != 0
        let engineStatus = data[1] != 0
        let batteryPct = Int32(data[2])
        return VehicleStatus(locked: lockStatus, engineOn: engineStatus, batteryPct: batteryPct)
    }
}

private extension Data {
    var hexString: String {
        map { String(format: "%02x", $0) }.joined()
    }
}
