/**
 * DigitalKey App - 添加钥匙 Activity
 */
package com.digitalkey.app.key

import android.os.Bundle
import android.view.MenuItem
import android.view.View
import android.view.inputmethod.EditorInfo
import android.widget.Toast
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.widget.doAfterTextChanged
import androidx.lifecycle.lifecycleScope
import com.digitalkey.app.R
import com.digitalkey.app.data.model.UiState
import com.digitalkey.app.databinding.ActivityAddKeyBinding
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

/**
 * 添加钥匙 Activity
 * 
 * 流程：
 * 1. 输入激活码 (至少8位)
 * 2. 输入钥匙名称
 * 3. 处理激活 (显示进度)
 * 4. 显示成功/失败结果
 */
@AndroidEntryPoint
class AddKeyActivity : AppCompatActivity() {

    private lateinit var binding: ActivityAddKeyBinding
    private val viewModel: AddKeyViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityAddKeyBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupToolbar()
        setupViews()
        observeState()
    }

    private fun setupToolbar() {
        setSupportActionBar(binding.toolbar)
        supportActionBar?.apply {
            setDisplayHomeAsUpEnabled(true)
            setDisplayShowHomeEnabled(true)
            title = getString(R.string.add_key)
        }
    }

    private fun setupViews() {
        // 激活码输入
        binding.editActivationCode.doAfterTextChanged { text ->
            viewModel.setActivationCode(text?.toString() ?: "")
            updateButtonState()
        }

        binding.editActivationCode.setOnEditorActionListener { _, actionId, _ ->
            if (actionId == EditorInfo.IME_ACTION_NEXT) {
                viewModel.nextStep()
                true
            } else false
        }

        // 钥匙名称输入
        binding.editKeyName.doAfterTextChanged { text ->
            viewModel.setKeyName(text?.toString() ?: "")
            updateButtonState()
        }

        binding.editKeyName.setOnEditorActionListener { _, actionId, _ ->
            if (actionId == EditorInfo.IME_ACTION_DONE) {
                if (viewModel.canProceed()) {
                    viewModel.nextStep()
                }
                true
            } else false
        }

        // 下一步按钮
        binding.btnNext.setOnClickListener {
            viewModel.nextStep()
        }

        // 上一步按钮
        binding.btnBack.setOnClickListener {
            viewModel.prevStep()
        }

        // 完成按钮
        binding.btnDone.setOnClickListener {
            finish()
        }

        // 重试按钮
        binding.btnRetry.setOnClickListener {
            viewModel.reset()
        }
    }

    private fun observeState() {
        lifecycleScope.launch {
            viewModel.addKeyStep.collectLatest { step ->
                updateStepUI(step)
            }
        }

        lifecycleScope.launch {
            viewModel.addKeyState.collectLatest { state ->
                when (state) {
                    is UiState.Loading -> showLoading()
                    is UiState.Success -> showSuccess(state.data.name)
                    is UiState.Error -> showError(state.message)
                    else -> {}
                }
            }
        }
    }

    private fun updateStepUI(step: AddKeyStep) {
        // 隐藏所有步骤
        binding.layoutStepCode.visibility = View.GONE
        binding.layoutStepName.visibility = View.GONE
        binding.layoutProcessing.visibility = View.GONE
        binding.layoutSuccess.visibility = View.GONE
        binding.layoutFailed.visibility = View.GONE

        // 更新步骤指示器
        binding.stepIndicator.text = when (step) {
            AddKeyStep.ENTER_CODE -> "1/2"
            AddKeyStep.ENTER_NAME -> "2/2"
            else -> ""
        }

        when (step) {
            AddKeyStep.ENTER_CODE -> {
                binding.layoutStepCode.visibility = View.VISIBLE
                binding.btnBack.visibility = View.GONE
                binding.btnNext.visibility = View.VISIBLE
                binding.btnNext.text = getString(R.string.next)
                binding.editActivationCode.requestFocus()
            }
            AddKeyStep.ENTER_NAME -> {
                binding.layoutStepName.visibility = View.VISIBLE
                binding.btnBack.visibility = View.VISIBLE
                binding.btnNext.visibility = View.VISIBLE
                binding.btnNext.text = getString(R.string.add_key)
                binding.editKeyName.requestFocus()
            }
            AddKeyStep.PROCESSING -> {
                binding.layoutProcessing.visibility = View.VISIBLE
                binding.btnBack.visibility = View.GONE
                binding.btnNext.visibility = View.GONE
            }
            AddKeyStep.SUCCESS -> {
                binding.layoutSuccess.visibility = View.VISIBLE
                binding.btnDone.visibility = View.VISIBLE
            }
            AddKeyStep.FAILED -> {
                binding.layoutFailed.visibility = View.VISIBLE
                binding.btnRetry.visibility = View.VISIBLE
            }
        }

        updateButtonState()
    }

    private fun updateButtonState() {
        binding.btnNext.isEnabled = viewModel.canProceed()
    }

    private fun showLoading() {
        binding.layoutProcessing.visibility = View.VISIBLE
    }

    private fun showSuccess(keyName: String) {
        binding.layoutSuccess.visibility = View.VISIBLE
        binding.textSuccessMessage.text = getString(R.string.add_key_success, keyName)
        binding.btnDone.visibility = View.VISIBLE
    }

    private fun showError(message: String) {
        binding.layoutFailed.visibility = View.VISIBLE
        binding.textErrorMessage.text = message
        binding.btnRetry.visibility = View.VISIBLE
        Toast.makeText(this, message, Toast.LENGTH_LONG).show()
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

    override fun onBackPressed() {
        // 在处理中或结果页面不允许返回
        if (viewModel.addKeyStep.value == AddKeyStep.PROCESSING ||
            viewModel.addKeyStep.value == AddKeyStep.SUCCESS ||
            viewModel.addKeyStep.value == AddKeyStep.FAILED) {
            return
        }
        super.onBackPressed()
    }
}
