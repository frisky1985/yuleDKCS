// BleManager.kt - BLE管理模块
package com.digitalkey.sdk.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.ParcelUuid
import androidx.core.content.ContextCompat
import com.digitalkey.sdk.error.DkError
import com.digitalkey.sdk.error.DkErrorCode
import com.digitalkey.sdk.logger.DkLogger
import com.digitalkey.sdk.telemetry.DkTelemetry
import kotlinx.coroutines.*
import java.util.*
import java.util.concurrent.ConcurrentHashMap

// BLE服务UUID — 支持多协议 (CCC / ICCE / ICCOA)
object BleUuids {
    // CCC Digital Key Service (SIG Assigned: 0xFFD1)
    val CCC_SERVICE = UUID.fromString("0000FFD1-0000-1000-8000-00805F9B34FB")
    val CCC_CHAR_PAIRING = UUID.fromString("0000FFD2-0000-1000-8000-00805F9B34FB")
    val CCC_CHAR_KEY_DATA = UUID.fromString("0000FFD3-0000-1000-8000-00805F9B34FB")
    val CCC_CHAR_AUTH = UUID.fromString("0000FFD4-0000-1000-8000-00805F9B34FB")
    val CCC_CHAR_STATE = UUID.fromString("0000FFD5-0000-1000-8000-00805F9B34FB")
    val CCC_CHAR_UWB_CONFIG = UUID.fromString("0000FFD6-0000-1000-8000-00805F9B34FB")
    val CCC_CHAR_RSSI = UUID.fromString("0000FFD7-0000-1000-8000-00805F9B34FB")

    // ICCE Digital Key Service (T/CA 110-2020: 0xFEFA)
    val ICCE_SERVICE = UUID.fromString("0000FEFA-0000-1000-8000-00805F9B34FB")
    val ICCE_CHAR_KEY_STATUS = UUID.fromString("0000FEFB-0000-1000-8000-00805F9B34FB")
    val ICCE_CHAR_RANGING_DATA = UUID.fromString("0000FEFC-0000-1000-8000-00805F9B34FB")
    val ICCE_CHAR_AUTH_CHALLENGE = UUID.fromString("0000FEFD-0000-1000-8000-00805F9B34FB")
    val ICCE_CHAR_CONTROL_CMD = UUID.fromString("0000FEFE-0000-1000-8000-00805F9B34FB")
    val ICCE_CHAR_SESSION_KEY = UUID.fromString("0000FEFF-0000-1000-8000-00805F9B34FB")

    // ICCOA Digital Key Service (SIG Assigned: 0xFEF5)
    val ICCOA_SERVICE = UUID.fromString("0000FEF5-0000-1000-8000-00805F9B34FB")

    val CLIENT_CHARACTERISTIC_CONFIG = UUID.fromString("00002902-0000-1000-8000-00805F9B34FB")

    /** 获取协议对应的Service UUID */
    fun serviceForProtocol(protocol: BleProtocolType): UUID = when (protocol) {
        BleProtocolType.CCC -> CCC_SERVICE
        BleProtocolType.ICCE -> ICCE_SERVICE
        BleProtocolType.ICCOA -> ICCOA_SERVICE
    }
}

/** BLE协议类型 */
enum class BleProtocolType {
    CCC,    // 0xFFD1
    ICCE,   // 0xFEFA
    ICCOA   // 0xFEF5
}

// BLE连接状态
enum class BleConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
}

// BLE设备信息
data class BleDevice(
    val name: String?,
    val address: String,
    val rssi: Int,
    val scanRecord: ByteArray?
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is BleDevice) return false
        return address == other.address
    }
    
    override fun hashCode(): Int = address.hashCode()
}

// BLE事件监听器
interface BleEventListener {
    fun onDeviceFound(device: BleDevice)
    fun onConnectionStateChanged(state: BleConnectionState, device: BleDevice?)
    fun onCharacteristicChanged(uuid: UUID, value: ByteArray)
    fun onError(error: DkError)
}

// BLE管理器
@SuppressLint("MissingPermission")
class BleManager(private val context: Context) {
    
