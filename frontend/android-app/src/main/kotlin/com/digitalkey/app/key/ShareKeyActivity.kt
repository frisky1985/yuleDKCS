/**
 * DigitalKey App - 分享钥匙 Activity
 */
package com.digitalkey.app.key

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.MenuItem
import android.view.View
import android.widget.Toast
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.widget.doAfterTextChanged
import androidx.lifecycle.lifecycleScope
import com.digitalkey.app.R
import com.digitalkey.app.data.model.Permission
import com.digitalkey.app.data.model.PermissionType
import com.digitalkey.app.data.model.UiState
import com.digitalkey.app.databinding.ActivityShareKeyBinding
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

/**
 * 分享钥匙 Activity
 * 
 * 功能：
 * - 输入接收者信息
 * - 选择分享的权限
 * - 生成分享码/链接
 * - 复制分享信息
 */
@AndroidEntryPoint
class ShareKeyActivity : AppCompatActivity() {

    companion object {
        const val EXTRA_KEY_ID = "extra_key_id"
        const val EXTRA_KEY_NAME = "extra_key_name"
    }

    private lateinit var binding: ActivityShareKeyBinding
    private val viewModel: AddKeyViewModel by viewModels()

    private var keyId: String = ""
    private var keyName: String = ""

    // 权限选择状态
    private val selectedPermissions = mutableSetOf<PermissionType>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityShareKeyBinding.inflate(layoutInflater)
        setContentView(binding.root)

        keyId = intent.getStringExtra(EXTRA_KEY_ID) ?: run {
            finish()
            return
        }
        keyName = intent.getStringExtra(EXTRA_KEY_NAME) ?: ""

        setupToolbar()
        setupViews()
        setupPermissions()
        observeState()
    }

    private fun setupToolbar() {
        setSupportActionBar(binding.toolbar)
        supportActionBar?.apply {
            setDisplayHomeAsUpEnabled(true)
            setDisplayShowHomeEnabled(true)
            title = getString(R.string.share_key)
        }
    }

    private fun setupViews() {
        // 接收者信息输入
        binding.editRecipientName.doAfterTextChanged { text ->
            viewModel.setRecipient(
                id = "", // 实际场景中需要先查询用户
                name = text?.toString() ?: ""
            )
        }

        binding.editShareMessage.doAfterTextChanged { text ->
            viewModel.setShareMessage(text?.toString() ?: "")
        }

        // 分享按钮
        binding.btnShare.setOnClickListener {
            performShare()
        }

        // 复制按钮
        binding.btnCopy.setOnClickListener {
            copyShareCode()
        }

        // 返回主页
        binding.btnDone.setOnClickListener {
            finish()
        }
    }

    private fun setupPermissions() {
        // 默认选中基础权限
        selectedPermissions.add(PermissionType.UNLOCK)
        selectedPermissions.add(PermissionType.LOCK)

        binding.chipUnlock.isChecked = true
        binding.chipLock.isChecked = true

        binding.chipUnlock.setOnCheckedChangeListener { _, isChecked ->
            togglePermission(PermissionType.UNLOCK, isChecked)
        }
        binding.chipLock.setOnCheckedChangeListener { _, isChecked ->
            togglePermission(PermissionType.LOCK, isChecked)
        }
        binding.chipStartEngine.setOnCheckedChangeListener { _, isChecked ->
            togglePermission(PermissionType.START_ENGINE, isChecked)
        }
        binding.chipTrunk.setOnCheckedChangeListener { _, isChecked ->
            togglePermission(PermissionType.OPEN_TRUNK, isChecked)
        }
        binding.chipClimate.setOnCheckedChangeListener { _, isChecked ->
            togglePermission(PermissionType.CLIMATE_CONTROL, isChecked)
        }
        binding.chipFindCar.setOnCheckedChangeListener { _, isChecked ->
            togglePermission(PermissionType.CAR_FIND, isChecked)
        }
    }

    private fun togglePermission(type: PermissionType, selected: Boolean) {
        if (selected) {
            selectedPermissions.add(type)
        } else {
            selectedPermissions.remove(type)
        }
    }

    private fun observeState() {
        lifecycleScope.launch {
            viewModel.shareState.collectLatest { state ->
                when (state) {
                    is UiState.Loading -> {
                        binding.progressBar.visibility = View.VISIBLE
                        binding.btnShare.isEnabled = false
                    }
                    is UiState.Success -> {
                        binding.progressBar.visibility = View.GONE
                        binding.btnShare.isEnabled = true
                        showShareResult(state.data.shareCode ?: "")
                    }
                    is UiState.Error -> {
                        binding.progressBar.visibility = View.GONE
                        binding.btnShare.isEnabled = true
                        Toast.makeText(this@ShareKeyActivity, state.message, Toast.LENGTH_LONG).show()
                    }
                    else -> {
                        binding.progressBar.visibility = View.GONE
                        binding.btnShare.isEnabled = true
                    }
                }
            }
        }
    }

    private fun performShare() {
        val recipientName = binding.editRecipientName.text?.toString()?.trim() ?: ""
        if (recipientName.isEmpty()) {
            binding.inputLayoutRecipientName.error = getString(R.string.error_recipient_required)
            return
        }
        binding.inputLayoutRecipientName.error = null

        if (selectedPermissions.isEmpty()) {
            Toast.makeText(this, R.string.error_permission_required, Toast.LENGTH_SHORT).show()
            return
        }

        viewModel.setRecipient(id = "user_${System.currentTimeMillis()}", name = recipientName)
        viewModel.shareKey(keyId, selectedPermissions.toList())
    }

    private fun showShareResult(shareCode: String) {
        binding.layoutInput.visibility = View.GONE
        binding.layoutResult.visibility = View.VISIBLE

        binding.textShareCode.text = shareCode
        binding.textShareInfo.text = getString(R.string.share_created_info, keyName)
    }

    private fun copyShareCode() {
        val shareCode = binding.textShareCode.text.toString()
        val clipboard = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clip = ClipData.newPlainText("数字钥匙分享码", shareCode)
        clipboard.setPrimaryClip(clip)
        Toast.makeText(this, R.string.share_code_copied, Toast.LENGTH_SHORT).show()
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            android.R.id.home -> {
                onBackPressedDispatcher.onBackPressed()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }
}
