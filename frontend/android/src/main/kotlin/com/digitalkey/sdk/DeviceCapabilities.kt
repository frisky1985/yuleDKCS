package com.digitalkey.sdk

import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.content.ContextCompat

/**
 * DeviceCapabilities detects and reports what hardware/software capabilities
 * the current device supports for digital key provisioning.
 */
object DeviceCapabilities {

    data class CapabilityReport(
        val platform: String = "android",
        val model: String = Build.MODEL,
        val osVersion: String = Build.VERSION.RELEASE,
        val appVersion: String = "",
        val ble: Boolean = false,
        val uwb: Boolean = false,
        val nfc: Boolean = false,
        val secureElement: Boolean = false,
    )

    /**
     * Detects all capabilities of the current device.
     * Call this on app startup to register the device with the cloud.
     */
    fun detect(context: Context, appVersion: String = ""): CapabilityReport {
        return CapabilityReport(
            platform = "android",
            model = "${Build.MANUFACTURER} ${Build.MODEL}",
            osVersion = Build.VERSION.RELEASE,
            appVersion = appVersion,
            ble = supportsBLE(context),
            uwb = supportsUWB(context),
            nfc = supportsNFC(context),
            secureElement = supportsSE(context),
        )
    }

    /** BLE: Android 4.3+ (API 18) has built-in BLE support */
    private fun supportsBLE(context: Context): Boolean {
        return context.packageManager.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)
    }

    /** UWB: Android 12+ (API 31) with UWB hardware */
    private fun supportsUWB(context: Context): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            context.packageManager.hasSystemFeature(PackageManager.FEATURE_UWB)
        } else false
    }

    /** NFC: Requires NFC hardware */
    private fun supportsNFC(context: Context): Boolean {
        return context.packageManager.hasSystemFeature(PackageManager.FEATURE_NFC)
    }

    /** Secure Element: StrongBox / TEE support */
    private fun supportsSE(context: Context): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            context.packageManager.hasSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE)
        } else false
    }
}
