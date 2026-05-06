/**
 * DigitalKey App - 钥匙详情 Activity
 */
package com.digitalkey.app.key

import android.content.Intent
import android.os.Bundle
import android.view.MenuItem
import android.view.View
import android.widget.Toast
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.digitalkey.app.R
import com.digitalkey.app.data.model.KeyModel
import com.digitalkey.app.data.model.KeyStatus
import com.digitalkey.app.data.model.Permission
import com.digitalkey.app.data.model.PermissionType
import com.digitalkey.app.databinding.ActivityKeyDetailBinding
import com.digitalkey.app.home.VehicleControlActivity
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import java.text.SimpleDateFormat
import java.util.*

/**
 * 钥匙详情 Activity
 * 
 * 功能：
 * - 显示钥匙基本信息（名称、车辆、状态）
 * - 显示钥匙权限列表
 * - 显示发放者信息
 * - 显示有效期限
 * - 点击控制按钮跳转车控界面
 * - 编辑钥匙名称
 */
@AndroidEntryPoint
class KeyDetailActivity : AppCompatActivity() {

    companion object {
        const val EXTRA_KEY_ID = "extra_key_id"
        const val EXTRA_KEY_NAME = "extra_key_name"
    }

    private lateinit var binding: ActivityKeyDetailBinding
    private val viewModel: KeyListViewModel by viewModels()

    private var keyId: String = ""
    private var currentKey: KeyModel? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityKeyDetailBinding.inflate(layoutInflater)
        setContentView(binding.root)

        keyId = intent.getStringExtra(EXTRA_KEY_ID) ?: run {
            finish()
            return
        }

