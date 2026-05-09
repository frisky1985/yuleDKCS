package com.yuledkcs

import android.content.Context
import android.util.Log
import com.yuledkcs.api.YuleDKCSApi
import com.yuledkcs.internal.SecurityProvider
import okhttp3.OkHttpClient
import okhttp3.logging.HttpLoggingInterceptor
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory

/**
 * YuleDKCS - Digital Key Control System for Android
 * 
 * Main entry point for the SDK. Initialize with your API key before using other features.
 */
object YuleDKCS {
    
    private const val TAG = "YuleDKCS"
    private const val DEFAULT_BASE_URL = "https://api.yuledkcs.com/v1/"
    
    @Volatile
    private var isInitialized = false
    
    internal lateinit var context: Context
        private set
    
    internal lateinit var apiKey: String
        private set
    
    internal lateinit var api: YuleDKCSApi
        private set
    
    internal lateinit var securityProvider: SecurityProvider
        private set
    
    /**
     * Initialize the YuleDKCS SDK
     * 
     * @param context Application context
     * @param apiKey Your API key from the YuleDKCS dashboard
     * @param baseUrl Optional custom base URL for the API
     */
    @JvmStatic
    fun initialize(
        context: Context,
        apiKey: String,
        baseUrl: String = DEFAULT_BASE_URL
    ) {
        if (isInitialized) {
            Log.w(TAG, "YuleDKCS already initialized")
            return
        }
        
        this.context = context.applicationContext
        this.apiKey = apiKey
        this.securityProvider = SecurityProvider(context)
        
        val loggingInterceptor = HttpLoggingInterceptor().apply {
            level = HttpLoggingInterceptor.Level.BODY
        }
        
        val okHttpClient = OkHttpClient.Builder()
            .addInterceptor { chain ->
                val request = chain.request().newBuilder()
                    .addHeader("Authorization", "Bearer $apiKey")
                    .addHeader("X-SDK-Version", BuildConfig.VERSION_NAME)
                    .build()
                chain.proceed(request)
            }
            .addInterceptor(loggingInterceptor)
            .build()
        
        val retrofit = Retrofit.Builder()
            .baseUrl(baseUrl)
            .client(okHttpClient)
            .addConverterFactory(GsonConverterFactory.create())
            .build()
        
        api = retrofit.create(YuleDKCSApi::class.java)
        
        // Initialize native library
        NativeLib.initialize()
        
        isInitialized = true
        Log.i(TAG, "YuleDKCS initialized successfully")
    }
    
    /**
     * Check if the SDK has been initialized
     */
    @JvmStatic
    fun isInitialized(): Boolean = isInitialized
    
    /**
     * Get the KeyManager instance for key operations
     */
    @JvmStatic
    fun getKeyManager(): KeyManager {
        checkInitialized()
        return KeyManager.getInstance()
    }
    
    /**
     * Get the BLEManager instance for Bluetooth operations
     */
    @JvmStatic
    fun getBLEManager(): BLEManager {
        checkInitialized()
        return BLEManager.getInstance()
    }
    
    internal fun checkInitialized() {
        if (!isInitialized) {
            throw IllegalStateException(
                "YuleDKCS not initialized. Call YuleDKCS.initialize() first."
            )
        }
    }
}
