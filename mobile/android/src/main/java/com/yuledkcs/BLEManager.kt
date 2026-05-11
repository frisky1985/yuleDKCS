package com.yuledkcs

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.ParcelUuid
import android.util.Log
import com.yuledkcs.model.Command
import com.yuledkcs.model.VehicleStatus
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import no.nordicsemi.android.ble.BleManager
import no.nordicsemi.android.ble.data.Data
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap

/**
 * BLE Manager for vehicle communication
 * 
 * Handles Bluetooth Low Energy connections, command sending, scanning,
 * and real-time status monitoring for vehicles.
 */
class BLEManager private constructor() : BleManager(YuleDKCS.context) {
    
    companion object {
        private const val TAG = "BLEManager"
        
        // BLE Service and Characteristic UUIDs
        val SERVICE_UUID = UUID.fromString("0000yule-0000-1000-8000-00805f9b34fb")
        val COMMAND_CHAR_UUID = UUID.fromString("0000cmd1-0000-1000-8000-00805f9b34fb")
        val RESPONSE_CHAR_UUID = UUID.fromString("0000resp-0000-1000-8000-00805f9b34fb")
        val STATUS_CHAR_UUID = UUID.fromString("0000stat-0000-1000-8000-00805f9b34fb")
        
        @Volatile
        private var instance: BLEManager? = null
        
        fun getInstance(): BLEManager {
            return instance ?: synchronized(this) {
                instance ?: BLEManager().also { instance = it }
            }
        }
        
        // Command codes
        const val CMD_LOCK = 0x01
        const val CMD_UNLOCK = 0x02
        const val CMD_START_ENGINE = 0x03
        const val CMD_STOP_ENGINE = 0x04
        const val CMD_OPEN_TRUNK = 0x05
        const val CMD_OPEN_WINDOWS = 0x06
        const val CMD_CLOSE_WINDOWS = 0x07
        const val CMD_FIND_VEHICLE = 0x08
        const val CMD_GET_STATUS = 0x09
    }
    
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var commandCharacteristic: BluetoothGattCharacteristic? = null
    private var responseCharacteristic: BluetoothGattCharacteristic? = null
    private var statusCharacteristic: BluetoothGattCharacteristic? = null
    
    private val pendingCommands = ConcurrentHashMap<String, (Result<ByteArray>) -> Unit>()
    private var connectionCallback: ((Boolean) -> Unit)? = null
    private var currentDeviceAddress: String? = null
    
    // Scanning state
    private val _scanResults = MutableStateFlow<List<ScanResult>>(emptyList())
    val scanResults: StateFlow<List<ScanResult>> = _scanResults
    
    private val _isScanning = MutableStateFlow(false)
    val isScanning: StateFlow<Boolean> = _isScanning
    
    // Connection state
    private val _connectionState = MutableStateFlow<ConnectionState>(ConnectionState.Disconnected)
    val connectionState: StateFlow<ConnectionState> = _connectionState
    
    // Vehicle status
    private val _vehicleStatus = MutableStateFlow<VehicleStatus?>(null)
    val vehicleStatus: StateFlow<VehicleStatus?> = _vehicleStatus
    
    private var scanCallback: ScanCallback? = null
    
    sealed class ConnectionState {
        object Disconnected : ConnectionState()
        object Scanning : ConnectionState()
        object Connecting : ConnectionState()
        data class Connected(val address: String) : ConnectionState()
        data class Error(val message: String) : ConnectionState()
    }
    