        setupToolbar()
        setupClickListeners()
        observeState()
        loadKeyDetails()
    }

    private fun setupToolbar() {
        setSupportActionBar(binding.toolbar)
        supportActionBar?.apply {
            setDisplayHomeAsUpEnabled(true)
            setDisplayShowHomeEnabled(true)
            title = intent.getStringExtra(EXTRA_KEY_NAME) ?: getString(R.string.key_detail)
        }
    }

    private fun setupClickListeners() {
        binding.btnControl.setOnClickListener {
            currentKey?.let { key ->
                val intent = Intent(this, VehicleControlActivity::class.java).apply {
                    putExtra(VehicleControlActivity.EXTRA_VEHICLE_ID, key.vehicleId)
                    putExtra(VehicleControlActivity.EXTRA_VEHICLE_NAME, key.vehicleName)
                }
                startActivity(intent)
            }
        }

        binding.btnShare.setOnClickListener {
            currentKey?.let { key ->
                val intent = Intent(this, ShareKeyActivity::class.java).apply {
                    putExtra(ShareKeyActivity.EXTRA_KEY_ID, key.id)
                    putExtra(ShareKeyActivity.EXTRA_KEY_NAME, key.name)
                }
                startActivity(intent)
            }
        }

        binding.btnEditName.setOnClickListener {
            showEditNameDialog()
        }

        binding.btnDelete.setOnClickListener {
            showDeleteConfirm()
        }
    }

    private fun observeState() {
        lifecycleScope.launch {
            viewModel.keyListState.collectLatest { state ->
                when (state) {
                    is com.digitalkey.app.data.model.UiState.Success -> {
                        val key = state.data.find { it.id == keyId }
                        if (key != null) {
                            currentKey = key
                            displayKeyDetails(key)
                        }
                    }
                    else -> {}
                }
            }
        }
    }

    private fun loadKeyDetails() {
        viewModel.loadKeys()
    }

    private fun displayKeyDetails(key: KeyModel) {
        // 基本信息
        binding.textKeyName.text = key.name
        binding.textVehicleName.text = key.vehicleName
        binding.textVehiclePlate.text = key.vehiclePlate
        binding.textIssuer.text = key.issuerName

        // 状态
        val statusText = when (key.status) {
            KeyStatus.ACTIVE -> getString(R.string.status_active)
            KeyStatus.INACTIVE -> getString(R.string.status_inactive)
            KeyStatus.EXPIRED -> getString(R.string.status_expired)
            KeyStatus.PENDING -> getString(R.string.status_pending)
            KeyStatus.REVOKED -> getString(R.string.status_revoked)
        }
        binding.chipStatus.text = statusText
        binding.chipStatus.setChipBackgroundColorResource(
            when (key.status) {
                KeyStatus.ACTIVE -> R.color.status_active
                KeyStatus.INACTIVE, KeyStatus.EXPIRED, KeyStatus.REVOKED -> R.color.status_inactive
                KeyStatus.PENDING -> R.color.status_pending
            }
        )

        // 有效期限
        if (key.expiresAt != null) {
            binding.layoutExpires.visibility = View.VISIBLE
            binding.textExpiresAt.text = formatDateTime(key.expiresAt)
        } else {
            binding.layoutExpires.visibility = View.GONE
        }

        // 发放时间
        binding.textIssuedAt.text = formatDateTime(key.issuedAt)

        // 权限列表
        displayPermissions(key.permissions)
    }

    private fun displayPermissions(permissions: List<Permission>) {
        binding.layoutPermissions.removeAllViews()
        permissions.filter { it.granted }.forEach { perm ->
            val permView = layoutInflater.inflate(
                R.layout.item_permission,
                binding.layoutPermissions,
                false
            ) as android.widget.TextView

            permView.text = getPermissionLabel(perm.type)
            binding.layoutPermissions.addView(permView)
        }

        if (permissions.isEmpty()) {
            binding.textNoPermissions.visibility = View.VISIBLE
        } else {
            binding.textNoPermissions.visibility = View.GONE
        }
    }

    private fun getPermissionLabel(type: PermissionType): String {
        return when (type) {
            PermissionType.UNLOCK -> getString(R.string.perm_unlock)
            PermissionType.LOCK -> getString(R.string.perm_lock)
            PermissionType.START_ENGINE -> getString(R.string.perm_start_engine)
            PermissionType.STOP_ENGINE -> getString(R.string.perm_stop_engine)
            PermissionType.OPEN_TRUNK -> getString(R.string.perm_open_trunk)
            PermissionType.OPEN_HOOD -> getString(R.string.perm_open_hood)
            PermissionType.CLIMATE_CONTROL -> getString(R.string.perm_climate)
            PermissionType.WINDOW_CONTROL -> getString(R.string.perm_window)
            PermissionType.VALET_MODE -> getString(R.string.perm_valet)
            PermissionType.SPEED_LIMIT -> getString(R.string.perm_speed_limit)
            PermissionType.GEO_FENCE -> getString(R.string.perm_geofence)
            PermissionType.REMOTE_START -> getString(R.string.perm_remote_start)
            PermissionType.CAR_FIND -> getString(R.string.perm_car_find)
            PermissionType.SHARE -> getString(R.string.perm_share)
            PermissionType.MANAGE -> getString(R.string.perm_manage)
        }
    }

    private fun formatDateTime(isoString: String): String {
        return try {
            val inputFormat = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'", Locale.US).apply {
                timeZone = TimeZone.getTimeZone("UTC")
            }
            val outputFormat = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault())
            val date = inputFormat.parse(isoString)
            outputFormat.format(date!!)
        } catch (e: Exception) {
            isoString
        }
    }

    private fun showEditNameDialog() {
        val editText = android.widget.EditText(this).apply {
            setText(currentKey?.name)
            hint = getString(R.string.hint_key_name)
        }

        androidx.appcompat.app.AlertDialog.Builder(this)
            .setTitle(R.string.edit_key_name)
            .setView(editText)
            .setPositiveButton(R.string.save) { _, _ ->
                val newName = editText.text.toString().trim()
                if (newName.isNotEmpty()) {
                    viewModel.updateKeyName(keyId, newName) { success ->
                        if (success) {
                            binding.textKeyName.text = newName
                            supportActionBar?.title = newName
                            Toast.makeText(this, R.string.update_success, Toast.LENGTH_SHORT).show()
                        } else {
                            Toast.makeText(this, R.string.update_failed, Toast.LENGTH_SHORT).show()
                        }
                    }
                }
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    private fun showDeleteConfirm() {
        androidx.appcompat.app.AlertDialog.Builder(this)
            .setTitle(R.string.delete_key_title)
            .setMessage(R.string.delete_key_confirm)
            .setPositiveButton(R.string.delete) { _, _ ->
                viewModel.deleteKey(keyId) { success ->
                    if (success) {
                        Toast.makeText(this, R.string.delete_key_success, Toast.LENGTH_SHORT).show()
                        finish()
                    } else {
                        Toast.makeText(this, R.string.delete_key_failed, Toast.LENGTH_SHORT).show()
                    }
                }
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
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
