package com.yuledkcs.model

/**
 * Commands that can be sent to a vehicle
 */
sealed class Command(
    open val keyId: String
) {
    /**
     * Lock the vehicle
     */
    data class Lock(
        override val keyId: String
    ) : Command(keyId)
    
    /**
     * Unlock the vehicle
     */
    data class Unlock(
        override val keyId: String
    ) : Command(keyId)
    
    /**
     * Start the engine
     */
    data class StartEngine(
        override val keyId: String
    ) : Command(keyId)
    
    /**
     * Stop the engine
     */
    data class StopEngine(
        override val keyId: String
    ) : Command(keyId)
    
    /**
     * Open the trunk
     */
    data class TrunkOpen(
        override val keyId: String
    ) : Command(keyId)
    
    /**
     * Custom command with raw data
     */
    data class Custom(
        override val keyId: String,
        val data: ByteArray
    ) : Command(keyId) {
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (javaClass != other?.javaClass) return false
            
            other as Custom
            
            if (keyId != other.keyId) return false
            if (!data.contentEquals(other.data)) return false
            
            return true
        }
        
        override fun hashCode(): Int {
            var result = keyId.hashCode()
            result = 31 * result + data.contentHashCode()
            return result
        }
    }
}
