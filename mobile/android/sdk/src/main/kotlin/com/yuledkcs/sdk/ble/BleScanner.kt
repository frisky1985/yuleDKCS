package com.yuledkcs.sdk.ble

import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanSettings
import android.os.ParcelUuid

/**
 * BLE 扫描抽象 — 2b-D
 *
 * 生产环境使用 [LeScannerBleScanEngine] (BluetoothLeScanner 实现),
 * 测试可注入 fake 引擎以驱动扫描回调, 无需真实蓝牙硬件。
 */
interface BleScanEngine {
    /** 启动扫描; 返回是否成功启动 */
    fun startScan(filters: List<ScanFilter>, callback: ScanCallback): Boolean

    /** 停止扫描 (重复调用安全) */
    fun stopScan(callback: ScanCallback)
}

/**
 * 基于 [BluetoothLeScanner] 的真实扫描引擎。
 * 使用低延迟扫描模式, 通过 service UUID 过滤减少无关广播。
 */
class LeScannerBleScanEngine(private val scanner: BluetoothLeScanner) : BleScanEngine {

    override fun startScan(filters: List<ScanFilter>, callback: ScanCallback): Boolean = try {
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .setCallbackType(ScanSettings.CALLBACK_TYPE_ALL_MATCHES)
            .build()
        scanner.startScan(filters, settings, callback)
        true
    } catch (_: Exception) {
        false
    }

    override fun stopScan(callback: ScanCallback) {
        try {
            scanner.stopScan(callback)
        } catch (_: Exception) {
            // 停止扫描失败可忽略 (可能已停止)
        }
    }
}

/**
 * 扫描过滤器工厂 — 按协议生成 service UUID 过滤器。
 * 过滤条件来自 [BleUuids] (CCC 0xFFD1 / ICCOA 0xFEF5 / ICCE 0xFEFA)。
 */
object BleScanFilterFactory {

    fun filtersForProtocols(protocols: List<BleProtocolType> = BleProtocolType.entries.toList()): List<ScanFilter> =
        protocols.map { type ->
            ScanFilter.Builder()
                .setServiceUuid(ParcelUuid(BleUuids.serviceForProtocol(type)))
                .build()
        }
}

/**
 * 扫描结果处理器 — 纯逻辑, 可单测。
 *
 * 将 (原始广播字节, RSSI) 依次交给各协议适配器解析 (2b-B),
 * 返回第一个匹配协议的 [VehicleAdvertise]; 无匹配返回 null。
 */
class ScanResultProcessor(
    private val adapters: List<BleProtocolAdapter> = BleProtocolType.entries.map { BleProtocolAdapterFactory.makeAdapter(it) }
) {

    fun process(scanRecord: ByteArray?, rssi: Int): VehicleAdvertise? {
        for (adapter in adapters) {
            val vehicle = adapter.parseAdvertisement(scanRecord, rssi) ?: continue
            return vehicle
        }
        return null
    }
}
