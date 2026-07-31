package com.yuledkcs.sdk.ble

import java.nio.ByteBuffer
import java.nio.charset.StandardCharsets

// ─── 协议适配器接口 ───────────────────────────────────────

/**
 * BLE 协议适配器 — 封装各协议（CCC/ICCOA/ICCE）的指令编解码
 */
interface BleProtocolAdapter {
    val protocolType: BleProtocolType

    /** 解析扫描记录 → 车辆信息（非本协议返回 null） */
    fun parseAdvertisement(scanRecord: ByteArray?, rssi: Int): VehicleAdvertise?

    fun buildUnlockCommand(keyId: String, session: SessionContext): ByteArray
    fun buildLockCommand(keyId: String, session: SessionContext): ByteArray
    fun buildStartEngineCommand(keyId: String, session: SessionContext): ByteArray
    fun parseCommandResponse(data: ByteArray): CommandResult
    fun parseVehicleStatus(data: ByteArray): VehicleStatus
}

// ─── 工厂 ─────────────────────────────────────────────────

object BleProtocolAdapterFactory {
    fun makeAdapter(type: BleProtocolType): BleProtocolAdapter = when (type) {
        BleProtocolType.CCC -> CCCBleAdapter()
        BleProtocolType.ICCOA -> ICCOABleAdapter()
        BleProtocolType.ICCE -> ICCEBleAdapter()
    }
}

// ─── 共享逻辑 ─────────────────────────────────────────────

/** 厂商特定数据原始字节 (companyId LE + data); 无厂商数据时返回 null */
private fun mfrRawBytes(mfr: BleAdvertiseParser.ManufacturerData?): ByteArray? = mfr?.toBytes()

// ─── CCC 适配器 ───────────────────────────────────────────

/**
 * CCC Digital Key v4.0 BLE 协议适配器 (CCC-TS-101)
 *
 * 指令帧格式:
 *   [0]     = command type
 *   [1-2]   = session handle (big endian u16)
 *   [3-6]   = message counter (big endian u32)
 *   [7]     = payload length
 *   [8...]  = payload (keyId, UTF-8)
 *
 * TODO(security): CCC 规范要求 ECDH + AES-CCM 加密通道 (CCC-TS-101 Reader Protocol),
 * 需在密钥协商完成后对 payload 加密 — 当前为明文帧结构。
 */
class CCCBleAdapter : BleProtocolAdapter {

    override val protocolType = BleProtocolType.CCC

    override fun parseAdvertisement(scanRecord: ByteArray?, rssi: Int): VehicleAdvertise? {
        val parsed = BleAdvertiseParser.parse(scanRecord) ?: return null
        if (!parsed.hasServiceUuid(BleUuids.CCC_SERVICE)) return null

        val mfr = parsed.manufacturerData
        // CCC 知识库未定义厂商数据结构; 沿用既有约定: 取厂商数据前 4 字节, 无厂商数据时退回记录前 4 字节
        val vehicleId = when {
            mfr != null && mfr.data.size >= 4 -> "ccc-" + mfr.data.copyOf(4).toHexString()
            scanRecord != null -> "ccc-" + scanRecord.copyOf(4).toHexString()
            else -> return null
        }
        return VehicleAdvertise(
            vehicleId = vehicleId,
            rssi = rssi,
            protocolType = BleProtocolType.CCC.ordinal + 1,
            supportsUwb = false,
            manufacturerData = mfrRawBytes(mfr)
        )
    }

    override fun buildUnlockCommand(keyId: String, session: SessionContext): ByteArray =
        buildCommand(BleCommandType.UNLOCK, keyId, session)

    override fun buildLockCommand(keyId: String, session: SessionContext): ByteArray =
        buildCommand(BleCommandType.LOCK, keyId, session)

    override fun buildStartEngineCommand(keyId: String, session: SessionContext): ByteArray =
        buildCommand(BleCommandType.ENGINE_ON, keyId, session)

