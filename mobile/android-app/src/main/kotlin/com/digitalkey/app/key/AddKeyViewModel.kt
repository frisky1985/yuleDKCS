/**
 * DigitalKey App - 添加钥匙 ViewModel
 */
package com.digitalkey.app.key

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.digitalkey.app.data.model.AddKeyRequest
import com.digitalkey.app.data.model.KeyModel
import com.digitalkey.app.data.model.ShareKeyRequest
import com.digitalkey.app.data.model.ShareKeyResult
import com.digitalkey.app.data.model.UiState
import com.digitalkey.app.data.repository.KeyRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import javax.inject.Inject

/**
 * 添加钥匙 ViewModel
 * 负责管理添加钥匙流程的UI状态和业务逻辑
 */
@HiltViewModel
class AddKeyViewModel @Inject constructor(
    private val keyRepository: KeyRepository
) : ViewModel() {

    // 添加钥匙步骤
    private val _addKeyStep = MutableStateFlow(AddKeyStep.ENTER_CODE)
    val addKeyStep: StateFlow<AddKeyStep> = _addKeyStep.asStateFlow()

    // 激活码输入
    private val _activationCode = MutableStateFlow("")
    val activationCode: StateFlow<String> = _activationCode.asStateFlow()

    // 钥匙名称
    private val _keyName = MutableStateFlow("")
    val keyName: StateFlow<String> = _keyName.asStateFlow()

    // 添加结果
    private val _addKeyState = MutableStateFlow<UiState<KeyModel>?>(null)
    val addKeyState: StateFlow<UiState<KeyModel>?> = _addKeyState.asStateFlow()

    // 分享请求参数
    private val _shareRequest = MutableStateFlow<ShareKeyRequest?>(null)
    val shareRequest: StateFlow<ShareKeyRequest?> = _shareRequest.asStateFlow()

    // 分享结果
    private val _shareState = MutableStateFlow<UiState<ShareKeyResult>?>(null)
    val shareState: StateFlow<UiState<ShareKeyResult>?> = _shareState.asStateFlow()

    // 接收者信息
    private val _recipientId = MutableStateFlow("")
    val recipientId: StateFlow<String> = _recipientId.asStateFlow()

    private val _recipientName = MutableStateFlow("")
    val recipientName: StateFlow<String> = _recipientName.asStateFlow()

    private val _shareMessage = MutableStateFlow("")
    val shareMessage: StateFlow<String> = _shareMessage.asStateFlow()

    /**
     * 设置激活码
     */
    fun setActivationCode(code: String) {
        _activationCode.value = code.uppercase().take(16)
    }

    /**
     * 设置钥匙名称
     */
    fun setKeyName(name: String) {
        _keyName.value = name.take(50)
    }

    /**
     * 下一步（输入激活码 -> 输入名称）
     */
    fun nextStep() {
        when (_addKeyStep.value) {
            AddKeyStep.ENTER_CODE -> {
                if (_activationCode.value.length >= 8) {
                    _addKeyStep.value = AddKeyStep.ENTER_NAME
                }
            }
            AddKeyStep.ENTER_NAME -> {
                // 触发添加流程
                addKey()
            }
            else -> {}
        }
    }

    /**
     * 上一步
     */
    fun prevStep() {
        when (_addKeyStep.value) {
            AddKeyStep.ENTER_NAME -> _addKeyStep.value = AddKeyStep.ENTER_CODE
            else -> {}
        }
    }

    /**
     * 开始添加钥匙流程
     */
    fun addKey() {
        if (_keyName.value.isBlank()) {
            _addKeyState.value = UiState.Error("请输入钥匙名称")
            return
        }

        viewModelScope.launch {
            _addKeyState.value = UiState.Loading
            _addKeyStep.value = AddKeyStep.PROCESSING

            keyRepository.addKey(
                AddKeyRequest(
                    activationCode = _activationCode.value,
                    keyName = _keyName.value
                )
            ).onSuccess { key ->
                _addKeyState.value = UiState.Success(key)
                _addKeyStep.value = AddKeyStep.SUCCESS
            }.onFailure { error ->
                _addKeyState.value = UiState.Error(
                    message = error.message ?: "添加钥匙失败",
                    code = null
                )
                _addKeyStep.value = AddKeyStep.FAILED
            }
        }
    }

    /**
     * 设置分享接收者
     */
    fun setRecipient(id: String, name: String) {
        _recipientId.value = id
        _recipientName.value = name
    }

    /**
     * 设置分享附言
     */
    fun setShareMessage(message: String) {
        _shareMessage.value = message.take(200)
    }

    /**
     * 分享钥匙
     */
    fun shareKey(keyId: String, permissions: List<com.digitalkey.app.data.model.PermissionType>) {
        if (_recipientId.value.isBlank() || _recipientName.value.isBlank()) {
            _shareState.value = UiState.Error("请填写接收者信息")
            return
        }

        viewModelScope.launch {
            _shareState.value = UiState.Loading

            keyRepository.shareKey(
                ShareKeyRequest(
                    keyId = keyId,
                    recipientId = _recipientId.value,
                    recipientName = _recipientName.value,
                    permissions = permissions,
                    message = _shareMessage.value.takeIf { it.isNotBlank() }
                )
            ).onSuccess { result ->
                _shareState.value = UiState.Success(result)
            }.onFailure { error ->
                _shareState.value = UiState.Error(
                    message = error.message ?: "分享失败",
                    code = null
                )
            }
        }
    }

    /**
     * 重置添加钥匙流程
     */
    fun reset() {
        _addKeyStep.value = AddKeyStep.ENTER_CODE
        _activationCode.value = ""
        _keyName.value = ""
        _addKeyState.value = null
        _shareState.value = null
        _recipientId.value = ""
        _recipientName.value = ""
        _shareMessage.value = ""
    }

    /**
     * 是否可以进入下一步
     */
    fun canProceed(): Boolean {
        return when (_addKeyStep.value) {
            AddKeyStep.ENTER_CODE -> _activationCode.value.length >= 8
            AddKeyStep.ENTER_NAME -> _keyName.value.isNotBlank()
            else -> false
        }
    }
}

/**
 * 添加钥匙步骤
 */
enum class AddKeyStep {
    ENTER_CODE,    // 输入激活码
    ENTER_NAME,    // 输入钥匙名称
    PROCESSING,    // 处理中
    SUCCESS,       // 成功
    FAILED         // 失败
}
