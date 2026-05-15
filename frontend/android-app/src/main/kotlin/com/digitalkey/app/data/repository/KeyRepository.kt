/**
 * DigitalKey App - 钥匙Repository
 */
package com.digitalkey.app.data.repository

import com.digitalkey.app.data.model.*
import com.digitalkey.sdk.DigitalKeySdk
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.util.UUID

/**
 * 钥匙仓库
 * 负责钥匙相关的业务逻辑和数据管理
 */
class KeyRepository {

    private val sdk = DigitalKeySdk.getInstance()
        ?: throw IllegalStateException("SDK未初始化，请先调用 DigitalKeySdk.init()")

    private val apiClient = sdk.keyManager

    /**
     * 获取所有钥匙列表
     */
    suspend fun getKeys(): Result<List<KeyModel>> = withContext(Dispatchers.IO) {
        try {
            // 模拟从SDK获取数据（实际由SDK内部API调用）
            val mockKeys = listOf(
                KeyModel(
                    id = UUID.randomUUID().toString(),
                    name = "我的主钥匙",
                    vehicleId = "v001",
                    vehicleName = "特斯拉 Model 3",
                    vehiclePlate = "京A·12345",
                    vehicleImageUrl = null,
                    permissions = listOf(
                        Permission(PermissionType.UNLOCK, true),
                        Permission(PermissionType.LOCK, true),
                        Permission(PermissionType.START_ENGINE, true),
                        Permission(PermissionType.REMOTE_START, true),
                        Permission(PermissionType.CAR_FIND, true),
                        Permission(PermissionType.OPEN_TRUNK, true),
                        Permission(PermissionType.CLIMATE_CONTROL, true),
                        Permission(PermissionType.SHARE, true),
                        Permission(PermissionType.MANAGE, true)
                    ),
                    status = KeyStatus.ACTIVE,
                    issuerId = "admin",
                    issuerName = "管理员",
                    issuedAt = "2026-05-01T10:00:00Z",
                    expiresAt = null,
                    createdAt = "2026-05-01T10:00:00Z",
                    updatedAt = "2026-05-06T08:00:00Z"
                ),
                KeyModel(
                    id = UUID.randomUUID().toString(),
                    name = "家人备用钥匙",
                    vehicleId = "v001",
                    vehicleName = "特斯拉 Model 3",
                    vehiclePlate = "京A·12345",
                    vehicleImageUrl = null,
                    permissions = listOf(
                        Permission(PermissionType.UNLOCK, true),
                        Permission(PermissionType.LOCK, true),
                        Permission(PermissionType.START_ENGINE, true)
                    ),
                    status = KeyStatus.ACTIVE,
                    issuerId = "admin",
                    issuerName = "管理员",
                    issuedAt = "2026-05-02T14:00:00Z",
                    expiresAt = "2026-12-31T23:59:59Z",
                    createdAt = "2026-05-02T14:00:00Z",
                    updatedAt = "2026-05-02T14:00:00Z"
                )
            )
            delay(500) // 模拟网络延迟
            Result.success(mockKeys)
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * 根据ID获取钥匙详情
     */
    suspend fun getKeyById(keyId: String): Result<KeyModel> = withContext(Dispatchers.IO) {
        try {
            val keys = getKeys().getOrThrow()
            val key = keys.find { it.id == keyId }
            if (key != null) {
                Result.success(key)
            } else {
                Result.failure(NoSuchElementException("钥匙不存在: $keyId"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * 添加新钥匙
     */
    suspend fun addKey(request: AddKeyRequest): Result<KeyModel> = withContext(Dispatchers.IO) {
        try {
            // 模拟添加钥匙流程
            delay(1500) // 模拟激活过程
            val newKey = KeyModel(
                id = UUID.randomUUID().toString(),
                name = request.keyName,
                vehicleId = request.vehicleId ?: "v001",
                vehicleName = "特斯拉 Model 3",
                vehiclePlate = "京A·12345",
                vehicleImageUrl = null,
                permissions = listOf(
                    Permission(PermissionType.UNLOCK, true),
                    Permission(PermissionType.LOCK, true)
                ),
                status = KeyStatus.ACTIVE,
                issuerId = "self",
                issuerName = "自己",
                issuedAt = "2026-05-06T10:51:00Z",
                expiresAt = null,
                createdAt = "2026-05-06T10:51:00Z",
                updatedAt = "2026-05-06T10:51:00Z"
            )
            Result.success(newKey)
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * 分享钥匙
     */
    suspend fun shareKey(request: ShareKeyRequest): Result<ShareKeyResult> = withContext(Dispatchers.IO) {
        try {
            delay(1000)
            val result = ShareKeyResult(
                shareId = UUID.randomUUID().toString(),
                status = ShareStatus.SUCCESS,
                message = "分享成功",
                shareCode = generateShareCode(),
                shareUrl = "digitalkey://share/${UUID.randomUUID()}",
                expiresAt = request.expiresAt
            )
            Result.success(result)
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * 删除钥匙
     */
    suspend fun deleteKey(keyId: String): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            delay(500)
            Result.success(Unit)
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * 更新钥匙名称
     */
    suspend fun updateKeyName(keyId: String, newName: String): Result<KeyModel> = withContext(Dispatchers.IO) {
        try {
            delay(300)
            val key = getKeyById(keyId).getOrThrow()
            Result.success(key.copy(name = newName, updatedAt = "2026-05-06T10:51:00Z"))
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * 生成6位分享码
     */
    private fun generateShareCode(): String {
        val chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        return (1..6).map { chars.random() }.joinToString("")
    }
}
