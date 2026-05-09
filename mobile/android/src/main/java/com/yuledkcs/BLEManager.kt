package com.yuledkcs

import android.bluetooth.BluetoothDevice
import android.content.Context
import android.util.Log
import com.yuledkcs.model.Command
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import no.nordicsemi.android.ble.BleManager
import no.nordicsemi.android.ble.callback.DataReceivedCallback
import no.nordicsemi.android.ble.data.Data
import java.util.UUID

/**
 * BLE Manager for vehicle communication
 * 
 * Handles Bluetooth Low Energy connections and command sending to vehicles.
 */
class BLEManager private constructor() : BleManager(YuleDKCS.context) {
    
    companion object {
        private const val TAG = "BLEManager"
        
        // BLE Service and Characteristic UUIDs
        private val SERVICE_UUID = UUID.fromString("0000yule-0000-1000-8000-00805f9b34fb")
        private val COMMAND_CHAR_UUID = UUID.fromString("0000cmd1-0000-1000-8000-00805f9b34fb")
        private val RESPONSE_CHAR_UUID = UUID.fromString("0000resp-0000-1000-8000-00805f9b34fb")
        
        @Volatile
        private var instance: BLEManager? = null
        
        fun getInstance(): BLEManager {
            return instance ?: synchronized(this) {
                instance ?: BLEManager().also { instance = it }
            }
        }
    }
    
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var commandCharacteristic: BluetoothGattCharacteristic? = null
    private var responseCharacteristic: BluetoothGattCharacteristic? = null
    
    private val pendingCommands = mutableMapOf<String, (Result<ByteArray>) -> Unit>()
    
    private var connectionCallback: ((Boolean) -> Unit)? = null
    
    /**
     * Connect to a vehicle via BLE
     * 
     * @param address MAC address of the BLE device
     * @param callback Optional callback for connection status
     */
    fun connect(address: String, callback: ((Boolean) -> Unit)? = null) {
        YuleDKCS.checkInitialized()
        
        val bluetoothAdapter = android.bluetooth.BluetoothAdapter.getDefaultAdapter()
        val device = bluetoothAdapter?.getRemoteDevice(address)
        
        if (device == null) {
            callback?.invoke(false)
            return
        }
        
        connectionCallback = callback
        
        scope.launch {
            try {
                connect(device)
                    .useAutoConnect(true)
                    .retry(3, 100)
                    .done {
                        Log.i(TAG, "Connected to $address")
                        connectionCallback?.invoke(true)
                    }
                    .fail { _, status ->
                        Log.e(TAG, "Connection failed with status: $status")
                        connectionCallback?.invoke(false)
                    }
                    .enqueue()
            } catch (e: Exception) {
                Log.e(TAG, "Error connecting", e)
                callback?.invoke(false)
            }
        }
    }
    
    /**
     * Disconnect from the current device
     */
    fun disconnect() {
        disconnect().enqueue()
    }
    
    /**
     * Send a command to the connected vehicle
     * 
     * @param command The command to send
     * @param callback Callback with result
     */
    fun sendCommand(
        command: Command,
        callback: (Result<Unit>) -> Unit
    ) {
        YuleDKCS.checkInitialized()
        
        if (!isConnected) {
            callback(Result.failure(IllegalStateException("Not connected to device")))
            return
        }
        
        scope.launch {
            try {
                // Build command packet with encryption
                val commandData = buildCommandPacket(command)
                
                // Send via BLE
                val data = Data(commandData)
                
                writeCharacteristic(
                    commandCharacteristic,
                    data,
                    BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                )
                    .await()
                
                withContext(Dispatchers.Main) {
                    callback(Result.success(Unit))
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error sending command", e)
                withContext(Dispatchers.Main) {
                    callback(Result.failure(e))
                }
            }
        }
    }
    
    /**
     * Check if currently connected to a device
     */
    fun isConnected(): Boolean = isConnected
    
    override fun initialize() {
        // Set up notification for responses
        setNotificationCallback(responseCharacteristic)
            .with { _, data ->
                handleResponse(data.value)
            }
        
        enableNotifications(responseCharacteristic).enqueue()
    }
    
    override fun isRequiredServiceSupported(gatt: BluetoothGatt): Boolean {
        val service = gatt.getService(SERVICE_UUID)
        if (service != null) {
            commandCharacteristic = service.getCharacteristic(COMMAND_CHAR_UUID)
            responseCharacteristic = service.getCharacteristic(RESPONSE_CHAR_UUID)
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
    }
    
    private fun buildCommandPacket(command: Command): ByteArray {
        // Use native crypto for command signing
        val keyId = command.keyId
        val key = YuleDKCS.getKeyManager().getKey(keyId)
            ?: throw IllegalStateException("Key not found: $keyId")
        
        val timestamp = System.currentTimeMillis() / 1000
        val commandBytes = when (command) {
            is Command.Lock -> byteArrayOf(0x01)
            is Command.Unlock -> byteArrayOf(0x02)
            is Command.StartEngine -> byteArrayOf(0x03)
            is Command.StopEngine -> byteArrayOf(0x04)
            is Command.TrunkOpen -> byteArrayOf(0x05)
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
        packet[offset++] = (timestamp shr 56).toByte()
        packet[offset++] = (timestamp shr 48).toByte()
        packet[offset++] = (timestamp shr 40).toByte()
        packet[offset++] = (timestamp shr 32).toByte()
        packet[offset++] = (timestamp shr 24).toByte()
        packet[offset++] = (timestamp shr 16).toByte()
        packet[offset++] = (timestamp shr 8).toByte()
        packet[offset++] = timestamp.toByte()
        
        // Command data
        System.arraycopy(commandBytes, 0, packet, offset, commandBytes.size)
        offset += commandBytes.size
        
        // Sign packet using native crypto
        val dataToSign = packet.copyOfRange(0, offset)
        val signature = NativeLib.signCommand(dataToSign, key.encryptedData)
        
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
                    callback?.invoke(Result.failure(Exception("Command failed with status: $status")))
                }
            }
        }
    }
    
    // Import needed
    private typealias BluetoothGattCharacteristic = android.bluetooth.BluetoothGattCharacteristic
    private typealias BluetoothGatt = android.bluetooth.BluetoothGatt
}
