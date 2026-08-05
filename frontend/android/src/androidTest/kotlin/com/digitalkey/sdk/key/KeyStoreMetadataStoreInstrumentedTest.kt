// KeyStoreMetadataStoreInstrumentedTest.kt
// Instrumented test — 需要 Android 设备/模拟器，验证 KeyStore AES 回话正确性。
//
// 测试内容：
//   1. AES-256/GCM 密钥在 Android KeyStore 中生成和可复用
//   2. writeMetadata → readMetadata 回话完整性
//   3. 首次写入后 IV 自动生成，再次写入 IV 重新生成（每次都不同）
//   4. 两次加密结果不同（IV 随机）
//   5. clearMetadata 后 read 返回 null
//   6. 错误密钥/篡改密文导致解密失败 → 返回 null

package com.digitalkey.sdk.key

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.security.KeyStore
import javax.crypto.Cipher

@RunWith(AndroidJUnit4::class)
class KeyStoreMetadataStoreInstrumentedTest {

    private val testPrefsName = "test_yuledkcs_keys"
    private val testKeyAlias = "test_yuledkcs_metadata_key"

    private lateinit var context: Context
    private lateinit var store: KeyStoreMetadataStore

    @Before
    fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        // 清理测试遗留
        cleanupTestKeyStore()
        cleanupTestPrefs()