    private fun buildCommand(type: BleCommandType, keyId: String, session: SessionContext): ByteArray {
        val keyIdBytes = keyId.toByteArray(StandardCharsets.UTF_8)
        require(keyIdBytes.size <= 0xFF) { "keyId too long: ${keyIdBytes.size} bytes" }

        val buf = ByteBuffer.allocate(8 + keyIdBytes.size)
        buf.put(type.value)
        buf.putShort(session.sessionHandle)
        buf.putInt(session.counter.toInt())
        buf.put(keyIdBytes.size.toByte())
        buf.put(keyIdBytes)
        return buf.array()
    }

    override fun parseCommandResponse(data: ByteArray): CommandResult {
        if (data.isEmpty()) return CommandResult(false, -1, "empty response")
        val status = data[0].toUInt8()
        return if (status == 0x00) {
            CommandResult(true)
        } else {
            CommandResult(false, status, "CCC command failed: 0x%02X".format(status))
        }
    }

    override fun parseVehicleStatus(data: ByteArray): VehicleStatus {
        if (data.size < 3) throw IllegalArgumentException("invalid status response")
        return VehicleStatus(
            locked = data[0] != 0x00.toByte(),
            engineOn = data[1] != 0x00.toByte(),
            batteryPct = data[2].toUInt8()
        )
    }
}

// ─── ICCOA 适配器 ─────────────────────────────────────────

/**
 * ICCOA Digital Key BLE 协议适配器 (ICCOA/T 002-2024 数字车钥匙3.0)
 *
 * 指令帧: ICCOA DK 3.0 帧 (见 [IcocaFrame]):
 *   SOP(0xAA) | CMD_ID | SEQ(2) | LEN(2) | PAYLOAD | XOR校验 | EOP(0x55)
 * 控制指令使用 CMD_CTRL_REQUEST (0x20), 响应为 CMD_CTRL_RESPONSE (0x21)。
 *
 * CTRL_REQUEST payload (知识库未定义具体负载结构, 采用下述内部约定并注明):
 *   [0]     control type (BleCommandType.value: 0x01解锁/0x02闭锁/0x03启动/0x04熄火/0x05状态)
 *   [1]     keyId 长度 (u8)
 *   [2..]   keyId (UTF-8)
 *   [..]    counter (BE u32)
 *
 * 安全: 绑定/认证完成后由密钥协商得到 SM4 会话密钥, 设置
 * [SessionContext.sessionKey]/[SessionContext.sessionIv] 后负载自动 SM4-CBC 加密;
 * 未协商时输出明文帧 (仅调试/预绑定阶段, 生产环境禁止)。
 */
class ICCOABleAdapter : BleProtocolAdapter {

    override val protocolType = BleProtocolType.ICCOA

    override fun parseAdvertisement(scanRecord: ByteArray?, rssi: Int): VehicleAdvertise? {
        val parsed = BleAdvertiseParser.parse(scanRecord) ?: return null
        if (!parsed.hasServiceUuid(BleUuids.ICCOA_SERVICE)) return null

        // ICCOA 广播厂商数据结构 (embedded/iccoa_protocol 模块设计 §3.1.5):
        //   mfg_id[2] | vehicle_id[6] | protocol_ver[1] | capability[1]
        val mfr = parsed.manufacturerData
        val vehicleId = when {
            mfr != null && mfr.data.size >= 6 -> "iccoa-" + mfr.data.copyOf(6).toHexString()
            scanRecord != null -> "iccoa-" + scanRecord.copyOf(4).toHexString()
            else -> return null
        }
        // capability 位定义: bit0 = UWB 支持 (DK 4.0), 假设值待规范确认
        val supportsUwb = mfr?.data?.getOrNull(7)?.let { (it.toUInt8() and 0x01) != 0 } ?: false
        return VehicleAdvertise(
            vehicleId = vehicleId,
            rssi = rssi,
            protocolType = BleProtocolType.ICCOA.ordinal + 1,
            supportsUwb = supportsUwb,
            manufacturerData = mfrRawBytes(mfr)
        )
    }

    override fun buildUnlockCommand(keyId: String, session: SessionContext): ByteArray =
        buildControlCommand(BleCommandType.UNLOCK, keyId, session)

    override fun buildLockCommand(keyId: String, session: SessionContext): ByteArray =
        buildControlCommand(BleCommandType.LOCK, keyId, session)

    override fun buildStartEngineCommand(keyId: String, session: SessionContext): ByteArray =
        buildControlCommand(BleCommandType.ENGINE_ON, keyId, session)

