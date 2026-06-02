/**
 * VehicleControlViewModel 单元测试
 *
 * 测试车辆控制页面的 ViewModel 业务逻辑。
 * 使用 MockK 模拟 VehicleRepository，验证车控命令、历史记录等状态流转。
 */
package com.digitalkey.app.home

import com.digitalkey.app.data.model.*
import com.digitalkey.app.data.repository.VehicleRepository
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.junit4.MockKRule
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test

@OptIn(ExperimentalCoroutinesApi::class)
class VehicleControlViewModelTest {

    @get:Rule
    val mockkRule = MockKRule(this)

    @RelaxedMockK
    private lateinit var vehicleRepository: VehicleRepository

    private lateinit var viewModel: VehicleControlViewModel
    private val testDispatcher = StandardTestDispatcher()

    private val testVehicles = listOf(
        VehicleModel(
            id = "v001",
            brand = "特斯拉",
            model = "Model 3",
            year = 2024,
            plate = "京A·12345",
            vin = "LVZZK58A9MA123456",
            color = "珍珠白",
            imageUrl = null,
            isConnected = true,
            lastSeenAt = "2026-05-06T10:00:00Z",
            location = VehicleLocation(
                latitude = 39.9042,
                longitude = 116.4074,
                address = "北京市朝阳区",
                timestamp = "2026-05-06T10:00:00Z"
            )
        ),
        VehicleModel(
            id = "v002",
            brand = "比亚迪",
            model = "汉 EV",
            year = 2025,
            plate = "沪B·67890",
            vin = "LVZZK58A9MB654321",
            color = "玄空灰",
            imageUrl = null,
            isConnected = false,
            lastSeenAt = "2026-05-05T18:30:00Z",
            location = VehicleLocation(
                latitude = 31.2304,
                longitude = 121.4737,
                address = "上海市浦东新区",
                timestamp = "2026-05-05T18:30:00Z"
            )
        )
    )

    private val testControlResult = VehicleControlResult(
        commandId = "cmd-001",
        command = VehicleCommand.LOCK,
        status = ControlStatus.SUCCESS,
        message = "锁车成功",
        vehicleId = "v001",
        timestamp = "2026-05-06T10:30:00Z"
    )

    private val testHistory = listOf(
        HistoryRecord(
            id = "h-001",
            keyId = "k001",
            vehicleId = "v001",
            vehicleName = "特斯拉 Model 3",
            command = VehicleCommand.UNLOCK,
            status = ControlStatus.SUCCESS,
            message = "解锁成功",
            location = VehicleLocation(39.9042, 116.4074, "北京市朝阳区"),
            timestamp = "2026-05-06T09:30:00Z"
        )
    )

    @Before
    fun setUp() {
        Dispatchers.setMain(testDispatcher)

        coEvery { vehicleRepository.getVehicles() } returns Result.success(testVehicles)
        coEvery { vehicleRepository.getVehicleById("v001") } returns Result.success(testVehicles[0])
        coEvery { vehicleRepository.getVehicleById("v002") } returns Result.success(testVehicles[1])
        coEvery { vehicleRepository.sendCommand(any(), any()) } returns Result.success(testControlResult)
        coEvery { vehicleRepository.getHistory(any(), any()) } returns Result.success(testHistory)
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
    }

    // ==================== 车辆列表 ====================

    @Test
    fun `init should load vehicles and auto-select first one`() = runTest(testDispatcher) {
        viewModel = VehicleControlViewModel(vehicleRepository)

        val state = viewModel.vehicleListState.value
        assert(state is UiState.Success) { "Initial vehicle state should be Success" }
        val successState = state as UiState.Success
        assert(successState.data.size == 2) { "Should have 2 vehicles" }

        // 默认选中第一辆车
        assert(viewModel.selectedVehicle.value == testVehicles[0]) {
            "Should auto-select the first vehicle"
        }
    }

    @Test
    fun `loadVehicles should emit Error on failure`() = runTest(testDispatcher) {
        coEvery { vehicleRepository.getVehicles() } returns Result.failure(RuntimeException("网络异常"))

        viewModel = VehicleControlViewModel(vehicleRepository)

        val state = viewModel.vehicleListState.value
        assert(state is UiState.Error) { "State should be Error on failure" }
    }

    @Test
    fun `selectVehicle should update selected vehicle`() = runTest(testDispatcher) {
        viewModel = VehicleControlViewModel(vehicleRepository)

        viewModel.selectVehicle(testVehicles[1])
        testDispatcher.scheduler.advanceUntilIdle()

        assert(viewModel.selectedVehicle.value == testVehicles[1]) {
            "Selected vehicle should be the second one"
        }
    }

    // ==================== 车辆控制命令 ====================

    @Test
    fun `lockVehicle should send LOCK command`() = runTest(testDispatcher) {
        viewModel = VehicleControlViewModel(vehicleRepository)

        viewModel.lockVehicle()
        testDispatcher.scheduler.advanceUntilIdle()

        coVerify(exactly = 1) { vehicleRepository.sendCommand("v001", VehicleCommand.LOCK) }

        val commandState = viewModel.commandState.value
        assert(commandState is UiState.Success) { "Command state should be Success" }
        val successState = commandState as UiState.Success
        assert(successState.data.command == VehicleCommand.LOCK) {
            "Command type should be LOCK"
        }
    }

