package com.digitalkey.wallet.ble

import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.os.ParcelUuid
import android.util.Log
import com.digitalkey.wallet.Models
import java.util.UUID

/**
 * BLE通信管理器
 * 负责与车端BLE模块建立GATT连接，发送/接收数字钥匙协议帧
 * 支持CCC/ICCOA/ICCE协议自动识别
 */
class BleManager(private val context: Context) {

    companion object {
        private const val TAG = "BleManager"

        // Service UUIDs
        val CCC_SERVICE_UUID: UUID   = UUID.fromString("0000FDE2-0000-1000-8000-00805F9B34FB")
        val ICCOA_SERVICE_UUID: UUID = UUID.fromString("0000FEF5-0000-1000-8000-00805F9B34FB")
        val ICCE_SERVICE_UUID: UUID  = UUID.fromString("0000FEFA-0000-1000-8000-00805F9B34FB")

        // CCC Characteristics
        val CCC_WRITE_UUID: UUID     = UUID.fromString("0000FDE3-0000-1000-8000-00805F9B34FB")
        val CCC_NOTIFY_UUID: UUID    = UUID.fromString("0000FDE4-0000-1000-8000-00805F9B34FB")

        // ICCOA Characteristics
        val ICCOA_WRITE_UUID: UUID   = UUID.fromString("0000FEF6-0000-1000-8000-00805F9B34FB")
        val ICCOA_NOTIFY_UUID: UUID  = UUID.fromString("0000FEF7-0000-1000-8000-00805F9B34FB")

        // ICCE Characteristics
        val ICCE_WRITE_UUID: UUID    = UUID.fromString("0000FEFB-0000-1000-8000-00805F9B34FB")
        val ICCE_NOTIFY_UUID: UUID   = UUID.fromString("0000FEFC-0000-1000-8000-00805F9B34FB")

        // MTU
        private const val REQUESTED_MTU = 247
    }

    private var bluetoothAdapter: BluetoothAdapter? = null
    private var gattConnection: BluetoothGatt? = null
    private var writeCharacteristic: BluetoothGattCharacteristic? = null
    private var notifyCharacteristic: BluetoothGattCharacteristic? = null
    private var currentProtocol: Models.Protocol? = null
    private var isConnected = false

    // Callbacks
    var onConnected: (() -> Unit)? = null
    var onDisconnected: (() -> Unit)? = null
    var onDataReceived: ((ByteArray) -> Unit)? = null
    var onError: ((String) -> Unit)? = null

    private val gattCallback = object : BluetoothGattCallback() {

        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    Log.i(TAG, "BLE connected, discovering services")
                    gatt.requestMtu(REQUESTED_MTU)
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    Log.i(TAG, "BLE disconnected")
                    isConnected = false
                    onDisconnected?.invoke()
                }
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.i(TAG, "MTU negotiated: $mtu")
            }
            gatt.discoverServices()
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                onError?.invoke("Service discovery failed: $status")
                return
            }

            // 自动识别协议
            val services = gatt.services
            val protocol = detectProtocol(services)
            if (protocol == null) {
                onError?.invoke("Unknown vehicle BLE service")
                return
            }

            currentProtocol = protocol
            Log.i(TAG, "Detected protocol: $protocol")

            // 查找读写特征
            val serviceUuid = when (protocol) {
                Models.Protocol.CCC_DK3 -> CCC_SERVICE_UUID
                Models.Protocol.ICCOA_DK30, Models.Protocol.ICCOA_DK40 -> ICCOA_SERVICE_UUID
                Models.Protocol.ICCE -> ICCE_SERVICE_UUID
            }

            val service = gatt.getService(serviceUuid) ?: run {
                onError?.invoke("Service not found: $serviceUuid")
                return
            }

            val writeUuid = when (protocol) {
                Models.Protocol.CCC_DK3 -> CCC_WRITE_UUID
                Models.Protocol.ICCOA_DK30, Models.Protocol.ICCOA_DK40 -> ICCOA_WRITE_UUID
                Models.Protocol.ICCE -> ICCE_WRITE_UUID
            }
            val notifyUuid = when (protocol) {
                Models.Protocol.CCC_DK3 -> CCC_NOTIFY_UUID
                Models.Protocol.ICCOA_DK30, Models.Protocol.ICCOA_DK40 -> ICCOA_NOTIFY_UUID
                Models.Protocol.ICCE -> ICCE_NOTIFY_UUID
            }

            writeCharacteristic = service.getCharacteristic(writeUuid)
            notifyCharacteristic = service.getCharacteristic(notifyUuid)

            // 订阅通知
            notifyCharacteristic?.let { char ->
                gatt.setCharacteristicNotification(char, true)
                val descriptor = char.getDescriptor(
                    UUID.fromString("00002902-0000-1000-8000-00805F9B34FB")
                )
                descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                gatt.writeDescriptor(descriptor)
            }

            isConnected = true
            onConnected?.invoke()
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            Log.d(TAG, "Received ${value.size} bytes")
            onDataReceived?.invoke(value)
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e(TAG, "Write failed: $status")
            }
        }
    }

    /**
     * 自动识别车端协议
     */
    private fun detectProtocol(services: List<BluetoothGattService>): Models.Protocol? {
        for (service in services) {
            when (service.uuid) {
                CCC_SERVICE_UUID -> return Models.Protocol.CCC_DK3
                ICCOA_SERVICE_UUID -> return Models.Protocol.ICCOA_DK40
                ICCE_SERVICE_UUID -> return Models.Protocol.ICCE
            }
        }
        return null
    }

    /**
     * 扫描并连接车辆BLE
     */
    fun scanAndConnect(vehicleBleMac: String? = null) {
        val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (vehicleBleMac != null) {
            // 直接连接已知MAC
            val device = bluetoothAdapter!!.getRemoteDevice(vehicleBleMac)
            gattConnection = device.connectGatt(context, false, gattCallback)
        } else {
            // 扫描车辆BLE广播
            val scanner = bluetoothAdapter!!.bluetoothLeScanner
            val scanFilter = ScanFilter.Builder()
                .setServiceUuid(ParcelUuid(CCC_SERVICE_UUID))
                .build()
            val scanFilter2 = ScanFilter.Builder()
                .setServiceUuid(ParcelUuid(ICCOA_SERVICE_UUID))
                .build()
            val scanFilter3 = ScanFilter.Builder()
                .setServiceUuid(ParcelUuid(ICCE_SERVICE_UUID))
                .build()

            val settings = ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .build()

            scanner.startScan(
                listOf(scanFilter, scanFilter2, scanFilter3),
                settings,
                scanCallback
            )
        }
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            Log.i(TAG, "Found vehicle: ${result.device.address} RSSI=${result.rssi}")
            // 自动连接信号最强的车辆
            if (result.rssi > -60) {
                bluetoothAdapter?.bluetoothLeScanner?.stopScan(this)
                gattConnection = result.device.connectGatt(context, false, gattCallback)
            }
        }
    }

    /**
     * 发送协议帧
     */
    fun sendData(data: ByteArray): Boolean {
        val char = writeCharacteristic ?: return false
        val gatt = gattConnection ?: return false

        char.value = data
        char.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        return gatt.writeCharacteristic(char)
    }

    /**
     * 断开连接
     */
    fun disconnect() {
        gattConnection?.disconnect()
        gattConnection?.close()
        gattConnection = null
        isConnected = false
    }

    fun getCurrentProtocol(): Models.Protocol? = currentProtocol
    fun isConnected(): Boolean = isConnected
}
