/**
 * VehicleRepository 单元测试
 *
 * 测试 VehicleRepository 中的车辆管理、命令发送、历史记录等功能。
 */
package com.digitalkey.app.data.repository

import com.digitalkey.app.data.model.*
import com.digitalkey.sdk.DigitalKeySdk
import com.digitalkey.sdk.DigitalKeySdkImpl
import com.digitalkey.sdk.vehicle.VehicleController
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import kotlinx.coroutines.runBlocking
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class VehicleRepositoryTest {

    private lateinit var repository: VehicleRepository

    @Before
    fun setUp() {
        mockkObject(DigitalKeySdk)
        val mockSdk = mockk<DigitalKeySdkImpl>()
        val mockVehicleController = mockk<VehicleController>()
        every { DigitalKeySdk.getInstance() } returns mockSdk
        every { mockSdk.vehicleController } returns mockVehicleController

        repository = VehicleRepository()
    }

    @Test
    fun `getVehicles should return non-empty list`() = runBlocking {
        val result = repository.getVehicles()

        assert(result.isSuccess) { "getVehicles() should return Success" }
        val vehicles = result.getOrThrow()
        assert(vehicles.isNotEmpty()) { "Vehicle list should not be empty" }
    }

    @Test
    fun `getVehicles should return vehicles with correct structure`() = runBlocking {
        val vehicles = repository.getVehicles().getOrThrow()
        val firstVehicle = vehicles.first()

        assert(firstVehicle.id.isNotBlank()) { "Vehicle ID should not be blank" }
        assert(firstVehicle.brand.isNotBlank()) { "Brand should not be blank" }
        assert(firstVehicle.model.isNotBlank()) { "Model should not be blank" }
        assert(firstVehicle.plate.isNotBlank()) { "Plate should not be blank" }
        assert(firstVehicle.year > 0) { "Year should be valid" }
    }

    @Test
    fun `getVehicles should contain both online and offline vehicles`() = runBlocking {
        val vehicles = repository.getVehicles().getOrThrow()

        val onlineCount = vehicles.count { it.isConnected }
        val offlineCount = vehicles.count { !it.isConnected }

        assert(onlineCount > 0) { "Should have at least one online vehicle" }
        assert(offlineCount > 0) { "Should have at least one offline vehicle" }
    }

    @Test
    fun `getVehicleById should return matching vehicle`() = runBlocking {
        val vehicles = repository.getVehicles().getOrThrow()
        val firstVehicle = vehicles.first()

        val result = repository.getVehicleById(firstVehicle.id)
        assert(result.isSuccess) { "getVehicleById() should return Success" }
        assert(result.getOrThrow().id == firstVehicle.id) { "Should return correct vehicle" }
    }

    @Test
    fun `getVehicleById should fail for non-existent vehicle`() = runBlocking {
        val result = repository.getVehicleById("non-existent")
        assert(result.isFailure) { "Should fail for non-existent vehicle" }
    }

    @Test
    fun `sendCommand to online vehicle should succeed`() = runBlocking {
        val vehicles = repository.getVehicles().getOrThrow()
        val onlineVehicle = vehicles.first { it.isConnected }

        val result = repository.sendCommand(onlineVehicle.id, VehicleCommand.LOCK)

        assert(result.isSuccess) { "sendCommand() should return Success" }
        val controlResult = result.getOrThrow()
        assert(controlResult.vehicleId == onlineVehicle.id) { "Vehicle ID should match" }
    }

    @Test
    fun `sendCommand LOCK should return lock success message`() = runBlocking {
        val vehicles = repository.getVehicles().getOrThrow()
        val onlineVehicle = vehicles.first { it.isConnected }

        val result = repository.sendCommand(onlineVehicle.id, VehicleCommand.LOCK)
        val controlResult = result.getOrThrow()

        assert(controlResult.command == VehicleCommand.LOCK) { "Command type should match" }
    }

    @Test
    fun `sendCommand to offline vehicle should return OFFLINE status`() = runBlocking {
        val vehicles = repository.getVehicles().getOrThrow()
        val offlineVehicle = vehicles.first { !it.isConnected }

        val result = repository.sendCommand(offlineVehicle.id, VehicleCommand.UNLOCK)

        assert(result.isSuccess) { "sendCommand() should return Success" }
        val controlResult = result.getOrThrow()
        assert(controlResult.status == ControlStatus.OFFLINE) {
            "Offline vehicle should return OFFLINE status"
        }
        assert(controlResult.message.contains("离线")) {
            "Message should indicate vehicle is offline"
        }
    }

    @Test
    fun `getHistory should return list of records`() = runBlocking {
        val result = repository.getHistory()

        assert(result.isSuccess) { "getHistory() should return Success" }
        val history = result.getOrThrow()
        assert(history.isNotEmpty()) { "History should not be empty" }
    }

    @Test
    fun `getHistory records should have valid structure`() = runBlocking {
        val history = repository.getHistory().getOrThrow()
        val record = history.first()

        assert(record.id.isNotBlank()) { "Record ID should not be blank" }
        assert(record.command is VehicleCommand) { "Command should be valid" }
        assert(record.status is ControlStatus) { "Status should be valid" }
        assert(record.timestamp.isNotBlank()) { "Timestamp should not be blank" }
    }

    @Test
    fun `getHistory should respect limit parameter`() = runBlocking {
        val limit = 3
        val history = repository.getHistory(limit = limit).getOrThrow()

        assert(history.size <= limit) { "History should respect limit" }
    }

    @Test
    fun `getHistory should filter by key ID`() = runBlocking {
        val keyId = "k001"
        val history = repository.getHistory(keyId = keyId).getOrThrow()

        assert(history.all { it.keyId == keyId }) {
            "All history records should match the key ID"
        }
    }

    @Test
    fun `getCommandMessage should return Chinese messages`() = runBlocking {
        val vehicles = repository.getVehicles().getOrThrow()
        val onlineVehicle = vehicles.first { it.isConnected }

        val lockResult = repository.sendCommand(onlineVehicle.id, VehicleCommand.LOCK).getOrThrow()
        assert(lockResult.message.isNotBlank()) { "Message should not be blank" }
        assert(lockResult.message.contains("锁车") || lockResult.message.contains("成功") ||
                lockResult.message.contains("失败")) {
            "Messages should be descriptive Chinese text"
        }
    }
}
