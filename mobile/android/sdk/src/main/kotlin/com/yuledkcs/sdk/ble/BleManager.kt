package com.yuledkcs.sdk.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.os.Build
import com.yuledkcs.sdk.hub.YDKError
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicReference

/**
 * Android BLE 管理器
 *
 * 负责扫描/连接/收发 GATT 数据。
 * 多协议支持: 通过 BleProtocolAdapter 工厂创建适配器。
 */
@SuppressLint("MissingPermission")
class BleManager(
    private val context: Context,
    /**
     * 扫描引擎 — 默认使用 BluetoothLeScanner 实现;
     * 测试可注入 fake 引擎驱动扫描回调。
     */
    private val scanEngine: BleScanEngine? = null
) {

    private val bluetoothManager: BluetoothManager =
        context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter? = bluetoothManager.adapter

    private val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())

    private var gatt: BluetoothGatt? = null
    private var protocolType: BleProtocolType = BleProtocolType.CCC
    private val adapter: BleProtocolAdapter
        get() = BleProtocolAdapterFactory.makeAdapter(protocolType)

    // 控制指令特征
    private var controlCharacteristic: BluetoothGattCharacteristic? = null
    private var statusCharacteristic: BluetoothGattCharacteristic? = null

    // 扫描状态
    private val discoveredDevices = ConcurrentHashMap<String, VehicleAdvertise>()
    private val scanCompleter = AtomicReference<CompletableDeferred<List<VehicleAdvertise>>?>(null)

    var state: BleState = BleState.UNKNOWN
        private set
    var connectionState: ConnectionState = ConnectionState.DISCONNECTED
        private set

    /** 连接状态变化回调 (0=disconnected 1=scanning 2=connecting 3=connected) */
    var connectionChangeHandler: ((Int) -> Unit)? = null

    // MARK: 扫描

    /**
     * 扫描车辆
     * @param timeoutMs 扫描超时（毫秒）
     * @param vehicleIds 可选过滤: 仅保留 vehicleId (或设备 MAC) 在此集合内的结果
     */
    suspend fun scanVehicles(timeoutMs: Long = 10000, vehicleIds: Set<String>? = null): List<VehicleAdvertise> =
        withContext(Dispatchers.Main) {
            val bleAdapter = bluetoothAdapter ?: throw YDKError.Internal("bluetooth not available")
            if (!bleAdapter.isEnabled) throw YDKError.Internal("bluetooth not enabled")

            discoveredDevices.clear()
            connectionState = ConnectionState.SCANNING
            connectionChangeHandler?.invoke(1)

            val deferred = CompletableDeferred<List<VehicleAdvertise>>()
            scanCompleter.set(deferred)

            // 真实扫描: BluetoothLeScanner + service UUID 过滤器 + 2b-B 解析
            val engine = scanEngine ?: LeScannerBleScanEngine(bleAdapter.bluetoothLeScanner)
            val processor = ScanResultProcessor()
            val filters = BleScanFilterFactory.filtersForProtocols(BleProtocolType.entries.toList())

            val scanCallback = object : ScanCallback() {
                override fun onScanResult(callbackType: Int, result: ScanResult) {
                    val vehicle = processor.process(result.scanRecord?.bytes, result.rssi) ?: return
                    // vehicleId 匹配: 支持按解析出的 vehicleId 或设备 MAC 过滤
                    if (vehicleIds != null &&
                        vehicle.vehicleId !in vehicleIds &&
                        result.device.address !in vehicleIds
                    ) {
                        return
                    }
                    discoveredDevices[result.device.address] = vehicle
                }

                override fun onScanFailed(errorCode: Int) {
                    scanCompleter.get()?.completeExceptionally(
                        YDKError.Internal("scan failed: code=$errorCode")
                    )
                    scanCompleter.set(null)
                }
            }

            val started = engine.startScan(filters, scanCallback)
            if (!started) {
                scanCompleter.set(null)
                throw YDKError.Internal("failed to start BLE scan")
            }

            try {
                withTimeout(timeoutMs) {
                    deferred.await()
                }
            } catch (_: TimeoutCancellationException) {
                // 超时正常返回已发现设备
            } finally {
                engine.stopScan(scanCallback)
                connectionState = ConnectionState.DISCONNECTED
            }

            discoveredDevices.values.toList()
        }

    // MARK: 连接

    /**
     * 连接车辆
     * @param address BLE 设备地址（扫描结果中的 vehicleId 或 MAC）
     */
    suspend fun connect(address: String): ConnectResponse = withContext(Dispatchers.Main) {
        val bleAdapter = bluetoothAdapter ?: return@withContext ConnectResponse(false, "bluetooth not available")

        val device: BluetoothDevice = try {
            bleAdapter.getRemoteDevice(address)
        } catch (e: IllegalArgumentException) {
            return@withContext ConnectResponse(false, "invalid device address")
        }

        connectionState = ConnectionState.CONNECTING
        connectionChangeHandler?.invoke(2)

        val deferred = CompletableDeferred<ConnectResponse>()

        val gattCallback = object : BluetoothGattCallback() {
            override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
                when (newState) {
                    BluetoothGatt.STATE_CONNECTED -> {
                        connectionState = ConnectionState.DISCOVERING
                        g.discoverServices()
                    }
                    BluetoothGatt.STATE_DISCONNECTED -> {
                        connectionState = ConnectionState.DISCONNECTED
                        connectionChangeHandler?.invoke(0)
                        if (!deferred.isCompleted) {
                            deferred.complete(ConnectResponse(false, "disconnected"))
                        }
                    }
                }
            }

            override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
                val service = g.getService(adapter.protocolType.serviceUuid)
                    ?: run {
                        deferred.complete(ConnectResponse(false, "service not found"))
                        return
                    }

                // 保存关键特征
                controlCharacteristic = service.getCharacteristic(BleUuids.CCC_CHAR_KEY_DATA)
                    ?: service.getCharacteristic(BleUuids.ICCE_CHAR_CONTROL_CMD)
                statusCharacteristic = service.getCharacteristic(BleUuids.CCC_CHAR_STATE)
                    ?: service.getCharacteristic(BleUuids.ICCE_CHAR_KEY_STATUS)

                // 订阅通知
                statusCharacteristic?.let { char ->
                    g.setCharacteristicNotification(char, true)
                }

                connectionState = ConnectionState.CONNECTED
                connectionChangeHandler?.invoke(3)
                deferred.complete(ConnectResponse(true))
            }
        }

        gatt = device.connectGatt(context, false, gattCallback)
        deferred.await()
    }

    suspend fun disconnect() = withContext(Dispatchers.Main) {
        gatt?.disconnect()
        gatt?.close()
        gatt = null
        connectionState = ConnectionState.DISCONNECTED
        connectionChangeHandler?.invoke(0)
    }

    // MARK: 控制指令

    suspend fun unlock(vehicleId: String): LocalControlResponse =
        sendCommand(BleCommandType.UNLOCK, vehicleId)

    suspend fun lock(vehicleId: String): LocalControlResponse =
        sendCommand(BleCommandType.LOCK, vehicleId)

    suspend fun startEngine(vehicleId: String): LocalControlResponse =
        sendCommand(BleCommandType.ENGINE_ON, vehicleId)

    suspend fun readVehicleStatus(vehicleId: String): VehicleStatus = withContext(Dispatchers.Main) {
        val g = gatt ?: throw YDKError.Internal("not connected")
        val char = statusCharacteristic ?: throw YDKError.Internal("status characteristic not found")

        val deferred = CompletableDeferred<ByteArray>()
        // 简化: 直接读已有缓存值（真实实现需 readCharacteristic + 回调）
        val value = char.value ?: ByteArray(3)
        deferred.complete(value)

        adapter.parseVehicleStatus(deferred.await())
    }

    private suspend fun sendCommand(type: BleCommandType, vehicleId: String): LocalControlResponse =
        withContext(Dispatchers.Main) {
            val g = gatt ?: return@withContext LocalControlResponse(false, "not connected")
            val char = controlCharacteristic ?: return@withContext LocalControlResponse(false, "control characteristic not found")

            val session = SessionContext(keyId = "local-key", vehicleId = vehicleId)
            val command = when (type) {
                BleCommandType.UNLOCK -> adapter.buildUnlockCommand(session.keyId, session)
                BleCommandType.LOCK -> adapter.buildLockCommand(session.keyId, session)
                BleCommandType.ENGINE_ON -> adapter.buildStartEngineCommand(session.keyId, session)
                else -> return@withContext LocalControlResponse(false, "unsupported command")
            }

            // 简化: 写入后假设成功（真实实现需等待回调验证）
            g.writeCharacteristic(char, command, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)

            // 读取响应（简化: 假设成功）
            LocalControlResponse(true)
        }

    /** 订阅特征通知流 */
    fun subscribe(characteristicUuid: UUID): Flow<ByteArray> = callbackFlow {
        // 简化实现：真实场景需通过 BluetoothGattCallback onCharacteristicChanged
        awaitClose { }
    }

    fun shutdown() {
        scope.cancel()
        gatt?.close()
        gatt = null
    }
}