    private val logger = DkLogger.getLogger("BleManager")
    private val telemetry = DkTelemetry
    private val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())
    
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var bluetoothLeScanner: BluetoothLeScanner? = null
    private var bluetoothGatt: BluetoothGatt? = null
    
    private val scannedDevices = ConcurrentHashMap<String, BleDevice>()
    private val listeners = mutableListOf<BleEventListener>()
    
    private var connectionState = BleConnectionState.DISCONNECTED
    private var currentDevice: BleDevice? = null
    private var scanJob: Job? = null
    
    private var isScanning = false
    
    /** 当前协议类型，用于选择正确的Service UUID */
    var protocolType: BleProtocolType = BleProtocolType.CCC
    
    init {
        initialize()
    }
    
    private fun initialize() {
        val manager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = manager.adapter
        bluetoothLeScanner = bluetoothAdapter?.bluetoothLeScanner
        logger.info("BLE Manager initialized")
    }
    
    // 添加监听器
    fun addListener(listener: BleEventListener) {
        if (!listeners.contains(listener)) {
            listeners.add(listener)
        }
    }
    
    // 移除监听器
    fun removeListener(listener: BleEventListener) {
        listeners.remove(listener)
    }
    
    // 检查BLE是否可用
    fun isAvailable(): Boolean {
        return bluetoothAdapter != null && bluetoothAdapter?.isEnabled == true
    }
    
    /**
     * 检查BLE扫描权限
     * Android 12+ 需要 BLUETOOTH_SCAN 权限
     * Android 11- 需要 ACCESS_FINE_LOCATION 权限
     * @return true 如果权限已授予
     */
    private fun hasBleScanPermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_SCAN) ==
                PackageManager.PERMISSION_GRANTED
        } else {
            ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION) ==
                PackageManager.PERMISSION_GRANTED
        }
    }

    // 开始扫描
    @SuppressLint("MissingPermission")
    fun startScan(timeoutMs: Long = 10000) {
        if (!isAvailable()) {
            notifyError(DkError(DkErrorCode.bleDisabled, "BLE is not available"))
            return
        }

        // [M-03] BLE扫描前检查权限
        if (!hasBleScanPermission()) {
            notifyError(DkError(DkErrorCode.bleScanFailed, "BLUETOOTH_SCAN or ACCESS_FINE_LOCATION permission not granted"))
            return
        }

        if (isScanning) {
            logger.warn("Already scanning")
            return
        }
        
        scannedDevices.clear()
        isScanning = true
        
        val filters = listOf(
            ScanFilter.Builder()
                .setServiceUuid(ParcelUuid(BleUuids.serviceForProtocol(protocolType)))
                .build()
        )
        
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .setReportDelay(0)
            .build()
        
        try {
            bluetoothLeScanner?.startScan(filters, settings, scanCallback)
            logger.info("BLE scan started")
            
            telemetry.track("ble_scan", mapOf(
                "timeout_ms" to timeoutMs,
                "filter_service" to BleUuids.serviceForProtocol(protocolType).toString()
            ))
            
            scanJob = scope.launch {
                delay(timeoutMs)
                stopScan()
            }
        } catch (e: Exception) {
            isScanning = false
            notifyError(DkError(DkErrorCode.bleScanFailed, cause = e))
        }
    }
    
    // 停止扫描
    @SuppressLint("MissingPermission")
    fun stopScan() {
        scanJob?.cancel()
        scanJob = null
        
        if (isScanning) {
            try {
                bluetoothLeScanner?.stopScan(scanCallback)
                isScanning = false
                logger.info("BLE scan stopped, found ${scannedDevices.size} devices")
            } catch (e: Exception) {
                logger.error("Failed to stop scan", e)
            }
        }
    }
    
    // 连接到设备
    @SuppressLint("MissingPermission")
    fun connect(address: String) {
        if (!isAvailable()) {
            notifyError(DkError(DkErrorCode.bleDisabled, "BLE is not available"))
            return
        }
        
        val device = scannedDevices[address] ?: run {
            notifyError(DkError(DkErrorCode.deviceNotFound, "Device not found in scan results"))
            return
        }
        
        updateConnectionState(BleConnectionState.CONNECTING, device)
        
        val androidDevice = bluetoothAdapter?.getRemoteDevice(address)
        if (androidDevice == null) {
            notifyError(DkError(DkErrorCode.bleConnectFailed, "Cannot get Bluetooth device"))
            return
        }
        
        try {
            bluetoothGatt = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                androidDevice.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
            } else {
                androidDevice.connectGatt(context, false, gattCallback)
            }
            
            telemetry.track("ble_connect_attempt", mapOf(
                "device_address" to address,
                "device_name" to (device.name ?: "unknown")
            ))
        } catch (e: Exception) {
            updateConnectionState(BleConnectionState.DISCONNECTED, null)
            notifyError(DkError(DkErrorCode.bleConnectFailed, cause = e))
        }
    }
    
    // 断开连接
    @SuppressLint("MissingPermission")
    fun disconnect() {
        updateConnectionState(BleConnectionState.DISCONNECTING, currentDevice)
        
        bluetoothGatt?.let { gatt ->
            gatt.disconnect()
            gatt.close()
            bluetoothGatt = null
        }
        
        updateConnectionState(BleConnectionState.DISCONNECTED, null)
    }
    
    // 写入特征值
    @SuppressLint("MissingPermission")
    fun writeCharacteristic(uuid: UUID, data: ByteArray) {
        val gatt = bluetoothGatt
        if (gatt == null || connectionState != BleConnectionState.CONNECTED) {
            notifyError(DkError(DkErrorCode.bleConnectFailed, "Not connected"))
            return
        }
        
        val serviceUuid = BleUuids.serviceForProtocol(protocolType)
        val service = gatt.getService(serviceUuid)
        val characteristic = service?.getCharacteristic(uuid)
        
        if (characteristic == null) {
            notifyError(DkError(DkErrorCode.invalidParameter, "Characteristic not found: $uuid"))
            return
        }
        
        characteristic.value = data
        characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        
        val success = gatt.writeCharacteristic(characteristic)
        if (!success) {
            notifyError(DkError(DkErrorCode.bleConnectFailed, "Write failed"))
        } else {
            telemetry.track("ble_write", mapOf(
                "characteristic_uuid" to uuid.toString(),
                "data_length" to data.size
            ))
        }
    }
    
    // 读取特征值
    @SuppressLint("MissingPermission")
    fun readCharacteristic(uuid: UUID) {
        val gatt = bluetoothGatt
        if (gatt == null || connectionState != BleConnectionState.CONNECTED) {
            notifyError(DkError(DkErrorCode.bleConnectFailed, "Not connected"))
            return
        }
        
        val serviceUuid = BleUuids.serviceForProtocol(protocolType)
        val service = gatt.getService(serviceUuid)
        val characteristic = service?.getCharacteristic(uuid)
        
        if (characteristic == null) {
            notifyError(DkError(DkErrorCode.invalidParameter, "Characteristic not found: $uuid"))
            return
        }
        
        val success = gatt.readCharacteristic(characteristic)
        if (!success) {
            notifyError(DkError(DkErrorCode.bleConnectFailed, "Read failed"))
        }
    }
    
    // 启用通知
    @SuppressLint("MissingPermission")
    fun enableNotification(uuid: UUID, enabled: Boolean) {
        val gatt = bluetoothGatt
        if (gatt == null) return
        
        val serviceUuid = BleUuids.serviceForProtocol(protocolType)
        val service = gatt.getService(serviceUuid)
        val characteristic = service?.getCharacteristic(uuid) ?: return
        
        gatt.setCharacteristicNotification(characteristic, enabled)
        
        val descriptor = characteristic.getDescriptor(BleUuids.CLIENT_CHARACTERISTIC_CONFIG)
        if (descriptor != null) {
            descriptor.value = if (enabled) {
                BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            } else {
                BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE
            }
            gatt.writeDescriptor(descriptor)
        }
    }
    
    // 获取已扫描设备列表
    fun getScannedDevices(): List<BleDevice> {
        return scannedDevices.values.toList()
    }
    
    // 获取当前连接状态
    fun getConnectionState(): BleConnectionState = connectionState
    
    // 获取当前连接设备
    fun getCurrentDevice(): BleDevice? = currentDevice
    
    // 获取MTU
    fun getMtu(): Int = bluetoothGatt?.mtu ?: 23
    
    // 释放资源
    fun release() {
        disconnect()
        stopScan()
        scope.cancel()
        listeners.clear()
    }
    
    // 私有方法
    
    private val scanCallback = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = BleDevice(
                name = result.device.name,
                address = result.device.address,
                rssi = result.rssi,
                scanRecord = result.scanRecord?.bytes
            )
            
            scannedDevices[device.address] = device
            
            telemetry.track("ble_device_found", mapOf(
                "device_address" to device.address,
                "device_name" to (device.name ?: "unknown"),
                "rssi" to device.rssi
            ))
            
            listeners.forEach { it.onDeviceFound(device) }
        }
        
        override fun onScanFailed(errorCode: Int) {
            logger.error("BLE scan failed: $errorCode")
            isScanning = false
            
            telemetry.trackError(
                DkErrorCode.bleScanFailed.toInt(),
                "BLE scan failed: $errorCode"
            )
            
            notifyError(DkError(DkErrorCode.bleScanFailed, "Scan failed with error code: $errorCode"))
        }
    }
    
    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    updateConnectionState(BleConnectionState.CONNECTED, currentDevice)
                    gatt.discoverServices()
                    
                    telemetry.track("ble_connect", mapOf(
                        "status" to status,
                        "mtu" to gatt.mtu
                    ))
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    updateConnectionState(BleConnectionState.DISCONNECTED, null)
                    
                    telemetry.track("ble_disconnect", mapOf(
                        "status" to status
                    ))
                }
            }
        }
        
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                logger.info("Services discovered")
                
                // Enable notifications for characteristics
                /* 根据协议类型启用不同特征的通知 */
                when (protocolType) {
                    BleProtocolType.CCC -> {
                        enableNotification(BleUuids.CCC_CHAR_STATE, true)
                        enableNotification(BleUuids.CCC_CHAR_AUTH, true)
                    }
                    BleProtocolType.ICCE -> {
                        enableNotification(BleUuids.ICCE_CHAR_KEY_STATUS, true)
                        enableNotification(BleUuids.ICCE_CHAR_AUTH_CHALLENGE, true)
                    }
                    BleProtocolType.ICCOA -> {
                        // ICCOA notifications handled separately
                    }
                }
            }
        }
        
        override fun onCharacteristicRead(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val data = characteristic.value
                logger.debug("Characteristic read: ${characteristic.uuid}, length: ${data.size}")
                
                telemetry.track("ble_read", mapOf(
                    "characteristic_uuid" to characteristic.uuid.toString(),
                    "data_length" to data.size
                ))
                
                listeners.forEach { it.onCharacteristicChanged(characteristic.uuid, data) }
            }
        }
        
        override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                logger.debug("Characteristic written: ${characteristic.uuid}")
            } else {
                logger.error("Characteristic write failed: ${characteristic.uuid}, status: $status")
                notifyError(DkError(DkErrorCode.bleConnectFailed, "Write failed with status: $status"))
            }
        }
        
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            val data = characteristic.value
            logger.debug("Characteristic changed: ${characteristic.uuid}, length: ${data.size}")
            
            telemetry.track("ble_notification", mapOf(
                "characteristic_uuid" to characteristic.uuid.toString(),
                "data_length" to data.size
            ))
            
            listeners.forEach { it.onCharacteristicChanged(characteristic.uuid, data) }
        }
        
        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
            logger.info("MTU changed to $mtu")
            
            telemetry.track("ble_mtu_changed", mapOf(
                "old_mtu" to (bluetoothGatt?.mtu ?: 23),
                "new_mtu" to mtu,
                "status" to status
            ))
        }
    }
    
    private fun updateConnectionState(state: BleConnectionState, device: BleDevice?) {
        connectionState = state
        currentDevice = if (state == BleConnectionState.CONNECTED) device else null
        listeners.forEach { it.onConnectionStateChanged(state, device) }
    }
    
    private fun notifyError(error: DkError) {
        logger.error(error.message, error)
        telemetry.trackError(error.code.toInt(), error.message)
        listeners.forEach { it.onError(error) }
    }
    
    private fun Int.toInt(): Int = this
}
