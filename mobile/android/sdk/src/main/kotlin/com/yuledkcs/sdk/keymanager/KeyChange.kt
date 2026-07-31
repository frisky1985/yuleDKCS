package com.yuledkcs.sdk.keymanager

import com.yuledkcs.sdk.hub.YDKKey

/**
 * 钥匙变更事件
 */
data class KeyChange(
    val keyId: String,
    val type: ChangeType,
    val key: YDKKey?
)

enum class ChangeType {
    ADDED, UPDATED, REMOVED
}

/**
 * 同步结果
 */
data class SyncResult(
    val added: List<YDKKey>,
    val updated: List<YDKKey>,
    val removed: List<YDKKey>,
    val unchanged: Int
) {
    val hasChanges: Boolean get() =
        added.isNotEmpty() || updated.isNotEmpty() || removed.isNotEmpty()
}
