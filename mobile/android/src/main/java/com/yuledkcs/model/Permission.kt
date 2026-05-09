package com.yuledkcs.model

import com.google.gson.annotations.SerializedName

/**
 * Permissions that can be granted for a digital key
 */
enum class Permission {
    @SerializedName("all")
    ALL,
    
    @SerializedName("unlock")
    UNLOCK,
    
    @SerializedName("lock")
    LOCK,
    
    @SerializedName("start_engine")
    START_ENGINE,
    
    @SerializedName("stop_engine")
    STOP_ENGINE,
    
    @SerializedName("trunk_open")
    TRUNK_OPEN,
    
    @SerializedName("trunk_close")
    TRUNK_CLOSE,
    
    @SerializedName("windows_open")
    WINDOWS_OPEN,
    
    @SerializedName("windows_close")
    WINDOWS_CLOSE,
    
    @SerializedName("climate_start")
    CLIMATE_START,
    
    @SerializedName("climate_stop")
    CLIMATE_STOP,
    
    @SerializedName("share")
    SHARE,
    
    @SerializedName("revoke")
    REVOKE
}