    /**
     * Start scanning for nearby vehicles
     * 
     * @param timeoutMs Scan timeout in milliseconds (default 10000)
     * @param serviceUuid Optional service UUID filter
     */
    fun startScan(
        timeoutMs: Long = 10000,
        serviceUuid: UUID? = SERVICE_UUID
    ) {
        YuleDKCS.checkInitialized()
        
        val bluetoothAdapter = android.bluetooth.BluetoothAdapter.getDefaultAdapter()
        val scanner = bluetoothAdapter?.bluetoothLeScanner
        
        if (scanner == null) {
            Log.e(TAG, "Bluetooth LE scanner not available")
            return
        }
        
        _isScanning.value = true
        _connectionState.value = ConnectionState.Scanning
        
        // Build scan filter
        val filters = serviceUuid?.let {
            listOf(
                ScanFilter.Builder()
                    .setServiceUuid(ParcelUuid(it))
                    .build()
            )
        } ?: emptyList()
        
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        
        scanCallback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                val current = _scanResults.value.toMutableList()
                // Update or add result
                val index = current.indexOfFirst { it.device.address == result.device.address }
                if (index >= 0) {
                    current[index] = result
                } else {
                    current.add(result)
                }
                // Sort by RSSI (signal strength)
                current.sortByDescending { it.rssi }
                _scanResults.value = current
            }
            
            override fun onBatchScanResults(results: List<ScanResult>) {
                _scanResults.value = results.sortedByDescending { it.rssi }
            }
            