    private fun buildControlCommand(type: BleCommandType, keyId: String, session: SessionContext): ByteArray {
        val plain = buildControlPayload(type, keyId, session)
        val payload = encryptPayload(plain, session)
        val seqNum = session.counter.toInt() and 0xFFFF
        return IcocaFrame.build(IcocaFrame.CMD_CTRL_REQUEST, seqNum, payload)
    }

    /** 构造 CTRL_REQUEST 明文负载 (结构见类注释) */
    private fun buildControlPayload(type: BleCommandType, keyId: String, session: SessionContext): ByteArray {
        val keyIdBytes = keyId.toByteArray(StandardCharsets.UTF_8)
        require(keyIdBytes.size <= 0xFF) { "keyId too long: ${keyIdBytes.size} bytes" }
        return ByteBuffer.allocate(1 + 1 + keyIdBytes.size + 4)
            .put(type.value)
            .put(keyIdBytes.size.toByte())
            .put(keyIdBytes)
            .putInt(session.counter.toInt())
            .array()
    }

    /** 会话密钥存在时 SM4-CBC 加密 (PKCS#7 填充), 否则返回明文 */
    private fun encryptPayload(plain: ByteArray, session: SessionContext): ByteArray {
        val key = session.sessionKey ?: return plain
        return Sm4.cbcEncrypt(key, session.sessionIv ?: Sm4.ZERO_IV, Sm4.pkcs7Pad(plain))
    }

    override fun parseCommandResponse(data: ByteArray): CommandResult {
        if (data.isEmpty()) return CommandResult(false, -1, "empty response")

        // ICCOA 响应为 DK 3.0 帧 (CMD_CTRL_RESPONSE 0x21), payload 首字节为状态;
        // 兼容裸状态字节 (旧行为)。
        val status = if (data[0].toUInt8() == IcocaFrame.SOP) {
            val frame = IcocaFrame.parse(data)
                ?: return CommandResult(false, -1, "invalid ICCOA frame")
            frame.payload.firstOrNull()?.toUInt8() ?: 0x00
        } else {
            data[0].toUInt8()
        }

        return if (status == 0x00) CommandResult(true)
        else CommandResult(false, status, "ICCOA command failed: 0x%02X".format(status))
    }

    override fun parseVehicleStatus(data: ByteArray): VehicleStatus {
        // 状态可经 STATUS_NOTIFY (0x30) 帧或裸 3 字节下发
        val payload = if (data.isNotEmpty() && data[0].toUInt8() == IcocaFrame.SOP) {
            IcocaFrame.parse(data)?.payload ?: throw IllegalArgumentException("invalid status frame")
        } else {
            data
        }
        if (payload.size < 3) throw IllegalArgumentException("invalid status response")
        return VehicleStatus(
            locked = payload[0] != 0x00.toByte(),
            engineOn = payload[1] != 0x00.toByte(),
            batteryPct = payload[2].toUInt8()
        )
    }
}

// ─── ICCE 适配器 ─────────────────────────────────────────

/**
 * ICCE 控制命令 (T/CA 110-2020, embedded/icce_protocol 模块设计 §3.1.4 control_command_t)
 *
 *   [0]     command_type (0x01 解锁 / 0x02 闭锁 / 0x03 启动 / 0x04 熄火 / 0x05 尾门 / 0x06 状态查询)
 *   [1]     target (0x00 = 车辆主体)
 *   [2-5]   user_id (BE u32)
 *   [6-37]  hmac[32]
 *
 * TODO(security): hmac 字段需要 HMAC-SHA256(会话密钥, 命令体) — 当前为零填充,
 * 待会话密钥管理就绪后实现 (勿在生产使用)。
 */
data class IcceControlCommand(
    val commandType: Int,
    val target: Int,
    val userId: Int,
    val hmac: ByteArray
) {
    override fun equals(other: Any?): Boolean =
        other is IcceControlCommand && commandType == other.commandType &&
            target == other.target && userId == other.userId && hmac.contentEquals(other.hmac)

    override fun hashCode(): Int = 31 * (31 * (31 * commandType + target) + userId) + hmac.contentHashCode()
}

