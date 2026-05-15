/**
 * DigitalKey App - 车辆控制 ViewModel
 */
package com.digitalkey.app.home

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.digitalkey.app.data.model.*
import com.digitalkey.app.data.repository.VehicleRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import javax.inject.Inject

/**
 * 车辆控制 ViewModel
 * 负责管理车控页面的UI状态和业务逻辑
 */
@HiltViewModel
class VehicleControlViewModel @Inject constructor(
    private val vehicleRepository: VehicleRepository
) : ViewModel() {

    // 车辆列表状态
    private val _vehicleListState = MutableStateFlow<UiState<List<VehicleModel>>>(UiState.Loading)
    val vehicleListState: StateFlow<UiState<List<VehicleModel>>> = _vehicleListState.asStateFlow()

    // 当前选中的车辆
    private val _selectedVehicle = MutableStateFlow<VehicleModel?>(null)
    val selectedVehicle: StateFlow<VehicleModel?> = _selectedVehicle.asStateFlow()

    // 命令执行状态
    private val _commandState = MutableStateFlow<UiState<VehicleControlResult>?>(null)
    val commandState: StateFlow<UiState<VehicleControlResult>?> = _commandState.asStateFlow()

    // 操作历史状态
    private val _historyState = MutableStateFlow<UiState<List<HistoryRecord>>>(UiState.Loading)
    val historyState: StateFlow<UiState<List<HistoryRecord>>> = _historyState.asStateFlow()

    init {
        loadVehicles()
    }

    /**
     * 加载车辆列表
     */
    fun loadVehicles() {
        viewModelScope.launch {
            _vehicleListState.value = UiState.Loading
            vehicleRepository.getVehicles()
                .onSuccess { vehicles ->
                    _vehicleListState.value = if (vehicles.isEmpty()) {
                        UiState.Empty
                    } else {
                        // 默认选中第一辆车
                        if (_selectedVehicle.value == null && vehicles.isNotEmpty()) {
                            _selectedVehicle.value = vehicles.first()
                        }
                        UiState.Success(vehicles)
                    }
                }
                .onFailure { error ->
                    _vehicleListState.value = UiState.Error(
                        message = error.message ?: "加载车辆列表失败",
                        code = null
                    )
                }
        }
    }

    /**
     * 选择车辆
     */
    fun selectVehicle(vehicle: VehicleModel) {
        _selectedVehicle.value = vehicle
    }

    /**
     * 发送控制命令
     */
    fun sendCommand(command: VehicleCommand) {
        val vehicle = _selectedVehicle.value ?: return

        viewModelScope.launch {
            _commandState.value = UiState.Loading

            vehicleRepository.sendCommand(vehicle.id, command)
                .onSuccess { result ->
                    _commandState.value = UiState.Success(result)
                    // 成功后刷新历史记录
                    loadHistory()
                }
                .onFailure { error ->
                    _commandState.value = UiState.Error(
                        message = error.message ?: "命令执行失败",
                        code = null
                    )
                }
        }
    }

    /**
     * 快捷命令 - 锁车
     */
    fun lockVehicle() = sendCommand(VehicleCommand.LOCK)

    /**
     * 快捷命令 - 解锁
     */
    fun unlockVehicle() = sendCommand(VehicleCommand.UNLOCK)

    /**
     * 快捷命令 - 寻车
     */
    fun findVehicle() = sendCommand(VehicleCommand.FIND_CAR)

    /**
     * 快捷命令 - 打开后备箱
     */
    fun openTrunk() = sendCommand(VehicleCommand.OPEN_TRUNK)

    /**
     * 快捷命令 - 远程启动
     */
    fun remoteStart() = sendCommand(VehicleCommand.REMOTE_START)

    /**
     * 加载操作历史
     */
    fun loadHistory(keyId: String? = null) {
        viewModelScope.launch {
            _historyState.value = UiState.Loading
            vehicleRepository.getHistory(keyId)
                .onSuccess { records ->
                    _historyState.value = if (records.isEmpty()) {
                        UiState.Empty
                    } else {
                        UiState.Success(records)
                    }
                }
                .onFailure { error ->
                    _historyState.value = UiState.Error(
                        message = error.message ?: "加载历史记录失败",
                        code = null
                    )
                }
        }
    }

    /**
     * 清除命令状态
     */
    fun clearCommandState() {
        _commandState.value = null
    }

    /**
     * 检查车辆是否在线
     */
    fun isVehicleOnline(): Boolean {
        return _selectedVehicle.value?.isConnected == true
    }
}