            override fun onScanFailed(errorCode: Int) {
                Log.e(TAG, "Scan failed with error: $errorCode")
                _isScanning.value = false
                _connectionState.value = ConnectionState.Error("扫描失败: $errorCode")
            }
        }
        
        scanner.startScan(filters, settings, scanCallback)
        
        // Auto stop after timeout
        scope.launch {
            delay(timeoutMs)
            stopScan()
        }
    }
    
    /**
     * Stop scanning for devices
     */
    fun stopScan() {
        val bluetoothAdapter = android.bluetooth.BluetoothAdapter.getDefaultAdapter()
        val scanner = bluetoothAdapter?.bluetoothLeScanner
        
        scanCallback?.let { callback ->
            scanner?.stopScan(callback)
        }
        
        _isScanning.value = false
        if (_connectionState.value is ConnectionState.Scanning) {
            _connectionState.value = ConnectionState.Disconnected
        }
    }
    
    /**
     * Connect to a vehicle via BLE
     * 
     * @param address MAC address of the BLE device
     * @param autoReconnect Enable auto-reconnect on disconnect
     * @param callback Optional callback for connection status
     */
    fun connect(
        address: String,
        autoReconnect: Boolean = true,
        callback: ((Boolean) -> Unit)? = null
    ) {
        YuleDKCS.checkInitialized()
        
        // Stop scanning first
        stopScan()
        
        val bluetoothAdapter = android.bluetooth.BluetoothAdapter.getDefaultAdapter()
        val device = bluetoothAdapter?.getRemoteDevice(address)
        
        if (device == null) {
            callback?.invoke(false)
            return
        }
        
        connectionCallback = callback
        currentDeviceAddress = address
        _connectionState.value = ConnectionState.Connecting
        
        scope.launch {
            try {
                connect(device)
                    .useAutoConnect(autoReconnect)
                    .retry(3, 100)
                    .done {
                        Log.i(TAG, "Connected to $address")
                        _connectionState.value = ConnectionState.Connected(address)
                        connectionCallback?.invoke(true)
                        
                        // Request initial status
                        requestVehicleStatus()
                    }
                    .fail { _, status ->
                        Log.e(TAG, "Connection failed with status: $status")
                        _connectionState.value = ConnectionState.Error("连接失败: $status")
                        connectionCallback?.invoke(false)
                    }
                    .enqueue()
            } catch (e: Exception) {
                Log.e(TAG, "Error connecting", e)
                _connectionState.value = ConnectionState.Error(e.message ?: "Unknown error")
                callback?.invoke(false)
            }
        }
    }
    
    /**
     * Connect to nearest vehicle (highest RSSI)
     * 
     * @param callback Callback with Result containing device address
     */
    fun connectToNearest(callback: (Result<String>) -> Unit) {
        val nearest = _scanResults.value.firstOrNull()
        if (nearest == null) {
            callback(Result.failure(Exception("没有发现附近的车辆")))
            return
        }
        
        connect(nearest.device.address) { success ->
            if (success) {
                callback(Result.success(nearest.device.address))
            } else {
                callback(Result.failure(Exception("连接失败")))
            }
        }
    }
    
    /**
     * Disconnect from the current device
     */
    fun disconnect() {
        disconnect().enqueue()
        currentDeviceAddress = null
        _connectionState.value = ConnectionState.Disconnected
        _vehicleStatus.value = null
    }
    
    /**
     * Send a command to the connected vehicle
     * 
     * @param keyId The key identifier for authentication
     * @param command The command to send
     * @param callback Callback with result
     */
    fun sendCommand(
        keyId: String,
        command: Command,
        callback: (Result<CommandResponse>) -> Unit
    ) {
        YuleDKCS.checkInitialized()
        
        if (!isConnected) {
            callback(Result.failure(IllegalStateException("未连接到设备")))
            return
        }
        
        // Check permission
        if (!YuleDKCS.keyManager.hasPermission(keyId, command.toPermission())) {
            callback(Result.failure(SecurityException("钥匙没有此权限")))
            return
        }
        
        scope.launch {
            try {
                // Build command packet with encryption
                val commandData = buildCommandPacket(keyId, command)
                val commandId = System.currentTimeMillis().toString()
                
                // Store callback for response handling
                val responseDeferred = kotlinx.coroutines.CompletableDeferred<Result<ByteArray>>()
                pendingCommands[commandId] = { result ->
                    responseDeferred.complete(result)
                }
                
                // Send via BLE
                val data = Data(commandData)
                
                writeCharacteristic(
                    commandCharacteristic,
                    data,
                    BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                ).await()
                
                // Wait for response with timeout
                val responseResult = withTimeoutOrNull(5000) {
                    responseDeferred.await()
                } ?: Result.failure(Exception("响应超时"))
                
                withContext(Dispatchers.Main) {
                    responseResult.fold(
                        onSuccess = { responseData ->
                            val cmdResponse = parseResponse(responseData)
                            
                            // Record usage log
                            YuleDKCS.keyManager.recordUsage(
                                keyId = keyId,
                                operation = command.toString(),
                                status = if (cmdResponse.success) 
                                    com.yuledkcs.model.KeyUsageLog.Status.SUCCESS 
                                else 
                                    com.yuledkcs.model.KeyUsageLog.Status.FAILURE,
                                failureReason = if (!cmdResponse.success) cmdResponse.message else null
                            )
                            
                            callback(Result.success(cmdResponse))
                        },
                        onFailure = { error ->
                            // Record failure
                            YuleDKCS.keyManager.recordUsage(
                                keyId = keyId,
                                operation = command.toString(),
                                status = com.yuledkcs.model.KeyUsageLog.Status.FAILURE,
                                failureReason = error.message
                            )
                            callback(Result.failure(error))
                        }
                    )
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error sending command", e)
                
                // Record failure
                YuleDKCS.keyManager.recordUsage(
                    keyId = keyId,
                    operation = command.toString(),
                    status = com.yuledkcs.model.KeyUsageLog.Status.FAILURE,
                    failureReason = e.message
                )
                
                withContext(Dispatchers.Main) {
                    callback(Result.failure(e))
                }
            }
        }
    }
    
    /**
     * Convenience method: Lock vehicle
     */
    fun lock(keyId: String, callback: (Result<CommandResponse>) -> Unit) {
        sendCommand(keyId, Command.Lock, callback)
    }
    
    /**
     * Convenience method: Unlock vehicle
     */
    fun unlock(keyId: String, callback: (Result<CommandResponse>) -> Unit) {
        sendCommand(keyId, Command.Unlock, callback)
    }
    
    /**
     * Convenience method: Start engine
     */
    fun startEngine(keyId: String, callback: (Result<CommandResponse>) -> Unit) {
        sendCommand(keyId, Command.StartEngine, callback)
    }
    
    /**
     * Convenience method: Stop engine
     */
    fun stopEngine(keyId: String, callback: (Result<CommandResponse>) -> Unit) {
        sendCommand(keyId, Command.StopEngine, callback)
    }
    
    /**
     * Convenience method: Open trunk
     */
    fun openTrunk(keyId: String, callback: (Result<CommandResponse>) -> Unit) {
        sendCommand(keyId, Command.TrunkOpen, callback)
    }
    
    /**
     * Convenience method: Open windows
     */
    fun openWindows(keyId: String, callback: (Result<CommandResponse>) -> Unit) {
        sendCommand(keyId, Command.OpenWindows, callback)
    }
    
    /**
     * Convenience method: Close windows
     */
    fun closeWindows(keyId: String, callback: (Result<CommandResponse>) -> Unit) {
        sendCommand(keyId, Command.CloseWindows, callback)
    }
    
    /**
     * Convenience method: Find vehicle (honk/flash)
     */
    fun findVehicle(keyId: String, callback: (Result<CommandResponse>) -> Unit) {
        sendCommand(keyId, Command.FindVehicle, callback)
    }
    
    /**
     * Request current vehicle status
     */
    fun requestVehicleStatus() {
        if (!isConnected) return
        
        scope.launch {
            try {
                val data = Data(byteArrayOf(CMD_GET_STATUS.toByte()))
                writeCharacteristic(commandCharacteristic, data, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT).await()
            } catch (e: Exception) {
                Log.e(TAG, "Error requesting status", e)
            }
        }
    }
    
    /**
     * Check if currently connected to a device
     */
    fun isConnected(): Boolean = isConnected
    
    /**
     * Get current connected device address
     */
    fun getConnectedDevice(): String? = currentDeviceAddress
    
    override fun initialize() {
        // Set up notification for responses
        setNotificationCallback(responseCharacteristic)
            .with { _, data ->
                handleResponse(data.value)
            }
        
        enableNotifications(responseCharacteristic).enqueue()
        
        // Set up notification for status updates
        setNotificationCallback(statusCharacteristic)
            .with { _, data ->
                handleStatusUpdate(data.value)
            }
        
        enableNotifications(statusCharacteristic).enqueue()
    }
    
    override fun isRequiredServiceSupported(gatt: BluetoothGatt): Boolean {
        val service = gatt.getService(SERVICE_UUID)
        if (service != null) {
            commandCharacteristic = service.getCharacteristic(COMMAND_CHAR_UUID)
            responseCharacteristic = service.getCharacteristic(RESPONSE_CHAR_UUID)
            statusCharacteristic = service.getCharacteristic(STATUS_CHAR_UUID)
        }
        
        var writeRequest = false
        var notifyRequest = false
        
        commandCharacteristic?.let {
            val properties = it.properties
            writeRequest = properties and BluetoothGattCharacteristic.PROPERTY_WRITE > 0
        }
        
        responseCharacteristic?.let {
            val properties = it.properties
            notifyRequest = properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY > 0
        }
        
        return commandCharacteristic != null && 
               responseCharacteristic != null &&
               writeRequest && notifyRequest
    }
    
    override fun onServicesInvalidated() {
        commandCharacteristic = null
        responseCharacteristic = null
        statusCharacteristic = null
    }
    
    private fun buildCommandPacket(keyId: String, command: Command): ByteArray {
        val key = YuleDKCS.keyManager.getKey(keyId)
            ?: throw IllegalStateException("钥匙未找到: $keyId")
        
        val timestamp = System.currentTimeMillis() / 1000
        val commandBytes = when (command) {
            is Command.Lock -> byteArrayOf(CMD_LOCK.toByte())
            is Command.Unlock -> byteArrayOf(CMD_UNLOCK.toByte())
            is Command.StartEngine -> byteArrayOf(CMD_START_ENGINE.toByte())
            is Command.StopEngine -> byteArrayOf(CMD_STOP_ENGINE.toByte())
            is Command.TrunkOpen -> byteArrayOf(CMD_OPEN_TRUNK.toByte())
            is Command.OpenWindows -> byteArrayOf(CMD_OPEN_WINDOWS.toByte())
            is Command.CloseWindows -> byteArrayOf(CMD_CLOSE_WINDOWS.toByte())
            is Command.FindVehicle -> byteArrayOf(CMD_FIND_VEHICLE.toByte())
            is Command.Custom -> command.data
        }
        
        // Build packet: [VERSION][KEY_ID][TIMESTAMP][COMMAND][SIGNATURE]
        val packet = ByteArray(1 + 16 + 8 + commandBytes.size + 64)
        var offset = 0
        
        // Version
        packet[offset++] = 0x01
        
        // Key ID (16 bytes)
        val keyIdBytes = keyId.toByteArray(Charsets.UTF_8)
        System.arraycopy(keyIdBytes, 0, packet, offset, minOf(keyIdBytes.size, 16))
        offset += 16
        
        // Timestamp (8 bytes, big-endian)
        for (i in 7 downTo 0) {
            packet[offset++] = (timestamp shr (i * 8)).toByte()
        }
        
        // Command data
        System.arraycopy(commandBytes, 0, packet, offset, commandBytes.size)
        offset += commandBytes.size
        
        // Sign packet using native crypto
        val dataToSign = packet.copyOfRange(0, offset)
        val signature = NativeLib.signCommand(dataToSign, key.keyData)
        
        System.arraycopy(signature, 0, packet, offset, signature.size)
        
        return packet
    }
    
    private fun handleResponse(data: ByteArray?) {
        data ?: return
        
        scope.launch(Dispatchers.Main) {
            if (data.size >= 2) {
                val status = data[0].toInt() and 0xFF
                val commandId = data[1].toInt() and 0xFF
                
                val callback = pendingCommands.remove(commandId.toString())
                
                if (status == 0x00) {
                    callback?.invoke(Result.success(data))
                } else {
                    callback?.invoke(Result.failure(Exception("命令执行失败: $status")))
                }
            }
        }
    }
    
    private fun handleStatusUpdate(data: ByteArray?) {
        data ?: return
        
        if (data.size >= 5) {
            val status = VehicleStatus(
                doorLocked = (data[0].toInt() and 0x01) != 0,
                engineRunning = (data[0].toInt() and 0x02) != 0,
                trunkOpen = (data[0].toInt() and 0x04) != 0,
                windowsOpen = (data[0].toInt() and 0x08) != 0,
                batteryLevel = data[1].toInt() and 0xFF,
                fuelLevel = data[2].toInt() and 0xFF,
                alarmActive = (data[0].toInt() and 0x10) != 0,
                timestamp = System.currentTimeMillis()
            )
            _vehicleStatus.value = status
        }
    }
    
    private fun parseResponse(data: ByteArray): CommandResponse {
        return if (data.isNotEmpty() && data[0].toInt() == 0x00) {
            CommandResponse(
                success = true,
                message = "成功"
            )
        } else {
            CommandResponse(
                success = false,
                message = "失败: ${data.getOrNull(0)?.toInt()}"
            )
        }
    }
    
    private fun Command.toPermission(): com.yuledkcs.model.Permission {
        return when (this) {
            is Command.Lock -> com.yuledkcs.model.Permission.LOCK
            is Command.Unlock -> com.yuledkcs.model.Permission.UNLOCK
            is Command.StartEngine -> com.yuledkcs.model.Permission.START_ENGINE
            is Command.StopEngine -> com.yuledkcs.model.Permission.STOP_ENGINE
            is Command.TrunkOpen -> com.yuledkcs.model.Permission.OPEN_TRUNK
            is Command.OpenWindows -> com.yuledkcs.model.Permission.OPEN_WINDOWS
            is Command.CloseWindows -> com.yuledkcs.model.Permission.CLOSE_WINDOWS
            is Command.FindVehicle -> com.yuledkcs.model.Permission.FIND_VEHICLE
            is Command.Custom -> com.yuledkcs.model.Permission.UNLOCK
        }
    }
    
    data class CommandResponse(
        val success: Boolean,
        val message: String
    )
}

// Extension for timeout
private suspend inline fun <T> withTimeoutOrNull(timeMillis: Long, block: () -> T): T? {
    return try {
        kotlinx.coroutines.withTimeout(timeMillis) {
            block()
        }
    } catch (e: kotlinx.coroutines.TimeoutCancellationException) {
        null
    }
}
