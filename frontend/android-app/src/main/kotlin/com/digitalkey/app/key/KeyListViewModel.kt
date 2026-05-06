/**
 * DigitalKey App - 钥匙列表 ViewModel
 */
package com.digitalkey.app.key

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.digitalkey.app.data.model.KeyModel
import com.digitalkey.app.data.model.UiState
import com.digitalkey.app.data.repository.KeyRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import javax.inject.Inject

/**
 * 钥匙列表 ViewModel
 * 负责管理钥匙列表页面的UI状态和业务逻辑
 */
@HiltViewModel
class KeyListViewModel @Inject constructor(
    private val keyRepository: KeyRepository
) : ViewModel() {

    // 钥匙列表状态
    private val _keyListState = MutableStateFlow<UiState<List<KeyModel>>>(UiState.Loading)
    val keyListState: StateFlow<UiState<List<KeyModel>>> = _keyListState.asStateFlow()

    // 选中的钥匙
    private val _selectedKey = MutableStateFlow<KeyModel?>(null)
    val selectedKey: StateFlow<KeyModel?> = _selectedKey.asStateFlow()

    // 加载状态标记
    private val _isRefreshing = MutableStateFlow(false)
    val isRefreshing: StateFlow<Boolean> = _isRefreshing.asStateFlow()

    init {
        loadKeys()
    }

    /**
     * 加载钥匙列表
     */
    fun loadKeys() {
        viewModelScope.launch {
            _keyListState.value = UiState.Loading
            keyRepository.getKeys()
                .onSuccess { keys ->
                    _keyListState.value = if (keys.isEmpty()) {
                        UiState.Empty
                    } else {
                        UiState.Success(keys)
                    }
                }
                .onFailure { error ->
                    _keyListState.value = UiState.Error(
                        message = error.message ?: "加载钥匙列表失败",
                        code = null
                    )
                }
        }
    }

    /**
     * 下拉刷新
     */
    fun refreshKeys() {
        viewModelScope.launch {
            _isRefreshing.value = true
            keyRepository.getKeys()
                .onSuccess { keys ->
                    _keyListState.value = if (keys.isEmpty()) {
                        UiState.Empty
                    } else {
                        UiState.Success(keys)
                    }
                }
                .onFailure { error ->
                    _keyListState.value = UiState.Error(
                        message = error.message ?: "刷新失败",
                        code = null
                    )
                }
            _isRefreshing.value = false
        }
    }

    /**
     * 选择钥匙
     */
    fun selectKey(key: KeyModel) {
        _selectedKey.value = key
    }

    /**
     * 清除选择
     */
    fun clearSelection() {
        _selectedKey.value = null
    }

    /**
     * 删除钥匙
     */
    fun deleteKey(keyId: String, onResult: (Boolean) -> Unit) {
        viewModelScope.launch {
            keyRepository.deleteKey(keyId)
                .onSuccess {
                    loadKeys() // 刷新列表
                    onResult(true)
                }
                .onFailure {
                    onResult(false)
                }
        }
    }

    /**
     * 更新钥匙名称
     */
    fun updateKeyName(keyId: String, newName: String, onResult: (Boolean) -> Unit) {
        viewModelScope.launch {
            keyRepository.updateKeyName(keyId, newName)
                .onSuccess {
                    loadKeys()
                    onResult(true)
                }
                .onFailure {
                    onResult(false)
                }
        }
    }
}
