package com.yuledkcs.sdk.ble

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * 协议适配器测试 — 2b-B 广告解析 / 2b-F 指令帧 + SM4
 */
class BleProtocolAdapterTest {

    // ─── 测试辅助 ─────────────────────────────────────────

    private fun ad(type: Int, vararg data: Int): ByteArray {
        val bytes = ByteArray(2 + data.size)
        bytes[0] = (data.size + 1).toByte()
        bytes[1] = type.toByte()
        data.forEachIndexed { i, v -> bytes[i + 2] = v.toByte() }
        return bytes
    }

    private fun concat(vararg arrays: ByteArray): ByteArray {
        var total = 0
        arrays.forEach { total += it.size }
        val out = ByteArray(total)
        var offset = 0
        arrays.forEach { a ->
            a.copyInto(out, offset)
            offset += a.size
        }
        return out
    }

    private fun hexToBytes(hex: String): ByteArray =
        ByteArray(hex.length / 2) { hex.substring(it * 2, it * 2 + 2).toInt(16).toByte() }

    private fun ByteArray.toHex(): String = joinToString("") { "%02x".format(it) }

    private fun sm4Key(): ByteArray = hexToBytes("0123456789ABCDEFFEDCBA9876543210")

    /** ICCOA 广播: FEF5 服务 + 厂商数据 (company 0x0100, vehicle_id 6B, ver, capability) */
    private fun iccoaAdvertise(capability: Int = 0x01): ByteArray = concat(
        ad(0x01, 0x06),
        ad(0x03, 0xF5, 0xFE),
        ad(0xFF, 0x00, 0x01, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, capability)
    )

    private fun cccAdvertise(): ByteArray = concat(
        ad(0x01, 0x06),
        ad(0x03, 0xF5, 0xFF), // CCC-TS-101 v4.0.0 DK Service 0xFFF5 (Table 19-6)
        ad(0xFF, 0x00, 0x01, 0x11, 0x22, 0x33, 0x44)
    )

    private fun icceAdvertise(): ByteArray = concat(
        ad(0x01, 0x06),
        ad(0x03, 0xFA, 0xFE),
        ad(0xFF, 0x00, 0x01, 0x55, 0x66, 0x77, 0x88)
    )

    // ─── 2b-B: 广告解析 ───────────────────────────────────

