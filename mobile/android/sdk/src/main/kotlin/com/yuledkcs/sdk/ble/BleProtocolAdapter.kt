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
 * 指令帧格式 (Table 19-19, 规范 4 字节帧头):
 *   [0]     = message header (Bit[5:0]=Message Type, Bit[7:6]=RFU)
 *   [1]     = payload header (Message ID: DK_APDU_RQ=0x0B / DK_APDU_RS=0x0C)
 *   [2-3]   = length (big endian u16)
 *   [4...]  = payload
 *
 * 控制指令: SE 消息 (Message Type=0x01) + DK_APDU_RQ (0x0B),
 * 载荷经安全提供者封装 (生产: [CccSecureChannel] SCP03 加密: AES-128 + CMAC-AES-128,
 * 依据 docs/certification/ccc-ts101-ble-secure-channel.md, 2026-07-31 规范裁决)。
 * 当前默认透传实现仅用于单测/联调, 生产接入前必须替换。
 */
class CCCBleAdapter(
    private val security: CccMessageSecurity = CccNullMessageSecurity
) : BleProtocolAdapter {

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

        val plain = ByteBuffer.allocate(8 + keyIdBytes.size)
            .put(type.value)
            .putShort(session.sessionHandle)
            .putInt(session.counter.toInt())
            .put(keyIdBytes.size.toByte())
            .put(keyIdBytes)
            .array()
        val protected = security.encrypt(plain)
        return CccFrame.build(CccFrame.MSG_TYPE_SE, CccFrame.MSG_ID_DK_APDU_RQ, protected)
    }

    override fun parseCommandResponse(data: ByteArray): CommandResult {
        // 规范响应: SE 消息 + DK_APDU_RS (0x0C), payload 首字节为 APDU SW1 (0x90=成功)
        val frame = CccFrame.parse(data)
        if (frame != null && frame.messageType == CccFrame.MSG_TYPE_SE) {
            val sw1 = frame.payload.firstOrNull()?.toUInt8() ?: 0x00
            return if (sw1 == 0x90 || sw1 == 0x00) CommandResult(true)
            else CommandResult(false, sw1, "CCC APDU failed: SW=0x%02X".format(sw1))
        }
        // 回退: 裸 status 字节 (旧联调格式)
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
 *   SOP(0xAA) | CMD_ID | SEQ(LE u16) | LEN(LE u16) | PAYLOAD | XOR校验(不含SOP) | EOP(0x55)
 * 控制指令使用 CMD_CTRL_REQUEST (0x20), 响应为 CMD_CTRL_RESPONSE (0x21)。
 *
 * CTRL_REQUEST payload (依据 embedded/iccoa_protocol/src/iccoa/dk30/iccoa_dk30.c:88-104):
 *   [0]     control cmd (DK 3.0 CTRL 枚举: 0x01 LOCK / 0x02 UNLOCK / 0x03 ENGINE_ON / ...)
 *   [1]     param (参数, 无附加参数时为 0x00)
 *   — 共 2 字节; dk30.c:90 要求 payload_len >= 2
 *
 * 通用 [BleCommandType] → DK 3.0 CTRL 枚举映射 (iccoa_digital_key.h:155-167):
 *   UNLOCK→0x02, LOCK→0x01, ENGINE_ON→0x03, ENGINE_OFF→0x04, STATUS→0x05
 *
 * 安全 (裁决 AD-1/AD-2): ICCOA DK 3.0 应用层无加密 — 加密由 BLE 链路层
 * LE Secure Connections 负责, 本适配器输出明文帧, 不做 SM4。
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
        // ICCOA 应用层明文帧 — 不加密 (AD-1: 加密由 BLE LE SC 链路层负责)
        // 注: CTRL payload 仅 [cmd][param] 2 字节, 不含 keyId (dk30.c:88-104)
        val payload = buildControlPayload(type)
        val seqNum = session.counter.toInt() and 0xFFFF
        return IcocaFrame.build(IcocaFrame.CMD_CTRL_REQUEST, seqNum, payload)
    }

    /**
     * 构造 CTRL_REQUEST 负载: [cmd(1)][param(1)] (dk30.c:88-104)
     * 通用 [BleCommandType] → DK 3.0 CTRL 枚举 (iccoa_digital_key.h:155-167)
     */
    private fun buildControlPayload(type: BleCommandType): ByteArray {
        val cmd = when (type) {
            BleCommandType.UNLOCK -> 0x02 // CTRL_UNLOCK
            BleCommandType.LOCK -> 0x01   // CTRL_LOCK
            BleCommandType.ENGINE_ON -> 0x03   // CTRL_ENGINE_ON
            BleCommandType.ENGINE_OFF -> 0x04  // CTRL_ENGINE_OFF
            BleCommandType.STATUS -> 0x05      // CTRL_TRUNK_OPEN 复用 (契约 2.2)
        }
        return byteArrayOf(cmd.toByte(), 0x00) // param = 0x00 (无附加参数)
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
 *   [6-37]  hmac[32] = HMAC-SHA256(会话密钥, 命令体前 6 字节 command_type..user_id)
 *           覆盖范围标注: 待真机确认
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
 * 控制指令写入 0xFEFE ControlCmd 特征, 格式见 [IcceControlCommand]:
 *   [command_type(1)][target(1)][user_id(BE u32)][hmac(32)] = 38 字节
 *
 * 通用 [BleCommandType] → ICCE command_type 映射 (module_design.md §3.1.4):
 *   UNLOCK→0x01 (CMD_UNLOCK_DOOR), LOCK→0x02, ENGINE_ON→0x03 (CMD_START_ENGINE),
 *   ENGINE_OFF→0x04, STATUS→0x06 (CMD_QUERY_STATUS)
 *
 * 安全: hmac[32] = HMAC-SHA256(会话密钥, 命令体前 6 字节) — 会话密钥存在时真实计算
 * (crypto_engine.c crypto_hmac_sha256, RFC 2104); 未协商时零填充并告警(仅调试)。
 * 设置 [SessionContext.sessionKey] 后命令体自动 SM4-CBC 加密 (PKCS#7 填充, AD-7)。
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
        // 命令体前 6 字节: command_type(1) + target(1) + user_id(BE u32)
        // 通用 BleCommandType → ICCE command_type 映射
        val commandType = when (type) {
            BleCommandType.UNLOCK -> 0x01 // CMD_UNLOCK_DOOR
            BleCommandType.LOCK -> 0x02    // CMD_LOCK_DOOR
            BleCommandType.ENGINE_ON -> 0x03   // CMD_START_ENGINE
            BleCommandType.ENGINE_OFF -> 0x04  // CMD_STOP_ENGINE
            BleCommandType.STATUS -> 0x06      // CMD_QUERY_STATUS
        }
        val plain = ByteBuffer.allocate(38)
            .put(commandType.toByte())
            .put(0x00.toByte())
            .putInt(session.userId)
            .array()

        // hmac[32] = HMAC-SHA256(会话密钥, 命令体前 6 字节) (AD-6, 覆盖范围待真机确认)
        val key = session.sessionKey
        val hmac = if (key != null) {
            hmacSha256(key, plain.copyOf(6))
        } else {
            LOG.warning("ICCE: 未协商会话密钥, hmac 零填充 (仅调试, 勿用于生产)")
            ByteArray(32)
        }
        hmac.copyInto(plain, 6)

        return if (key != null) {
            Sm4.cbcEncrypt(key, session.sessionIv ?: Sm4.ZERO_IV, Sm4.pkcs7Pad(plain))
        } else {
            plain
        }
    }

    /** HMAC-SHA256 (RFC 2104), 输出 32 字节 — 与 crypto_engine.c crypto_hmac_sha256 同构 */
    private fun hmacSha256(key: ByteArray, data: ByteArray): ByteArray {
        val mac = javax.crypto.Mac.getInstance("HmacSHA256")
        mac.init(javax.crypto.spec.SecretKeySpec(key, "HmacSHA256"))
        return mac.doFinal(data)
    }

    companion object {
        private val LOG = java.util.logging.Logger.getLogger(ICCEBleAdapter::class.java.name)
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
