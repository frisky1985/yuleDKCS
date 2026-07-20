/**
 * DigitalKey SDK - Android SDK入口
 * 
 * 提供数字钥匙核心功能的统一入口点
 * 
 * @version 1.0.0
 * @date 2026-05-06
 */
package com.digitalkey.sdk

import android.content.Context
import com.digitalkey.sdk.key.KeyManager
import com.digitalkey.sdk.key.KeyManagerImpl
import com.digitalkey.sdk.vehicle.VehicleController
import com.digitalkey.sdk.vehicle.VehicleControllerImpl
import com.digitalkey.sdk.channel.ChannelManager
import com.digitalkey.sdk.channel.ChannelManagerImpl
import com.digitalkey.sdk.security.SecurityModule
import com.digitalkey.sdk.security.SecurityModuleImpl
import com.digitalkey.sdk.network.ApiClient
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * SDK配置参数
 * 
 * @property serverUrl 服务器地址，如 "https://api.digitalkey.cn"
 * @property appId 应用标识
 * @property clientId 客户端ID
 * @property apiKey API密钥
 * @property enableLog 是否启用日志
 */
data class SdkConfig(
    val serverUrl: String,
    val appId: String,
    val clientId: String,
    val apiKey: String,
    val enableLog: Boolean = false
)

/**
 * SDK初始化结果
 */
sealed class InitResult {
    data class Success(val sdk: DigitalKeySdk) : InitResult()
    data class Failure(val error: DigitalKeyException) : InitResult()
}

/**
 * DigitalKey SDK 主入口对象
 * 
 * 使用示例:
 * ```kotlin
 * val result = DigitalKeySdk.init(context, SdkConfig(
 *     serverUrl = "https://api.digitalkey.cn",
 *     appId = "com.example.app",
 *     clientId = "android_xxxxx",
 *     apiKey = "your_api_key"
 * ))
 * ```
 */
object DigitalKeySdk {
    
    private var _instance: DigitalKeySdkImpl? = null
    private val lock = Any()
    
    /**
     * 初始化SDK
     * 
     * @param context Android应用上下文
     * @param config SDK配置参数
     * @return 初始化结果
     */
    fun init(context: Context, config: SdkConfig): InitResult {
        synchronized(lock) {
            if (_instance != null) {
                return InitResult.Success(_instance!!)
            }
            
            try {
                val instance = DigitalKeySdkImpl(context, config)
                _instance = instance
                return InitResult.Success(instance)
            } catch (e: Exception) {
                return InitResult.Failure(
                    DigitalKeyException.Network(-1, "SDK初始化失败: ${e.message}")
                )
            }
        }
    }
    
    /**
     * 获取SDK实例
     * 
     * @return SDK实例，未初始化时返回null
     */
    fun getInstance(): DigitalKeySdkImpl? = _instance
    
    /**
     * 检查SDK是否已初始化
     */
    fun isInitialized(): Boolean = _instance != null
    
    /**
     * 销毁SDK实例
     */
    fun destroy() {
        synchronized(lock) {
            _instance?.destroy()
            _instance = null
        }
    }
}

/**
 * SDK实现类
 * 
 * 提供各模块的访问入口
 */
class DigitalKeySdkImpl(
    private val context: Context,
    private val config: SdkConfig
) {
    // API客户端
    private val apiClient: ApiClient = ApiClient(config)
    
    // 各模块实例
    private val _keyManager: KeyManager by lazy {
        KeyManagerImpl(apiClient, context)
    }
    
    private val _vehicleController: VehicleController by lazy {
        VehicleControllerImpl(apiClient, context)
    }
    
    private val _channelManager: ChannelManager by lazy {
        ChannelManagerImpl(context)
    }
    
    private val _securityModule: SecurityModule by lazy {
        SecurityModuleImpl(context)
    }
    
    /**
     * 密钥管理模块
     */
    val keyManager: KeyManager
        get() = _keyManager
    
    /**
     * 车辆控制模块
     */
    val vehicleController: VehicleController
        get() = _vehicleController
    
    /**
     * 通道管理模块
     */
    val channelManager: ChannelManager
        get() = _channelManager
    
    /**
     * 安全模块
     */
    val securityModule: SecurityModule
        get() = _securityModule
    
    /**
     * 获取SDK配置
     */
    fun getConfig(): SdkConfig = config
    
    /**
     * 销毁SDK，释放资源
     */
    fun destroy() {
        apiClient.destroy()
    }
}

/**
 * 统一异常类型
 */
sealed class DigitalKeyException : Exception() {
    /** 网络异常 */
    data class Network(val code: Int, override val message: String) : DigitalKeyException()
    
    /** 认证异常 */
    data class Auth(val code: Int, override val message: String) : DigitalKeyException()
    
    /** 密钥异常 */
    data class Key(val code: Int, override val message: String) : DigitalKeyException()
    
    /** 车辆异常 */
    data class Vehicle(val code: Int, override val message: String) : DigitalKeyException()
    
    /** 硬件异常 */
    data class Hardware(override val message: String) : DigitalKeyException()
}

/**
 * 硬件异常子类
 */
class NfcDisabledException : DigitalKeyException.Hardware("NFC未启用")
class BluetoothDisabledException : DigitalKeyException.Hardware("蓝牙未启用")
class LocationDisabledException : DigitalKeyException.Hardware("位置服务未启用")
class SecureElementException : DigitalKeyException.Hardware("安全元件不可用")
class UwbNotSupportedException : DigitalKeyException.Hardware("UWB不支持")