    @Test
    fun `iccoa adapter parses advertisement with vehicle id and uwb`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.ICCOA)

        val vehicle = adapter.parseAdvertisement(iccoaAdvertise(capability = 0x01), rssi = -55)
        assertNotNull(vehicle)
        assertEquals("iccoa-aabbccddeeff", vehicle!!.vehicleId)
        assertEquals(BleProtocolType.ICCOA.ordinal + 1, vehicle.protocolType)
        assertTrue(vehicle.supportsUwb)
    }

    @Test
    fun `iccoa adapter rejects advertisement without iccoa service`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.ICCOA)
        assertNull(adapter.parseAdvertisement(cccAdvertise(), rssi = -55))
        assertNull(adapter.parseAdvertisement(icceAdvertise(), rssi = -55))
        assertNull(adapter.parseAdvertisement(null, rssi = -55))
    }

    @Test
    fun `ccc adapter parses advertisement`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.CCC)
        val vehicle = adapter.parseAdvertisement(cccAdvertise(), rssi = -70)
        assertNotNull(vehicle)
        assertEquals("ccc-11223344", vehicle!!.vehicleId)
        assertEquals(BleProtocolType.CCC.ordinal + 1, vehicle.protocolType)
    }

    @Test
    fun `icce adapter parses advertisement`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.ICCE)
        val vehicle = adapter.parseAdvertisement(icceAdvertise(), rssi = -80)
        assertNotNull(vehicle)
        assertEquals("icce-55667788", vehicle!!.vehicleId)
        assertEquals(BleProtocolType.ICCE.ordinal + 1, vehicle.protocolType)
    }

    @Test
    fun `each adapter only matches its own service`() {
        for (type in BleProtocolType.entries) {
            val adapter = BleProtocolAdapterFactory.makeAdapter(type)
            val others = BleProtocolType.entries.filter { it != type }
            for (other in others) {
                val record = when (other) {
                    BleProtocolType.CCC -> cccAdvertise()
                    BleProtocolType.ICCOA -> iccoaAdvertise()
                    BleProtocolType.ICCE -> icceAdvertise()
                }
                assertNull("${type.name} should not match ${other.name} service", adapter.parseAdvertisement(record, -50))
            }
        }
    }

    // ─── 2b-F: CCC 指令帧 ─────────────────────────────────

    @Test
    fun `CCC adapter builds unlock command`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.CCC)
        val session = SessionContext(keyId = "key-1", vehicleId = "VH-1", sessionHandle = 0x0102, counter = 3)

        val command = adapter.buildUnlockCommand("key-1", session)
        // 规范 4 字节帧头 (CCC-TS-101 Table 19-19):
        //   [0] = Message Header (Message Type: SE=0x01)
        //   [1] = Payload Header (Message ID: DK_APDU_RQ=0x0B)
        //   [2-3] = length (big endian u16)
        assertEquals(CccFrame.MSG_TYPE_SE, command[0].toInt() and 0xFF)
        assertEquals(CccFrame.MSG_ID_DK_APDU_RQ, command[1].toInt() and 0xFF)
        val declaredLen = ((command[2].toInt() and 0xFF) shl 8) or (command[3].toInt() and 0xFF)
        assertEquals(13, declaredLen) // type(1) + handle(2) + counter(4) + keyIdLen(1) + keyId(5)
        assertEquals(CccFrame.HEADER_SIZE + 13, command.size)

        // 帧解析往返
        val frame = CccFrame.parse(command)
        assertNotNull(frame)
        assertEquals(CccFrame.MSG_TYPE_SE, frame!!.messageType)
        assertEquals(CccFrame.MSG_ID_DK_APDU_RQ, frame.messageId)

        // 透传载荷 (CccNullMessageSecurity): [0] type | [1-2] handle BE | [3-6] counter BE | [7] keyId len | [8..] keyId
        val p = frame.payload
        assertEquals(13, p.size)
        assertEquals(0x01, p[0].toInt() and 0xFF) // UNLOCK
        assertEquals(0x01, p[1].toInt() and 0xFF)
        assertEquals(0x02, p[2].toInt() and 0xFF)
        assertEquals(0x00, p[3].toInt() and 0xFF)
        assertEquals(0x00, p[4].toInt() and 0xFF)
        assertEquals(0x00, p[5].toInt() and 0xFF)
        assertEquals(0x03, p[6].toInt() and 0xFF)
        assertEquals(5, p[7].toInt() and 0xFF)
        assertArrayEquals("key-1".toByteArray(), p.copyOfRange(8, 13))
    }

    // ─── 2b-F: ICCOA 指令帧 ───────────────────────────────

    @Test
    fun `ICCOA adapter builds framed control command`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.ICCOA)
        val session = SessionContext(keyId = "key-1", vehicleId = "VH-1", counter = 0x1234)

        val command = adapter.buildUnlockCommand("key-1", session)

        val frame = IcocaFrame.parse(command)
        assertNotNull(frame)
        assertEquals(IcocaFrame.CMD_CTRL_REQUEST, frame!!.cmdId)
        assertEquals(0x1234, frame.seqNum)

        // 未设置会话密钥 → 明文负载: [type=0x01][keyIdLen=5]["key-1"][counter BE]
        assertEquals(11, frame.payload.size)
        assertEquals(0x01, frame.payload[0].toInt() and 0xFF)
        assertEquals(5, frame.payload[1].toInt() and 0xFF)
        assertArrayEquals("key-1".toByteArray(), frame.payload.copyOfRange(2, 7))
    }

    @Test
    fun `ICCOA adapter builds lock and engine commands`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.ICCOA)
        val session = SessionContext(keyId = "k", vehicleId = "VH-1", counter = 1)

        val lock = IcocaFrame.parse(adapter.buildLockCommand("k", session))!!
        assertEquals(0x02, lock.payload[0].toInt() and 0xFF)

        val engine = IcocaFrame.parse(adapter.buildStartEngineCommand("k", session))!!
        assertEquals(0x03, engine.payload[0].toInt() and 0xFF)
    }

    @Test
    fun `ICCOA adapter encrypts payload with SM4 when session key present`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.ICCOA)
        val session = SessionContext(
            keyId = "key-1", vehicleId = "VH-1", counter = 7,
            sessionKey = sm4Key(), sessionIv = Sm4.ZERO_IV
        )

        val command = adapter.buildUnlockCommand("key-1", session)

        val frame = IcocaFrame.parse(command)
        assertNotNull(frame)
        // 明文 11 字节 → PKCS7 填充 → 16 字节 SM4 密文
        assertEquals(16, frame!!.payload.size)

        // 用同一会话密钥解密还原明文负载
        val decrypted = Sm4.pkcs7Unpad(Sm4.cbcDecrypt(sm4Key(), Sm4.ZERO_IV, frame.payload))
        assertEquals(0x01, decrypted[0].toInt() and 0xFF)
        assertEquals(5, decrypted[1].toInt() and 0xFF)
        assertArrayEquals("key-1".toByteArray(), decrypted.copyOfRange(2, 7))
    }

    @Test
    fun `ICCOA adapter parses framed command response`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.ICCOA)

        // CTRL_RESPONSE (0x21) 帧, payload [0x00] = 成功
        val successFrame = IcocaFrame.build(IcocaFrame.CMD_CTRL_RESPONSE, 1, byteArrayOf(0x00))
        val success = adapter.parseCommandResponse(successFrame)
        assertTrue(success.success)

        // payload [0x12] = 失败, 错误码 0x12
        val failureFrame = IcocaFrame.build(IcocaFrame.CMD_CTRL_RESPONSE, 1, byteArrayOf(0x12))
        val failure = adapter.parseCommandResponse(failureFrame)
        assertFalse(failure.success)
        assertEquals(0x12, failure.errorCode)
    }

    @Test
    fun `ICCOA adapter parses raw response bytes for backward compatibility`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.ICCOA)
        assertTrue(adapter.parseCommandResponse(byteArrayOf(0x00)).success)
        val failure = adapter.parseCommandResponse(byteArrayOf(0x10))
        assertFalse(failure.success)
        assertEquals(16, failure.errorCode)
    }

    @Test
    fun `ICCOA adapter rejects malformed frame`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.ICCOA)
        val badFrame = IcocaFrame.build(IcocaFrame.CMD_CTRL_RESPONSE, 1, byteArrayOf(0x00))
        badFrame[badFrame.size - 1] = 0x00 // 破坏 EOP
        val result = adapter.parseCommandResponse(badFrame)
        assertFalse(result.success)
        assertEquals(-1, result.errorCode)
    }

    @Test
    fun `ICCOA adapter parses vehicle status from framed and raw data`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.ICCOA)

        val status = adapter.parseVehicleStatus(byteArrayOf(0x01, 0x00, 0x50))
        assertTrue(status.locked)
        assertFalse(status.engineOn)
        assertEquals(80, status.batteryPct)

        // STATUS_NOTIFY (0x30) 帧形式
        val framed = IcocaFrame.build(IcocaFrame.CMD_STATUS_NOTIFY, 1, byteArrayOf(0x01, 0x01, 0x64))
        val framedStatus = adapter.parseVehicleStatus(framed)
        assertTrue(framedStatus.locked)
        assertTrue(framedStatus.engineOn)
        assertEquals(100, framedStatus.batteryPct)
    }

    // ─── 2b-F: ICCE 指令帧 ────────────────────────────────

    @Test
    fun `ICCE adapter builds control command`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.ICCE)
        val session = SessionContext(keyId = "key-1", vehicleId = "VH-1", userId = 0x01020304)

        val command = adapter.buildUnlockCommand("key-1", session)

        // control_command_t: type(1) + target(1) + user_id(4) + hmac(32) = 38
        assertEquals(38, command.size)
        assertEquals(0x01, command[0].toInt() and 0xFF) // CMD_UNLOCK_DOOR
        assertEquals(0x00, command[1].toInt() and 0xFF) // target = 车辆主体
        assertEquals(0x01, command[2].toInt() and 0xFF)
        assertEquals(0x02, command[3].toInt() and 0xFF)
        assertEquals(0x03, command[4].toInt() and 0xFF)
        assertEquals(0x04, command[5].toInt() and 0xFF)
    }

    @Test
    fun `ICCE adapter parses control command roundtrip`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.ICCE)
        val session = SessionContext(keyId = "key-1", vehicleId = "VH-1", userId = 0xDEADBEEF)

        val command = adapter.buildLockCommand("key-1", session)
        val parsed = adapter.parseControlCommand(command)

        assertEquals(0x02, parsed.commandType)
        assertEquals(0x00, parsed.target)
        assertEquals(0xDEADBEEF, parsed.userId)
        assertEquals(32, parsed.hmac.size)
    }

    @Test
    fun `ICCE adapter encrypts control command with SM4 when session key present`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.ICCE)
        val session = SessionContext(
            keyId = "key-1", vehicleId = "VH-1", userId = 1,
            sessionKey = sm4Key(), sessionIv = Sm4.ZERO_IV
        )

        val command = adapter.buildStartEngineCommand("key-1", session)
        // 明文 38 字节 → PKCS7 → 48 字节 SM4 密文
        assertEquals(48, command.size)

        val parsed = adapter.parseControlCommand(command, sessionKey = sm4Key())
        assertEquals(0x03, parsed.commandType)
        assertEquals(1, parsed.userId)
    }

    @Test
    fun `ICCE adapter parseControlCommand rejects short data`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.ICCE)
        try {
            adapter.parseControlCommand(ByteArray(4))
            assertTrue("should have thrown", false)
        } catch (e: IllegalArgumentException) {
            // expected
        }
    }

    // ─── 响应解析 (原有行为) ───────────────────────────────

    @Test
    fun `CCC adapter parses command response`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.CCC)

        val success = adapter.parseCommandResponse(byteArrayOf(0x00))
        assertTrue(success.success)

        val failure = adapter.parseCommandResponse(byteArrayOf(0x10))
        assertFalse(failure.success)
        assertEquals(16, failure.errorCode)
    }

    @Test
    fun `all adapters parse responses`() {
        for (type in BleProtocolType.entries) {
            val adapter = BleProtocolAdapterFactory.makeAdapter(type)
            val result = adapter.parseCommandResponse(byteArrayOf(0x00))
            assertTrue("${type.name} should succeed on 0x00", result.success)
        }
    }

    @Test
    fun `CCC adapter parses vehicle status`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.CCC)

        val status = adapter.parseVehicleStatus(byteArrayOf(0x01, 0x00, 0x50))
        assertTrue(status.locked)
        assertFalse(status.engineOn)
        assertEquals(80, status.batteryPct)
    }

    @Test
    fun `service uuids match spec`() {
        assertEquals("0000FFF5-0000-1000-8000-00805F9B34FB", BleUuids.CCC_SERVICE.toString())
        assertEquals("0000FEF5-0000-1000-8000-00805F9B34FB", BleUuids.ICCOA_SERVICE.toString())
        assertEquals("0000FEFA-0000-1000-8000-00805F9B34FB", BleUuids.ICCE_SERVICE.toString())
    }
}
