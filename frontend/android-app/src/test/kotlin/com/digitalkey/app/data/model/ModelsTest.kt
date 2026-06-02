/**
 * 数据模型单元测试
 *
 * 验证关键数据模型的正确性，包括：
 * - 序列化/反序列化
 * - 枚举值覆盖
 * - 数据类常用操作
 */
package com.digitalkey.app.data.model

import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class ModelsTest {

    @Test
    fun `keyModel should be properly constructed`() {
        val key = KeyModel(
            id = "test-id",
            name = "测试钥匙",
            vehicleId = "v001",
            vehicleName = "测试车辆",
            vehiclePlate = "京A·00000",
            permissions = listOf(
                Permission(PermissionType.UNLOCK, true),
                Permission(PermissionType.LOCK, false)
            ),
            status = KeyStatus.ACTIVE,
            issuerId = "issuer-1",
            issuerName = "发放者",
            issuedAt = "2026-01-01T00:00:00Z",
            createdAt = "2026-01-01T00:00:00Z",
            updatedAt = "2026-01-01T00:00:00Z"
        )

        assert(key.id == "test-id")
        assert(key.name == "测试钥匙")
        assert(key.vehiclePlate == "京A·00000")
        assert(key.permissions.size == 2)
        assert(key.status == KeyStatus.ACTIVE)
    }

    @Test
    fun `keyModel copy should preserve fields`() {
        val original = KeyModel(
            id = "key-1", name = "旧名称", vehicleId = "v001",
            vehicleName = "车", vehiclePlate = "京A·11111",
            issuerId = "u1", issuerName = "用户",
            issuedAt = "2026-01-01T00:00:00Z"
        )

        val updated = original.copy(name = "新名称")

        assert(updated.name == "新名称")
        assert(updated.id == "key-1")     // copy 保留其他字段
        assert(updated.vehicleId == "v001")
        assert(updated.issuerName == "用户")
    }

    @Test
    fun `vehicleModel should have correct defaults`() {
        val vehicle = VehicleModel(
            id = "v001", brand = "Tesla", model = "Model 3",
            year = 2024, plate = "京A·12345", vin = "VIN001",
            color = "白"
        )

        assert(vehicle.imageUrl == null)
        assert(!vehicle.isConnected)  // 默认离线
        assert(vehicle.location == null)
    }

    @Test
    fun `uiState sealed class should cover all cases`() {
        val loading: UiState<String> = UiState.Loading
        val success: UiState<String> = UiState.Success("data")
        val error: UiState<String> = UiState.Error("出错了")
        val empty: UiState<List<String>> = UiState.Empty

        assert(loading is UiState.Loading)
        assert(success is UiState.Success)
        assert((success as UiState.Success).data == "data")
        assert(error is UiState.Error)
        assert((error as UiState.Error).message == "出错了")
        assert(empty is UiState.Empty)
    }

    @Test
    fun `keyStatus enum should have all values`() {
        val allStatuses = KeyStatus.values().toSet()
        assert(allStatuses == setOf(
            KeyStatus.ACTIVE,
            KeyStatus.INACTIVE,
            KeyStatus.EXPIRED,
            KeyStatus.PENDING,
            KeyStatus.REVOKED
        ))
    }

    @Test
    fun `permissionType enum should have comprehensive values`() {
        val allTypes = PermissionType.values().toSet()
        assert(PermissionType.UNLOCK in allTypes)
        assert(PermissionType.LOCK in allTypes)
        assert(PermissionType.START_ENGINE in allTypes)
        assert(PermissionType.SHARE in allTypes)
        assert(PermissionType.MANAGE in allTypes)
    }

    @Test
    fun `vehicleCommand enum should have all control commands`() {
        assert(VehicleCommand.LOCK in VehicleCommand.values())
        assert(VehicleCommand.UNLOCK in VehicleCommand.values())
        assert(VehicleCommand.START_ENGINE in VehicleCommand.values())
        assert(VehicleCommand.REMOTE_START in VehicleCommand.values())
        assert(VehicleCommand.FIND_CAR in VehicleCommand.values())
    }

    @Test
    fun `controlStatus enum should cover all states`() {
        val allStatus = ControlStatus.values().toSet()
        assert(allStatus == setOf(
            ControlStatus.SUCCESS,
            ControlStatus.FAILED,
            ControlStatus.PENDING,
            ControlStatus.TIMEOUT,
            ControlStatus.OFFLINE
        ))
    }

    @Test
    fun `ShareKeyRequest should be constructable`() {
        val request = ShareKeyRequest(
            keyId = "key-1",
            recipientId = "user-2",
            recipientName = "接收者",
            permissions = listOf(PermissionType.UNLOCK, PermissionType.LOCK),
            expiresAt = "2026-12-31T23:59:59Z",
            message = "交给你了"
        )

        assert(request.keyId == "key-1")
        assert(request.recipientName == "接收者")
        assert(request.permissions.size == 2)
        assert(request.message == "交给你了")
    }

    @Test
    fun `ShareKeyResult should contain share code and URL`() {
        val result = ShareKeyResult(
            shareId = "share-1",
            status = ShareStatus.SUCCESS,
            message = "分享成功",
            shareCode = "ABC123",
            shareUrl = "digitalkey://share/abc-123"
        )

        assert(result.status == ShareStatus.SUCCESS)
        assert(result.shareCode == "ABC123")
        assert(result.shareUrl != null)
    }

    @Test
    fun `AddKeyRequest should have optional fields`() {
        val request = AddKeyRequest(
            activationCode = "CODE123",
            keyName = "新钥匙",
            vehicleId = null
        )

        assert(request.activationCode == "CODE123")
        assert(request.keyName == "新钥匙")
        assert(request.vehicleId == null)
    }

    @Test
    fun `HistoryRecord should track command execution`() {
        val record = HistoryRecord(
            id = "h-001",
            keyId = "k001",
            vehicleId = "v001",
            vehicleName = "特斯拉 Model 3",
            command = VehicleCommand.UNLOCK,
            status = ControlStatus.SUCCESS,
            message = "解锁成功",
            location = null,
            timestamp = "2026-01-01T00:00:00Z"
        )

        assert(record.command == VehicleCommand.UNLOCK)
        assert(record.status == ControlStatus.SUCCESS)
        assert(record.message == "解锁成功")
        assert(record.location == null)
    }
}
