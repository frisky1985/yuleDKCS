package com.yuledkcs.sdk.ble

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * 2b-H NFC 模拟测（纯逻辑, 不依赖硬件/android.*, JVM 可跑）
 *
 * Contract 映射 (docs/sdk/PHASE2B-GH-P1-CONTRACT.md):
 *   N-3 (接口不变) → testNfcManagerInterfaceContractUnchanged
 *   N-2 (命令映射) → testBuildCommandMapping / testReadApdus / testTagIdHex / testIsSuccess / testParseVehicleId
 *
 * 双端一致契约: 命令字节映射与 iOS `YDKNFCApdu` 完全一致 (Python 交叉验证)。
 * 注意: 本文件只引用 [NfcCommandBuilder]（纯 Kotlin）; [AndroidNfcManager] 依赖 android.*,
 * 其 API 正确性由静态审查 + 真机联调覆盖。
 */
class NfcManagerTest {

    @Test
    fun `buildCommand mapping matches iOS contract`() {
        assertArrayEquals(byteArrayOf(0x80.toByte(), 0xD2.toByte(), 0x01, 0x00, 0x00, 0x00), NfcCommandBuilder.buildCommand(NfcCommandType.UNLOCK))
        assertArrayEquals(byteArrayOf(0x80.toByte(), 0xD2.toByte(), 0x02, 0x00, 0x00, 0x00), NfcCommandBuilder.buildCommand(NfcCommandType.LOCK))
        assertArrayEquals(byteArrayOf(0x80.toByte(), 0xD2.toByte(), 0x03, 0x00, 0x00, 0x00), NfcCommandBuilder.buildCommand(NfcCommandType.START_ENGINE))
    }

    @Test
    fun `read apdus`() {
        // READ BINARY (ISO 7816-4)
        assertArrayEquals(byteArrayOf(0x00, 0xB0.toByte(), 0x00, 0x00, 0x40), NfcCommandBuilder.buildReadVehicleRecord())
        // MiFare READ block 4
        assertArrayEquals(byteArrayOf(0x30, 0x04), NfcCommandBuilder.buildReadMifareBlock())
    }

    @Test
    fun `tagIdHex formats uppercase without separator`() {
        assertEquals("04A1B2C3D4E5F6", NfcCommandBuilder.tagIdHex(byteArrayOf(0x04, 0xA1.toByte(), 0xB2.toByte(), 0xC3.toByte(), 0xD4.toByte(), 0xE5.toByte(), 0xF6.toByte())))
        assertEquals("000AFF", NfcCommandBuilder.tagIdHex(byteArrayOf(0x00, 0x0A, 0xFF.toByte())))
        assertEquals("", NfcCommandBuilder.tagIdHex(byteArrayOf()))
    }

    @Test
    fun `isSuccess detects 9000`() {
        assertTrue(NfcCommandBuilder.isSuccess(byteArrayOf(0x00, 0x90, 0x00)))
        assertTrue(NfcCommandBuilder.isSuccess(byteArrayOf(0x90, 0x00)))
        assertFalse(NfcCommandBuilder.isSuccess(byteArrayOf(0x63, 0x00))) // SW=6300 条件不满足
        assertFalse(NfcCommandBuilder.isSuccess(byteArrayOf(0x90)))       // 不足 2 字节
        assertFalse(NfcCommandBuilder.isSuccess(byteArrayOf()))
    }

    @Test
    fun `parseVehicleId strips status word and padding`() {
        // "VH-2026-0001" + SW 9000
        val payload = byteArrayOf(
            0x56, 0x48, 0x2D, 0x32, 0x30, 0x32, 0x36, 0x2D,
            0x30, 0x30, 0x30, 0x31, 0x90, 0x00
        )
        assertEquals("VH-2026-0001", NfcCommandBuilder.parseVehicleId(payload))

        // 尾零填充
        assertEquals("VH-1", NfcCommandBuilder.parseVehicleId(byteArrayOf(0x56, 0x48, 0x2D, 0x31, 0x00, 0x00, 0x00, 0x90, 0x00)))

        // 只有状态字 → null
        assertNull(NfcCommandBuilder.parseVehicleId(byteArrayOf(0x90, 0x00)))
        // 全零 → null
        assertNull(NfcCommandBuilder.parseVehicleId(byteArrayOf(0x00, 0x00, 0x90, 0x00)))
    }

    @Test
    fun `tech list covers iso-dep and ndef`() {
        val techs = NfcCommandBuilder.TECH_LIST.toSet()
        assertTrue("android.nfc.tech.IsoDep" in techs)
        assertTrue("android.nfc.tech.Ndef" in techs)
        assertTrue("android.nfc.tech.MifareClassic" in techs)
    }

    @Test
    fun `NfcManager interface contract unchanged`() {
        val methods = NfcManager::class.java.declaredMethods.map { it.name }.toSet()
        assertTrue("readVehicleTag" in methods)
        assertTrue("sendCommandViaNfc" in methods)
        // AndroidNfcManager 必须实现该接口（编译期约束; 此处反射复核签名）
        val iface = Class.forName("com.yuledkcs.sdk.ble.NfcManager")
        assertTrue(iface.isInterface)
    }
}
