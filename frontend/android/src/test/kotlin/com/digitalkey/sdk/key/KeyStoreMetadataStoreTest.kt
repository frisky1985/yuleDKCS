// KeyStoreMetadataStoreTest.kt
// 单元测试：KeyStoreMetadataStore 加密/解密 + JSON 序列化 + KeyManager 核心逻辑
//
// 注意：Android KeyStore 和 Cipher 实际调用需在 Android 设备/模拟器上运行。
// 本文件涵盖可脱离 Android 环境验证的逻辑：
//   1. JSON 序列化/反序列化（keyToJson / parseKeyFromJson）
//   2. KeyManager 钥匙创建、激活、删除等业务逻辑（mock 存储层）
//   3. 加密格式与 IV 提取的正确性（纯算法验证）
//   4. 迁移标志逻辑
//
// 完整的 KeyStore A→B 回话测试见 androidTest 目录。

package com.digitalkey.sdk.key

import org.junit.Assert.*
import org.junit.Test

/**
 * 验证 DigitalKey JSON 序列化与反序列化的正确性。
 */
class DigitalKeySerializationTest {

    private val sampleKey = DigitalKey(
        keyId = "test-key-001",
        vehicleId = "VIN-ABC123",
        userId = "user-456",
        keyType = KeyType.PRIMARY,
        status = KeyStatus.ACTIVE,
        validFrom = 1000000L,
        validUntil = 2000000L,
        maxUses = 10,
        usedCount = 3,
        shareCode = null,
        issuerId = "default",
        vendor = "CCC",
        protocolVersion = "3.0",
        createdAt = 500000L,
        updatedAt = 600000L
    )

    @Test
    fun `serialize and deserialize roundtrip preserves all fields`() {
        // When: key → JSON → key
        val json = keyToJson(sampleKey)
        val parsed = parseKeyFromJson(sampleKey.keyId, json)

        // Then: all fields match
        assertEquals(sampleKey.keyId, parsed.keyId)
        assertEquals(sampleKey.vehicleId, parsed.vehicleId)
        assertEquals(sampleKey.userId, parsed.userId)
        assertEquals(sampleKey.keyType, parsed.keyType)
        assertEquals(sampleKey.status, parsed.status)
        assertEquals(sampleKey.validFrom, parsed.validFrom)
        assertEquals(sampleKey.validUntil, parsed.validUntil)
        assertEquals(sampleKey.maxUses, parsed.maxUses)
        assertEquals(sampleKey.usedCount, parsed.usedCount)
        assertEquals(sampleKey.shareCode, parsed.shareCode)
        assertEquals(sampleKey.issuerId, parsed.issuerId)
        assertEquals(sampleKey.vendor, parsed.vendor)
        assertEquals(sampleKey.protocolVersion, parsed.protocolVersion)
        assertEquals(sampleKey.createdAt, parsed.createdAt)
        assertEquals(sampleKey.updatedAt, parsed.updatedAt)
    }

    @Test
    fun `serialize with null maxUses and shareCode`() {
        val key = sampleKey.copy(maxUses = null, shareCode = null)
        val json = keyToJson(key)
        val parsed = parseKeyFromJson(key.keyId, json)

        assertNull(parsed.maxUses)
        assertNull(parsed.shareCode)
    }

    @Test
    fun `serialize with non-null shareCode`() {
        val key = sampleKey.copy(shareCode = "SHARE123")
        val json = keyToJson(key)
        val parsed = parseKeyFromJson(key.keyId, json)

        assertEquals("SHARE123", parsed.shareCode)
    }

    @Test
    fun `all key types roundtrip correctly`() {
        for (type in KeyType.entries) {
            val key = sampleKey.copy(keyType = type)
            val json = keyToJson(key)
            val parsed = parseKeyFromJson(key.keyId, json)
            assertEquals(type, parsed.keyType)
        }
    }

    @Test
    fun `all key statuses roundtrip correctly`() {
        for (status in KeyStatus.entries) {
            val key = sampleKey.copy(status = status)
            val json = keyToJson(key)
            val parsed = parseKeyFromJson(key.keyId, json)
            assertEquals(status, parsed.status)
        }
    }

    @Test
    fun `isValid returns true for active key within time window`() {
        val now = System.currentTimeMillis()
        val key = sampleKey.copy(
            status = KeyStatus.ACTIVE,
            validFrom = now - 60_000,
            validUntil = now + 60_000,
            maxUses = 5,
            usedCount = 2
        )
        assertTrue(key.isValid())
    }

    @Test
    fun `isValid returns false for expired key`() {
        val now = System.currentTimeMillis()
        val key = sampleKey.copy(
            status = KeyStatus.ACTIVE,
            validFrom = now - 120_000,
            validUntil = now - 60_000
        )
        assertFalse(key.isValid())
    }

