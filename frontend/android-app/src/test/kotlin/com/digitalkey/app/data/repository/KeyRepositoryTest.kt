/**
 * KeyRepository 单元测试
 *
 * 测试 KeyRepository 中的各种数据操作是否正确返回 Result 包装的结果。
 * 由于 KeyRepository 内部使用模拟数据，测试重点验证数据结构和业务逻辑的正确性。
 */
package com.digitalkey.app.data.repository

import com.digitalkey.app.data.model.*
import com.digitalkey.sdk.DigitalKeySdk
import com.digitalkey.sdk.DigitalKeySdkImpl
import com.digitalkey.sdk.key.KeyManager
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import kotlinx.coroutines.runBlocking
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class KeyRepositoryTest {

    private lateinit var repository: KeyRepository

    @Before
    fun setUp() {
        // 模拟 SDK 单例，避免 IllegalStateException
        mockkObject(DigitalKeySdk)
        val mockSdk = mockk<DigitalKeySdkImpl>()
        val mockKeyManager = mockk<KeyManager>()
        every { DigitalKeySdk.getInstance() } returns mockSdk
        every { mockSdk.keyManager } returns mockKeyManager

        repository = KeyRepository()
    }

    @Test
    fun `getKeys should return success with non-empty list`() = runBlocking {
        val result = repository.getKeys()

        assert(result.isSuccess) { "getKeys() should return Success" }
        val keys = result.getOrThrow()
        assert(keys.isNotEmpty()) { "Key list should not be empty" }
        assert(keys.size >= 2) { "Should contain at least 2 keys" }
    }

    @Test
    fun `getKeys should return keys with correct structure`() = runBlocking {
        val keys = repository.getKeys().getOrThrow()
        val firstKey = keys.first()

        assert(firstKey.id.isNotBlank()) { "Key ID should not be blank" }
        assert(firstKey.name.isNotBlank()) { "Key name should not be blank" }
        assert(firstKey.vehicleId.isNotBlank()) { "Vehicle ID should not be blank" }
        assert(firstKey.permissions.isNotEmpty()) { "Permissions should not be empty" }
        assert(firstKey.status == KeyStatus.ACTIVE) { "First key should be ACTIVE" }
    }

    @Test
    fun `getKeys should return keys with full permissions for admin key`() = runBlocking {
        val keys = repository.getKeys().getOrThrow()
        // 第一把钥匙（主钥匙）应拥有所有权限
        val primaryKey = keys.first()

        val allPermissionTypes = PermissionType.values().toSet()
        val grantedTypes = primaryKey.permissions.map { it.type }.toSet()

        assert(grantedTypes.containsAll(allPermissionTypes)) {
            "Primary key should have all permission types"
        }
    }

    @Test
    fun `getKeyById should return matching key`() = runBlocking {
        val keys = repository.getKeys().getOrThrow()
        val firstKey = keys.first()

        val result = repository.getKeyById(firstKey.id)

        assert(result.isSuccess) { "getKeyById() should return Success" }
        assert(result.getOrThrow().id == firstKey.id) { "Should return the same key" }
    }

    @Test
    fun `getKeyById should return failure for non-existent key`() = runBlocking {
        val result = repository.getKeyById("non-existent-id")

        assert(result.isFailure) { "Should return Failure for non-existent key" }
    }

    @Test
    fun `addKey should create a new key successfully`() = runBlocking {
        val request = AddKeyRequest(
            activationCode = "ABC123",
            keyName = "测试钥匙",
            vehicleId = "v001"
        )

        val result = repository.addKey(request)

        assert(result.isSuccess) { "addKey() should return Success" }
        val newKey = result.getOrThrow()
        assert(newKey.name == "测试钥匙") { "Key name should match request" }
        assert(newKey.id.isNotBlank()) { "New key should have an ID" }
        assert(newKey.status == KeyStatus.ACTIVE) { "New key should be ACTIVE" }
    }

    @Test
    fun `shareKey should return success with share code`() = runBlocking {
        val request = ShareKeyRequest(
            keyId = "test-key-id",
            recipientId = "user-123",
            recipientName = "测试用户",
            permissions = listOf(PermissionType.UNLOCK, PermissionType.LOCK),
            expiresAt = null,
            message = "请帮我停车"
        )

        val result = repository.shareKey(request)

        assert(result.isSuccess) { "shareKey() should return Success" }
        val shareResult = result.getOrThrow()
        assert(shareResult.status == ShareStatus.SUCCESS) { "Share status should be SUCCESS" }
        assert(shareResult.shareCode != null) { "Should generate a share code" }
        assert(shareResult.shareUrl != null) { "Should generate a share URL" }
    }

    @Test
    fun `deleteKey should succeed`() = runBlocking {
        val result = repository.deleteKey("test-key-id")

        assert(result.isSuccess) { "deleteKey() should return Success" }
    }

    @Test
    fun `updateKeyName should update the key name`() = runBlocking {
        val keys = repository.getKeys().getOrThrow()
        val firstKey = keys.first()
        val newName = "更新后的钥匙名称"

        val result = repository.updateKeyName(firstKey.id, newName)

        assert(result.isSuccess) { "updateKeyName() should return Success" }
        assert(result.getOrThrow().name == newName) { "Key name should be updated" }
    }

    @Test
    fun `updateKeyName should fail for non-existent key`() = runBlocking {
        val result = repository.updateKeyName("non-existent-id", "新名称")

        assert(result.isFailure) { "Should fail for non-existent key" }
    }
}