    @Test
    fun `unlockVehicle should send UNLOCK command`() = runTest(testDispatcher) {
        viewModel = VehicleControlViewModel(vehicleRepository)

        viewModel.unlockVehicle()
        testDispatcher.scheduler.advanceUntilIdle()

        coVerify(exactly = 1) { vehicleRepository.sendCommand("v001", VehicleCommand.UNLOCK) }
    }

    @Test
    fun `findVehicle should send FIND_CAR command`() = runTest(testDispatcher) {
        viewModel = VehicleControlViewModel(vehicleRepository)

        viewModel.findVehicle()
        testDispatcher.scheduler.advanceUntilIdle()

        coVerify(exactly = 1) { vehicleRepository.sendCommand("v001", VehicleCommand.FIND_CAR) }
    }

    @Test
    fun `openTrunk should send OPEN_TRUNK command`() = runTest(testDispatcher) {
        viewModel = VehicleControlViewModel(vehicleRepository)

        viewModel.openTrunk()
        testDispatcher.scheduler.advanceUntilIdle()

        coVerify(exactly = 1) { vehicleRepository.sendCommand("v001", VehicleCommand.OPEN_TRUNK) }
    }

    @Test
    fun `remoteStart should send REMOTE_START command`() = runTest(testDispatcher) {
        viewModel = VehicleControlViewModel(vehicleRepository)

        viewModel.remoteStart()
        testDispatcher.scheduler.advanceUntilIdle()

        coVerify(exactly = 1) { vehicleRepository.sendCommand("v001", VehicleCommand.REMOTE_START) }
    }

    @Test
    fun `sendCommand should emit Error on failure`() = runTest(testDispatcher) {
        coEvery { vehicleRepository.sendCommand(any(), any()) } returns
                Result.failure(RuntimeException("命令执行失败"))

        viewModel = VehicleControlViewModel(vehicleRepository)

        viewModel.sendCommand(VehicleCommand.LOCK)
        testDispatcher.scheduler.advanceUntilIdle()

        val state = viewModel.commandState.value
        assert(state is UiState.Error) { "Command state should be Error on failure" }
        val errorState = state as UiState.Error
        assert(errorState.message.contains("命令执行失败")) {
            "Error message should match"
        }
    }

    @Test
    fun `successful command should trigger history refresh`() = runTest(testDispatcher) {
        viewModel = VehicleControlViewModel(vehicleRepository)

        viewModel.lockVehicle()
        testDispatcher.scheduler.advanceUntilIdle()

        // 成功后应该刷新历史记录
        coVerify(atLeast = 1) { vehicleRepository.getHistory(any(), any()) }
    }

    // ==================== 操作历史 ====================

    @Test
    fun `loadHistory should return history records`() = runTest(testDispatcher) {
        viewModel = VehicleControlViewModel(vehicleRepository)

        viewModel.loadHistory()
        testDispatcher.scheduler.advanceUntilIdle()

        val state = viewModel.historyState.value
        assert(state is UiState.Success) { "History state should be Success" }
        val successState = state as UiState.Success
        assert(successState.data.isNotEmpty()) { "History should not be empty" }
    }

    @Test
    fun `loadHistory with key ID should pass through`() = runTest(testDispatcher) {
        viewModel = VehicleControlViewModel(vehicleRepository)

        viewModel.loadHistory("k001")
        testDispatcher.scheduler.advanceUntilIdle()

        coVerify(exactly = 1) { vehicleRepository.getHistory("k001", 50) }
    }

    @Test
    fun `loadHistory should emit Empty when no records`() = runTest(testDispatcher) {
        coEvery { vehicleRepository.getHistory(any(), any()) } returns Result.success(emptyList())

        viewModel = VehicleControlViewModel(vehicleRepository)

        viewModel.loadHistory()
        testDispatcher.scheduler.advanceUntilIdle()

        val state = viewModel.historyState.value
        assert(state is UiState.Empty) { "History state should be Empty" }
    }

    // ==================== 状态管理 ====================

    @Test
    fun `clearCommandState should reset to null`() = runTest(testDispatcher) {
        viewModel = VehicleControlViewModel(vehicleRepository)

        viewModel.lockVehicle()
        testDispatcher.scheduler.advanceUntilIdle()
        assert(viewModel.commandState.value != null) { "Command state should be set" }

        viewModel.clearCommandState()
        assert(viewModel.commandState.value == null) { "Command state should be cleared" }
    }

    @Test
    fun `isVehicleOnline should reflect selected vehicle state`() = runTest(testDispatcher) {
        viewModel = VehicleControlViewModel(vehicleRepository)

        // 第一辆车在线
        assert(viewModel.isVehicleOnline()) { "First vehicle should be online" }

        // 切换为离线车辆
        viewModel.selectVehicle(testVehicles[1])
        assert(!viewModel.isVehicleOnline()) { "Second vehicle should be offline" }
    }

    @Test
    fun `sendCommand when no vehicle selected should be no-op`() = runTest(testDispatcher) {
        // 创建一个没有默认车辆的 ViewModel
        coEvery { vehicleRepository.getVehicles() } returns Result.success(emptyList())

        viewModel = VehicleControlViewModel(vehicleRepository)

        // 没有选中车辆时发送命令应为空操作
        viewModel.sendCommand(VehicleCommand.LOCK)
        testDispatcher.scheduler.advanceUntilIdle()

        assert(viewModel.commandState.value == null) {
            "Command state should remain null when no vehicle selected"
        }
    }
}