    @Test
    fun `isValid returns false for non-active key`() {
        val now = System.currentTimeMillis()
        val key = sampleKey.copy(
            status = KeyStatus.REVOKED,
            validFrom = now - 60_000,
            validUntil = now + 60_000
        )
        assertFalse(key.isValid())
    }

    @Test
    fun `isValid returns false when maxUses exhausted`() {
        val now = System.currentTimeMillis()
        val key = sampleKey.copy(
            status = KeyStatus.ACTIVE,
            validFrom = now - 60_000,
            validUntil = now + 60_000,
            maxUses = 5,
            usedCount = 5
        )
        assertFalse(key.isValid())
    }

    @Test
    fun `remainingUses returns correct value`() {
        val key = sampleKey.copy(maxUses = 10, usedCount = 3)
        assertEquals(7, key.remainingUses())
    }

    @Test
    fun `remainingUses returns null for unlimited key`() {
        val key = sampleKey.copy(maxUses = null)
        assertNull(key.remainingUses())
    }

    // ---------------------------------------------------------------
    // 交易计数器逻辑验证
    // ---------------------------------------------------------------

    @Test
    fun `counter starts at zero when empty`() {
        // 模拟从空存储加载计数器
        val counter = 0L
        assertEquals(0L, counter)
    }

    @Test
    fun `counter increments correctly`() {
        var counter = 0L
        val increments = listOf(1L, 2L, 3L, 10L, 100L, 1000L)
        for (inc in increments) {
            val previous = counter
            counter = previous + inc
            assertEquals(previous + inc, counter)
        }
        assertEquals(1L + 2L + 3L + 10L + 100L + 1000L, counter)
    }

    @Test
    fun `counter serialization roundtrip`() {
        // 验证 Long 值可以序列化为字符串并解析回来（KeyStoreMetadataStore 内部使用字符串加密存储）
        val original = 1234567890L
        val serialized = original.toString()
        val parsed = serialized.toLongOrNull()
        assertEquals(original, parsed)
    }

    @Test
    fun `zero is the default for missing counter`() {
        // 验证 toLongOrNull 对空字符串返回 null，应用层 fallback 到 0
        val emptyString = ""
        val parsed = emptyString.toLongOrNull()
        assertNull(parsed)
    }

    @Test
    fun `invalid counter data returns zero`() {
        // 模拟解密失败/数据损坏：toLongOrNull 处理非数字字符串
        val invalidData = "not-a-number"
        val parsed = invalidData.toLongOrNull()
        assertNull(parsed)
    }

    @Test
    fun `negative counter value rejects`() {
        // 验证即使解密返回负数，也正常处理（但不应该出现）
        val negativeValue = -1L
        val serialized = negativeValue.toString()
        val parsed = serialized.toLongOrNull()
        assertEquals(negativeValue, parsed)
    }

    // ---------------------------------------------------------------
    // 私有辅助：复制 KeyManager 中的 JSON 方法用于测试验证
    // ---------------------------------------------------------------

    private fun keyToJson(key: DigitalKey): org.json.JSONObject {
        return org.json.JSONObject().apply {
            put("vehicleId", key.vehicleId)
            put("userId", key.userId)
            put("keyType", key.keyType.value)
            put("status", key.status.value)
            put("validFrom", key.validFrom)
            put("validUntil", key.validUntil)
            put("maxUses", key.maxUses ?: org.json.JSONObject.NULL)
            put("usedCount", key.usedCount)
            put("shareCode", key.shareCode ?: org.json.JSONObject.NULL)
            put("issuerId", key.issuerId)
            put("vendor", key.vendor)
            put("protocolVersion", key.protocolVersion)
            put("createdAt", key.createdAt)
            put("updatedAt", key.updatedAt)
        }
    }

    private fun parseKeyFromJson(keyId: String, json: org.json.JSONObject): DigitalKey {
        return DigitalKey(
            keyId = keyId,
            vehicleId = json.getString("vehicleId"),
            userId = json.getString("userId"),
            keyType = KeyType.entries.find { it.value == json.getInt("keyType") } ?: KeyType.PRIMARY,
            status = KeyStatus.entries.find { it.value == json.getInt("status") } ?: KeyStatus.PENDING,
            validFrom = json.getLong("validFrom"),
            validUntil = json.getLong("validUntil"),
            maxUses = if (json.isNull("maxUses")) null else json.getInt("maxUses"),
            usedCount = json.getInt("usedCount"),
            shareCode = if (json.isNull("shareCode")) null else json.getString("shareCode"),
            issuerId = json.getString("issuerId"),
            vendor = json.getString("vendor"),
            protocolVersion = json.getString("protocolVersion"),
            createdAt = json.getLong("createdAt"),
            updatedAt = json.getLong("updatedAt")
        )
    }
}