        store = KeyStoreMetadataStore(context, testPrefsName, testKeyAlias)
    }

    @After
    fun tearDown() {
        cleanupTestKeyStore()
        cleanupTestPrefs()
    }

    // ---------------------------------------------------------------
    // 基本回话测试
    // ---------------------------------------------------------------

    @Test
    fun writeAndReadRoundtrip() {
        val original = """{"key1":{"vehicleId":"VIN-001","userId":"u1","keyType":1}}"""
        store.writeMetadata(original)

        val result = store.readMetadata()
        assertEquals(original, result)
    }

    @Test
    fun readReturnsNullWhenEmpty() {
        val result = store.readMetadata()
        assertNull("No data written should return null", result)
    }

    @Test
    fun writeMultipleTimes_roundtripPreservesLatest() {
        store.writeMetadata("""{"k":"v1"}""")
        store.writeMetadata("""{"k":"v2"}""")

        val result = store.readMetadata()
        assertEquals("""{"k":"v2"}""", result)
    }

    @Test
    fun clearMetadata_removesAllData() {
        store.writeMetadata("""{"key":"value"}""")
        store.clearMetadata()

        val result = store.readMetadata()
        assertNull("After clear, readMetadata should return null", result)
    }

    // ---------------------------------------------------------------
    // IV 随机性验证
    // ---------------------------------------------------------------

    @Test
    fun eachEncryptionProducesDifferentCiphertext() {
        val payload = """{"stable":"data"}"""

        store.writeMetadata(payload)
        // 读取存储格式
        val prefs = context.getSharedPreferences(testPrefsName, Context.MODE_PRIVATE)
        val ciphertext1 = prefs.getString("keys_encrypted", null)

        store.writeMetadata(payload)
        val ciphertext2 = prefs.getString("keys_encrypted", null)

        // IV 随机，所以两次加密结果不同
        assertNotNull(ciphertext1)
        assertNotNull(ciphertext2)
        assertNotEquals(
            "Same plaintext should produce different ciphertext (random IV)",
            ciphertext1, ciphertext2
        )
    }

    // ---------------------------------------------------------------
    // 大 JSON 负载测试
    // ---------------------------------------------------------------

    @Test
    fun writeAndReadLargeJson() {
        val sb = StringBuilder()
        sb.append("{")
        for (i in 0 until 200) {
            if (i > 0) sb.append(",")
            sb.append(""" "key_$i": {"id":$i,"vehicle":"V-$i","type":1,"status":0} """)
        }
        sb.append("}")

        val largeJson = sb.toString()
        store.writeMetadata(largeJson)

        val result = store.readMetadata()
        assertEquals("Large JSON roundtrip", largeJson, result)
    }

    // ---------------------------------------------------------------
    // 密钥持久化验证：两个 store 实例共享同一密钥
    // ---------------------------------------------------------------

    @Test
    fun keyIsReusedAcrossStoreInstances() {
        val payload = """{"persistent":"test"}"""

        // 第一个实例写入
        store.writeMetadata(payload)

        // 创建新实例（同一 keyAlias）读取
        val store2 = KeyStoreMetadataStore(context, testPrefsName, testKeyAlias)
        val result = store2.readMetadata()

        assertEquals("Same key alias should decrypt cross-instance", payload, result)
    }

    // ---------------------------------------------------------------
    // 篡改检测：密文被修改后解密失败
    // ---------------------------------------------------------------

    @Test
    fun tamperedCiphertextReturnsNull() {
        store.writeMetadata("""{"sensitive":"data"}""")

        val prefs = context.getSharedPreferences(testPrefsName, Context.MODE_PRIVATE)
        val original = prefs.getString("keys_encrypted", null)!!

        // 篡改 Base64 密文
        val tampered = original.replace('A', 'B').replace('a', 'b')
        prefs.edit().putString("keys_encrypted", tampered).apply()

        val result = store.readMetadata()
        assertNull("Tampered ciphertext should fail decryption", result)
    }

    // ---------------------------------------------------------------
    // UTF-8 多语言文本
    // ---------------------------------------------------------------

    @Test
    fun writeAndReadUnicodeText() {
        val payload = """{"message":"数字钥匙元数据测试 - 🔑 🚗 ✅"}"""
        store.writeMetadata(payload)
        val result = store.readMetadata()
        assertEquals(payload, result)
    }

    // ---------------------------------------------------------------
    // 迁移标志测试
    // ---------------------------------------------------------------

    @Test
    fun migrationFlagIsSetAfterRead() {
        // 新 store 没有数据时，migrateFromLegacyIfNeeded 应标记完成
        // 验证未执行旧数据写入
        store.migrateFromLegacyIfNeeded()

        val prefs = context.getSharedPreferences(testPrefsName, Context.MODE_PRIVATE)
        assertTrue("Migration flag should be set", prefs.getBoolean("migration_complete", false))
        assertNull("No encrypted data should exist",
            prefs.getString("keys_encrypted", null))
    }

    // ---------------------------------------------------------------
    // 交易计数器持久化测试
    // ---------------------------------------------------------------

    @Test
    fun counterReadWriteRoundtrip() {
        store.writeCounter(42L)
        val result = store.readCounter()
        assertEquals(42L, result)
    }

    @Test
    fun counterReadReturnsZeroWhenEmpty() {
        val result = store.readCounter()
        assertEquals(0L, "No counter written should return 0", result)
    }

    @Test
    fun counterWriteMultipleTimes_preservesLatest() {
        store.writeCounter(100L)
        store.writeCounter(200L)
        store.writeCounter(300L)

        val result = store.readCounter()
        assertEquals(300L, result)
    }

    @Test
    fun counterPersistsAcrossStoreInstances() {
        store.writeCounter(888L)

        // 新实例（同一 keyAlias 和 prefsName）应能读取相同的计数器值
        val store2 = KeyStoreMetadataStore(context, testPrefsName, testKeyAlias)
        val result = store2.readCounter()

        assertEquals(888L, "Same key alias should decrypt counter cross-instance", result)
    }

    @Test
    fun counterIndependentFromMetadata() {
        // 验证计数器与钥匙元数据使用不同的 SharedPreferences 键
        store.writeMetadata("""{"key":"value"}""")
        store.writeCounter(777L)

        // 读取各自的值
        val metadata = store.readMetadata()
        val counter = store.readCounter()

        assertEquals("""{"key":"value"}""", metadata)
        assertEquals(777L, counter)
    }

    @Test
    fun eachCounterEncryptionProducesDifferentCiphertext() {
        store.writeCounter(1L)
        val prefs = context.getSharedPreferences(testPrefsName, Context.MODE_PRIVATE)
        val ciphertext1 = prefs.getString("tx_counter_encrypted", null)

        store.writeCounter(1L) // 相同值
        val ciphertext2 = prefs.getString("tx_counter_encrypted", null)

        assertNotNull(ciphertext1)
        assertNotNull(ciphertext2)
        assertNotEquals(
            "Same counter value should produce different ciphertext (random IV)",
            ciphertext1, ciphertext2
        )
    }

    @Test
    fun tamperedCounterCiphertextReturnsZero() {
        store.writeCounter(999L)

        val prefs = context.getSharedPreferences(testPrefsName, Context.MODE_PRIVATE)
        val original = prefs.getString("tx_counter_encrypted", null)!!

        // 篡改 Base64 密文
        val tampered = original.replace('A', 'B').replace('a', 'b')
        prefs.edit().putString("tx_counter_encrypted", tampered).apply()

        val result = store.readCounter()
        assertEquals(0L, "Tampered counter ciphertext should return 0", result)
    }

    @Test
    fun clearMetadata_alsoClearsCounter() {
        store.writeMetadata("""{"key":"data"}""")
        store.writeCounter(555L)
        store.clearMetadata()

        assertNull(store.readMetadata())
        assertEquals(0L, "clearMetadata should also clear counter", store.readCounter())
    }

    @Test
    fun clearCounter_onlyRemovesCounter() {
        store.writeMetadata("""{"key":"value"}""")
        store.writeCounter(444L)
        store.clearCounter()

        assertEquals(0L, store.readCounter())
        assertEquals("""{"key":"value"}""", store.readMetadata())
    }

    @Test
    fun zeroAndLargeCounterValues() {
        // 边界值测试
        store.writeCounter(0L)
        assertEquals(0L, store.readCounter())

        store.writeCounter(Long.MAX_VALUE)
        assertEquals(Long.MAX_VALUE, store.readCounter())

        store.writeCounter(1L)
        assertEquals(1L, store.readCounter())
    }

    // ---------------------------------------------------------------
    // 清理辅助
    // ---------------------------------------------------------------

    private fun cleanupTestKeyStore() {
        try {
            val ks = KeyStore.getInstance("AndroidKeyStore")
            ks.load(null)
            if (ks.containsAlias(testKeyAlias)) {
                ks.deleteEntry(testKeyAlias)
            }
        } catch (_: Exception) {
            // ignore — fresh environment
        }
    }

    private fun cleanupTestPrefs() {
        context.getSharedPreferences(testPrefsName, Context.MODE_PRIVATE)
            .edit().clear().apply()
    }
}