/**
 * ICCE Digital Key BLE 协议适配器 (T/CA 110-2020)
 *
 * 控制指令写入 0xFEFE ControlCmd 特征, 格式见 [IcceControlCommand]。
 * 安全: 设置 [SessionContext.sessionKey] 后命令体自动 SM4-CBC 加密 (PKCS#7 填充)。
 */
class ICCEBleAdapter : BleProtocolAdapter {

    override val protocolType = BleProtocolType.ICCE

    override fun parseAdvertisement(scanRecord: ByteArray?, rssi: Int): VehicleAdvertise? {
        val parsed = BleAdvertiseParser.parse(scanRecord) ?: return null
        if (!parsed.hasServiceUuid(BleUuids.ICCE_SERVICE)) return null

        // ICCE 知识库未定义广播厂商数据结构; 沿用 iOS 端约定: 厂商数据 [2..6) → vehicleId
        val mfr = parsed.manufacturerData
        val vehicleId = when {
            mfr != null && mfr.data.size >= 4 -> "icce-" + mfr.data.copyOf(4).toHexString()
            scanRecord != null -> "icce-" + scanRecord.copyOf(4).toHexString()
            else -> return null
        }
        return VehicleAdvertise(
            vehicleId = vehicleId,
            rssi = rssi,
            protocolType = BleProtocolType.ICCE.ordinal + 1,
            supportsUwb = false,
            manufacturerData = mfrRawBytes(mfr)
        )
    }

    override fun buildUnlockCommand(keyId: String, session: SessionContext): ByteArray =
        buildControlCommand(BleCommandType.UNLOCK, session)

    override fun buildLockCommand(keyId: String, session: SessionContext): ByteArray =
        buildControlCommand(BleCommandType.LOCK, session)

    override fun buildStartEngineCommand(keyId: String, session: SessionContext): ByteArray =
        buildControlCommand(BleCommandType.ENGINE_ON, session)

    private fun buildControlCommand(type: BleCommandType, session: SessionContext): ByteArray {
        val plain = ByteBuffer.allocate(38)
            .put(type.value)
            .put(0x00.toByte())
            .putInt(session.userId)
            .put(ByteArray(32)) // TODO(security): HMAC-SHA256(会话密钥, 命令体)
            .array()
        val key = session.sessionKey
        return if (key != null) {
            Sm4.cbcEncrypt(key, session.sessionIv ?: Sm4.ZERO_IV, Sm4.pkcs7Pad(plain))
        } else {
            plain
        }
    }

    /**
     * 解析 ICCE 控制命令 (自动处理 SM4 解密)。
     * @param sessionKey 非 null 时按 SM4-CBC 解密
     * @param sessionIv  解密 IV (默认全零)
     */
    fun parseControlCommand(data: ByteArray, sessionKey: ByteArray? = null, sessionIv: ByteArray? = null): IcceControlCommand {
        val payload = if (sessionKey != null) {
            Sm4.pkcs7Unpad(Sm4.cbcDecrypt(sessionKey, sessionIv ?: Sm4.ZERO_IV, data))
        } else {
            data
        }
        require(payload.size >= 38) { "ICCE control command must be at least 38 bytes, got ${payload.size}" }
        return IcceControlCommand(
            commandType = payload[0].toUInt8(),
            target = payload[1].toUInt8(),
            userId = ((payload[2].toUInt8()) shl 24) or ((payload[3].toUInt8()) shl 16) or
                ((payload[4].toUInt8()) shl 8) or payload[5].toUInt8(),
            hmac = payload.copyOfRange(6, 38)
        )
    }

    override fun parseCommandResponse(data: ByteArray): CommandResult {
        if (data.isEmpty()) return CommandResult(false, -1, "empty response")
        val status = data[0].toUInt8()
        return if (status == 0x00) CommandResult(true)
        else CommandResult(false, status, "ICCE command failed: 0x%02X".format(status))
    }

    override fun parseVehicleStatus(data: ByteArray): VehicleStatus {
        if (data.size < 3) throw IllegalArgumentException("invalid status response")
        return VehicleStatus(
            locked = data[0] != 0x00.toByte(),
            engineOn = data[1] != 0x00.toByte(),
            batteryPct = data[2].toUInt8()
        )
    }
}
